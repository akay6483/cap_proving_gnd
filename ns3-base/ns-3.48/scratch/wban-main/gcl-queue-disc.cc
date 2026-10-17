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
#include "ns3/ipv4-queue-disc-item.h"   // Ipv4QueueDiscItem: carries IP header alongside packet
#include "ns3/ipv4-header.h"            // Ipv4Header: needed to read TOS/protocol field
#include "ns3/udp-header.h"             // UdpHeader: needed to read destination port (port-based classification)
#include "ns3/tcp-header.h"             // TcpHeader: same, for TCP flows
#include "ns3/object-factory.h"         // ObjectFactory: used to create internal DropTailQueues dynamically
#include "ns3/drop-tail-queue.h"        // DropTailQueue<QueueDiscItem>: the per-class FIFO buffer
#include "ns3/simulator.h"              // Simulator::Now(): wall-clock time inside simulation

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
        TypeId("ns3::wban::GclQueueDisc")
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
                          UintegerValue(3),                 // default 3: CP (Alarms) / RP (Vitals) / OP (Imaging)
                          MakeUintegerAccessor(&GclQueueDisc::m_nQueues),
                          MakeUintegerChecker<uint32_t>(1, 8));
    return tid;
}

// ─── Constructor ──────────────────────────────────────────────────────────────
GclQueueDisc::GclQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE), // base class counts packets across all internal queues as one pool
      m_usePcp(true),                // DSCP/PCP classification ON by default for WBAN
      m_usePortBased(false),         // Port-based classification OFF by default
      m_nQueues(3),                  // Default to 3 queues
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

// ─── SetUsePcp ────────────────────────────────────────────────────────────────
// Switches classification to DSCP/TOS field instead of destination port.
// Mutually exclusive with port-based mode; caller should set one or the other.
void
GclQueueDisc::SetUsePcp(bool usePcp)
{
    NS_LOG_FUNCTION(this << usePcp);
    m_usePcp = usePcp;

    if (usePcp)
    {
        NS_LOG_INFO("Classification method: PCP/DSCP-based");
    }
}

