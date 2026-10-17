/*
 * TSN Helper - Simplifies TSN switch configuration
 * 
 * Place in: contrib/tsn-switch/helper/tsn-helper.h
 */

#ifndef TSN_HELPER_H
#define TSN_HELPER_H

#include "gcl-queue-disc.h"
#include "ns3/net-device-container.h"
#include "ns3/queue-disc-container.h"
#include "ns3/traffic-control-helper.h"
#include <vector>

namespace ns3
{
namespace wban 
{

/**
 * \ingroup tsn-switch
 *
 * \brief Helper class to simplify TSN switch configuration
 *
 * This helper provides convenient methods to:
 * - Install GCL queue disciplines on network devices
 * - Configure common GCL schedules (equal time, priority-based, etc.)
 * - Set up classification rules
 *
 * Example usage:
 * \code
 *   TsnHelper tsnHelper;
 *   tsnHelper.SetScheduleEqualTime(1.0, 3); // 1ms per queue, 3 queues
 *   tsnHelper.SetClassificationMethod(TsnHelper::PCP_BASED);
 *   QueueDiscContainer qdiscs = tsnHelper.Install(device);
 * \endcode
 */
class TsnHelper
{
public:
    /**
     * \brief Classification method options
     */
    enum ClassificationMethod
    {
        PORT_BASED, ///< Classify by destination port
        PCP_BASED   ///< Classify by DSCP/PCP field
    };

    /**
     * \brief Create a TsnHelper
     */
    TsnHelper();

    /**
     * \brief Set equal time GCL schedule
     * \param slotDuration Duration of each time slot in milliseconds
     * \param nQueues Number of priority queues
     *
     * Creates a schedule where each queue gets equal transmission time
     */
    void SetScheduleEqualTime(double slotDuration, uint32_t nQueues);

    /**
     * \brief Set custom GCL schedule
     * \param durations Vector of durations (in milliseconds) for each gate
     *
     * Example: {2.0, 1.0, 0.5} creates 3.5ms cycle with varying slot times
     */
    void SetScheduleCustom(std::vector<double> durations);

    /**
     * \brief Set priority-weighted schedule
     * \param weights Vector of weights for each queue
     * \param totalCycle Total cycle time in milliseconds
     *
     * Example: weights {3, 2, 1} with totalCycle 6ms creates
     * slots of 3ms, 2ms, and 1ms respectively
     */
    void SetScheduleWeighted(std::vector<double> weights, double totalCycle);

    /**
     * \brief Set the classification method
     * \param method PORT_BASED or PCP_BASED
     */
    void SetClassificationMethod(ClassificationMethod method);

    /**
     * \brief Set the number of priority queues
     * \param nQueues Number of queues (default: 3)
     */
    void SetNumberOfQueues(uint32_t nQueues);

    /**
     * \brief Install TSN switch on a network device
     * \param device The network device (typically switch egress interface)
     * \return Container with the installed queue disc
     */
    QueueDiscContainer Install(Ptr<NetDevice> device);

    /**
     * \brief Install TSN switch on multiple devices
     * \param devices Container of network devices
     * \return Container with all installed queue discs
     */
    QueueDiscContainer Install(NetDeviceContainer devices);

    /**
     * \brief Get the configured GCL schedule
     * \return Vector of GCL entries
     */
    std::vector<GclQueueDisc::GclEntry> GetSchedule(void) const;

private:
    std::vector<GclQueueDisc::GclEntry> m_schedule; ///< Configured GCL schedule
    ClassificationMethod m_classificationMethod;    ///< Classification method
    uint32_t m_nQueues;                             ///< Number of priority queues
};

} // namespace wban
} // namespace ns3

#endif /* TSN_HELPER_H */