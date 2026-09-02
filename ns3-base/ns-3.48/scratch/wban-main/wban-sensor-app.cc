#include "wban-sensor-app.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/packet-socket-address.h"
#include "ns3/double.h"
#include "ns3/socket.h"
#include "ns3/lr-wpan-mac.h"

namespace ns3 {
namespace wban {

NS_LOG_COMPONENT_DEFINE("WbanSensorApp");

TypeId WbanSensorApp::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::wban::WbanSensorApp")
        .SetParent<Application>()
        .SetGroupName("Wban")
        .AddConstructor<WbanSensorApp>()
        .AddTraceSource("Tx", "Packet generated and sent down to MAC layer", 
                        MakeTraceSourceAccessor(&WbanSensorApp::m_txTrace),
                        "ns3::Packet::TracedCallback");
    return tid;
}

WbanSensorApp::WbanSensorApp() 
    : m_socket(nullptr), 
      m_currentBufferSize(0),
      m_maxPayloadSize(0),
      m_macSyncLost(true),
      m_allocatedSlots(0)
{
    m_staggerVar = CreateObject<UniformRandomVariable>();
    m_staggerVar->SetAttribute("Min", DoubleValue(0.000));
    m_staggerVar->SetAttribute("Max", DoubleValue(0.050));
}

WbanSensorApp::~WbanSensorApp() 
{ 
    m_socket = nullptr; 
}

void WbanSensorApp::Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, 
                          uint32_t maxPayloadSize, Ptr<ns3::lrwpan::LrWpanMac> mac, 
                          uint8_t channel, uint8_t requestedGtsSlots)
{
    m_peerAddress = destAddr;
    m_generator = std::move(generator);
    m_maxPayloadSize = maxPayloadSize;
    m_mac = mac;           
    m_channel = channel;   
    m_allocatedSlots = requestedGtsSlots;
}

int64_t WbanSensorApp::AssignStreams(int64_t stream)
{
    m_staggerVar->SetStream(stream);
    return 1; 
}

void WbanSensorApp::StartApplication(void)
{
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        
        PacketSocketAddress local;
        local.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        m_socket->Bind(local);

        PacketSocketAddress remote;
        remote.SetPhysicalAddress(m_peerAddress);
        remote.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        remote.SetProtocol(0);
        m_socket->Connect(remote);
    }

    m_mac->SetRxOnWhenIdle(false);

    ns3::lrwpan::MlmeSyncRequestParams syncParams;
    syncParams.m_logCh = m_channel;
    syncParams.m_trackBcn = true;
    m_mac->MlmeSyncRequest(syncParams);

    double firstInterval = m_generator->GetInterval();
    m_generateEvent = Simulator::Schedule(Seconds(firstInterval), &WbanSensorApp::GenerateData, this);
}

