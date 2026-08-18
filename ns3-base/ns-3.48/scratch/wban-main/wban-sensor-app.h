#ifndef WBAN_SENSOR_APP_H
#define WBAN_SENSOR_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/socket.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "wban-traffic-generator.h"
#include <memory>
#include <string>
#include <vector>

namespace ns3 {
namespace wban {

// Structure to store individual sample attributes before batch transmission
struct SensorSample {
    TrafficClass type;
    uint32_t size;
};

class WbanSensorApp : public Application {
public:
    static TypeId GetTypeId(void);

    WbanSensorApp();
    virtual ~WbanSensorApp();

    // Setup the application with destination address, traffic generator engine, and MTU threshold
    void Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, uint32_t maxPayloadSize); 

protected:
    virtual void StartApplication(void) override;
    virtual void StopApplication(void) override;

private:
    void ExecuteSamplingCycle();
    void FlushAndTransmitBuffer();
    void ScheduleNextSamplingCycle();
    std::string GetTrafficClassName(TrafficClass c) const;

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    std::unique_ptr<WbanTrafficGenerator> m_generator;
    EventId m_sendEvent;
    
    // Decoupled buffering components to preserve generation ratios
    std::vector<SensorSample> m_sampleBuffer;
    uint32_t m_currentBufferSize;
    uint32_t m_maxPayloadSize; 
    
    Ptr<UniformRandomVariable> m_staggerVar;

    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_txTrace;
};

} // namespace wban
} // namespace ns3

#endif // WBAN_SENSOR_APP_H