// ─── SetPortBasedPriority ─────────────────────────────────────────────────────
// Enables/disables port-based classification (UDP/TCP dest port 5001+ → queue index).
void
GclQueueDisc::SetPortBasedPriority(bool usePort)
{
    NS_LOG_FUNCTION(this << usePort);
    m_usePortBased = usePort;

    if (usePort)
    {
        NS_LOG_INFO("Classification method: Port-based");
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

// ─── ClassifyByPcp ────────────────────────────────────────────────────────────
// Maps a packet's IP TOS/DSCP field to a queue index.
// Used when m_usePcp=true. 
int32_t
GclQueueDisc::ClassifyByPcp(Ptr<Ipv4QueueDiscItem> ipv4Item)
{
    NS_LOG_FUNCTION(this);

    Ipv4Header ipv4Header = ipv4Item->GetHeader();
    uint8_t tos  = ipv4Header.GetTos();  // full TOS byte from IP header
    uint8_t dscp = tos >> 2;             // DSCP = top 6 bits of TOS (shift out 2-bit ECN field)

    int32_t queueIndex;

    if (m_nQueues == 8)
    {
        // Full IEEE 802.1Q 8-priority mapping: use top 3 bits of DSCP (range 0–7)
        queueIndex = (dscp >> 3);          // DSCP[5:3] → priority 0–7
        if (queueIndex >= 8) queueIndex = 7; // clamp to highest queue
    }
    else if (m_nQueues == 3)
    {
        // Simplified 3-class mapping matching the DynaTAS/WBAN traffic profile
        if (dscp >= 48)
        {
            queueIndex = 0; // DSCP ≥ 48 (EF/CS6/CS7) → Q0 = CP / Alarms (highest priority)
        }
        else if (dscp >= 32)
        {
            queueIndex = 1; // DSCP 32–47 (AF4x/CS4/CS5) → Q1 = RP / Vitals (medium)
        }
        else
        {
            queueIndex = 2; // DSCP < 32 → Q2 = OP / Imaging (lowest priority)
        }
    }
    else
    {
        // Generic linear mapping for arbitrary queue counts
        queueIndex = (dscp * m_nQueues) / 64;                  // scale DSCP [0,63] → [0, m_nQueues-1]
        if (queueIndex >= (int32_t)m_nQueues) queueIndex = m_nQueues - 1; // clamp
    }

    NS_LOG_DEBUG("Packet DSCP=" << (int)dscp << " -> Queue " << queueIndex);
    return queueIndex;
}

// ─── ClassifyByPort ───────────────────────────────────────────────────────────
// Maps a packet's UDP/TCP destination port to a queue index.
// Port 5001 → Q0 (Alarms), 5002 → Q1 (Vitals), 5003 → Q2 (Imaging).
// Returns -1 if the port is outside the expected range (packet falls to default queue).
int32_t
GclQueueDisc::ClassifyByPort(Ptr<Ipv4QueueDiscItem> ipv4Item)
{
    NS_LOG_FUNCTION(this);

    Ipv4Header ipv4Header = ipv4Item->GetHeader();
    uint8_t protocol = ipv4Header.GetProtocol(); // 17 = UDP, 6 = TCP

    if (protocol == 17) // UDP
    {
        Ptr<Packet> packet = ipv4Item->GetPacket()->Copy(); // copy so PeekHeader doesn't consume bytes
        UdpHeader udpHeader;
        packet->PeekHeader(udpHeader);                      // non-destructive read of UDP header
        uint16_t destPort = udpHeader.GetDestinationPort();

        if (destPort >= 5001 && destPort < 5001 + m_nQueues) // port within the configured range
        {
            int32_t queueIndex = destPort - 5001;            // 5001→0, 5002→1, 5003→2
            NS_LOG_DEBUG("UDP port " << destPort << " -> Queue " << queueIndex);
            return queueIndex;
        }
    }
    else if (protocol == 6) // TCP
    {
        Ptr<Packet> packet = ipv4Item->GetPacket()->Copy();
        TcpHeader tcpHeader;
        packet->PeekHeader(tcpHeader);
        uint16_t destPort = tcpHeader.GetDestinationPort();

        if (destPort >= 5001 && destPort < 5001 + m_nQueues)
        {
            int32_t queueIndex = destPort - 5001;
            NS_LOG_DEBUG("TCP port " << destPort << " -> Queue " << queueIndex);
            return queueIndex;
        }
    }

    NS_LOG_DEBUG("Packet not classified by port"); // non-UDP/TCP or out-of-range port
    return -1; // caller will fall back to the lowest-priority queue
}

// ─── DoEnqueue ────────────────────────────────────────────────────────────────
// Called by ns-3 whenever a packet arrives at this disc from the network layer.
// Classifies the packet into a priority queue and enqueues it there.
// Gate state is NOT checked here — TAS gates only affect dequeue timing.
bool
GclQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem>(item); // cast to access IP header

    int32_t queueIndex = -1; // -1 means unclassified

    if (ipv4Item)
    {
        if (m_usePcp)
        {
            queueIndex = ClassifyByPcp(ipv4Item);      // DSCP-based classification
        }
        else if (m_usePortBased)
        {
            queueIndex = ClassifyByPort(ipv4Item);     // port-based classification
        }
    }
    else
    {
        NS_LOG_WARN("Item is not an Ipv4QueueDiscItem"); // non-IP packet — will hit default queue
    }

    // Default to lowest priority queue if not classified
    if (queueIndex < 0 || queueIndex >= (int32_t)GetNInternalQueues())
    {
        queueIndex = GetNInternalQueues() - 1; // Q2 = OP = lowest priority fallback
        NS_LOG_DEBUG("Packet not classified, using default queue " << queueIndex);
    }

    // Track arrivals
    m_arrivalCounts[queueIndex]++;                             // increment for the RL observation interval

    item->SetTimeStamp(Simulator::Now());                     // stamp enqueue time so DoDequeue can measure HOL waiting

    bool retval = GetInternalQueue(queueIndex)->Enqueue(item); // try to push into the DropTailQueue

    if (!retval)
    {
        NS_LOG_WARN("Queue " << queueIndex << " is full, dropping packet");
        m_dropCounts[queueIndex]++;                            // record drop for RL observation
    }

    return retval; // true = enqueued, false = dropped
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

} // namespace wban
} // namespace ns3