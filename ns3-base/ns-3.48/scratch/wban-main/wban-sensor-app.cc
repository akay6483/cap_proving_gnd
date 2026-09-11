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

TypeId WbanDemandTag::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::wban::WbanDemandTag")
        .SetParent<Tag>()
        .AddConstructor<WbanDemandTag>();
    return tid;
}
TypeId WbanDemandTag::GetInstanceTypeId(void) const { return GetTypeId(); }
uint32_t WbanDemandTag::GetSerializedSize(void) const { return 8; }
void WbanDemandTag::Serialize(TagBuffer i) const { i.WriteU32(m_nodeId); i.WriteU32(m_demand); }
void WbanDemandTag::Deserialize(TagBuffer i) { m_nodeId = i.ReadU32(); m_demand = i.ReadU32(); }
void WbanDemandTag::Print(std::ostream &os) const { os << "NodeId=" << m_nodeId << " Demand=" << m_demand; }

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
    : m_socket(nullptr), m_currentBufferSize(0), m_maxPayloadSize(0), m_allocatedSlots(0)
{
    m_staggerVar = CreateObject<UniformRandomVariable>();
    m_staggerVar->SetAttribute("Min", DoubleValue(0.000));
    m_staggerVar->SetAttribute("Max", DoubleValue(0.050));
}

WbanSensorApp::~WbanSensorApp() { m_socket = nullptr; }

void WbanSensorApp::Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, 
                          uint32_t maxPayloadSize, Ptr<ns3::lrwpan::LrWpanMac> mac, 
                          uint8_t channel, uint8_t requestedGtsSlots, SensorHierarchy baseHierarchy)
{
    m_peerAddress = destAddr;
    m_generator = std::move(generator);
    m_maxPayloadSize = maxPayloadSize;
    m_mac = mac;           
    m_channel = channel;   
    m_allocatedSlots = requestedGtsSlots;
    WbanCentralScheduler::GetInstance().RegisterNode(GetNode()->GetId(), baseHierarchy, requestedGtsSlots);
}

int64_t WbanSensorApp::AssignStreams(int64_t stream) { m_staggerVar->SetStream(stream); return 1; }

void WbanSensorApp::StartApplication(void)
{
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        
        PacketSocketAddress local;
        local.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        local.SetProtocol(0);
        m_socket->Bind(local);

        PacketSocketAddress remote;
        remote.SetPhysicalAddress(m_peerAddress);
        remote.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        remote.SetProtocol(0);
        m_socket->Connect(remote);
        
        // NEW: Listen directly to the network socket for the Coordinator's Sync Frame
        m_socket->SetRecvCallback(MakeCallback(&WbanSensorApp::ReceiveSyncPacket, this));
    }

    NS_LOG_INFO("Node " << GetNode()->GetId() << " | App Started. Waiting for Network Sync Frame.");

    double firstInterval = m_generator->GetInterval();
    m_generateEvent = Simulator::Schedule(Seconds(firstInterval), &WbanSensorApp::GenerateData, this);
}

void WbanSensorApp::StopApplication(void)
{
    if (m_sendEvent.IsPending()) Simulator::Cancel(m_sendEvent);
    if (m_socket) m_socket->Close();
}

std::string WbanSensorApp::GetQosPriorityName(QosPriority c) const
{
    switch (c) { case QOS_CP: return "CP"; case QOS_RP: return "RP"; case QOS_DP: return "DP"; case QOS_OP: return "OP"; default: return "UNKNOWN"; }
}

void WbanSensorApp::GenerateData()
{
    QosPriority sampledType = m_generator->GenerateNextPacketType();
    uint32_t sampledSize = m_generator->GetPayloadSize();
    m_sampleBuffer.push_back({sampledType, sampledSize});
    m_currentBufferSize += sampledSize;

    NS_LOG_INFO("Node " << GetNode()->GetId() << " | Generated " << sampledSize 
                << " bytes (Total Buffer: " << m_currentBufferSize << "B). Tag: " << GetQosPriorityName(sampledType));

    double nextInterval = m_generator->GetInterval();
    m_generateEvent = Simulator::Schedule(Seconds(nextInterval), &WbanSensorApp::GenerateData, this);
}

