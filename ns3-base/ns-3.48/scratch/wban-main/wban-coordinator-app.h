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
 * \brief Layer 2.5 Bridge Application for the WBAN Coordinator.
 * 
 * This application listens on the 802.15.4 interface, extracts TDMA demand 
 * telemetry for the central scheduler, and forwards the payload to the LPU 
 * over the Wi-Fi interface. It ensures QoS tags are preserved so the 
 * downstream Traffic Control Layer (TasQueueDisc) can shape the traffic.
 */
class WbanCoordinatorApp : public Application {
public:
    static TypeId GetTypeId(void);
    
    WbanCoordinatorApp();
    virtual ~WbanCoordinatorApp();

    /**
     * \brief Injects the required network dependencies.
     * \param rxSocketLrWpan Socket bound to the 802.15.4 NetDevice.
     * \param txSocketWifi Socket bound to the Wi-Fi NetDevice.
     * \param lpuWifiMac The physical MAC address of the destination LPU.
     */
    void Setup(Ptr<Socket> rxSocketLrWpan, Ptr<Socket> txSocketWifi, Address lpuWifiMac);

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
    
    /**
     * \brief Main socket receive callback. Triggers the pipeline below.
     */
    void ReceivePacket(Ptr<Socket> socket);

    /**
     * \brief Step 1: Ingress Filtering. 
     * Identifies if the packet is a loopback 5-byte sync beacon.
     */
    bool IsSyncBeacon(Ptr<const Packet> packet) const;

    /**
     * \brief Step 2 & 3: Metadata Extraction & Cross-Module Signaling.
     * Looks for WbanDemandTag and updates the WbanCentralScheduler.
     */
    void ProcessDemandTags(Ptr<const Packet> packet);

    /**
     * \brief Step 4: Egress Preparation.
     * Reads the QoS priority tag set by the sensor node (defaults to best-effort).
     */
    uint32_t ExtractPriority(Ptr<const Packet> packet) const;

    /**
     * \brief Step 5: Transmission.
     * Attaches the necessary priority tags for the Traffic Control Layer
     * and pushes the packet into the Wi-Fi socket.
     */
    void ForwardToLpu(Ptr<Packet> packet, uint32_t priority);


    // ========================================================================
    // INTERNAL STATE & DEPENDENCIES
    // ========================================================================
    
    Ptr<Socket> m_rxSocketLrWpan;
    Ptr<Socket> m_txSocketWifi;
    Address m_lpuWifiMac;

    // Basic telemetry counters (useful for RL state space later)
    uint32_t m_bytesReceived;
    uint32_t m_bytesForwarded;

    // --- Trace Sources ---
    // Allows main script to hook into these events without modifying this file
    TracedCallback<Ptr<const Packet>, Address> m_rxTrace;
    TracedCallback<Ptr<const Packet>, uint32_t> m_txTrace;
};

} // namespace wban
} // namespace ns3

#endif // WBAN_COORDINATOR_APP_H