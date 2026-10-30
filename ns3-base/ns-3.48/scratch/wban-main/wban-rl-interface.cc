#include "wban-rl-interface.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

namespace ns3 {
namespace wban {

// CHANGED: Hypercycle strictly locked to 10.0ms to prevent phase drift.
WbanRlInterface::WbanRlInterface() : m_shmFd(-1), m_shm(nullptr), m_hypercycleMs(10.0) {}

void WbanRlInterface::Initialize(Ptr<GclQueueDisc> gcl) {
    m_gcl = gcl;

    m_shmFd = shm_open("/wban_rl_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(m_shmFd, sizeof(WbanShmRegion));
    m_shm = (WbanShmRegion*)mmap(0, sizeof(WbanShmRegion), PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);

    m_shm->python_ready = 0;

    Simulator::Schedule(Seconds(1.0), &WbanRlInterface::TriggerRlStep, this);
    std::cout << "[RL Interface] Shared memory bridge initialized at /dev/shm/wban_rl_shm" << std::endl;
}

void WbanRlInterface::TriggerRlStep() {
    if (!m_gcl || !m_shm) return;

    GclQueueDisc::QueueStats stats = m_gcl->GetQueueStats();

    for (int i = 0; i < 3; ++i) {
        m_shm->queueSizes[i]    = static_cast<double>(stats.queueSizes[i]);
        m_shm->arrivalCounts[i] = static_cast<double>(stats.arrivalCount[i]);
        m_shm->delaySums[i]     = stats.delaySum[i];
        m_shm->dropCounts[i]    = static_cast<double>(stats.dropCount[i]);
    }

    m_shm->python_ready = 1;

    while (m_shm->python_ready == 1) {
        usleep(50); 
    }

    if (m_shm->python_ready == -1) return;

    std::vector<GclQueueDisc::GclEntry> newSchedule(3);
    for (int i = 0; i < 3; ++i) {
        newSchedule[i].duration = MilliSeconds(m_shm->gateRatios[i] * m_hypercycleMs);
        newSchedule[i].gatesOpen.resize(3, false);
        newSchedule[i].gatesOpen[i] = true;
    }

    m_gcl->UpdateGclSchedule(newSchedule);
    m_gcl->ResetStats();

    Simulator::Schedule(MilliSeconds(m_hypercycleMs), &WbanRlInterface::TriggerRlStep, this);
}

void WbanRlInterface::StopSimulation() {
    if (m_shm) {
        m_shm->python_ready = -1; 
        std::cout << "[RL Interface] Termination signal sent to Python agent." << std::endl;
    }
}

WbanRlInterface::~WbanRlInterface() {
    if (m_shm) munmap(m_shm, sizeof(WbanShmRegion));
    if (m_shmFd != -1) close(m_shmFd);
    shm_unlink("/wban_rl_shm");
}

} 
}