#ifndef WBAN_SENSOR_APP_H
#define WBAN_SENSOR_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "ns3/random-variable-stream.h"
#include "wban-traffic-generator.h" 
#include <memory>

namespace ns3 {
namespace wban {

// ============================================================================
// CLASS: WbanSensorApp
// The ns-3 Application layer for IoMT sensors. It consumes the stochastic 
// math engine, generates packets, and pushes them down to the MAC layer.
// ============================================================================
class WbanSensorApp : public Application 
{
public:
    // Required by ns-3 for object instantiation
    static TypeId GetTypeId(void);

    WbanSensorApp();
    virtual ~WbanSensorApp();

    /**
     * @brief Configures the application before the simulator starts.
     * @param destAddr The MAC address of the Coordinator (Sink).
     * @param generator Unique pointer passing ownership of the math engine.
     */
    
    void Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator);

protected:
    // Inherited Application Lifecycle hooks
    virtual void StartApplication(void) override;
    virtual void StopApplication(void) override;

private:
    /**
     * @brief The core execution event. Evaluates the generator, forms the packet,
     * pushes it to the socket, and chains the next event.
     */
    void SendPacket();

    /**
     * @brief Queries the hardware interval and schedules SendPacket on the ns-3 clock.
     */
    void ScheduleNextPacket();

    // --- State Variables ---
    std::unique_ptr<WbanTrafficGenerator> m_generator; 
    Ptr<Socket> m_socket;              
    Address m_peerAddress;        
    EventId m_sendEvent; // Handle to the pending event on the ns-3 calendar

    // --- Discrete Time Step Constraints ---
    uint32_t m_ticksCompleted; // Tracks how many times the hardware has polled

    // --- Staggered Startup ---
    Ptr<UniformRandomVariable> m_staggerVar; 

    // --- Trace Sources ---
    // Exposes (Packet, NodeId, TrafficClass) to external listeners (MetricsTracker)
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_txTrace;
};

} // namespace wban
} // namespace ns3

#endif // WBAN_SENSOR_APP_H