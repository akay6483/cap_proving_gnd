#ifndef WBAN_CENTRAL_SCHEDULER_H
#define WBAN_CENTRAL_SCHEDULER_H

#include <map>
#include <vector>
#include <cstdint>
#include "ns3/simulator.h"
#include "wban-config.h"

namespace ns3 {

struct NodeState {
    uint32_t nodeId;
    SensorHierarchy baseHierarchy; // FIXED: Aligned with the Tier-1 Enum
    uint8_t maxRequestedSlots;
    uint32_t currentBufferBytes;
};

class WbanCentralScheduler {
public:
    static WbanCentralScheduler& GetInstance() {
        static WbanCentralScheduler instance;
        return instance;
    }

    // FIXED: Signature updated to use SensorHierarchy
    void RegisterNode(uint32_t nodeId, SensorHierarchy hierarchy, uint8_t maxSlots);

    void UpdateNodeDemand(uint32_t nodeId, uint32_t bytesInBuffer);
    void ComputeMapSchedule();
    double GetTransmissionOffset(uint32_t nodeId);

private:
    WbanCentralScheduler() : m_roundRobinIndex(2) {}

    std::map<uint32_t, NodeState> m_registry;
    std::map<uint32_t, double> m_activeSchedule;
    uint32_t m_roundRobinIndex;

    const uint8_t MAX_USABLE_SLOTS = 12;
    const double SLOT_DURATION_SEC = 0.00768;
    const uint32_t BYTES_PER_SLOT = 120;
};

} // namespace ns3

#endif // WBAN_CENTRAL_SCHEDULER_H