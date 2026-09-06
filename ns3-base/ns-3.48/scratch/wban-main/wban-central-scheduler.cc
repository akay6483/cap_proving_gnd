#include "wban-central-scheduler.h"
#include "ns3/log.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("WbanCentralScheduler");

void WbanCentralScheduler::RegisterNode(uint32_t nodeId, SensorHierarchy hierarchy, uint8_t maxSlots)
{
    NodeState state;
    state.nodeId = nodeId;
    state.baseHierarchy = hierarchy; 
    state.maxRequestedSlots = maxSlots;
    
    // Initial state assumes empty buffer until the Coordinator physically receives a demand tag
    state.currentBufferBytes = 0; 

    m_registry[nodeId] = state;
    
    NS_LOG_INFO("Registered Node " << nodeId << " | Hierarchy: " << hierarchy << " | Max Slots: " << (uint32_t)maxSlots);
}

void WbanCentralScheduler::UpdateNodeDemand(uint32_t nodeId, uint32_t bytesInBuffer)
{
    auto it = m_registry.find(nodeId);
    if (it != m_registry.end()) {
        it->second.currentBufferBytes = bytesInBuffer;
    }
}

void WbanCentralScheduler::ComputeMapSchedule()
{
    m_activeSchedule.clear();
    uint8_t slotsRemaining = MAX_USABLE_SLOTS; 
    
    // Push the TDMA schedule start time forward by 4 slots to legally bypass the CAP.
    double currentOffset = SLOT_DURATION_SEC * 4.0; 
    
    std::vector<NodeState*> p0_streaming;   
    std::vector<NodeState*> p1_high_event;  
    std::vector<NodeState*> p2_routine;     
    
    // 1. FILTER & SORT DEMAND
    for (auto& pair : m_registry) {
        NodeState& state = pair.second;
        
        if (state.currentBufferBytes > 0) {
            if (state.baseHierarchy == NODE_CRITICAL_STREAM) {
                p0_streaming.push_back(&state); 
            } else if (state.baseHierarchy == NODE_HIGH_EVENT) {
                p1_high_event.push_back(&state); 
            } else if (state.baseHierarchy == NODE_ROUTINE) {
                p2_routine.push_back(&state); 
            }
        }
    }

    // 2. ALLOCATE PRIORITY 0 (Critical Streaming)
    for (NodeState* node : p0_streaming) {
        if (slotsRemaining == 0) break; 
        
        uint32_t slotsNeeded = (node->currentBufferBytes + BYTES_PER_SLOT - 1) / BYTES_PER_SLOT;
        uint8_t needed8 = static_cast<uint8_t>(std::min<uint32_t>(slotsNeeded, 255));
        uint8_t slotsGranted = std::min({needed8, node->maxRequestedSlots, slotsRemaining});

        if (slotsGranted > 0) {
            m_activeSchedule[node->nodeId] = currentOffset;
            currentOffset += (slotsGranted * SLOT_DURATION_SEC);
            slotsRemaining -= slotsGranted;
        }
    }

    // 3. ALLOCATE PRIORITY 1 (High Event / EAP Abstraction)
    for (NodeState* node : p1_high_event) {
        if (slotsRemaining == 0) break; 
        
        uint32_t slotsNeeded = (node->currentBufferBytes + BYTES_PER_SLOT - 1) / BYTES_PER_SLOT;
        uint8_t needed8 = static_cast<uint8_t>(std::min<uint32_t>(slotsNeeded, 255));
        uint8_t slotsGranted = std::min({needed8, node->maxRequestedSlots, slotsRemaining});

        if (slotsGranted > 0) {
            m_activeSchedule[node->nodeId] = currentOffset;
            currentOffset += (slotsGranted * SLOT_DURATION_SEC);
            slotsRemaining -= slotsGranted;
        }
    }

    // 4. ALLOCATE PRIORITY 2/3 (Routine Data via Round-Robin)
    if (slotsRemaining > 0 && !p2_routine.empty()) {
        
        size_t startIndex = 0;
        for (size_t i = 0; i < p2_routine.size(); i++) {
            if (p2_routine[i]->nodeId >= m_roundRobinIndex) {
                startIndex = i;
                break;
            }
        }

        size_t checkedNodes = 0;
        size_t currentIndex = startIndex;

        while (slotsRemaining > 0 && checkedNodes < p2_routine.size()) {
            NodeState* node = p2_routine[currentIndex];
            
            uint32_t slotsNeeded = (node->currentBufferBytes + BYTES_PER_SLOT - 1) / BYTES_PER_SLOT;
            uint8_t needed8 = static_cast<uint8_t>(std::min<uint32_t>(slotsNeeded, 255));
            uint8_t slotsGranted = std::min({needed8, node->maxRequestedSlots, slotsRemaining});

            if (slotsGranted > 0) {
                m_activeSchedule[node->nodeId] = currentOffset;
                currentOffset += (slotsGranted * SLOT_DURATION_SEC);
                slotsRemaining -= slotsGranted;
            }

            // Prevent node starvation. If slots run out, pin the round-robin index
            // to ensure this specific node is strictly prioritized in the next superframe.
            if (slotsRemaining == 0) {
                if (slotsGranted < needed8) {
                    m_roundRobinIndex = node->nodeId;
                } else {
                    m_roundRobinIndex = p2_routine[(currentIndex + 1) % p2_routine.size()]->nodeId;
                }
                break;
            }

            currentIndex = (currentIndex + 1) % p2_routine.size();
            checkedNodes++;
        }
        
        // If all nodes were serviced successfully and slots remain, 
        // cleanly rotate the start index to guarantee fairness.
        if (slotsRemaining > 0) {
            m_roundRobinIndex = p2_routine[(startIndex + 1) % p2_routine.size()]->nodeId;
        }
    }
}

double WbanCentralScheduler::GetTransmissionOffset(uint32_t nodeId)
{
    auto it = m_activeSchedule.find(nodeId);
    if (it != m_activeSchedule.end()) {
        return it->second; 
    }
    // Return -1.0 if the node was not granted a slot, informing the sensor app to fall back to the CAP.
    return -1.0; 
}

} // namespace ns3