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

static std::string GetQosName(uint32_t priority) {
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

void WbanCoordinatorApp::Setup(Ptr<Socket> rxSocketLrWpan, Ptr<Socket> txSocketWifi, Address destAddress)
{
    NS_LOG_FUNCTION(this << rxSocketLrWpan << txSocketWifi << destAddress);
    m_rxSocketLrWpan = rxSocketLrWpan;
    m_txSocketWifi = txSocketWifi;
    m_destAddress = destAddress;
}

uint32_t WbanCoordinatorApp::GetTotalBytesReceived() const { return m_bytesReceived; }
uint32_t WbanCoordinatorApp::GetTotalBytesForwarded() const { return m_bytesForwarded; }

void WbanCoordinatorApp::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_rxSocketLrWpan) {
        m_rxSocketLrWpan->SetRecvCallback(MakeCallback(&WbanCoordinatorApp::ReceivePacket, this));
    }
    
    NS_LOG_INFO("WbanCoordinatorApp started. Listening on 802.15.4 and ready to forward to Wi-Fi over IPv4.");
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

void WbanCoordinatorApp::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address senderAddress;

    while ((packet = socket->RecvFrom(senderAddress))) {
        
        if (IsSyncBeacon(packet)) {
            continue; 
        }

        m_bytesReceived += packet->GetSize();
        m_rxTrace(packet, senderAddress);

        WbanTelemetryHeader tHeader;
        uint32_t nodeId = 0;
        uint32_t seqNum = 0;
        
        if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
            nodeId = tHeader.GetNodeId();
            seqNum = tHeader.GetSeqNum();
        }

        NS_LOG_DEBUG("[T=" << Simulator::Now().GetSeconds() << "s] Coordinator Rx | Node: " 
                     << nodeId << " | Seq: " << seqNum << " | Size: " << packet->GetSize() << " bytes");

        ProcessDemandTags(packet);

        if (packet->GetSize() == 10) {
            continue;
        }

        uint32_t priority = ExtractPriority(packet);
        ForwardToLpu(packet, priority);
    }
}

bool WbanCoordinatorApp::IsSyncBeacon(Ptr<const Packet> packet) const
{
    return (packet->GetSize() == 5);
}

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

uint32_t WbanCoordinatorApp::ExtractPriority(Ptr<Packet> packet) const
{
    if (packet->GetSize() < 15) {
        return 2; 
    }

    WbanTelemetryHeader header;
    if (packet->PeekHeader(header)) {
        uint32_t priority = header.GetPriority();
        if (priority > 2) {
            return 2; 
        }
        return priority;
    }
    
    return 2; 
}

void WbanCoordinatorApp::ForwardToLpu(Ptr<Packet> packet, uint32_t internalPriority)
{
    packet->RemoveAllPacketTags();

    // 1. Map 3-Tier Priority to standard IPv4 DSCP values
    uint8_t dscp = (internalPriority == 0) ? 48 : (internalPriority == 1) ? 32 : 0;
    
    // 2. Shift DSCP by 2 bits to form the 8-bit Type of Service (ToS) byte
    uint8_t tos = dscp << 2;

    // THE FIX: Directly update the UDP Socket's internal state.
    // The socket will inherently stamp the next outgoing IPv4 header with this ToS.
    m_txSocketWifi->SetIpTos(tos); 

    int bytesSent = m_txSocketWifi->Send(packet);

    if (bytesSent >= 0) {
        m_bytesForwarded += bytesSent;
        m_txTrace(packet, internalPriority);
        
        WbanTelemetryHeader tHeader;
        uint32_t nodeId = 0;
        uint32_t seqNum = 0;
        
        if (packet->GetSize() >= 15 && packet->PeekHeader(tHeader)) {
            nodeId = tHeader.GetNodeId();
            seqNum = tHeader.GetSeqNum();
        }
        
        NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Tx | Coordinator -> LPU | Fwd Node: " 
                    << nodeId << " | Seq: " << seqNum << " | DSCP: " << (uint32_t)dscp 
                    << " | QoS Type: " << GetQosName(internalPriority));
    } else {
        NS_LOG_ERROR("Coordinator failed to forward packet via UDP socket.");
    }
}

} // namespace wban
} // namespace ns3