/*
 * TSN Helper Implementation
 * Prabharoop C C
 * Place in: contrib/tsn-switch/helper/tsn-helper.cc
 */

#include "tsn-helper.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/queue-disc.h"

namespace ns3
{
namespace wban 
{

NS_LOG_COMPONENT_DEFINE("TsnHelper");

TsnHelper::TsnHelper()
    : m_classificationMethod(PCP_BASED), // Defaulted to PCP/DSCP classification for WBAN project
      m_nQueues(3)                       // 3 Queues: CP, RP, OP
{
    NS_LOG_FUNCTION(this);
    
    // Default schedule: 1ms per queue, 3 queues
    SetScheduleEqualTime(1.0, 3);
}

void
TsnHelper::SetScheduleEqualTime(double slotDuration, uint32_t nQueues)
{
    NS_LOG_FUNCTION(this << slotDuration << nQueues);
    
    m_nQueues = nQueues;
    m_schedule.clear();
    
    for (uint32_t i = 0; i < nQueues; ++i)
    {
        GclQueueDisc::GclEntry entry;
        entry.duration = MilliSeconds(slotDuration);
        entry.gatesOpen.resize(nQueues, false);
        entry.gatesOpen[i] = true; // Only this gate is open
        
        m_schedule.push_back(entry);
    }
    
    NS_LOG_INFO("Created equal-time GCL schedule:");
    NS_LOG_INFO("  Slot duration: " << slotDuration << " ms");
    NS_LOG_INFO("  Number of queues: " << nQueues);
    NS_LOG_INFO("  Total cycle time: " << (slotDuration * nQueues) << " ms");
}

void
TsnHelper::SetScheduleCustom(std::vector<double> durations)
{
    NS_LOG_FUNCTION(this);
    
    m_nQueues = durations.size();
    m_schedule.clear();
    
    double totalCycle = 0.0;
    
    for (size_t i = 0; i < durations.size(); ++i)
    {
        GclQueueDisc::GclEntry entry;
        entry.duration = MilliSeconds(durations[i]);
        entry.gatesOpen.resize(durations.size(), false);
        entry.gatesOpen[i] = true;
        
        m_schedule.push_back(entry);
        totalCycle += durations[i];
    }
    
    NS_LOG_INFO("Created custom GCL schedule:");
    NS_LOG_INFO("  Number of slots: " << durations.size());
    NS_LOG_INFO("  Total cycle time: " << totalCycle << " ms");
    
    for (size_t i = 0; i < durations.size(); ++i)
    {
        NS_LOG_INFO("    Slot " << i << ": " << durations[i] << " ms");
    }
}

void
TsnHelper::SetScheduleWeighted(std::vector<double> weights, double totalCycle)
{
    NS_LOG_FUNCTION(this << totalCycle);
    
    m_nQueues = weights.size();
    m_schedule.clear();
    
    // Calculate sum of weights
    double sumWeights = 0.0;
    for (double w : weights)
    {
        sumWeights += w;
    }
    
    NS_LOG_INFO("Created weighted GCL schedule:");
    NS_LOG_INFO("  Number of queues: " << weights.size());
    NS_LOG_INFO("  Total cycle time: " << totalCycle << " ms");
    
    // Create entries proportional to weights
    for (size_t i = 0; i < weights.size(); ++i)
    {
        GclQueueDisc::GclEntry entry;
        double slotDuration = (weights[i] / sumWeights) * totalCycle;
        entry.duration = MilliSeconds(slotDuration);
        entry.gatesOpen.resize(weights.size(), false);
        entry.gatesOpen[i] = true;
        
        m_schedule.push_back(entry);
        
        NS_LOG_INFO("    Queue " << i << ": " << slotDuration << " ms (weight=" 
                    << weights[i] << ")");
    }
}

void
TsnHelper::SetClassificationMethod(ClassificationMethod method)
{
    NS_LOG_FUNCTION(this << method);
    
    m_classificationMethod = method;
    
    if (method == PORT_BASED)
    {
        NS_LOG_INFO("Classification method set to: PORT_BASED");
    }
    else if (method == PCP_BASED)
    {
        NS_LOG_INFO("Classification method set to: PCP_BASED");
    }
}

void
TsnHelper::SetNumberOfQueues(uint32_t nQueues)
{
    NS_LOG_FUNCTION(this << nQueues);
    
    NS_ASSERT_MSG(nQueues >= 1 && nQueues <= 8, 
                  "Number of queues must be between 1 and 8");
    
    m_nQueues = nQueues;
}

QueueDiscContainer
TsnHelper::Install(Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    
    // First, uninstall any existing queue disc
    TrafficControlHelper tchUninstall;
    tchUninstall.Uninstall(device);
    
    // Install GCL queue disc with updated namespace
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::wban::GclQueueDisc");
    
    QueueDiscContainer qdiscs = tch.Install(device);
    
    // Configure the installed queue disc
    Ptr<GclQueueDisc> gclDisc = qdiscs.Get(0)->GetObject<GclQueueDisc>();
    
    if (gclDisc)
    {
        NS_LOG_INFO("Configuring GCL queue disc on device");
        
        // Set number of queues
        gclDisc->SetNumberOfQueues(m_nQueues);
        
        // Set classification method
        if (m_classificationMethod == PORT_BASED)
        {
            gclDisc->SetUsePcp(false);
            gclDisc->SetPortBasedPriority(true);
        }
        else // PCP_BASED
        {
            gclDisc->SetUsePcp(true);
            gclDisc->SetPortBasedPriority(false);
        }
        
        // Set GCL schedule
        gclDisc->SetGclSchedule(m_schedule);
        
        NS_LOG_INFO("GCL queue disc configured successfully");
    }
    else
    {
        NS_LOG_ERROR("Failed to get GclQueueDisc object");
    }
    
    return qdiscs;
}

QueueDiscContainer
TsnHelper::Install(NetDeviceContainer devices)
{
    NS_LOG_FUNCTION(this);
    
    QueueDiscContainer qdiscs;
    
    for (NetDeviceContainer::Iterator i = devices.Begin(); i != devices.End(); ++i)
    {
        QueueDiscContainer qd = Install(*i);
        qdiscs.Add(qd);
    }
    
    NS_LOG_INFO("Installed GCL queue disc on " << devices.GetN() << " devices");
    
    return qdiscs;
}

std::vector<GclQueueDisc::GclEntry>
TsnHelper::GetSchedule(void) const
{
    return m_schedule;
}

} // namespace wban
} // namespace ns3