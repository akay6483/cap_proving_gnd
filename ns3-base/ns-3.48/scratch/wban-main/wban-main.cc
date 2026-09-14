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
#include "ns3/socket.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/internet-module.h"             
#include "ns3/ipv4-global-routing-helper.h"  

#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        
#include "wban-central-scheduler.h" 
#include "wban-coordinator-app.h"
#include "wban-lpu-app.h"                    
#include "wban-telemetry.h"                  // NEW: Telemetry system inclusion

using namespace ns3;
using namespace ns3::wban;
using namespace ns3::lrwpan; 

NS_LOG_COMPONENT_DEFINE("WbanTsnDrlTopology");

std::map<uint32_t, Ptr<LrWpanNetDevice>> g_lrwpanDevices;       
std::map<uint32_t, Ptr<WifiNetDevice>> g_backhaulDevices;   
std::map<uint32_t, Ptr<ns3::energy::BasicEnergySource>> g_nodeBatteries; 

// Runtime terminal log (preserves existing terminal output)
void SensorTxTrace(Ptr<const Packet> packet, uint32_t nodeId, uint32_t priorityClass)
{
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Tx | Node " << nodeId 
                << " | Prio: " << priorityClass << " | Size: " << packet->GetSize() << " bytes");
}

// Runtime terminal log (preserves existing terminal output)
void CoordinatorRxTrace(Ptr<const Packet> packet, const Address& address)
{
    if (packet->GetSize() <= 5) return;
    
    WbanDemandTag demandTag;
    if (packet->PeekPacketTag(demandTag)) {
        WbanCentralScheduler::GetInstance().UpdateNodeDemand(
            demandTag.GetNodeId(), demandTag.GetDemand()
        );
    }
    
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Rx | Node 0 (Coordinator) received 802.15.4 frame | Size: " << packet->GetSize() << " bytes");
}

