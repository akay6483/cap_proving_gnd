#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/basic-energy-source.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/energy-source-container.h"
#include "ns3/energy-source-container.h"
#include "ns3/packet-socket-helper.h"

// WBAN Specific Modules
#include "ns3/wban-helper.h"
#include "ns3/wban-net-device.h"

// Custom Application & Math Engine
#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        

#include <map>
#include <memory> 

using namespace ns3;
using namespace ns3::wban;

NS_LOG_COMPONENT_DEFINE("WbanTsnDrlTopology");

std::map<uint32_t, Ptr<WbanNetDevice>> g_wbanDevices;       
std::map<uint32_t, Ptr<WifiNetDevice>> g_backhaulDevices;   
std::map<uint32_t, Ptr<ns3::energy::BasicEnergySource>> g_nodeBatteries; 

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("WbanTsnDrlTopology", LOG_LEVEL_INFO);
    LogComponentEnable("WbanSensorApp", LOG_LEVEL_INFO); 

    NS_LOG_INFO("Initializing End-to-End Wireless IoMT Topology...");

    // ========================================================================
    // PHASE 1: NODE CREATION & MOBILITY 
    // ========================================================================
    NodeContainer allNodes;
    allNodes.Create(WBAN_HETEROGENEOUS_NETWORK.size());

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (const auto& config : WBAN_HETEROGENEOUS_NETWORK) {
        positionAlloc->Add(Vector3D(config.position.x, config.position.y, config.position.z));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);
    
    // FIX: Install raw MAC-layer socket capabilities on all nodes
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    NS_LOG_INFO("Phase 1 Complete: Nodes created and anchored to 3D Cartesian coordinates.");

    // ========================================================================
    // PHASE 2: WBAN (802.15.6) INTRA-BAN LAYER SETUP
    // ========================================================================
    WbanHelper wbanHelper;
    
    for (const auto& config : WBAN_HETEROGENEOUS_NETWORK)
    {
        // Skip LPU (Node 1) as it only uses Wi-Fi
        if (config.nodeId == 1) continue; 

        NetDeviceContainer dev = wbanHelper.Install(allNodes.Get(config.nodeId));
        Ptr<WbanNetDevice> wbanDev = DynamicCast<WbanNetDevice>(dev.Get(0));
        NS_ABORT_MSG_IF(!wbanDev, "CRITICAL ERROR: Failed to cast NetDevice to WbanNetDevice.");
        
        g_wbanDevices[config.nodeId] = wbanDev;
    }
    NS_LOG_INFO("Phase 2 Complete: Intra-BAN WBAN NetDevices installed on Coordinator and Sensors.");

    // ========================================================================
    // PHASE 3: WI-FI (802.11) INTER-BAN LAYER SETUP 
    // ========================================================================
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices;
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(0))); // Coordinator Wi-Fi
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(1))); // LPU Wi-Fi

    g_backhaulDevices[0] = DynamicCast<WifiNetDevice>(wifiDevices.Get(0));
    g_backhaulDevices[1] = DynamicCast<WifiNetDevice>(wifiDevices.Get(1));
    
    NS_LOG_INFO("Phase 3 Complete: Inter-BAN Wi-Fi Backhaul established between Coordinator and LPU.");

    // ========================================================================
    // PHASE 4: ENERGY MODEL ATTACHMENT (Virtual Batteries)
    // ========================================================================
    BasicEnergySourceHelper basicSourceHelper;
    
    for (const auto& config : WBAN_HETEROGENEOUS_NETWORK)
    {
        basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(config.initialEnergyJoules));
        ns3::energy::EnergySourceContainer nodeSource = basicSourceHelper.Install(allNodes.Get(config.nodeId));
        
        Ptr<ns3::energy::BasicEnergySource> bat = nodeSource.Get(0)->GetObject<ns3::energy::BasicEnergySource>();
        NS_ABORT_MSG_IF(!bat, "CRITICAL ERROR: Failed to attach battery to Node " << config.nodeId);
        
        g_nodeBatteries[config.nodeId] = bat;
    }
    NS_LOG_INFO("Phase 4 Complete: Virtual batteries successfully installed.");

    // ========================================================================
    // PHASE 5: APPLICATION LAYER INTEGRATION (The Traffic Generators)
    // ========================================================================
    // Grab the generic Address of the Coordinator's WBAN device
    Address coordAddress = g_wbanDevices[0]->GetAddress();

    for (const auto& config : WBAN_HETEROGENEOUS_NETWORK)
    {
        if (config.nodeId == 0 || config.nodeId == 1) {
            continue; 
        }

        // FIX: Replaced the placeholder comments with the actual variables
        std::unique_ptr<WbanTrafficGenerator> mathEngine = std::make_unique<WbanTrafficGenerator>(
            config.nodeId, 
            config.applicationIntervalSec, 
            config.maxPayloadSize, 
            config.activeProbability,
            config.trafficRatios
        );

        Ptr<WbanSensorApp> sensorApp = CreateObject<WbanSensorApp>();
        sensorApp->Setup(coordAddress, std::move(mathEngine));

        Ptr<Node> node = allNodes.Get(config.nodeId);
        node->AddApplication(sensorApp);

        sensorApp->SetStartTime(Seconds(1.0));
        sensorApp->SetStopTime(Seconds(20.0));
    }
    NS_LOG_INFO("Phase 5 Complete: Stochastic Sensor Applications deployed to Nodes 2-11.");

    // ========================================================================
    // PHASE 6: EXECUTION
    // ========================================================================
    NS_LOG_INFO("Booting Simulation Engine...");
    
    Simulator::Stop(Seconds(21.0)); 
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation Successfully Terminated.");
    return 0;
}