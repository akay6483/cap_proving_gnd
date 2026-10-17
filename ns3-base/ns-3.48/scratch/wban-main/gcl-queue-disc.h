/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * GCL Queue Disc - IEEE 802.1Qbv Time-Aware Shaper
 * 
 * Place in: contrib/tsn-switch/model/gcl-queue-disc.h
 */

#ifndef GCL_QUEUE_DISC_H
#define GCL_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/ipv4-queue-disc-item.h"
#include <vector>

namespace ns3
{
namespace wban 
{

/**
 * \ingroup traffic-control
 *
 * \brief Implements IEEE 802.1Qbv Gate Control List (GCL) scheduling
 *
 * This queue disc implements time-aware shaping using a Gate Control List.
 * It maintains multiple internal queues (one per priority) and opens/closes
 * gates according to a configurable schedule.
 *
 * In this project: 3 queues (CP/RP/OP), T_GCL=3ms, PCP_BASED classification.
 * RL agent updates the schedule dynamically via UpdateGclSchedule() (applied at next cycle boundary).
 *
 * Features:
 * - Configurable number of priority queues (default: 8)
 * - Flexible GCL schedule with microsecond precision
 * - Port-based or DSCP-based packet classification
 * - Automatic cycle repetition
 */
class GclQueueDisc : public QueueDisc
{
public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * \brief GclQueueDisc constructor
     */
    GclQueueDisc();

    /**
     * \brief GclQueueDisc destructor
     */
    virtual ~GclQueueDisc();

    /**
     * \brief Structure representing a single GCL entry
     */
    struct GclEntry
    {
        Time duration;              ///< Duration this entry is active
        std::vector<bool> gatesOpen; ///< Gate states (true = open, false = closed)
    };

    /**
     * \brief Set the Gate Control List schedule
     * \param schedule Vector of GCL entries defining the schedule
     */
    void SetGclSchedule(std::vector<GclEntry> schedule);

    /**
     * \brief Enable/disable PCP-based classification
     * \param usePcp If true, use DSCP field for classification
     */
    void SetUsePcp(bool usePcp);

    /**
     * \brief Enable/disable port-based classification
     * \param usePort If true, use destination port for classification
     */
    void SetPortBasedPriority(bool usePort);

    /**
     * \brief Set the number of priority queues
     * \param nQueues Number of internal queues (typically 3 or 8)
     */
    void SetNumberOfQueues(uint32_t nQueues);

    /**
     * \brief Get current cycle time
     * \return Total duration of one GCL cycle
     */
    Time GetCycleTime(void) const;

    /**
     * \brief Get statistics for a specific queue
     * \param queueIndex Index of the queue
     * \return Number of packets in the queue
     */
    uint32_t GetQueueSize(uint32_t queueIndex) const;

    /**
     * \brief Get queue occupancy in bytes for a specific queue
     * \param queueIndex Index of the queue
     * \return Number of bytes currently queued
     */
    uint32_t GetQueueBytes(uint32_t queueIndex) const;

    /**
     * \brief Get the currently active GCL schedule
     * \return Vector of GCL entries currently in effect
     */
    std::vector<GclEntry> GetCurrentSchedule() const;

    /**
     * \brief Update GCL schedule dynamically at the next cycle boundary
     * \param newSchedule New schedule to apply
     */
    void UpdateGclSchedule(std::vector<GclEntry> newSchedule);

    /**
     * \brief Structure to hold queue statistics
     */
    struct QueueStats
    {
        std::vector<uint32_t> queueSizes;        ///< Current packet count per queue
        std::vector<uint32_t> arrivalCount;      ///< Packet arrivals in last interval
        std::vector<uint32_t> dropCount;         ///< Packets dropped per queue
        std::vector<double>   delaySum;          ///< Sum of HOL waiting time (s) of pkts dequeued this interval, per queue
        std::vector<uint32_t> dequeueCount;      ///< Packets dequeued this interval, per queue (denominator for mean delay)
        uint32_t totalArrivals;                  ///< Total arrivals in interval
        Time lastUpdateTime;                     ///< Last stat collection time
    };

    /**
     * \brief Get current queue statistics
     * \return QueueStats structure with current metrics
     */
    GclQueueDisc::QueueStats GetQueueStats();

    /**
     * \brief Reset statistics counters for new measurement interval
     */
    void ResetStats();

    /**
     * \brief Get the cycle count since start
     * \return Number of complete cycles
     */
    uint32_t GetCycleCount() const;

private:
    // Inherited from QueueDisc
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    virtual Ptr<QueueDiscItem> DoDequeue(void) override;
    virtual Ptr<const QueueDiscItem> DoPeek(void) override;
    virtual bool CheckConfig(void) override;
    virtual void InitializeParams(void) override;

    /**
     * \brief Determine which gate is currently open
     * \return Index of the currently open gate, or -1 if none
     */
    int GetCurrentActiveGate(void);

    /**
     * \brief Classify packet by PCP/DSCP value
     * \param ipv4Item The IPv4 queue disc item
     * \return Queue index for the packet
     */
    int32_t ClassifyByPcp(Ptr<Ipv4QueueDiscItem> ipv4Item);

    /**
     * \brief Classify packet by destination port
     * \param ipv4Item The IPv4 queue disc item
     * \return Queue index for the packet
     */
    int32_t ClassifyByPort(Ptr<Ipv4QueueDiscItem> ipv4Item);

    std::vector<GclEntry> m_gclSchedule; ///< The GCL schedule
    Time m_cycleTime;                     ///< Total cycle time
    bool m_usePcp;                        ///< Use PCP-based classification
    bool m_usePortBased;                  ///< Use port-based classification
    uint32_t m_nQueues;                   ///< Number of priority queues

    // Statistics tracking
    std::vector<uint32_t> m_dropCounts;   ///< Drop count per queue
    std::vector<uint32_t> m_arrivalCounts; ///< Arrival count per queue
    std::vector<double>   m_delaySum;      ///< HOL waiting-time sum (s) per queue, since last ResetStats()
    std::vector<uint32_t> m_dequeueCount;  ///< Packets dequeued per queue, since last ResetStats()
    uint32_t m_cycleCount;                ///< Number of completed cycles
    Time m_lastStatUpdateTime;            ///< Last time stats were collected
    bool m_scheduleUpdatePending;         ///< Pending schedule update
    std::vector<GclEntry> m_pendingSchedule; ///< Schedule pending application
};

} // namespace wban
} // namespace ns3

#endif /* GCL_QUEUE_DISC_H */