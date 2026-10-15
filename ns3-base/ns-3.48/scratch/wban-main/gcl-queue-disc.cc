/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * GCL Queue Disc Implementation - IEEE 802.1Qbv Time-Aware Shaper
 *
 * Place in: contrib/tsn-switch/model/gcl-queue-disc.cc
 */

#include "gcl-queue-disc.h"             // own header: GclEntry, QueueStats, class declaration
#include "ns3/log.h"                    // NS_LOG_* macros
#include "ns3/object-factory.h"         // ObjectFactory: used to create internal DropTailQueues dynamically
#include "ns3/drop-tail-queue.h"        // DropTailQueue<QueueDiscItem>: the per-class FIFO buffer
#include "ns3/simulator.h"              // Simulator::Now(): wall-clock time inside simulation
#include "ns3/socket.h"                 // Needed for SocketPriorityTag

namespace ns3
{
namespace wban
{

NS_LOG_COMPONENT_DEFINE("GclQueueDisc");       // registers "GclQueueDisc" as a log component — enable with NS_LOG=GclQueueDisc
NS_OBJECT_ENSURE_REGISTERED(GclQueueDisc);     // registers class with ns-3 type system so it can be created by name

// ─── TypeId / attribute registration ──────────────────────────────────────────
TypeId
GclQueueDisc::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::GclQueueDisc")
            .SetParent<QueueDisc>()                         // inherits ns-3 QueueDisc interface (DoEnqueue/DoDequeue/DoPeek)
            .SetGroupName("TrafficControl")                 // groups this disc with other TC objects in documentation
            .AddConstructor<GclQueueDisc>()                 // allows creation via ObjectFactory::Create<GclQueueDisc>()
            .AddAttribute("MaxSize",                        // attribute: total packet capacity of this disc
                          "The maximum number of packets accepted by this queue disc",
                          QueueSizeValue(QueueSize("1000p")),                          // default 1000 packets
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker())
            .AddAttribute("NumberOfQueues",                 // attribute: how many priority classes (1–8)
                          "The number of priority queues",
                          UintegerValue(3),                 // default 3: CP / RP / OP
                          MakeUintegerAccessor(&GclQueueDisc::m_nQueues),
                          MakeUintegerChecker<uint32_t>(1, 8));
    return tid;
}

// ─── Constructor ──────────────────────────────────────────────────────────────
GclQueueDisc::GclQueueDisc()
    // FIX: Unlocked the queue disc to support multiple independent priority bands
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES), 
      m_nQueues(3),                  // default to 3 queues (CP, RP, OP)
      m_cycleCount(0),               // number of completed GCL cycles (incremented on schedule swap)
      m_lastStatUpdateTime(Seconds(0)), // timestamp of last ResetStats() call
      m_scheduleUpdatePending(false) // no pending schedule swap at construction
{
    NS_LOG_FUNCTION(this);
    m_cycleTime = Seconds(0);          // will be computed as sum of GclEntry durations when schedule is set
    m_dropCounts.resize(8, 0);         // pre-allocate for up to 8 queues; zeroed
    m_arrivalCounts.resize(8, 0);      // same — tracks packets enqueued per class since last ResetStats()
    m_delaySum.resize(8, 0.0);         // per-queue HOL waiting-time sum (s) since last ResetStats()
    m_dequeueCount.resize(8, 0);       // per-queue dequeued packet count since last ResetStats()
}

GclQueueDisc::~GclQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

