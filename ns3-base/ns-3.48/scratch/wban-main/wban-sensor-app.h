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
#include "wban-config.h" 
#include "ns3/lr-wpan-mac-base.h" 
#include "ns3/lr-wpan-mac.h"

#include <memory>
#include <string>
#include <vector>

namespace ns3 {
namespace wban {

struct SensorSample {
    QosPriority type;
    uint32_t size;
};

class WbanSensorApp : public Application {
public:
    static TypeId GetTypeId(void);

    WbanSensorApp();
    virtual ~WbanSensorApp();

    void Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, 
               uint32_t maxPayloadSize, Ptr<ns3::lrwpan::LrWpanMac> mac, 
               uint8_t channel, uint8_t requestedGtsSlots); 
               
    int64_t AssignStreams(int64_t stream);

    // --- MLME MAC Layer Callback Handlers ---
    void OnMacStartConfirm(ns3::lrwpan::MlmeStartConfirmParams params);
    void OnMacBeaconNotify(ns3::lrwpan::MlmeBeaconNotifyIndicationParams params);
    void OnMacSyncLoss(ns3::lrwpan::MlmeSyncLossIndicationParams params);

protected:
    virtual void StartApplication(void) override;
    virtual void StopApplication(void) override;

private:
    void ExecuteSamplingCycle();
    void FlushAndTransmitBuffer();
    
    std::string GetQosPriorityName(QosPriority c) const; 

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    std::unique_ptr<WbanTrafficGenerator> m_generator;
    EventId m_sendEvent;
    
    std::vector<SensorSample> m_sampleBuffer;
    uint32_t m_currentBufferSize;
    uint32_t m_maxPayloadSize; 
    
    Ptr<UniformRandomVariable> m_staggerVar;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_txTrace;

    bool m_macSyncLost; 
    Ptr<ns3::lrwpan::LrWpanMac> m_mac;
    uint8_t m_channel;
    
    // TDMA Scheduling Variables
    uint8_t m_allocatedSlots; 
};

} // namespace wban
} // namespace ns3

#endif // WBAN_SENSOR_APP_H


