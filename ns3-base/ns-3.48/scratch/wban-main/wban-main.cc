#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/basic-energy-source.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-sink-helper.h" 
#include "ns3/lr-wpan-mac.h"

// Explicit inclusion for socket tags to verify Tier-2 ingress
#include "ns3/socket.h"

// Exclusively relying on standard ns-3 lr-wpan headers
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        
#include "wban-central-scheduler.h" 

using namespace ns3;
using namespace ns3::wban;
using namespace ns3::lrwpan; 

NS_LOG_COMPONENT_DEFINE("WbanTsnDrlTopology");

// Global registries to track specific layer pointers for cross-layer application bindings
std::map<uint32_t, Ptr<LrWpanNetDevice>> g_lrwpanDevices;       
std::map<uint32_t, Ptr<WifiNetDevice>> g_backhaulDevices;   
std::map<uint32_t, Ptr<ns3::energy::BasicEnergySource>> g_nodeBatteries; 

// ============================================================================
// TRACE CALLBACKS FOR LOGGING & METRICS
// ============================================================================

void SensorTxTrace(Ptr<const Packet> packet, uint32_t nodeId, uint32_t priorityClass)
{
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Tx | Node " << nodeId 
                << " | Prio (Tier-2 Tag): " << priorityClass << " | Size: " << packet->GetSize() << " bytes");
}

void CoordinatorRxTrace(Ptr<const Packet> packet, const Address& address)
{
    SocketPriorityTag tag;
    uint32_t prio = 3; 
    
    if (packet->PeekPacketTag(tag)) {
        prio = tag.GetPriority();
    }
    
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Rx | Node 0 (Coordinator) received packet | Size: " 
                << packet->GetSize() << " bytes | Extracted Prio Tag: " << prio);
}

// ============================================================================
// COORDINATOR TIMING & SCHEDULING HOOKS
// ============================================================================

void TriggerMapSchedule()
{
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Hub | Computing MAP TDMA Schedule...");
    WbanCentralScheduler::GetInstance().ComputeMapSchedule();
    Simulator::Schedule(Seconds(0.49152), &TriggerMapSchedule);
}

static bool g_panStarted = false; 

void CoordinatorStartConfirm(MlmeStartConfirmParams params)
{
    if (params.m_status == MacStatus::SUCCESS) {
        if (!g_panStarted) {
            g_panStarted = true;
            NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Node 0 | Coordinator PAN successfully started.");
            TriggerMapSchedule();
        }
    } else {
        NS_LOG_ERROR("Node 0 | PAN Coordinator failed to start.");
    }
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    double simDuration = 300.0; 
    
    // ========================================================================
    // LOGGING CONFIGURATION
    // ========================================================================
    LogComponentEnable("WbanTsnDrlTopology", LOG_LEVEL_INFO);
    LogComponentEnable("WbanSensorApp", LOG_LEVEL_INFO); 
    LogComponentEnable("WbanCentralScheduler", LOG_LEVEL_ALL); 
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
        if (config.nodeId == 1) continue; 

        NetDeviceContainer dev = lrWpanHelper.Install(allNodes.Get(config.nodeId));
        Ptr<LrWpanNetDevice> lrwpanDev = DynamicCast<LrWpanNetDevice>(dev.Get(0)); 

        Ptr<LrWpanMac> mac = lrwpanDev->GetMac();
        Ptr<LrWpanCsmaCa> csma = lrwpanDev->GetCsmaCa();

        // Restore legal CSMA limits so physical layer operates without crashing.
        csma->SetMacMinBE(3);
        csma->SetMacMaxBE(5); 
        csma->SetMacMaxCSMABackoffs(4);
        
        mac->SetPanId(unifiedPanId); 
        mac->SetShortAddress(Mac16Address(config.nodeId));
        
        if (config.isCoordinator) 
        {
            // CRITICAL FIX: Keep the Coordinator awake during the inactive superframe.
            // If it sleeps, packets will not reach the PacketSink to trigger the backhaul.
            mac->SetRxOnWhenIdle(true);

            mac->SetMlmeStartConfirmCallback(MakeCallback(&CoordinatorStartConfirm));

            MlmeStartRequestParams params;
            params.m_panCoor = true;          
            params.m_PanId = unifiedPanId;    
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
    
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));
    
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());
    
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices;
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

    PacketSocketAddress localSinkAddr;
    localSinkAddr.SetSingleDevice(g_lrwpanDevices[0]->GetIfIndex());
    localSinkAddr.SetProtocol(0); 
    
    PacketSinkHelper packetSinkHelper("ns3::PacketSocketFactory", localSinkAddr);
    ApplicationContainer sinkApp = packetSinkHelper.Install(allNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simDuration));

    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&CoordinatorRxTrace));

    for (const auto& config : WBAN_NETWORK)
    {
        if (config.nodeId == 0 || config.nodeId == 1) continue; 

        std::unique_ptr<WbanTrafficGenerator> mathEngine = std::make_unique<WbanTrafficGenerator>(
            config.nodeId, config.applicationIntervalSec, config.maxPayloadSize, 
            config.payloadJitter, config.intervalJitter, config.trafficRatios
        );

        Ptr<WbanSensorApp> sensorApp = CreateObject<WbanSensorApp>();
        
        sensorApp->AssignStreams(config.nodeId * 10);
        mathEngine->AssignStreams((config.nodeId * 10) + 1);

        Ptr<Node> node = allNodes.Get(config.nodeId);
        node->AddApplication(sensorApp);

        Ptr<LrWpanMac> mac = g_lrwpanDevices[config.nodeId]->GetMac();
        
        sensorApp->Setup(coordMacAddress, std::move(mathEngine), config.maxPayloadSize, 
                         mac, config.channel, config.requestedGtsSlots, config.baseHierarchy);
        
        sensorApp->TraceConnectWithoutContext("Tx", MakeCallback(&SensorTxTrace));
        
        mac->SetMlmeBeaconNotifyIndicationCallback(MakeCallback(&WbanSensorApp::OnMacBeaconNotify, sensorApp));
        mac->SetMlmeSyncLossIndicationCallback(MakeCallback(&WbanSensorApp::OnMacSyncLoss, sensorApp));
        mac->SetMlmeStartConfirmCallback(MakeCallback(&WbanSensorApp::OnMacStartConfirm, sensorApp));
        
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