// ─── SetGclSchedule ───────────────────────────────────────────────────────────
// Called once at simulation start (via TsnHelper) to set the initial static schedule.
// After that, the RL agent uses UpdateGclSchedule() to change it dynamically.
void
GclQueueDisc::SetGclSchedule(std::vector<GclEntry> schedule)
{
    NS_LOG_FUNCTION(this);

    m_gclSchedule = schedule;          // replace the active schedule immediately (safe at t=0 before sim runs)
    m_cycleTime = Seconds(0);          // reset and recompute cycle time

    for (const auto& entry : m_gclSchedule)
    {
        m_cycleTime += entry.duration; // cycle time = sum of all slot durations (e.g. 3 × 1ms = 3ms)
    }

    NS_LOG_INFO("GCL Schedule configured:");
    NS_LOG_INFO("  Number of entries: " << m_gclSchedule.size());
    NS_LOG_INFO("  Cycle time: " << m_cycleTime.GetMilliSeconds() << " ms");

    for (size_t i = 0; i < m_gclSchedule.size(); ++i)
    {
        NS_LOG_INFO("  Entry " << i << ": duration=" << m_gclSchedule[i].duration.GetMilliSeconds()
                               << "ms, gates=" << m_gclSchedule[i].gatesOpen.size()); // log each slot's duration and gate count
    }
}

// ─── SetNumberOfQueues ────────────────────────────────────────────────────────
// Must match the number of GclEntry gate vectors and the number of traffic classes.
void
GclQueueDisc::SetNumberOfQueues(uint32_t nQueues)
{
    NS_LOG_FUNCTION(this << nQueues);
    NS_ASSERT_MSG(nQueues >= 1 && nQueues <= 8, "Number of queues must be between 1 and 8");
    m_nQueues = nQueues;
}

// ─── GetCycleTime ─────────────────────────────────────────────────────────────
// Returns the total GCL cycle duration (sum of all slot durations). Used by
// TsnDynaTasEnv to normalise the tau vector in the RL observation.
Time
GclQueueDisc::GetCycleTime(void) const
{
    return m_cycleTime;
}

// ─── GetQueueSize ─────────────────────────────────────────────────────────────
// Returns current packet count in a specific priority queue.
// Used by TsnDynaTasEnv::GetObservation() to build the RL state vector.
uint32_t
GclQueueDisc::GetQueueSize(uint32_t queueIndex) const
{
    NS_ASSERT_MSG(queueIndex < GetNInternalQueues(), "Queue index out of bounds");
    return GetInternalQueue(queueIndex)->GetNPackets(); // asks the DropTailQueue directly
}

// ─── GetQueueBytes ────────────────────────────────────────────────────────────
// Returns current byte occupancy of a specific priority queue.
// Included in RL observation as a second capacity signal alongside packet count.
uint32_t
GclQueueDisc::GetQueueBytes(uint32_t queueIndex) const
{
    NS_ASSERT_MSG(queueIndex < GetNInternalQueues(), "Queue index out of bounds");
    return GetInternalQueue(queueIndex)->GetNBytes();
}

// ─── GetCurrentSchedule ───────────────────────────────────────────────────────
// Returns a copy of the active GCL schedule.
// Used by TsnDynaTasEnv::ComputeTauVector() and dynatas-sim.cc::LogQueueStats().
std::vector<GclQueueDisc::GclEntry>
GclQueueDisc::GetCurrentSchedule() const
{
    return m_gclSchedule;
}

// ─── UpdateGclSchedule ────────────────────────────────────────────────────────
// Called by TsnDynaTasEnv::ExecuteActions() after the RL agent produces an action.
// Does NOT apply the new schedule immediately — stores it in m_pendingSchedule and
// sets a flag so DoDequeue() can apply it atomically at the next cycle boundary.
// This prevents a mid-slot schedule change that would corrupt gate timing.
void
GclQueueDisc::UpdateGclSchedule(std::vector<GclEntry> newSchedule)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("GCL schedule update requested");

    m_pendingSchedule = newSchedule;   // stash the new schedule
    m_scheduleUpdatePending = true;    // signal DoDequeue() to swap it at the next cycle wrap
}

