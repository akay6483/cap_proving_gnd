#include "gcl-queue-disc.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/simulator.h"
#include "ns3/socket.h" // Needed for SocketPriorityTag

namespace ns3 {

namespace wban {

NS_LOG_COMPONENT_DEFINE("GclQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(GclQueueDisc);

TypeId GclQueueDisc::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::GclQueueDisc")
        .SetParent<QueueDisc>()
        .SetGroupName("TrafficControl")
        .AddConstructor<GclQueueDisc>()
        .AddAttribute("MaxSize",
                      "The maximum number of packets accepted by this queue disc",
                      QueueSizeValue(QueueSize("1000p")),
                      MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                      MakeQueueSizeChecker())
        .AddAttribute("NumberOfQueues",
                      "The number of priority queues",
                      UintegerValue(4), // Default to 4 queues for WBAN (CP, RP, DP, OP)
                      MakeUintegerAccessor(&GclQueueDisc::m_nQueues),
                      MakeUintegerChecker<uint32_t>(1, 8));
    return tid;
}

GclQueueDisc::GclQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE),
      m_nQueues(4),
      m_cycleCount(0),
      m_lastStatUpdateTime(Seconds(0)),
      m_scheduleUpdatePending(false)
{
    NS_LOG_FUNCTION(this);
    m_cycleTime = Seconds(0);
    m_dropCounts.resize(8, 0);
    m_arrivalCounts.resize(8, 0);
    m_delaySum.resize(8, 0.0);
    m_dequeueCount.resize(8, 0);
}

GclQueueDisc::~GclQueueDisc() {
    NS_LOG_FUNCTION(this);
}

void GclQueueDisc::SetGclSchedule(std::vector<GclEntry> schedule) {
    m_gclSchedule = schedule;
    m_cycleTime = Seconds(0);
    for (const auto& entry : m_gclSchedule) {
        m_cycleTime += entry.duration;
    }
}

void GclQueueDisc::SetNumberOfQueues(uint32_t nQueues) {
    m_nQueues = nQueues;
}

Time GclQueueDisc::GetCycleTime(void) const { return m_cycleTime; }
uint32_t GclQueueDisc::GetQueueSize(uint32_t queueIndex) const { return GetInternalQueue(queueIndex)->GetNPackets(); }
uint32_t GclQueueDisc::GetQueueBytes(uint32_t queueIndex) const { return GetInternalQueue(queueIndex)->GetNBytes(); }
std::vector<GclQueueDisc::GclEntry> GclQueueDisc::GetCurrentSchedule() const { return m_gclSchedule; }

void GclQueueDisc::UpdateGclSchedule(std::vector<GclEntry> newSchedule) {
    m_pendingSchedule = newSchedule;
    m_scheduleUpdatePending = true;
}

GclQueueDisc::QueueStats GclQueueDisc::GetQueueStats() {
    QueueStats stats;
    stats.queueSizes.resize(m_nQueues);
    stats.arrivalCount.resize(m_nQueues);
    stats.dropCount.resize(m_nQueues);
    stats.delaySum.resize(m_nQueues);
    stats.dequeueCount.resize(m_nQueues);
    stats.totalArrivals = 0;
    stats.lastUpdateTime = Simulator::Now();

    for (uint32_t i = 0; i < m_nQueues; ++i) {
        stats.queueSizes[i]   = GetQueueSize(i);
        stats.arrivalCount[i] = m_arrivalCounts[i];
        stats.dropCount[i]    = m_dropCounts[i];
        stats.delaySum[i]     = m_delaySum[i];
        stats.dequeueCount[i] = m_dequeueCount[i];
        stats.totalArrivals  += m_arrivalCounts[i];
    }
    return stats;
}

void GclQueueDisc::ResetStats() {
    for (uint32_t i = 0; i < m_nQueues; ++i) {
        m_arrivalCounts[i] = 0;
        m_dropCounts[i]    = 0;
        m_delaySum[i]      = 0.0;
        m_dequeueCount[i]  = 0;
    }
    m_lastStatUpdateTime = Simulator::Now();
}

uint32_t GclQueueDisc::GetCycleCount() const { return m_cycleCount; }

bool GclQueueDisc::CheckConfig(void) {
    if (GetNQueueDiscClasses() > 0) return false;
    if (GetNInternalQueues() == 0) {
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        factory.Set("MaxSize", QueueSizeValue(QueueSize("1000p")));
        for (uint32_t i = 0; i < m_nQueues; i++) {
            AddInternalQueue(factory.Create<DropTailQueue<QueueDiscItem>>());
        }
    }
    return (GetNInternalQueues() == m_nQueues);
}

void GclQueueDisc::InitializeParams(void) {}

// UPDATED ENQUEUE LOGIC: Classify using SocketPriorityTag
bool GclQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item) {
    Ptr<Packet> packet = item->GetPacket();
    SocketPriorityTag priorityTag;
    
    int32_t queueIndex = m_nQueues - 1; // Default to lowest priority (OP)

    if (packet->PeekPacketTag(priorityTag)) {
        uint32_t priority = priorityTag.GetPriority();
        if (priority < m_nQueues) {
            queueIndex = priority; // Map exactly: 0->CP, 1->RP, 2->DP, 3->OP
        }
    }

    m_arrivalCounts[queueIndex]++;
    item->SetTimeStamp(Simulator::Now());

    bool retval = GetInternalQueue(queueIndex)->Enqueue(item);
    if (!retval) {
        m_dropCounts[queueIndex]++;
    }
    return retval;
}

