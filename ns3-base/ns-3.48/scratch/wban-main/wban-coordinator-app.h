#ifndef WBAN_COORDINATOR_APP_H
#define WBAN_COORDINATOR_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"

namespace ns3 {
namespace wban {

/**
 * \brief Layer 3 Bridge Application for the WBAN Coordinator.
 * 
 * This application listens on the 802.15.4 interface, extracts TDMA demand 
 * telemetry for the central scheduler, and forwards the payload to the LPU 
 * over an IPv4 UDP socket. It translates internal priorities into standard 
 * IPv4 DSCP/ToS tags so the downstream GclQueueDisc can classify the traffic.
 */
class WbanCoordinatorApp : public Application {
public:
    static TypeId GetTypeId(void);
    
    WbanCoordinatorApp();
    virtual ~WbanCoordinatorApp();

    /**
     * \brief Injects the required network dependencies.
     * \param rxSocketLrWpan Socket bound to the 802.15.4 NetDevice.
     * \param txSocketWifi UDP Socket bound to the Wi-Fi NetDevice.
     * \param destAddress The destination IPv4 address of the LPU.
     */
    void Setup(Ptr<Socket> rxSocketLrWpan, Ptr<Socket> txSocketWifi, Address destAddress);

    // --- RL Agent / Statistics Observation Surface ---
    uint32_t GetTotalBytesReceived() const;
    uint32_t GetTotalBytesForwarded() const;

protected:
    // --- Inherited ns-3 Application Lifecycle ---
    virtual void StartApplication(void) override;
    virtual void StopApplication(void) override;

private:
    // ========================================================================
    // THE PACKET PROCESSING PIPELINE
    // ========================================================================
    
    void ReceivePacket(Ptr<Socket> socket);
    bool IsSyncBeacon(Ptr<const Packet> packet) const;
    void ProcessDemandTags(Ptr<const Packet> packet);
    uint32_t ExtractPriority(Ptr<Packet> packet) const;

    /**
     * \brief Step 5: Transmission.
     * Translates internal QoS to IPv4 DSCP and attaches a SocketIpTosTag 
     * before pushing the packet into the UDP socket.
     */
    void ForwardToLpu(Ptr<Packet> packet, uint32_t priority);

    // ========================================================================
    // INTERNAL STATE & DEPENDENCIES
    // ========================================================================
    
    Ptr<Socket> m_rxSocketLrWpan;
    Ptr<Socket> m_txSocketWifi;
    Address m_destAddress;

    uint32_t m_bytesReceived;
    uint32_t m_bytesForwarded;

    // --- Trace Sources ---
    TracedCallback<Ptr<const Packet>, const Address&> m_rxTrace;
    TracedCallback<Ptr<const Packet>, uint32_t> m_txTrace;
};

} // namespace wban
} // namespace ns3

#endif // WBAN_COORDINATOR_APP_H