// ─── GetQueueStats ────────────────────────────────────────────────────────────
// Snapshot of per-queue packet counts, arrivals, and drops since last ResetStats().
// Called by TsnDynaTasEnv::GetObservation() to build the RL state each step.
GclQueueDisc::QueueStats
GclQueueDisc::GetQueueStats()
{
    NS_LOG_FUNCTION(this);

    QueueStats stats;
    stats.queueSizes.resize(m_nQueues);    // one entry per priority class
    stats.arrivalCount.resize(m_nQueues);
    stats.dropCount.resize(m_nQueues);
    stats.delaySum.resize(m_nQueues);
    stats.dequeueCount.resize(m_nQueues);
    stats.totalArrivals = 0;
    stats.lastUpdateTime = Simulator::Now(); // timestamp so caller knows the observation age

    for (uint32_t i = 0; i < m_nQueues; ++i)
    {
        stats.queueSizes[i]   = GetQueueSize(i);         // live packet count
        stats.arrivalCount[i] = m_arrivalCounts[i];      // arrivals since last reset
        stats.dropCount[i]    = m_dropCounts[i];         // drops since last reset
        stats.delaySum[i]     = m_delaySum[i];           // HOL waiting-time sum since last reset
        stats.dequeueCount[i] = m_dequeueCount[i];       // dequeued packets since last reset
        stats.totalArrivals  += m_arrivalCounts[i];      // aggregate across all classes
    }

    return stats;
}

// ─── ResetStats ───────────────────────────────────────────────────────────────
// Zeroes per-interval arrival/drop counters. Called at the end of GetObservation()
// so each RL step sees counts for the most recent 6 ms interval only, not cumulative.
void
GclQueueDisc::ResetStats()
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < m_nQueues; ++i)
    {
        m_arrivalCounts[i] = 0; // reset arrival counter for this class
        m_dropCounts[i]    = 0; // reset drop counter for this class
        m_delaySum[i]      = 0.0; // reset HOL waiting-time accumulator for this class
        m_dequeueCount[i]  = 0; // reset dequeued-packet counter for this class
    }
    m_lastStatUpdateTime = Simulator::Now(); // record when the reset happened
}

// ─── GetCycleCount ────────────────────────────────────────────────────────────
// Returns how many times the GCL schedule has been swapped in via DoDequeue().
// Logged in queue-stats CSV for debugging schedule update latency.
uint32_t
GclQueueDisc::GetCycleCount() const
{
    return m_cycleCount;
}

// ─── CheckConfig ──────────────────────────────────────────────────────────────
// Called by ns-3 before the simulation starts to validate the disc configuration
// and create the internal per-class DropTailQueues if none were added manually.
bool
GclQueueDisc::CheckConfig(void)
{
    NS_LOG_FUNCTION(this);

    if (GetNQueueDiscClasses() > 0)
    {
        // GclQueueDisc manages classification internally; child classes would conflict
        NS_LOG_ERROR("GclQueueDisc does not support child queue disc classes");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // No queues added externally — create them now
        NS_LOG_INFO("Creating " << m_nQueues << " internal queues");

        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>"); // simple FIFO, drops tail when full
        factory.Set("MaxSize", QueueSizeValue(QueueSize("1000p"))); // 1000 packets per class

        for (uint32_t i = 0; i < m_nQueues; i++)
        {
            AddInternalQueue(factory.Create<DropTailQueue<QueueDiscItem>>()); // one FIFO per priority class
        }
    }

    if (GetNInternalQueues() != m_nQueues)
    {
        // Mismatch between configured queue count and actually installed queues
        NS_LOG_ERROR("GclQueueDisc needs exactly " << m_nQueues << " internal queues");
        return false;
    }

    return true; // configuration is valid, simulation can proceed
}

// ─── InitializeParams ─────────────────────────────────────────────────────────
// Called by ns-3 after CheckConfig(). Nothing to initialise here beyond what the
// constructor already does — schedule is set separately via SetGclSchedule().
void
GclQueueDisc::InitializeParams(void)
{
    NS_LOG_FUNCTION(this);
}

