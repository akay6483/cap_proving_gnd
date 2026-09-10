#include "wban-coordinator-app.h"
#include "wban-sensor-app.h"        // Provides WbanDemandTag and QosPriority
#include "wban-central-scheduler.h" // Singleton for TDMA scheduling
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"

namespace ns3 {
namespace wban {

NS_LOG_COMPONENT_DEFINE("WbanCoordinatorApp");

TypeId WbanCoordinatorApp::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::wban::WbanCoordinatorApp")
        .SetParent<Application>()
        .SetGroupName("Wban")
        .AddConstructor<WbanCoordinatorApp>()
        .AddTraceSource("RxFromSensor", 
                        "A packet was received from the 802.15.4 network.",
                        MakeTraceSourceAccessor(&WbanCoordinatorApp::m_rxTrace),
                        "ns3::Packet::Address")
        .AddTraceSource("TxToLpu", 
                        "A packet was forwarded to the Wi-Fi backhaul.",
                        MakeTraceSourceAccessor(&WbanCoordinatorApp::m_txTrace),
                        "ns3::Packet::Uint32");
    return tid;
}

WbanCoordinatorApp::WbanCoordinatorApp()
    : m_rxSocketLrWpan(nullptr),
      m_txSocketWifi(nullptr),
      m_bytesReceived(0),
      m_bytesForwarded(0)
{
    NS_LOG_FUNCTION(this);
}

WbanCoordinatorApp::~WbanCoordinatorApp()
{
    NS_LOG_FUNCTION(this);
    m_rxSocketLrWpan = nullptr;
    m_txSocketWifi = nullptr;
}

void WbanCoordinatorApp::Setup(Ptr<Socket> rxSocketLrWpan, Ptr<Socket> txSocketWifi, Address lpuWifiMac)
{
    NS_LOG_FUNCTION(this << rxSocketLrWpan << txSocketWifi << lpuWifiMac);
    m_rxSocketLrWpan = rxSocketLrWpan;
    m_txSocketWifi = txSocketWifi;
    m_lpuWifiMac = lpuWifiMac;
}

uint32_t WbanCoordinatorApp::GetTotalBytesReceived() const { return m_bytesReceived; }
uint32_t WbanCoordinatorApp::GetTotalBytesForwarded() const { return m_bytesForwarded; }

void WbanCoordinatorApp::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_rxSocketLrWpan) {
        m_rxSocketLrWpan->SetRecvCallback(MakeCallback(&WbanCoordinatorApp::ReceivePacket, this));
    }
    
    NS_LOG_INFO("WbanCoordinatorApp started. Listening on 802.15.4 and ready to forward to Wi-Fi.");
}

void WbanCoordinatorApp::StopApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_rxSocketLrWpan) {
        m_rxSocketLrWpan->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_rxSocketLrWpan->Close();
    }
    if (m_txSocketWifi) {
        m_txSocketWifi->Close();
    }
}

// ============================================================================
// PIPELINE STEP 0: INGRESS
// ============================================================================
void WbanCoordinatorApp::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address senderAddress;

    while ((packet = socket->RecvFrom(senderAddress))) {
        
        // Pipeline Step 1: Ingress Filtering
        if (IsSyncBeacon(packet)) {
            continue; 
        }

        m_bytesReceived += packet->GetSize();
        m_rxTrace(packet, senderAddress);

        NS_LOG_DEBUG("[T=" << Simulator::Now().GetSeconds() << "s] Coordinator Rx | Size: " 
                     << packet->GetSize() << " bytes");

        // Pipeline Step 2 & 3: Metadata Extraction & Dispatching
        ProcessDemandTags(packet);

        // Pipeline Step 4: Egress Preparation
        uint32_t priority = ExtractPriority(packet);

        // Pipeline Step 5: Transmission
        ForwardToLpu(packet, priority);
    }
}

// ============================================================================
// PIPELINE STEP 1: FILTERING
// ============================================================================
bool WbanCoordinatorApp::IsSyncBeacon(Ptr<const Packet> packet) const
{
    // The Coordinator broadcasts 5-byte sync frames. If the socket receives one, 
    // it is a local loopback that should be immediately dropped.
    return (packet->GetSize() == 5);
}

// ============================================================================
// PIPELINE STEP 2 & 3: METADATA EXTRACTION & DISPATCHING
// ============================================================================
void WbanCoordinatorApp::ProcessDemandTags(Ptr<const Packet> packet)
{
    WbanDemandTag demandTag;
    if (packet->PeekPacketTag(demandTag)) {
        // Feed the physical telemetry to the TDMA central scheduler singleton
        WbanCentralScheduler::GetInstance().UpdateNodeDemand(
            demandTag.GetNodeId(), 
            demandTag.GetDemand()
        );
    }
}

// ============================================================================
// PIPELINE STEP 4: EGRESS PREPARATION
// ============================================================================
uint32_t WbanCoordinatorApp::ExtractPriority(Ptr<const Packet> packet) const
{
    SocketPriorityTag priorityTag;
    
    // Attempt to read the QosPriority tag set by the WbanSensorApp
    if (packet->PeekPacketTag(priorityTag)) {
        return priorityTag.GetPriority();
    }
    
    // Default to Best Effort / Ordinary Packet (QOS_OP) if no tag exists
    return 3; 
}

// ============================================================================
// PIPELINE STEP 5: TRANSMISSION
// ============================================================================
// ============================================================================
// PIPELINE STEP 5: TRANSMISSION
// ============================================================================
void WbanCoordinatorApp::ForwardToLpu(Ptr<Packet> packet, uint32_t priority)
{
    packet->RemoveAllPacketTags();

    SocketPriorityTag pTag;
    pTag.SetPriority(priority);
    packet->AddPacketTag(pTag); 

    // Simply use Send() since the UDP socket is already connected to the LPU's IP
    int bytesSent = m_txSocketWifi->Send(packet);

    if (bytesSent >= 0) {
        m_bytesForwarded += bytesSent;
        m_txTrace(packet, priority);
        NS_LOG_DEBUG("Forwarded " << bytesSent << " bytes over UDP with Priority: " << priority);
    } else {
        NS_LOG_ERROR("Coordinator failed to forward packet via UDP.");
    }
}

} // namespace wban
} // namespace ns3