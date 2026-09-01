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
        m_socket->Connect(remote);
    }

    m_mac->SetRxOnWhenIdle(false);

    ns3::lrwpan::MlmeSyncRequestParams syncParams;
    syncParams.m_logCh = m_channel;
    syncParams.m_trackBcn = true;
    m_mac->MlmeSyncRequest(syncParams);
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

void WbanSensorApp::FlushAndTransmitBuffer()
{
    if (m_sampleBuffer.empty()) return;

    QosPriority aggregateClass = QOS_OP;
    uint32_t totalPayload = 0;

    for (const auto& sample : m_sampleBuffer) {
        totalPayload += sample.size;
        if (sample.type < aggregateClass) aggregateClass = sample.type;
    }

    Ptr<Packet> packet = Create<Packet>(totalPayload);
    SocketPriorityTag priorityTag;
    priorityTag.SetPriority(static_cast<uint32_t>(aggregateClass));
    packet->AddPacketTag(priorityTag);

    if (m_socket->Send(packet) >= 0) {
        m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(aggregateClass));
    }
    
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

    // --- APPLICATION-LAYER TDMA SCHEDULING ---
    // The beacon marks T=0. 
    // We statically assign slots based on Node ID (Nodes 2 through 11).
    // An SO=3 slot is ~7.68 milliseconds.
    
    uint32_t nodeId = GetNode()->GetId();
    uint32_t slotMultiplier = (nodeId >= 2) ? (nodeId - 1) : 1; 
    
    double offsetSeconds = slotMultiplier * 0.00768; 

    // Schedule the packet generation for the exact microsecond this node's TDMA slot opens
    Simulator::Schedule(Seconds(offsetSeconds), &WbanSensorApp::ExecuteSamplingCycle, this);
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