// ─── DoEnqueue ────────────────────────────────────────────────────────────────
// Called by ns-3 whenever a packet arrives at this disc from the network layer.
// Classifies the packet into a priority queue and enqueues it there.
// Gate state is NOT checked here — TAS gates only affect dequeue timing.

bool
GclQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    
    // Default fallback to Ordinary Priority (OP = 2)
    int32_t queueIndex = m_nQueues - 1; 

    Ptr<Packet> packet = item->GetPacket();
    SocketPriorityTag priorityTag;
    uint8_t tosValue = 0; // Lifted to main scope for the NS_LOG_INFO statement
    
    // Step 1: Look for a native socket-level priority tag
    if (packet->PeekPacketTag(priorityTag)) 
    {
        uint32_t priority = priorityTag.GetPriority();
        if (priority < m_nQueues) 
        {
            queueIndex = priority; 
        }
    } 
    // Step 2: Fallback if the tag is obscured. 
    // Directly query the base QueueItem for the IP layer DSFIELD (TOS)
    else 
    {
        if (item->GetUint8Value(QueueItem::IP_DSFIELD, tosValue))
        {
            // Recover the priority from the TOS bits
            uint32_t priority = tosValue >> 2; 
            if (priority < m_nQueues)
            {
                queueIndex = priority;
            }
        }
    }

    NS_LOG_INFO("DoEnqueue | Tag Intact? " << packet->PeekPacketTag(priorityTag) 
            << " | IP TOS: " << (int)tosValue 
            << " | Assigned Queue: " << queueIndex);

    // Track arrivals
    m_arrivalCounts[queueIndex]++;                            
    item->SetTimeStamp(Simulator::Now());                     

    bool retval = GetInternalQueue(queueIndex)->Enqueue(item); 

    if (!retval)
    {
        NS_LOG_WARN("Queue " << queueIndex << " is full, dropping packet");
        m_dropCounts[queueIndex]++;                            
    }

    return retval; 
}

// ─── GetCurrentActiveGate ─────────────────────────────────────────────────────
// Determines which gate (queue index) is open right now based on simulation time.
// Computes position within the current GCL cycle using modular arithmetic,
// then walks the schedule entries until it finds the active slot.
// Returns the first open gate in that slot, or -1 if in a guard band (all closed).
int
GclQueueDisc::GetCurrentActiveGate(void)
{
    if (m_gclSchedule.empty() || m_cycleTime.IsZero())
    {
        NS_LOG_DEBUG("No GCL schedule configured, all gates closed");
        return -1; // no schedule = nothing can be dequeued
    }

    Time now = Simulator::Now();
    // Phase within the current cycle: now mod T_GCL (in nanoseconds to avoid float precision loss)
    Time timeInCycle = NanoSeconds(now.GetNanoSeconds() % m_cycleTime.GetNanoSeconds());

    Time elapsed = Seconds(0);
    for (size_t i = 0; i < m_gclSchedule.size(); ++i)
    {
        if (timeInCycle < elapsed + m_gclSchedule[i].duration) // timeInCycle falls inside slot i
        {
            // Found the active GCL entry
            // Return the first open gate (highest priority first)
            for (size_t j = 0; j < m_gclSchedule[i].gatesOpen.size(); ++j)
            {
                if (m_gclSchedule[i].gatesOpen[j])
                {
                    NS_LOG_DEBUG("Gate " << j << " is open at time " << now.GetSeconds());
                    return j; // index of the open queue (only one gate open per slot in this impl)
                }
            }
            NS_LOG_DEBUG("All gates closed at time " << now.GetSeconds()); // guard band slot
            return -1;
        }
        elapsed += m_gclSchedule[i].duration; // advance to next slot boundary
    }

    NS_LOG_ERROR("Time calculation error in GCL schedule"); // shouldn't reach here if cycle math is correct
    return -1;
}

