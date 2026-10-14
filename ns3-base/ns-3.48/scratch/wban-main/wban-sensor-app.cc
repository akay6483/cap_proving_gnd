#include "wban-sensor-app.h"
#include "wban-telemetry.h" 
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
                        "ns3::Packet::TracedCallback")
        .AddTraceSource("Drop", "Packet dropped due to buffer overflow (NodeId, DroppedBytes, Priority)",
                        MakeTraceSourceAccessor(&WbanSensorApp::m_dropTrace),
                        "ns3::TracedCallback<uint32_t, uint32_t, uint32_t>");
    return tid;
}

WbanSensorApp::WbanSensorApp() 
    : m_socket(nullptr), m_currentBufferSize(0), m_maxPayloadSize(0), m_maxBufferSize(512), m_seqCounter(0), m_allocatedSlots(0)
{
    m_staggerVar = CreateObject<UniformRandomVariable>();
    m_staggerVar->SetAttribute("Min", DoubleValue(0.000));
    m_staggerVar->SetAttribute("Max", DoubleValue(0.050));
}

WbanSensorApp::~WbanSensorApp() { m_socket = nullptr; }

void WbanSensorApp::Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, 
                          uint32_t maxPayloadSize, Ptr<ns3::lrwpan::LrWpanMac> mac, 
                          uint8_t channel, uint8_t requestedGtsSlots, SensorHierarchy baseHierarchy,
                          uint32_t maxBufferSize)
{
    m_peerAddress = destAddr;
    m_generator = std::move(generator);
    m_maxPayloadSize = maxPayloadSize;
    m_maxBufferSize = maxBufferSize; 
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
    switch (c) { 
        case QOS_CP: return "CP"; 
        case QOS_RP: return "RP"; 
        case QOS_OP: return "OP"; 
        default: return "UNKNOWN"; 
    }
}

void WbanSensorApp::GenerateData()
{
    QosPriority sampledType = m_generator->GenerateNextPacketType();
    uint32_t sampledSize = m_generator->GetPayloadSize();

    // ========================================================================
    // BYTE-LEVEL BUFFER CAPACITY CHECK (Drop-Tail Overflow Control)
    // ========================================================================
    if (m_currentBufferSize + sampledSize > m_maxBufferSize)
    {
        NS_LOG_WARN("Node " << GetNode()->GetId() << " | Buffer Overflow! Current Buffer (" 
                    << m_currentBufferSize << "B) + Sample (" << sampledSize << "B) > Max Buffer (" 
                    << m_maxBufferSize << "B). Dropping sample.");
        
        m_dropTrace(GetNode()->GetId(), sampledSize, static_cast<uint32_t>(sampledType));

        double nextInterval = m_generator->GetInterval();
        m_generateEvent = Simulator::Schedule(Seconds(nextInterval), &WbanSensorApp::GenerateData, this);
        return;
    }

    m_sampleBuffer.push_back({sampledType, sampledSize});
    m_currentBufferSize += sampledSize;

    NS_LOG_INFO("Node " << GetNode()->GetId() << " | Generated " << sampledSize 
                << " bytes (Total Buffer: " << m_currentBufferSize << "/" << m_maxBufferSize << "B). Tag: " << GetQosPriorityName(sampledType));

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
    
    // UPDATED: Dynamically calculate max chunk size based on config limit minus the 15-byte telemetry header.
    // This guarantees the final physical packet size never exceeds the 802.15.4 MTU.
    const uint32_t TELEMETRY_HEADER_SIZE = 15;
    uint32_t effectiveMacPayload = (m_maxPayloadSize > TELEMETRY_HEADER_SIZE) ? (m_maxPayloadSize - TELEMETRY_HEADER_SIZE) : 75;
    
    uint32_t maxAllowedBytes = m_allocatedSlots * effectiveMacPayload;
    uint32_t bytesToSend = std::min(totalPayload, maxAllowedBytes);
    uint32_t remainingDemand = totalPayload - bytesToSend;
    
    while (bytesToSend > 0) {
        uint32_t chunkSize = std::min(bytesToSend, effectiveMacPayload);
        
        Ptr<Packet> packet = Create<Packet>(chunkSize);
        
        m_seqCounter++;
        WbanTelemetryHeader tHeader;
        tHeader.SetData(GetNode()->GetId(), m_seqCounter, static_cast<uint8_t>(aggregateClass), Simulator::Now().GetTimeStep());
        
        // Appending the header increases the packet's physical size by 15 bytes.
        // E.g., If config max is 100B, chunk is 85B. 85B + 15B = 100B safe MAC delivery.
        packet->AddHeader(tHeader);
        
        SocketPriorityTag priorityTag;
        priorityTag.SetPriority(static_cast<uint32_t>(aggregateClass));
        packet->AddPacketTag(priorityTag);

        WbanDemandTag demandTag;
        demandTag.SetNodeId(GetNode()->GetId());
        demandTag.SetDemand(remainingDemand);
        packet->AddPacketTag(demandTag); // Tags do not increase physical packet size

        m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(aggregateClass));

        if (m_socket->Send(packet) < 0) {
            NS_LOG_ERROR("Node " << GetNode()->GetId() << " | Socket Send Failed!");
        }
        bytesToSend -= chunkSize;
    }
    
    m_sampleBuffer.clear();
    m_currentBufferSize = remainingDemand;
}

void WbanSensorApp::ReceiveSyncPacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        
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