#include "tsn-helper.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/queue-disc.h"

namespace ns3 {

namespace wban {

NS_LOG_COMPONENT_DEFINE("TsnHelper");

TsnHelper::TsnHelper() : m_nQueues(4) {
    NS_LOG_FUNCTION(this);
    SetScheduleEqualTime(1.0, 4); // Default schedule for 4 WBAN priority queues
}

void TsnHelper::SetScheduleEqualTime(double slotDuration, uint32_t nQueues) {
    m_nQueues = nQueues;
    m_schedule.clear();
    for (uint32_t i = 0; i < nQueues; ++i) {
        GclQueueDisc::GclEntry entry;
        entry.duration = MilliSeconds(slotDuration);
        entry.gatesOpen.resize(nQueues, false);
        entry.gatesOpen[i] = true;
        m_schedule.push_back(entry);
    }
}

void TsnHelper::SetScheduleCustom(std::vector<double> durations) {
    m_nQueues = durations.size();
    m_schedule.clear();
    for (size_t i = 0; i < durations.size(); ++i) {
        GclQueueDisc::GclEntry entry;
        entry.duration = MilliSeconds(durations[i]);
        entry.gatesOpen.resize(durations.size(), false);
        entry.gatesOpen[i] = true;
        m_schedule.push_back(entry);
    }
}

void TsnHelper::SetScheduleWeighted(std::vector<double> weights, double totalCycle) {
    m_nQueues = weights.size();
    m_schedule.clear();
    double sumWeights = 0.0;
    for (double w : weights) sumWeights += w;
    
    for (size_t i = 0; i < weights.size(); ++i) {
        GclQueueDisc::GclEntry entry;
        double slotDuration = (weights[i] / sumWeights) * totalCycle;
        entry.duration = MilliSeconds(slotDuration);
        entry.gatesOpen.resize(weights.size(), false);
        entry.gatesOpen[i] = true;
        m_schedule.push_back(entry);
    }
}

void TsnHelper::SetNumberOfQueues(uint32_t nQueues) { m_nQueues = nQueues; }

QueueDiscContainer TsnHelper::Install(Ptr<NetDevice> device) {
    TrafficControlHelper tchUninstall;
    tchUninstall.Uninstall(device);
    
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::GclQueueDisc");
    QueueDiscContainer qdiscs = tch.Install(device);
    
    Ptr<GclQueueDisc> gclDisc = qdiscs.Get(0)->GetObject<GclQueueDisc>();
    if (gclDisc) {
        gclDisc->SetNumberOfQueues(m_nQueues);
        gclDisc->SetGclSchedule(m_schedule);
    }
    return qdiscs;
}

QueueDiscContainer TsnHelper::Install(NetDeviceContainer devices) {
    QueueDiscContainer qdiscs;
    for (NetDeviceContainer::Iterator i = devices.Begin(); i != devices.End(); ++i) {
        qdiscs.Add(Install(*i));
    }
    return qdiscs;
}

std::vector<GclQueueDisc::GclEntry> TsnHelper::GetSchedule(void) const { return m_schedule; }

} // namespace wban

} // namespace ns3