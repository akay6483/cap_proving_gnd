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
#include "ns3/ai-module.h"

#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        
#include "wban-central-scheduler.h" 
#include "wban-coordinator-app.h"
#include "wban-lpu-app.h"                    
#include "wban-telemetry.h" 
#include "tsn-helper.h"      
#include "wban-rl-interface.h"          

using namespace ns3;
using namespace ns3::wban;
using namespace ns3::lrwpan; 

NS_LOG_COMPONENT_DEFINE("WbanTsnDrlTopology");

std::map<uint32_t, Ptr<LrWpanNetDevice>> g_lrwpanDevices;       
std::map<uint32_t, Ptr<WifiNetDevice>> g_backhaulDevices;   
std::map<uint32_t, Ptr<ns3::energy::BasicEnergySource>> g_nodeBatteries; 

std::string GetQosString(uint32_t priority) {
    switch(priority) { 
        case 0: return "CP"; 
        case 1: return "RP"; 
        case 2: return "OP"; 
        default: return "UNKNOWN"; 
    }
}

void SensorTxTrace(Ptr<const Packet> packet, uint32_t nodeId, uint32_t priorityClass)
{
    WbanTelemetryHeader tHeader;
    uint32_t seqNum = 0;
    
    if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
        seqNum = tHeader.GetSeqNum();
    }
    
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Tx | Node " << nodeId 
                << " | Seq: " << seqNum << " | Prio: " << GetQosString(priorityClass) 
                << " | Size: " << packet->GetSize() << " bytes");
}

