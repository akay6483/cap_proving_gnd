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
    SensorHierarchy baseHierarchy; 
    uint8_t maxRequestedSlots;
    uint32_t currentBufferBytes;
};

class WbanCentralScheduler {
public:
    static WbanCentralScheduler& GetInstance() {
        static WbanCentralScheduler instance;
        return instance;
    }

    void RegisterNode(uint32_t nodeId, SensorHierarchy hierarchy, uint8_t maxSlots);
    
    // This is now strictly driven by physical telemetry extracted by the Coordinator's 
    // packet sink from incoming WbanDemandTag headers, not by application-layer memory cheats.
    void UpdateNodeDemand(uint32_t nodeId, uint32_t bytesInBuffer);
    
    void ComputeMapSchedule();
    double GetTransmissionOffset(uint32_t nodeId);

private:
    WbanCentralScheduler() : m_roundRobinIndex(2) {}

    std::map<uint32_t, NodeState> m_registry;
    std::map<uint32_t, double> m_activeSchedule;
    uint32_t m_roundRobinIndex;

    // ============================================================================
    // MAC LAYER CONSTANTS (Updated for Superframe Order = 4)
    // ============================================================================
    // A standard 802.15.4 superframe consists of 16 slots.
    // We reserve 4 slots for the CAP to allow nodes to physically request bandwidth.
    const uint8_t MAX_USABLE_SLOTS = 12;
    
    // SO=4 active duration = 245.76ms. 245.76ms / 16 slots = 15.36ms per slot.
    const double SLOT_DURATION_SEC = 0.01536; 
    
    // At 250 kbps, 15.36ms allows for ~480 bytes total. 
    // Accounting for PHY/MAC headers and interframe spaces, ~240 bytes payload per slot.
    const uint32_t BYTES_PER_SLOT = 240;
};

} // namespace ns3

#endif // WBAN_CENTRAL_SCHEDULER_H