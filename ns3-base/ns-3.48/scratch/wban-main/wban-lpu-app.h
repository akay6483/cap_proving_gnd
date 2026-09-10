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
    // 1. ns-3 Object Framework Required Methods
    static TypeId GetTypeId (void);
    LpuApp ();
    virtual ~LpuApp ();

    // 2. Application Setup
    /**
     * \brief Configures the application port.
     * \param port The local port to listen on.
     */
    void Setup (uint16_t port);

protected:
    // 3. Application Lifecycle (Inherited from ns3::Application)
    virtual void StartApplication (void) override;
    virtual void StopApplication (void) override;

    // 4. Extensible Processing Logic
    /**
     * \brief Virtual function to parse specific WBAN payloads (DP, RP, OP).
     * Subclasses can override this to implement custom data extraction.
     */
    virtual void ProcessPayload (Ptr<Packet> packet);

private:
    // 5. Network I/O
    void HandleRead (Ptr<Socket> socket);

    // 6. State Variables
    Ptr<Socket> m_socket;     //!< Listening socket
    uint16_t    m_port;       //!< Listening port
    uint32_t    m_totalRxPackets;
    uint64_t    m_totalRxBytes;
    Time        m_lastRxTime; //!< Tracks the last received packet for jitter/inter-arrival calculations

    // 7. Telemetry Trace Sources
    /**
     * TracedCallback signature: Packet pointer, source address.
     * Used for PDR, Throughput, and Packet Size.
     */
    TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
    
    /**
     * TracedCallback signature: Delay time.
     * Used for End-to-End Delay.
     */
    TracedCallback<Time> m_delayTrace;

    /**
     * TracedCallback signature: Inter-arrival time (Jitter).
     */
    TracedCallback<Time> m_jitterTrace;
};

} // namespace ns3

#endif /* LPU_APP_H */