#ifndef GCL_QUEUE_DISC_H
#define GCL_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include <vector>

namespace ns3 {

namespace wban {

class GclQueueDisc : public QueueDisc {
public:
    static TypeId GetTypeId(void);
    GclQueueDisc();
    virtual ~GclQueueDisc();

    struct GclEntry {
        Time duration;
        std::vector<bool> gatesOpen;
    };

    void SetGclSchedule(std::vector<GclEntry> schedule);
    void SetNumberOfQueues(uint32_t nQueues);
    Time GetCycleTime(void) const;
    uint32_t GetQueueSize(uint32_t queueIndex) const;
    uint32_t GetQueueBytes(uint32_t queueIndex) const;
    std::vector<GclEntry> GetCurrentSchedule() const;
    void UpdateGclSchedule(std::vector<GclEntry> newSchedule);

    struct QueueStats {
        std::vector<uint32_t> queueSizes;
        std::vector<uint32_t> arrivalCount;
        std::vector<uint32_t> dropCount;
        std::vector<double>   delaySum;
        std::vector<uint32_t> dequeueCount;
        uint32_t totalArrivals;
        Time lastUpdateTime;
    };

    GclQueueDisc::QueueStats GetQueueStats();
    void ResetStats();
    uint32_t GetCycleCount() const;

private:
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    virtual Ptr<QueueDiscItem> DoDequeue(void) override;
    virtual Ptr<const QueueDiscItem> DoPeek(void) override;
    virtual bool CheckConfig(void) override;
    virtual void InitializeParams(void) override;

    int GetCurrentActiveGate(void);

    std::vector<GclEntry> m_gclSchedule;
    Time m_cycleTime;
    uint32_t m_nQueues;

    std::vector<uint32_t> m_dropCounts;
    std::vector<uint32_t> m_arrivalCounts;
    std::vector<double>   m_delaySum;
    std::vector<uint32_t> m_dequeueCount;
    uint32_t m_cycleCount;
    Time m_lastStatUpdateTime;
    bool m_scheduleUpdatePending;
    std::vector<GclEntry> m_pendingSchedule;
};

} //namespace wban

} // namespace ns3
#endif /* GCL_QUEUE_DISC_H */