void WbanSensorApp::StopApplication(void)
{
    if (m_sendEvent.IsPending()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}

std::string WbanSensorApp::GetQosPriorityName(QosPriority c) const
{
    switch (c) {
        case QOS_CP: return "CP";
        case QOS_RP: return "RP";
        case QOS_DP: return "DP";
        case QOS_OP: return "OP";
        default:     return "UNKNOWN";
    }
}

void WbanSensorApp::GenerateData()
{
    // Ask the math engine for the priority (CP, RP, DP, OP) and byte size of the next packet
    QosPriority sampledType = m_generator->GenerateNextPacketType();
    uint32_t sampledSize = m_generator->GetPayloadSize();

    // Store the sample in the buffer instead of sending it immediately
    m_sampleBuffer.push_back({sampledType, sampledSize});
    m_currentBufferSize += sampledSize;

    // Schedule the next sensor reading based on the node's customized jitter interval
    double nextInterval = m_generator->GetInterval();
    m_generateEvent = Simulator::Schedule(Seconds(nextInterval), &WbanSensorApp::GenerateData, this);
}


void WbanSensorApp::ExecuteSamplingCycle()
{
    if (m_macSyncLost) return; 

    QosPriority sampledType = m_generator->GenerateNextPacketType();
    uint32_t sampledSize = m_generator->GetPayloadSize();

    m_sampleBuffer.push_back({sampledType, sampledSize});
    m_currentBufferSize += sampledSize;

    if (sampledType == QOS_CP || sampledType == QOS_RP || m_currentBufferSize >= m_maxPayloadSize) 
    {
        FlushAndTransmitBuffer();
    }
}

void WbanSensorApp::TransmitSlot()
{
    if (m_macSyncLost) return; 
    
    // The TDMA slot is open. Push whatever data has accumulated to the MAC.
    FlushAndTransmitBuffer();
}

void WbanSensorApp::FlushAndTransmitBuffer()
{
    if (m_sampleBuffer.empty()) return; // Nothing to send, go back to sleep

    QosPriority aggregateClass = QOS_OP;
    uint32_t totalPayload = 0;

    // Combine all tiny sensor samples into one bulk payload for efficiency
    // The overall packet inherits the priority of the most critical sample inside it
    for (const auto& sample : m_sampleBuffer) {
        totalPayload += sample.size;
        if (sample.type < aggregateClass) aggregateClass = sample.type;
    }

    Ptr<Packet> packet = Create<Packet>(totalPayload);
    
    // Create the tag required by your TSN architecture
    SocketPriorityTag priorityTag;
    priorityTag.SetPriority(static_cast<uint32_t>(aggregateClass));
    packet->AddPacketTag(priorityTag);

    // Send down to the LrWpan MAC layer
    if (m_socket->Send(packet) >= 0) {
        m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(aggregateClass));
    }
    
    // Empty the buffer for the next cycle
    m_sampleBuffer.clear();
    m_currentBufferSize = 0;
}

// ============================================================================
// MLME MAC CALLBACK IMPLEMENTATIONS
// ============================================================================

void WbanSensorApp::OnMacStartConfirm(ns3::lrwpan::MlmeStartConfirmParams params)
{
    // Silenced to prevent terminal spam
}

void WbanSensorApp::OnMacBeaconNotify(ns3::lrwpan::MlmeBeaconNotifyIndicationParams params)
{
    if (m_macSyncLost) m_macSyncLost = false;

    uint32_t myNodeId = GetNode()->GetId();
    uint32_t cumulativeSlotsBeforeMe = 0;

    // Loop through the global network config to find out how many slots 
    // are reserved by nodes that transmit before this node.
    for (const auto& config : WBAN_NETWORK) {
        // Ignore the Coordinator (0) and LPU (1)
        if (config.isCoordinator || config.nodeId == 1) continue;
        
        // If the node ID is lower than mine, add its requested slots to my wait time
        if (config.nodeId < myNodeId) {
            cumulativeSlotsBeforeMe += config.requestedGtsSlots;
        }
    }

    // Calculate exact transmission offset based on accumulated slots
    // 1 SO=3 slot = 7.68 milliseconds
    double offsetSeconds = cumulativeSlotsBeforeMe * 0.00768; 

    // Schedule TransmitSlot to open at this precise microsecond
    m_sendEvent = Simulator::Schedule(Seconds(offsetSeconds), &WbanSensorApp::TransmitSlot, this);
}

void WbanSensorApp::OnMacSyncLoss(ns3::lrwpan::MlmeSyncLossIndicationParams params)
{
    m_macSyncLost = true;
    m_sampleBuffer.clear();
    m_currentBufferSize = 0;

    ns3::lrwpan::MlmeSyncRequestParams syncParams;
    syncParams.m_logCh = m_channel;
    syncParams.m_trackBcn = true;
    Simulator::Schedule(Seconds(0.5), &ns3::lrwpan::LrWpanMac::MlmeSyncRequest, m_mac, syncParams);
}

} // namespace wban
} // namespace ns3
