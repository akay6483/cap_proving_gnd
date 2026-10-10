#include "wban-coordinator-app.h"
#include "wban-sensor-app.h"        
#include "wban-central-scheduler.h" 
#include "wban-telemetry.h"         
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"

namespace ns3 {
namespace wban {

NS_LOG_COMPONENT_DEFINE("WbanCoordinatorApp");

std::string GetQosName(uint32_t priority) {
    // Updated to the 3-Tier QoS Architecture
    switch(priority) { 
        case 0: return "CP"; 
        case 1: return "RP"; 
        case 2: return "OP"; 
        default: return "UNKNOWN"; 
    }
}

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

        // Peek the tracking header purely for clean terminal logging
        WbanTelemetryHeader tHeader;
        uint32_t nodeId = 0;
        uint32_t seqNum = 0;
        
        // UPDATED: Strict bounds check prevents NS_ASSERT crash on 10-byte GTS Request packets
        if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
            nodeId = tHeader.GetNodeId();
            seqNum = tHeader.GetSeqNum();
        }

        NS_LOG_DEBUG("[T=" << Simulator::Now().GetSeconds() << "s] Coordinator Rx | Node: " 
                     << nodeId << " | Seq: " << seqNum << " | Size: " << packet->GetSize() << " bytes");

        // Pipeline Step 2 & 3: Metadata Extraction & Dispatching
        ProcessDemandTags(packet);

        // Ingress Filtering (GTS Requests)
        // 10-byte packets are dropped here, preventing them from hitting the Wi-Fi backhaul
        if (packet->GetSize() == 10) {
            continue;
        }

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
    return (packet->GetSize() == 5);
}

// ============================================================================
// PIPELINE STEP 2 & 3: METADATA EXTRACTION & DISPATCHING
// ============================================================================
void WbanCoordinatorApp::ProcessDemandTags(Ptr<const Packet> packet)
{
    WbanDemandTag demandTag;
    if (packet->PeekPacketTag(demandTag)) {
        WbanCentralScheduler::GetInstance().UpdateNodeDemand(
            demandTag.GetNodeId(), 
            demandTag.GetDemand()
        );
    }
}

// ============================================================================
// PIPELINE STEP 4: EGRESS PREPARATION
// ============================================================================
uint32_t WbanCoordinatorApp::ExtractPriority(Ptr<Packet> packet) const
{
    // Defensive Memory Bounds Check
    if (packet->GetSize() < 15) {
        return 2; // Default to OP (2) under the new 3-tier system
    }

    WbanTelemetryHeader header;
    
    if (packet->PeekHeader(header)) {
        uint32_t priority = header.GetPriority();
        
        // Hard-cap priority at 2 (OP) to protect the downstream WiFi QoS queues
        if (priority > 2) {
            return 2; 
        }
        return priority;
    }
    
    return 2; // Default fallback to OP (2)
}

// ============================================================================
// PIPELINE STEP 5: TRANSMISSION
// ============================================================================
void WbanCoordinatorApp::ForwardToLpu(Ptr<Packet> packet, uint32_t priority)
{
    packet->RemoveAllPacketTags();

    SocketPriorityTag pTag;
    pTag.SetPriority(priority);
    packet->AddPacketTag(pTag); 

    int bytesSent = m_txSocketWifi->Send(packet);

    if (bytesSent >= 0) {
        m_bytesForwarded += bytesSent;
        m_txTrace(packet, priority);
        
        // Peek the header again to display which exact packet is moving across the Tier-2 bridge
        WbanTelemetryHeader tHeader;
        uint32_t nodeId = 0;
        uint32_t seqNum = 0;
        
        // UPDATED: Defensive bounds check applied to egress logging as well
        if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
            nodeId = tHeader.GetNodeId();
            seqNum = tHeader.GetSeqNum();
        }
        
        NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Tx | Node 0 (Coordinator) -> LPU | Fwd Node: " 
                    << nodeId << " | Seq: " << seqNum << " | Size: " << packet->GetSize() 
                    << " bytes | QoS Type: " << GetQosName(priority));
    } else {
        NS_LOG_ERROR("Coordinator failed to forward packet via UDP.");
    }
}

} // namespace wban
} // namespace ns3