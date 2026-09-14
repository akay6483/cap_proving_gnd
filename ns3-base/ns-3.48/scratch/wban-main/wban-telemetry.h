#ifndef WBAN_TELEMETRY_H
#define WBAN_TELEMETRY_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <fstream>
#include <string>

namespace ns3 {
namespace wban {

// ============================================================================
// TELEMETRY PASSPORT (Custom ns-3 Header)
// Attached by the Sensor, read by the LPU to calculate Delay, PDR, and Jitter.
// ============================================================================
class WbanTelemetryHeader : public Header {
public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const override;
    virtual uint32_t GetSerializedSize(void) const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream &os) const override;

    void SetData(uint16_t nodeId, uint32_t seqNum, uint8_t priority, uint64_t txTime);
    
    uint16_t GetNodeId() const;
    uint32_t GetSeqNum() const;
    uint8_t GetPriority() const;
    uint64_t GetTxTime() const;

private:
    uint16_t m_nodeId;
    uint32_t m_seqNum;
    uint8_t m_priority;
    uint64_t m_txTime; 
};

// ============================================================================
// TELEMETRY TRACE RECORDER
// ============================================================================
class WbanTelemetry {
public:
    static void Initialize(const std::string& prefix);
    static void Close();

    // 1. Source (Sensor) Application Trace
    static void RecordSensorTx(Ptr<const Packet> packet, uint32_t nodeId, uint32_t priority);

    // 2. Intermediary (Coordinator) Traces - For Throughput/Routing verification
    static void RecordCoordinatorRx(Ptr<const Packet> packet, const Address& address);
    static void RecordCoordinatorTx(Ptr<const Packet> packet, uint32_t priority);

    // 3. Sink (LPU) Trace - For End-to-End Metrics (Delay, Jitter, Goodput)
    static void RecordLpuRx(Ptr<const Packet> packet, const Address& address);

    // 4. Native ns-3 MAC Layer Drops (To calculate exact cause of PDR loss)
    static void RecordMacDrop(Ptr<const Packet> packet);

private:
    static std::ofstream m_txCsv;
    static std::ofstream m_hubCsv;
    static std::ofstream m_rxCsv;
    static std::ofstream m_dropCsv;
};

} // namespace wban
} // namespace ns3

#endif // WBAN_TELEMETRY_H