void WbanSensorApp::SendGtsRequest()
{
    if (m_currentBufferSize == 0) return;
    Ptr<Packet> ctrlPacket = Create<Packet>(10); 
    WbanDemandTag demandTag;
    demandTag.SetNodeId(GetNode()->GetId());
    demandTag.SetDemand(m_currentBufferSize);
    ctrlPacket->AddPacketTag(demandTag);
    
    SocketPriorityTag prioTag;
    prioTag.SetPriority(static_cast<uint32_t>(QOS_CP)); 
    ctrlPacket->AddPacketTag(prioTag);
    
    m_socket->Send(ctrlPacket);
    NS_LOG_INFO("Node " << GetNode()->GetId() << " | Sent Physical GTS Request over CAP for " << m_currentBufferSize << " bytes.");
}

void WbanSensorApp::TransmitSlot() { FlushAndTransmitBuffer(); }

void WbanSensorApp::FlushAndTransmitBuffer()
{
    if (m_sampleBuffer.empty()) return; 

    QosPriority aggregateClass = QOS_OP;
    uint32_t totalPayload = 0;
    for (const auto& sample : m_sampleBuffer) {
        totalPayload += sample.size;
        if (sample.type < aggregateClass) aggregateClass = sample.type;
    }
    
    const uint32_t MAX_MAC_PAYLOAD = 89;
    uint32_t maxAllowedBytes = m_allocatedSlots * MAX_MAC_PAYLOAD;
    uint32_t bytesToSend = std::min(totalPayload, maxAllowedBytes);
    uint32_t remainingDemand = totalPayload - bytesToSend;
    
    while (bytesToSend > 0) {
        uint32_t chunkSize = std::min(bytesToSend, MAX_MAC_PAYLOAD);
        
        // NEW: Encode the priority into the very first byte of the packet buffer
        uint8_t buffer[MAX_MAC_PAYLOAD + 1] = {0};
        buffer[0] = static_cast<uint8_t>(aggregateClass); 
        
        // Create the packet with the buffer (size + 1 to account for the priority byte)
        Ptr<Packet> packet = Create<Packet>(buffer, chunkSize + 1);
        
        SocketPriorityTag priorityTag;
        priorityTag.SetPriority(static_cast<uint32_t>(aggregateClass));
        packet->AddPacketTag(priorityTag);

        WbanDemandTag demandTag;
        demandTag.SetNodeId(GetNode()->GetId());
        demandTag.SetDemand(remainingDemand);
        packet->AddPacketTag(demandTag);

        if (m_socket->Send(packet) >= 0) {
            m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(aggregateClass));
        } else {
            NS_LOG_ERROR("Node " << GetNode()->GetId() << " | Socket Send Failed!");
        }
        bytesToSend -= chunkSize;
    }
    
    m_sampleBuffer.clear();
    m_currentBufferSize = remainingDemand;
}

// NEW: Application-Layer TDMA synchronization. Bypasses the 802.15.4 MAC bugs.
void WbanSensorApp::ReceiveSyncPacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        
        // If packet size is exactly 5 bytes, it's the Coordinator's Application Sync Frame
        if (packet->GetSize() == 5) {
            double offsetSeconds = WbanCentralScheduler::GetInstance().GetTransmissionOffset(GetNode()->GetId());
            
            if (offsetSeconds >= 0.0) {
                NS_LOG_INFO("Node " << GetNode()->GetId() << " | Sync Rx -> GRANTED TDMA offset: " << offsetSeconds << "s");
                m_sendEvent = Simulator::Schedule(Seconds(offsetSeconds), &WbanSensorApp::TransmitSlot, this);
            } else {
                NS_LOG_INFO("Node " << GetNode()->GetId() << " | Sync Rx -> Starved / Empty Buffer.");
                if (m_currentBufferSize > 0) {
                    double jitter = m_staggerVar->GetValue(); 
                    Simulator::Schedule(Seconds(jitter), &WbanSensorApp::SendGtsRequest, this);
                }
            }
        }
    }
}

} // namespace wban
} // namespace ns3
