#ifndef TSN_HELPER_H
#define TSN_HELPER_H

#include "gcl-queue-disc.h"
#include "ns3/net-device-container.h"
#include "ns3/queue-disc-container.h"
#include "ns3/traffic-control-helper.h"
#include <vector>

namespace ns3 {

namespace wban {

class TsnHelper {
public:
    TsnHelper();
    void SetScheduleEqualTime(double slotDuration, uint32_t nQueues);
    void SetScheduleCustom(std::vector<double> durations);
    void SetScheduleWeighted(std::vector<double> weights, double totalCycle);
    void SetNumberOfQueues(uint32_t nQueues);
    
    QueueDiscContainer Install(Ptr<NetDevice> device);
    QueueDiscContainer Install(NetDeviceContainer devices);
    std::vector<GclQueueDisc::GclEntry> GetSchedule(void) const;

private:
    std::vector<GclQueueDisc::GclEntry> m_schedule;
    uint32_t m_nQueues;
};
} // namespace wban

} // namespace ns3

#endif /* TSN_HELPER_H */