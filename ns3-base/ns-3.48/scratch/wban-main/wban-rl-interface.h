#ifndef WBAN_RL_INTERFACE_H
#define WBAN_RL_INTERFACE_H

#include "ns3/core-module.h"
#include "gcl-queue-disc.h" 
#include <cstdint>

namespace ns3 {
namespace wban {

// Exact 128-byte layout shared between C++ and Python
struct WbanShmRegion {
    // Sync flag: 0 = C++ turn, 1 = Python turn, -1 = Simulation Complete (Terminate)
    volatile int32_t python_ready; 
    int32_t _padding;              // 4-byte padding to align doubles perfectly
    
    // State metrics sent to Python
    double queueSizes[3];
    double arrivalCounts[3];
    double delaySums[3];
    double dropCounts[3];
    
    // Action metrics received from Python
    double gateRatios[3];
};

class WbanRlInterface {
public:
    WbanRlInterface();
    ~WbanRlInterface();

    void Initialize(Ptr<GclQueueDisc> gcl);
    void TriggerRlStep();
    void StopSimulation(); // NEW: Graceful termination handshake

private:
    Ptr<GclQueueDisc> m_gcl;
    int m_shmFd;
    WbanShmRegion* m_shm;
    double m_hypercycleMs;
};

} // namespace wban
} // namespace ns3

#endif