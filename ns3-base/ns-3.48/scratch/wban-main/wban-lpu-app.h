#ifndef WBAN_LPU_APP_H
#define WBAN_LPU_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"

namespace ns3 {

class LpuApp : public Application 
{
public:
    static TypeId GetTypeId (void);
    LpuApp ();
    virtual ~LpuApp ();

    void Setup (uint16_t port);

protected:
    virtual void StartApplication (void) override;
    virtual void StopApplication (void) override;
    virtual void ProcessPayload (Ptr<Packet> packet);

private:
    void HandleRead (Ptr<Socket> socket);

    Ptr<Socket> m_socket;     
    uint16_t    m_port;       
    uint32_t    m_totalRxPackets;
    uint64_t    m_totalRxBytes;
    Time        m_lastRxTime; 

    TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
    TracedCallback<Time> m_delayTrace;
    TracedCallback<Time> m_jitterTrace;
};

} // namespace ns3

#endif /* WBAN_LPU_APP_H */