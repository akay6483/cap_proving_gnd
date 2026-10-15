#include "wban-telemetry.h"
#include "ns3/simulator.h"

namespace ns3 {
namespace wban {

// ============================================================================
// WBAN TELEMETRY HEADER IMPLEMENTATION
// ============================================================================
NS_OBJECT_ENSURE_REGISTERED(WbanTelemetryHeader);

TypeId WbanTelemetryHeader::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::wban::WbanTelemetryHeader")
        .SetParent<Header>()
        .AddConstructor<WbanTelemetryHeader>();
    return tid;
}

TypeId WbanTelemetryHeader::GetInstanceTypeId(void) const { return GetTypeId(); }

uint32_t WbanTelemetryHeader::GetSerializedSize(void) const { return 15; } 

void WbanTelemetryHeader::Serialize(Buffer::Iterator start) const {
    start.WriteHtonU16(m_nodeId);
    start.WriteHtonU32(m_seqNum);
    start.WriteU8(m_priority);
    start.WriteHtonU64(m_txTime);
}

uint32_t WbanTelemetryHeader::Deserialize(Buffer::Iterator start) {
    m_nodeId = start.ReadNtohU16();
    m_seqNum = start.ReadNtohU32();
    m_priority = start.ReadU8();
    m_txTime = start.ReadNtohU64();
    return GetSerializedSize();
}

void WbanTelemetryHeader::Print(std::ostream &os) const {
    os << "NodeId=" << m_nodeId << " Seq=" << m_seqNum << " Prio=" << (uint32_t)m_priority;
}

void WbanTelemetryHeader::SetData(uint16_t nodeId, uint32_t seqNum, uint8_t priority, uint64_t txTime) {
    m_nodeId = nodeId; m_seqNum = seqNum; m_priority = priority; m_txTime = txTime;
}
uint16_t WbanTelemetryHeader::GetNodeId() const { return m_nodeId; }
uint32_t WbanTelemetryHeader::GetSeqNum() const { return m_seqNum; }
uint8_t WbanTelemetryHeader::GetPriority() const { return m_priority; }
uint64_t WbanTelemetryHeader::GetTxTime() const { return m_txTime; }


// ============================================================================
// WBAN TELEMETRY RECORDER IMPLEMENTATION
// ============================================================================
std::ofstream WbanTelemetry::m_txCsv;
std::ofstream WbanTelemetry::m_hubCsv;
std::ofstream WbanTelemetry::m_rxCsv;
std::ofstream WbanTelemetry::m_dropCsv;
std::ofstream WbanTelemetry::m_queueCsv;

Ptr<GclQueueDisc> WbanTelemetry::m_gclDisc = nullptr;
Time WbanTelemetry::m_pollInterval = Seconds(0);

void WbanTelemetry::Initialize(const std::string& prefix) {
    m_txCsv.open(prefix + "_tx.csv");
    m_txCsv << "Time_s,NodeID,SeqNum,Priority,Size_bytes\n";

    m_hubCsv.open(prefix + "_hub.csv");
    m_hubCsv << "Time_s,Event,NodeID,SeqNum,Priority,Size_bytes\n";

    m_rxCsv.open(prefix + "_lpu_rx.csv");
    m_rxCsv << "Time_s,NodeID,SeqNum,Priority,E2EDelay_s,Size_bytes\n";

    m_dropCsv.open(prefix + "_drops.csv");
    m_dropCsv << "Time_s,Layer,NodeID,Priority,Size_bytes\n";

    // EXPANDED GCL METRICS
    m_queueCsv.open(prefix + "_queues.csv");
    m_queueCsv << "Time_s,CP_Size,RP_Size,OP_Size,CP_Drops,RP_Drops,OP_Drops,CP_Delay,RP_Delay,OP_Delay,CP_Arrivals,RP_Arrivals,OP_Arrivals\n";
}

void WbanTelemetry::Close() {
    if(m_txCsv.is_open()) m_txCsv.close();
    if(m_hubCsv.is_open()) m_hubCsv.close();
    if(m_rxCsv.is_open()) m_rxCsv.close();
    if(m_dropCsv.is_open()) m_dropCsv.close();
    if(m_queueCsv.is_open()) m_queueCsv.close();
}

void WbanTelemetry::RecordSensorTx(Ptr<const Packet> packet, uint32_t nodeId, uint32_t priority) {
    WbanTelemetryHeader header;
    if (packet->GetSize() >= 15 && packet->PeekHeader(header)) {
        m_txCsv << Simulator::Now().GetSeconds() << "," << nodeId << "," << header.GetSeqNum() << "," 
                << priority << "," << packet->GetSize() << "\n";
    }
}

void WbanTelemetry::RecordCoordinatorRx(Ptr<const Packet> packet, const Address& address) {
    if (packet->GetSize() <= 5) return;
    WbanTelemetryHeader header;
    uint32_t nodeId = 0;
    uint32_t seqNum = 0;
    
    if (packet->GetSize() >= 15 && packet->PeekHeader(header)) {
        nodeId = header.GetNodeId();
        seqNum = header.GetSeqNum();
    }
    m_hubCsv << Simulator::Now().GetSeconds() << ",Rx_802.15.4," << nodeId << "," 
             << seqNum << ",N/A," << packet->GetSize() << "\n";
}

void WbanTelemetry::RecordCoordinatorTx(Ptr<const Packet> packet, uint32_t priority) {
    WbanTelemetryHeader header;
    uint32_t nodeId = 0;
    uint32_t seqNum = 0;
    
    if (packet->GetSize() >= 15 && packet->PeekHeader(header)) {
        nodeId = header.GetNodeId();
        seqNum = header.GetSeqNum();
    }
    m_hubCsv << Simulator::Now().GetSeconds() << ",Tx_WiFi," << nodeId << "," 
             << seqNum << "," << priority << "," << packet->GetSize() << "\n";
}

void WbanTelemetry::RecordLpuRx(Ptr<const Packet> packet, const Address& address) {
    WbanTelemetryHeader header;
    if (packet->GetSize() >= 15 && packet->PeekHeader(header)) {
        double rxTime = Simulator::Now().GetSeconds();
        double txTime = Time(header.GetTxTime()).GetSeconds();
        
        m_rxCsv << rxTime << "," << header.GetNodeId() << "," << header.GetSeqNum() << "," 
                << (uint32_t)header.GetPriority() << "," << (rxTime - txTime) << "," << packet->GetSize() << "\n";
    }
}

void WbanTelemetry::RecordMacDrop(Ptr<const Packet> packet) {
    WbanTelemetryHeader header;
    uint32_t nodeId = 0;
    uint32_t priority = 2; 
    
    if (packet->GetSize() >= 15 && packet->PeekHeader(header)) {
        nodeId = header.GetNodeId();
        priority = header.GetPriority();
        if (priority > 2) priority = 2;
    }
    m_dropCsv << Simulator::Now().GetSeconds() << ",MAC," << nodeId << "," 
              << priority << "," << packet->GetSize() << "\n";
}

void WbanTelemetry::RecordAppDrop(uint32_t nodeId, uint32_t droppedBytes, uint32_t priority) {
    m_dropCsv << Simulator::Now().GetSeconds() << ",APP," << nodeId << "," 
              << priority << "," << droppedBytes << "\n";
}

// ============================================================================
// GCL PASSIVE POLLING ENGINE
// ============================================================================
void WbanTelemetry::StartGclMonitoring(Ptr<GclQueueDisc> gclDisc, Time pollInterval) {
    m_gclDisc = gclDisc;
    m_pollInterval = pollInterval;
    
    // Kick off the continuous recursive loop
    Simulator::Schedule(m_pollInterval, &WbanTelemetry::DoPollGclStats);
}

void WbanTelemetry::DoPollGclStats() {
    if (!m_gclDisc) return;
    
    // Passively read the metrics without resetting them
    GclQueueDisc::QueueStats stats = m_gclDisc->GetQueueStats();

    // Map metrics: Index 0 (CP), 1 (RP), 2 (OP)
    m_queueCsv << Simulator::Now().GetSeconds() << "," 
               << stats.queueSizes[0]   << "," << stats.queueSizes[1]   << "," << stats.queueSizes[2]   << ","
               << stats.dropCount[0]    << "," << stats.dropCount[1]    << "," << stats.dropCount[2]    << ","
               << stats.delaySum[0]     << "," << stats.delaySum[1]     << "," << stats.delaySum[2]     << ","
               << stats.arrivalCount[0] << "," << stats.arrivalCount[1] << "," << stats.arrivalCount[2] << "\n";

    // Re-schedule the next poll seamlessly
    Simulator::Schedule(m_pollInterval, &WbanTelemetry::DoPollGclStats);
}

} // namespace wban
} // namespace ns3