#ifndef WBAN_SENSOR_APP_H
#define WBAN_SENSOR_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/socket.h"
#include "ns3/tag.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "wban-traffic-generator.h"
#include "wban-config.h" 
#include "wban-central-scheduler.h" 
#include "ns3/lr-wpan-mac-base.h" 
#include "ns3/lr-wpan-mac.h"

#include <memory>
#include <string>
#include <vector>

namespace ns3 {
namespace wban {

class WbanDemandTag : public Tag {
public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const override;
    virtual uint32_t GetSerializedSize(void) const override;
    virtual void Serialize(TagBuffer i) const override;
    virtual void Deserialize(TagBuffer i) override;
    virtual void Print(std::ostream &os) const override;

    void SetDemand(uint32_t demand) { m_demand = demand; }
    uint32_t GetDemand(void) const { return m_demand; }
    void SetNodeId(uint32_t id) { m_nodeId = id; }
    uint32_t GetNodeId(void) const { return m_nodeId; }
private:
    uint32_t m_demand{0};
    uint32_t m_nodeId{0};
};

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
               uint8_t channel, uint8_t requestedGtsSlots, SensorHierarchy baseHierarchy); 
               
    int64_t AssignStreams(int64_t stream);

protected:
    virtual void StartApplication(void) override;
    virtual void StopApplication(void) override;

private:
    void GenerateData();
    void TransmitSlot();
    void FlushAndTransmitBuffer();
    void SendGtsRequest(); 
    
    // NEW: Replaces the unstable MAC MLME callbacks
    void ReceiveSyncPacket(Ptr<Socket> socket);
    
    std::string GetQosPriorityName(QosPriority c) const;

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    std::unique_ptr<WbanTrafficGenerator> m_generator;
    
    EventId m_sendEvent;
    EventId m_generateEvent;
    
    std::vector<SensorSample> m_sampleBuffer;
    uint32_t m_currentBufferSize;
    uint32_t m_maxPayloadSize; 
    
    Ptr<UniformRandomVariable> m_staggerVar;
    TracedCallback<Ptr<const Packet>, uint32_t, uint32_t> m_txTrace;

    Ptr<ns3::lrwpan::LrWpanMac> m_mac;
    uint8_t m_channel;
    uint8_t m_allocatedSlots; 
};

} // namespace wban
} // namespace ns3

#endif // WBAN_SENSOR_APP_H