// Transmits the Application-Layer Sync Frame every 0.49s
void TriggerMapSchedule(Ptr<Socket> coordBroadcastSocket, Address destBroadcast)
{
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Hub | Computing MAP TDMA Schedule & Broadcasting Sync...");
    WbanCentralScheduler::GetInstance().ComputeMapSchedule();
    
    Ptr<Packet> syncBeacon = Create<Packet>(5);
    coordBroadcastSocket->SendTo(syncBeacon, 0, destBroadcast);
    
    Simulator::Schedule(Seconds(0.49152), &TriggerMapSchedule, coordBroadcastSocket, destBroadcast);
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    double simDuration = 100.0; 
    
    LogComponentEnable("WbanTsnDrlTopology", LOG_LEVEL_INFO);
    LogComponentEnable("WbanSensorApp", LOG_LEVEL_INFO); 
    LogComponentEnable("WbanCentralScheduler", LOG_LEVEL_ALL);
    LogComponentEnable("WbanCoordinatorApp", LOG_LEVEL_DEBUG);
    LogComponentEnable("LpuApp", LOG_LEVEL_ALL);
    
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

        csma->SetMacMinBE(3);
        csma->SetMacMaxBE(5); 
        csma->SetMacMaxCSMABackoffs(4);
        
        mac->SetPanId(unifiedPanId); 
        mac->SetShortAddress(Mac16Address(config.nodeId));
         
        if (config.isCoordinator) mac->SetRxOnWhenIdle(true);
        
        g_lrwpanDevices[config.nodeId] = lrwpanDev;
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); 
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices;
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(0))); 
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(1))); 
    g_backhaulDevices[0] = DynamicCast<WifiNetDevice>(wifiDevices.Get(0));
    g_backhaulDevices[1] = DynamicCast<WifiNetDevice>(wifiDevices.Get(1));

    BasicEnergySourceHelper basicSourceHelper;
    for (const auto& config : WBAN_NETWORK)
    {
        basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(config.initialEnergyJoules));
        ns3::energy::EnergySourceContainer nodeSource = basicSourceHelper.Install(allNodes.Get(config.nodeId));
        g_nodeBatteries[config.nodeId] = nodeSource.Get(0)->GetObject<ns3::energy::BasicEnergySource>();
    }

    InternetStackHelper internet;
    internet.SetIpv6StackInstall(false); 
    internet.Install(allNodes.Get(0)); 
    internet.Install(allNodes.Get(1)); 

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    
    Ipv4InterfaceContainer wifiInterfaces = ipv4.Assign(wifiDevices);
    Ipv4Address lpuIpAddress = wifiInterfaces.GetAddress(1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ========================================================================
    // COORDINATOR SYNC SETUP & BINDINGS
    // ======================================================================== 
    
    Address coordMacAddress = Mac16Address("ff:ff");

    Ptr<Socket> coordLrWpanRxSocket = Socket::CreateSocket(allNodes.Get(0), TypeId::LookupByName("ns3::PacketSocketFactory"));
    PacketSocketAddress localLrWpanAddr;
    localLrWpanAddr.SetSingleDevice(g_lrwpanDevices[0]->GetIfIndex());
    localLrWpanAddr.SetProtocol(0); 
    coordLrWpanRxSocket->Bind(localLrWpanAddr);

    Ptr<Socket> coordUdpTxSocket = Socket::CreateSocket(allNodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    InetSocketAddress lpuUdpAddr(lpuIpAddress, 9); 
    coordUdpTxSocket->Connect(lpuUdpAddr);

    Ptr<WbanCoordinatorApp> coordApp = CreateObject<WbanCoordinatorApp>();
    coordApp->Setup(coordLrWpanRxSocket, coordUdpTxSocket, lpuUdpAddr);
    allNodes.Get(0)->AddApplication(coordApp);
    coordApp->SetStartTime(Seconds(0.0));
    coordApp->SetStopTime(Seconds(simDuration));

    // Dual-Bind Coordinator Rx: One for Terminal Log, One for CSV Trace
    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&CoordinatorRxTrace));
    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&WbanTelemetry::RecordCoordinatorRx));
    
    // Bind Coordinator Tx to CSV Trace
    coordApp->TraceConnectWithoutContext("TxToLpu", MakeCallback(&WbanTelemetry::RecordCoordinatorTx));

    Ptr<LpuApp> lpuApp = CreateObject<LpuApp>();
    lpuApp->Setup(9); 
    allNodes.Get(1)->AddApplication(lpuApp);
    lpuApp->SetStartTime(Seconds(0.0));
    lpuApp->SetStopTime(Seconds(simDuration));

    // Bind LPU Rx to CSV Trace
    lpuApp->TraceConnectWithoutContext("Rx", MakeCallback(&WbanTelemetry::RecordLpuRx));

    Ptr<Socket> coordBroadcastSocket = Socket::CreateSocket(allNodes.Get(0), TypeId::LookupByName("ns3::PacketSocketFactory"));
    PacketSocketAddress destBroadcast;
    destBroadcast.SetSingleDevice(g_lrwpanDevices[0]->GetIfIndex());
    destBroadcast.SetPhysicalAddress(Mac16Address("ff:ff")); 
    destBroadcast.SetProtocol(0);

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
        
        // Dual-Bind Sensor Tx: One for Terminal Log, One for CSV Trace
        sensorApp->TraceConnectWithoutContext("Tx", MakeCallback(&SensorTxTrace));
        sensorApp->TraceConnectWithoutContext("Tx", MakeCallback(&WbanTelemetry::RecordSensorTx));
        
        sensorApp->SetStartTime(Seconds(1.0));
        sensorApp->SetStopTime(Seconds(simDuration));
    }

    Simulator::Schedule(Seconds(1.0), &TriggerMapSchedule, coordBroadcastSocket, destBroadcast);

    // ========================================================================
    // TELEMETRY & SIMULATION EXECUTION
    // ======================================================================== 
    
    // Initialize the CSV files before the simulation starts
    WbanTelemetry::Initialize("wban_sim");

    // Globally hook into ALL 802.15.4 MAC Drop events to log exact packet loss
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDrop", 
                                  MakeCallback(&WbanTelemetry::RecordMacDrop));

    Simulator::Stop(Seconds(simDuration + 1.0)); 
    Simulator::Run();
    
    // Close the CSV files to flush buffers safely before destroying the simulator
    WbanTelemetry::Close();
    Simulator::Destroy();

    return 0;
}