int GclQueueDisc::GetCurrentActiveGate(void) {
    if (m_gclSchedule.empty() || m_cycleTime.IsZero()) return -1;
    
    Time now = Simulator::Now();
    Time timeInCycle = NanoSeconds(now.GetNanoSeconds() % m_cycleTime.GetNanoSeconds());
    Time elapsed = Seconds(0);
    
    for (size_t i = 0; i < m_gclSchedule.size(); ++i) {
        if (timeInCycle < elapsed + m_gclSchedule[i].duration) {
            for (size_t j = 0; j < m_gclSchedule[i].gatesOpen.size(); ++j) {
                if (m_gclSchedule[i].gatesOpen[j]) return j;
            }
            return -1;
        }
        elapsed += m_gclSchedule[i].duration;
    }
    return -1;
}

Ptr<QueueDiscItem> GclQueueDisc::DoDequeue(void) {
    Time now = Simulator::Now();
    Time timeInCycle = NanoSeconds(now.GetNanoSeconds() % m_cycleTime.GetNanoSeconds());

    if (m_scheduleUpdatePending && timeInCycle < Seconds(0.00001)) {
        m_gclSchedule = m_pendingSchedule;
        m_cycleTime = Seconds(0);
        for (const auto& entry : m_gclSchedule) m_cycleTime += entry.duration;
        m_scheduleUpdatePending = false;
        m_cycleCount++;
    }

    int activeGate = GetCurrentActiveGate();
    if (activeGate < 0) return nullptr;

    if (GetInternalQueue(activeGate)->GetNPackets() > 0) {
        Ptr<QueueDiscItem> item = GetInternalQueue(activeGate)->Dequeue();
        if (item) {
            m_delaySum[activeGate] += (Simulator::Now() - item->GetTimeStamp()).GetSeconds();
            m_dequeueCount[activeGate]++;
            return item;
        }
    }
    return nullptr;
}

Ptr<const QueueDiscItem> GclQueueDisc::DoPeek(void) {
    int activeGate = GetCurrentActiveGate();
    if (activeGate >= 0 && GetInternalQueue(activeGate)->GetNPackets() > 0) {
        return GetInternalQueue(activeGate)->Peek();
    }
    return nullptr;
}

} // namespace wban

} // namespace ns3