// ─── DoDequeue ────────────────────────────────────────────────────────────────
// Called by ns-3 whenever the link is ready to transmit a packet.
// Two responsibilities:
//   1. Apply any pending RL schedule update at the cycle boundary (safe swap).
//   2. Dequeue a packet from whichever queue the current GCL gate points to.
Ptr<QueueDiscItem>
GclQueueDisc::DoDequeue(void)
{
    NS_LOG_FUNCTION(this);

    // Check if we're at a cycle boundary and have a pending update
    Time now = Simulator::Now();
    Time timeInCycle = NanoSeconds(now.GetNanoSeconds() % m_cycleTime.GetNanoSeconds());

    // If very close to start of cycle and update pending, apply it
    // (threshold 10 µs = one simulator tick granularity — avoids applying mid-slot)
    if (m_scheduleUpdatePending && timeInCycle < Seconds(0.00001))
    {
        m_gclSchedule = m_pendingSchedule;  // atomically swap to new RL-chosen schedule
        m_cycleTime = Seconds(0);           // recompute cycle time from new entry durations
        for (const auto& entry : m_gclSchedule)
        {
            m_cycleTime += entry.duration;
        }
        m_scheduleUpdatePending = false;    // clear flag — update consumed
        m_cycleCount++;                     // track how many RL-driven schedule swaps have occurred
        NS_LOG_INFO("GCL schedule updated at cycle boundary. Cycle count: " << m_cycleCount);
    }

    int activeGate = GetCurrentActiveGate(); // which queue may transmit right now

    if (activeGate < 0)
    {
        NS_LOG_DEBUG("No gate is currently open"); // guard band or no schedule — link idles
        return nullptr;                            // ns-3 will retry on next wakeup event
    }

    if (GetInternalQueue(activeGate)->GetNPackets() > 0)
    {
        Ptr<QueueDiscItem> item = GetInternalQueue(activeGate)->Dequeue(); // pop head packet
        if (item)
        {
            // EDCA Hardware Translation: Strips the WBAN internal tag and replaces it with 
            // 802.11n EDCA MAC priorities (7, 5, 0) before handing to the Wi-Fi radio.
            Ptr<Packet> packet = item->GetPacket();
            packet->RemoveAllPacketTags(); 

            SocketPriorityTag edcaTag;
            if (activeGate == 0) edcaTag.SetPriority(7);      // CP -> AC_VO
            else if (activeGate == 1) edcaTag.SetPriority(5); // RP -> AC_VI 
            else edcaTag.SetPriority(0);                      // OP -> AC_BE
            
            packet->AddPacketTag(edcaTag);

            // Head-of-line waiting time: how long this packet sat queued before its gate opened.
            // Accumulated per queue so the RL env can compute priority-weighted mean delay.
            m_delaySum[activeGate] += (Simulator::Now() - item->GetTimeStamp()).GetSeconds();
            m_dequeueCount[activeGate]++;
            NS_LOG_DEBUG("Dequeued packet from queue " << activeGate);
            return item; // handed to the link layer for transmission
        }
    }

    NS_LOG_DEBUG("Queue " << activeGate << " is empty"); // gate open but nothing queued — link idles
    return nullptr;
}

// ─── DoPeek ───────────────────────────────────────────────────────────────────
// Non-destructive lookahead — ns-3 uses this to check if a packet is available
// without actually removing it. Respects the same gate logic as DoDequeue.
Ptr<const QueueDiscItem>
GclQueueDisc::DoPeek(void)
{
    NS_LOG_FUNCTION(this);

    int activeGate = GetCurrentActiveGate(); // same gate check as DoDequeue

    if (activeGate >= 0 && GetInternalQueue(activeGate)->GetNPackets() > 0)
    {
        return GetInternalQueue(activeGate)->Peek(); // returns pointer without removing the packet
    }

    return nullptr; // gate closed or queue empty
}

} //namespace wban
} // namespace ns3