#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/basic-energy-source.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-sink-helper.h" 

// Exclusively relying on standard ns-3 lr-wpan headers
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        

using namespace ns3;
using namespace ns3::wban;
using namespace ns3::lrwpan; 

NS_LOG_COMPONENT_DEFINE("WbanTsnDrlTopology");

// Global registries to track specific layer pointers for cross-layer application bindings
std::map<uint32_t, Ptr<LrWpanNetDevice>> g_lrwpanDevices;       
std::map<uint32_t, Ptr<WifiNetDevice>> g_backhaulDevices;   
std::map<uint32_t, Ptr<ns3::energy::BasicEnergySource>> g_nodeBatteries; 

// ============================================================================
// COORDINATOR MLME CALLBACKS
// ============================================================================

// Bound to Node 0's MAC to track exactly when the beacon train starts.
void CoordinatorStartConfirm(MlmeStartConfirmParams params)
{
    if (params.m_status == MacStatus::SUCCESS) {
        NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Node 0 | Coordinator PAN successfully started beacon generation.");
    } else {
        NS_LOG_ERROR("Node 0 | PAN Coordinator failed to start.");
    }
}

int main(int argc, char *argv[])
{
    // Allow command-line parsing to easily modify parameters without recompiling
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    double simDuration = 150.0; 
    
    // ========================================================================
    // LOGGING CONFIGURATION
    // ========================================================================
    // 1. Coordinator & Topology Logs
    LogComponentEnable("WbanTsnDrlTopology", LOG_LEVEL_INFO);
    // 2. Sensor Node Application Logs (Generation & TDMA triggers)
    LogComponentEnable("WbanSensorApp", LOG_LEVEL_ALL);
    // 3. Native 802.15.4 MAC Logs (Beacons and MLME states)
    LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO); 

    // ========================================================================
    // PHASE 1: NODE CREATION & MOBILITY 
    // ========================================================================
    NodeContainer allNodes;
    allNodes.Create(WBAN_NETWORK.size());

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (const auto& config : WBAN_NETWORK) {
        positionAlloc->Add(Vector3D(config.position.x, config.position.y, config.position.z));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);
    
    // PacketSocket allows us to bypass heavy IP/routing layers (IPv4/AODV) 
    // and send raw MAC frames directly from the application.
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    // ========================================================================
    // PHASE 2: LR-WPAN (802.15.4) INTRA-BAN LAYER SETUP & TIMING
    // ========================================================================
    LrWpanHelper lrWpanHelper;
    
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    channel->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    lrWpanHelper.SetChannel(channel);
    
    uint16_t unifiedPanId = 0x0001;

    for (const auto& config : WBAN_NETWORK)
    {
        // Node 1 is the external LPU. It communicates solely over Wi-Fi backhaul, 
        // so it does not receive an 802.15.4 WBAN radio.
        if (config.nodeId == 1) continue; 

        NetDeviceContainer dev = lrWpanHelper.Install(allNodes.Get(config.nodeId));
        Ptr<LrWpanNetDevice> lrwpanDev = DynamicCast<LrWpanNetDevice>(dev.Get(0));
        
        Ptr<LrWpanMac> mac = lrwpanDev->GetMac();
        mac->SetPanId(unifiedPanId); 
        mac->SetShortAddress(Mac16Address(config.nodeId)); 
        
        if (config.isCoordinator) 
        {
            // Wire the confirmation callback so we know T=0 for the beacon train
            mac->SetMlmeStartConfirmCallback(MakeCallback(&CoordinatorStartConfirm));

            MlmeStartRequestParams params;
            params.m_panCoor = true;          
            params.m_PanId = unifiedPanId;    
            
            // SUPERFRAME DUTY CYCLING MATH:
            // BO = 5: Beacon arrives every ~491.52 ms
            // SO = 3: Active superframe window is ~122.88 ms long.
            // This 122ms window comfortably fits all 10 of our Application-Layer 
            // TDMA sensor transmissions before the Coordinator goes to sleep.
            params.m_bcnOrd = 5;              
            params.m_sfrmOrd = 3;             
            params.m_logCh = config.channel;  
            
            mac->MlmeStartRequest(params); 
        } 
        
        g_lrwpanDevices[config.nodeId] = lrwpanDev;
    }

    // ========================================================================
    // PHASE 3: WI-FI BACKHAUL SETUP (TIER 2)
    // ========================================================================
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); 
    
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());
    
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices;
    // Install Wi-Fi backhaul on Node 0 (Coordinator) and Node 1 (LPU).
    // This provides the NetDevice where the custom GCL QueueDisc will eventually be installed[cite: 18].
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(0))); 
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(1))); 

    g_backhaulDevices[0] = DynamicCast<WifiNetDevice>(wifiDevices.Get(0));
    g_backhaulDevices[1] = DynamicCast<WifiNetDevice>(wifiDevices.Get(1));

    // ========================================================================
    // PHASE 4: ENERGY MODEL ATTACHMENT 
    // ========================================================================
    BasicEnergySourceHelper basicSourceHelper;
    
    for (const auto& config : WBAN_NETWORK)
    {
        basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(config.initialEnergyJoules));
        ns3::energy::EnergySourceContainer nodeSource = basicSourceHelper.Install(allNodes.Get(config.nodeId));
        g_nodeBatteries[config.nodeId] = nodeSource.Get(0)->GetObject<ns3::energy::BasicEnergySource>();
    }

    // ========================================================================
    // PHASE 5: APPLICATION LAYER INTEGRATION (WITH MLME BINDINGS)
    // ======================================================================== 
    Address coordMacAddress = g_lrwpanDevices[0]->GetAddress();

    // Set up the generic PacketSink on Node 0 to catch the incoming sensor payloads.
    // The Rx packet trace has been explicitly removed.
    PacketSocketAddress localSinkAddr;
    localSinkAddr.SetSingleDevice(g_lrwpanDevices[0]->GetIfIndex());
    localSinkAddr.SetProtocol(0); 
    
    PacketSinkHelper packetSinkHelper("ns3::PacketSocketFactory", localSinkAddr);
    ApplicationContainer sinkApp = packetSinkHelper.Install(allNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simDuration));

    // Initialize all Sensor Applications
    for (const auto& config : WBAN_NETWORK)
    {
        if (config.nodeId == 0 || config.nodeId == 1) continue; 

        // 1. Setup the math engine for dynamic payload/interval sizes
        std::unique_ptr<WbanTrafficGenerator> mathEngine = std::make_unique<WbanTrafficGenerator>(
            config.nodeId, config.applicationIntervalSec, config.maxPayloadSize, 
            config.payloadJitter, config.intervalJitter, config.trafficRatios
        );

        Ptr<WbanSensorApp> sensorApp = CreateObject<WbanSensorApp>();
        
        sensorApp->AssignStreams(config.nodeId * 10);
        mathEngine->AssignStreams((config.nodeId * 10) + 1);

        Ptr<Node> node = allNodes.Get(config.nodeId);
        node->AddApplication(sensorApp);

        // 2. Extract the standard MAC pointer to pass into the application
        Ptr<LrWpanMac> mac = g_lrwpanDevices[config.nodeId]->GetMac();
        
        // Pass MAC, Channel, and the blueprint's requested slots into the Sensor App
        // This relies entirely on the Application-Layer TDMA math fallback.
        sensorApp->Setup(coordMacAddress, std::move(mathEngine), config.maxPayloadSize, mac, config.channel, config.requestedGtsSlots);
        
        // 3. WIRE MAC MLME EVENTS DIRECTLY TO THE APPLICATION
        // This bridges the standard MAC layer to your custom WbanSensorApp, 
        // allowing the app to calculate its TDMA offsets the moment the MAC sees a beacon.
        mac->SetMlmeBeaconNotifyIndicationCallback(MakeCallback(&WbanSensorApp::OnMacBeaconNotify, sensorApp));
        mac->SetMlmeSyncLossIndicationCallback(MakeCallback(&WbanSensorApp::OnMacSyncLoss, sensorApp));
        mac->SetMlmeStartConfirmCallback(MakeCallback(&WbanSensorApp::OnMacStartConfirm, sensorApp));
        
        // Stagger application boot-up slightly to let the Coordinator establish the network at T=0
        sensorApp->SetStartTime(Seconds(1.0));
        sensorApp->SetStopTime(Seconds(simDuration));
    }

    // ========================================================================
    // PHASE 6: EXECUTION
    // ========================================================================
    Simulator::Stop(Seconds(simDuration + 1.0)); 
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}