void CoordinatorRxTrace(Ptr<const Packet> packet, const Address& address)
{
    if (packet->GetSize() <= 5) return;
    
    WbanDemandTag demandTag;
    if (packet->PeekPacketTag(demandTag)) {
        WbanCentralScheduler::GetInstance().UpdateNodeDemand(
            demandTag.GetNodeId(), demandTag.GetDemand()
        );
    }
    
    WbanTelemetryHeader tHeader;
    uint32_t nodeId = 0;
    uint32_t seqNum = 0;
    
    if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
        nodeId = tHeader.GetNodeId();
        seqNum = tHeader.GetSeqNum();
    }
    
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Rx | Node 0 (Coordinator) received 802.15.4 frame | Node: " 
                << nodeId << " | Seq: " << seqNum << " | Size: " << packet->GetSize() << " bytes");
}

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

    double simDuration = 120.0; 
    
    LogComponentEnable("WbanTsnDrlTopology", LOG_LEVEL_INFO);
    LogComponentEnable("WbanSensorApp", LOG_LEVEL_INFO); 
    LogComponentEnable("WbanCentralScheduler", LOG_LEVEL_ALL);
    LogComponentEnable("WbanCoordinatorApp", LOG_LEVEL_DEBUG);
    LogComponentEnable("LpuApp", LOG_LEVEL_ALL);
    LogComponentEnable("GclQueueDisc", LOG_LEVEL_INFO);
    
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

    wifiPhy.EnablePcap("wban-wifi-coordinator", wifiDevices.Get(0));

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
    // WI-FI EDCA QoS AGGRESSIVE TUNING (Medical Tier-2 Link)
    // ======================================================================== 
    NS_LOG_INFO("Configuring aggressive Wi-Fi EDCA queues for Coordinator.");
    Ptr<WifiMac> coordMac = g_backhaulDevices[0]->GetMac();
    
    // 1. Critical Priority (CP) -> AC_VO (Voice, Tag 7)
    PointerValue ptrVO;
    coordMac->GetAttribute("VO_Txop", ptrVO);
    Ptr<QosTxop> edcaVO = ptrVO.Get<QosTxop>();
    edcaVO->SetMinCw(3);    
    edcaVO->SetMaxCw(7);    
    edcaVO->SetAifsn(2);    

    // 2. Reliable Priority (RP) -> AC_VI (Video, Tag 5)
    PointerValue ptrVI;
    coordMac->GetAttribute("VI_Txop", ptrVI);
    Ptr<QosTxop> edcaVI = ptrVI.Get<QosTxop>();
    edcaVI->SetMinCw(7);
    edcaVI->SetMaxCw(15);
    edcaVI->SetAifsn(3);

    // 3. Ordinary Priority (OP) -> AC_BE (Best Effort, Tag 0)
    PointerValue ptrBE;
    coordMac->GetAttribute("BE_Txop", ptrBE);
    Ptr<QosTxop> edcaBE = ptrBE.Get<QosTxop>();
    edcaBE->SetMinCw(15);
    edcaBE->SetMaxCw(1023);
    edcaBE->SetAifsn(7);

    // ========================================================================
    // TSN TRAFFIC CONTROL: GATE CONTROL LIST (GCL) INSTALLATION
    // ======================================================================== 
    NS_LOG_INFO("Installing TSN Gate Control List on Coordinator Egress (Wi-Fi)");
    
    TsnHelper tsnHelper;
    tsnHelper.SetNumberOfQueues(3); // 3 Queues: CP (0), RP (1), OP (2)
    
    // Weighted schedule prioritizing CP latency over OP latency.
    // Over a 10ms hypercycle, CP gate is open for 5ms, RP for 3ms, OP for 2ms.
    std::vector<double> gateWeights = {50.0, 30.0, 20.0}; 
    tsnHelper.SetScheduleWeighted(gateWeights, 10.0);
    
    QueueDiscContainer egressQdisc = tsnHelper.Install(g_backhaulDevices[0]);
    Ptr<GclQueueDisc> gcl = egressQdisc.Get(0)->GetObject<GclQueueDisc>();

    // Start the passive polling engine (50ms interval) via the Telemetry module
    WbanTelemetry::StartGclMonitoring(gcl, MilliSeconds(50));
    
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

    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&CoordinatorRxTrace));
    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&WbanTelemetry::RecordCoordinatorRx));
    coordApp->TraceConnectWithoutContext("TxToLpu", MakeCallback(&WbanTelemetry::RecordCoordinatorTx));

    Ptr<LpuApp> lpuApp = CreateObject<LpuApp>();
    lpuApp->Setup(9); 
    allNodes.Get(1)->AddApplication(lpuApp);
    lpuApp->SetStartTime(Seconds(0.0));
    lpuApp->SetStopTime(Seconds(simDuration));

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
                         mac, config.channel, config.requestedGtsSlots, config.baseHierarchy, config.maxBufferSize);
        
        sensorApp->TraceConnectWithoutContext("Tx", MakeCallback(&SensorTxTrace));
        sensorApp->TraceConnectWithoutContext("Tx", MakeCallback(&WbanTelemetry::RecordSensorTx));
        sensorApp->TraceConnectWithoutContext("Drop", MakeCallback(&WbanTelemetry::RecordAppDrop));
        
        sensorApp->SetStartTime(Seconds(1.0));
        sensorApp->SetStopTime(Seconds(simDuration));
    }

    Simulator::Schedule(Seconds(1.0), &TriggerMapSchedule, coordBroadcastSocket, destBroadcast);

    // ========================================================================
    // TELEMETRY & SIMULATION EXECUTION
    // ======================================================================== 
    
    // Prepend "scratch/wban-main/" if you want the CSVs to save directly in your project folder
    WbanTelemetry::Initialize("wban_sim");

    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDrop", 
                                  MakeCallback(&WbanTelemetry::RecordMacDrop));

    Simulator::Stop(Seconds(simDuration + 1.0)); 
    
    Ptr<wban::GclQueueDisc> gclDisc = egressQdisc.Get(0)->GetObject<wban::GclQueueDisc>();

    wban::WbanRlInterface rlAgent;
    rlAgent.Initialize(gclDisc);

    Simulator::Run();
    
    WbanTelemetry::Close();
    Simulator::Destroy();

    return 0;
}