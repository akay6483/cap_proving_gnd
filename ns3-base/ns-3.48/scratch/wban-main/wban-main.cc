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
#include "ns3/traffic-control-module.h" 

#include "wban-config.h"
#include "wban-traffic-generator.h" 
#include "wban-sensor-app.h"        
#include "wban-central-scheduler.h" 
#include "wban-coordinator-app.h"
#include "wban-lpu-app.h"                    
#include "wban-telemetry.h" 
#include "tsn-helper.h"      
#include "wban-rl-interface.h"          

#include <cstdlib> 

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
    double simTime = 300.0; 
    bool enableTsn = true;
    bool enableRl = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total duration of the simulation (seconds)", simTime);
    cmd.AddValue("tsn", "Enable RL-driven TSN GCL (true) or default Wi-Fi EDCA (false)", enableTsn);
    cmd.AddValue("rl", "Enable Python RL Agent control over TSN gates (true/false)", enableRl);
    cmd.Parse(argc, argv);
    
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
    
    wifi.SetStandard(WIFI_STANDARD_80211b); 
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
                                 "DataMode", StringValue("DsssRate1Mbps"), 
                                 "ControlMode", StringValue("DsssRate1Mbps"));
    
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); 

    NetDeviceContainer wifiDevices;
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(0))); 
    wifiDevices.Add(wifi.Install(wifiPhy, wifiMac, allNodes.Get(1))); 
    g_backhaulDevices[0] = DynamicCast<WifiNetDevice>(wifiDevices.Get(0));
    g_backhaulDevices[1] = DynamicCast<WifiNetDevice>(wifiDevices.Get(1));

    wifiPhy.EnablePcap("wban-edca", wifiDevices);

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
    // STATIC ARP INJECTION
    // ========================================================================
    Ptr<Ipv4L3Protocol> ipv4Coord = allNodes.Get(0)->GetObject<Ipv4L3Protocol>();
    Ptr<ArpCache> arpCache = ipv4Coord->GetInterface(1)->GetArpCache(); 
    
    Mac48Address lpuMac48 = Mac48Address::ConvertFrom(g_backhaulDevices[1]->GetAddress());
    ArpCache::Entry *entry = arpCache->Add(lpuIpAddress);
    entry->SetMacAddress(lpuMac48);
    entry->MarkPermanent();

    // ========================================================================
    // TSN TRAFFIC CONTROL & BOTTLENECK SHAPING (MAC-AGNOSTIC OVERLAY)
    // ======================================================================== 
    QueueDiscContainer egressQdisc;
    
    if (enableTsn) {
        NS_LOG_INFO("Installing TBF Shaper (Root) and TSN Gate Control List (Child) on Coordinator");
        
        TrafficControlHelper tchUninstall;
        tchUninstall.Uninstall(g_backhaulDevices[0]);

        TrafficControlHelper tc;
        
        // 1. Establish the Token Bucket Filter (TBF) as the Root QueueDisc
        uint16_t rootHandle = tc.SetRootQueueDisc("ns3::TbfQueueDisc", 
                                                  "Rate", StringValue("40kbps"), 
                                                  "Burst", UintegerValue(5000), 
                                                  "MaxSize", StringValue("65535B"));
        
        // 2. Add a classification band to the TBF to hold our custom GCL
        tc.AddQueueDiscClasses(rootHandle, 1, "ns3::QueueDiscClass");
        
        // 3. Set the GCL Time-Aware Shaper as the child of the TBF
        tc.AddChildQueueDisc(rootHandle, 0, "ns3::wban::GclQueueDisc", 
                             "NumberOfQueues", UintegerValue(3));
        
        // Install the nested architecture onto the Wi-Fi interface
        egressQdisc = tc.Install(g_backhaulDevices[0]);
        
        // 4. Retrieve the GCL instance by traversing the hierarchy from the Root (TBF) down to the Child (GCL)
        Ptr<QueueDisc> rootTbf = egressQdisc.Get(0);                                  // Grab the Root
        Ptr<QueueDiscClass> tbfClass = rootTbf->GetQueueDiscClass(0);                 // Grab the classification band
        Ptr<GclQueueDisc> gcl = tbfClass->GetQueueDisc()->GetObject<GclQueueDisc>();  // Grab the Child
        
        // 5. Manually configure the static 50/30/20 schedule
        std::vector<GclQueueDisc::GclEntry> schedule(3);
        
        schedule[0].duration = MilliSeconds(5.0); // 50% of 10ms for CP
        schedule[0].gatesOpen = {true, false, false};
        
        schedule[1].duration = MilliSeconds(3.0); // 30% of 10ms for RP
        schedule[1].gatesOpen = {false, true, false};
        
        schedule[2].duration = MilliSeconds(2.0); // 20% of 10ms for OP
        schedule[2].gatesOpen = {false, false, true};
        
        gcl->SetGclSchedule(schedule);
        gcl->SetUsePcp(true);

        WbanTelemetry::StartGclMonitoring(gcl, MilliSeconds(1));
    } else {
        NS_LOG_INFO("TSN Disabled: Relying on standard Wi-Fi EDCA (Baseline Mode)");
    }
    
    // ========================================================================
    // COORDINATOR SYNC SETUP & BINDINGS
    // ======================================================================== 
    
    Address coordMacAddress = Mac16Address("ff:ff");

    Ptr<Socket> coordLrWpanRxSocket = Socket::CreateSocket(allNodes.Get(0), TypeId::LookupByName("ns3::PacketSocketFactory"));
    PacketSocketAddress localLrWpanAddr;
    localLrWpanAddr.SetSingleDevice(g_lrwpanDevices[0]->GetIfIndex());
    localLrWpanAddr.SetProtocol(0); 
    coordLrWpanRxSocket->Bind(localLrWpanAddr);

    Ptr<Socket> coordWifiTxSocket = Socket::CreateSocket(allNodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    InetSocketAddress lpuInetAddr(lpuIpAddress, 9); 
    coordWifiTxSocket->Connect(lpuInetAddr);

    Ptr<WbanCoordinatorApp> coordApp = CreateObject<WbanCoordinatorApp>();
    coordApp->Setup(coordLrWpanRxSocket, coordWifiTxSocket, lpuInetAddr);

    allNodes.Get(0)->AddApplication(coordApp);
    coordApp->SetStartTime(Seconds(0.0));
    coordApp->SetStopTime(Seconds(simTime));

    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&CoordinatorRxTrace));
    coordApp->TraceConnectWithoutContext("RxFromSensor", MakeCallback(&WbanTelemetry::RecordCoordinatorRx));
    coordApp->TraceConnectWithoutContext("TxToLpu", MakeCallback(&WbanTelemetry::RecordCoordinatorTx));

    // ========================================================================
    // LPU SINK CONFIGURATION
    // ========================================================================
    Ptr<LpuApp> lpuApp = CreateObject<LpuApp>();
    lpuApp->Setup(9); 
    allNodes.Get(1)->AddApplication(lpuApp);
    lpuApp->SetStartTime(Seconds(0.0));
    lpuApp->SetStopTime(Seconds(simTime));

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
        sensorApp->SetStopTime(Seconds(simTime));
    }

    Simulator::Schedule(Seconds(1.0), &TriggerMapSchedule, coordBroadcastSocket, destBroadcast);

    // ========================================================================
    // TELEMETRY & SIMULATION EXECUTION
    // ======================================================================== 
    
    const char* homeEnv = getenv("HOME");
    std::string homePath = homeEnv ? std::string(homeEnv) : "";
    std::string csvPrefix = homePath + "/Documents/1_Projects/capstone/cap_proving_gnd/data-analysis/csv/wban_sim";
    
    WbanTelemetry::Initialize(csvPrefix);

    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDrop", 
                                  MakeCallback(&WbanTelemetry::RecordMacDrop));

    Simulator::Stop(Seconds(simTime + 1.0)); 
    
    wban::WbanRlInterface rlAgent; 

    if (enableTsn && enableRl) {
        NS_LOG_INFO("TSN and RL enabled. Mounting RL Shared Memory Bridge...");
        // Replicate the hierarchy traversal logic here for the RL Bridge
        Ptr<QueueDisc> rootTbf = egressQdisc.Get(0);
        Ptr<QueueDiscClass> tbfClass = rootTbf->GetQueueDiscClass(0);
        Ptr<wban::GclQueueDisc> gclDisc = tbfClass->GetQueueDisc()->GetObject<wban::GclQueueDisc>();
        
        rlAgent.Initialize(gclDisc);
        Simulator::Schedule(Seconds(simTime - 0.01), &wban::WbanRlInterface::StopSimulation, &rlAgent);
    } else if (enableTsn && !enableRl) {
        NS_LOG_INFO("TSN enabled, RL disabled. Running Static TSN Schedule.");
    }

    Simulator::Run();
    
    WbanTelemetry::Close();
    Simulator::Destroy();

    return 0;
}