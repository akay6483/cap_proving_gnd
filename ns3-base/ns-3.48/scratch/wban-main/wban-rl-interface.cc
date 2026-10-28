#include "wban-rl-interface.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

namespace ns3 {
namespace wban {

WbanRlInterface::WbanRlInterface() : m_shmFd(-1), m_shm(nullptr), m_hypercycleMs(18.0) {}

void WbanRlInterface::Initialize(Ptr<GclQueueDisc> gcl) {
    m_gcl = gcl;

    // 1. Create native POSIX Shared Memory block
    m_shmFd = shm_open("/wban_rl_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(m_shmFd, sizeof(WbanShmRegion));
    m_shm = (WbanShmRegion*)mmap(0, sizeof(WbanShmRegion), PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);

    // 2. Initialize sync flag (Give C++ control first)
    m_shm->python_ready = 0;

    // 3. Start the recurring hypercycle loop at T = 1.0s
    Simulator::Schedule(Seconds(1.0), &WbanRlInterface::TriggerRlStep, this);
    std::cout << "[RL Interface] Shared memory bridge initialized at /dev/shm/wban_rl_shm" << std::endl;
}

void WbanRlInterface::TriggerRlStep() {
    if (!m_gcl || !m_shm) return;

    // 1. Gather stats from the GCL
    GclQueueDisc::QueueStats stats = m_gcl->GetQueueStats();

    // 2. Write State to Shared Memory
    for (int i = 0; i < 3; ++i) {
        m_shm->queueSizes[i]    = static_cast<double>(stats.queueSizes[i]);
        m_shm->arrivalCounts[i] = static_cast<double>(stats.arrivalCount[i]);
        m_shm->delaySums[i]     = stats.delaySum[i];
        m_shm->dropCounts[i]    = static_cast<double>(stats.dropCount[i]);
    }

    // 3. Hand control to Python
    m_shm->python_ready = 1;

    // 4. Block ns-3 simulation thread and wait for Python to compute the action
    while (m_shm->python_ready == 1) {
        usleep(50); 
    }

    // NEW: If the simulation was stopped manually during the wait, abort safely
    if (m_shm->python_ready == -1) return;

    // 5. Read Action from Python
    std::vector<GclQueueDisc::GclEntry> newSchedule(3);
    for (int i = 0; i < 3; ++i) {
        newSchedule[i].duration = MilliSeconds(m_shm->gateRatios[i] * m_hypercycleMs);
        newSchedule[i].gatesOpen.resize(3, false);
        newSchedule[i].gatesOpen[i] = true;
    }

    // 6. Apply schedule and schedule next hypercycle step
    m_gcl->UpdateGclSchedule(newSchedule);
    m_gcl->ResetStats();

    Simulator::Schedule(MilliSeconds(m_hypercycleMs), &WbanRlInterface::TriggerRlStep, this);
}

// NEW: Graceful termination payload triggered by ns-3 right before Simulator::Stop
void WbanRlInterface::StopSimulation() {
    if (m_shm) {
        m_shm->python_ready = -1; 
        std::cout << "[RL Interface] Termination signal sent to Python agent." << std::endl;
    }
}

WbanRlInterface::~WbanRlInterface() {
    // Cleanup memory map on exit
    if (m_shm) munmap(m_shm, sizeof(WbanShmRegion));
    if (m_shmFd != -1) close(m_shmFd);
    shm_unlink("/wban_rl_shm");
}

} // namespace wban
} // namespace ns3