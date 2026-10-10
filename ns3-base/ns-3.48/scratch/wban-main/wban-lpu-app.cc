#include "wban-lpu-app.h"
#include "wban-telemetry.h" 
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"

namespace ns3 {
using namespace wban;

NS_LOG_COMPONENT_DEFINE ("LpuApp");

NS_OBJECT_ENSURE_REGISTERED (LpuApp);

std::string GetQosName(uint32_t priority) {
    // Updated to the 3-Tier QoS Architecture
    switch(priority) { 
        case 0: return "CP"; 
        case 1: return "RP"; 
        case 2: return "OP"; 
        default: return "UNKNOWN"; 
    }
}

TypeId 
LpuApp::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::LpuApp")
        .SetParent<Application> ()
        .SetGroupName ("Applications")
        .AddConstructor<LpuApp> ()
        .AddAttribute ("Port", 
                       "Port on which we listen for incoming packets.",
                       UintegerValue (9),
                       MakeUintegerAccessor (&LpuApp::m_port),
                       MakeUintegerChecker<uint16_t> ())
        .AddTraceSource ("Rx",
                         "A packet has been received",
                         MakeTraceSourceAccessor (&LpuApp::m_rxTrace),
                         "ns3::Packet::AddressTracedCallback")
        .AddTraceSource ("Delay",
                         "End-to-End delay of a received packet",
                         MakeTraceSourceAccessor (&LpuApp::m_delayTrace),
                         "ns3::Time::TracedCallback")
        .AddTraceSource ("Jitter",
                         "Inter-arrival time between consecutive packets",
                         MakeTraceSourceAccessor (&LpuApp::m_jitterTrace),
                         "ns3::Time::TracedCallback");
    return tid;
}

LpuApp::LpuApp ()
    : m_socket (nullptr),
      m_port (9),
      m_totalRxPackets (0),
      m_totalRxBytes (0),
      m_lastRxTime (Seconds (0))
{
    NS_LOG_FUNCTION (this);
}

LpuApp::~LpuApp ()
{
    NS_LOG_FUNCTION (this);
    m_socket = nullptr;
}

void 
LpuApp::Setup (uint16_t port)
{
    m_port = port;
}

void 
LpuApp::StartApplication (void)
{
    NS_LOG_FUNCTION (this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket (GetNode (), tid);
        
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        if (m_socket->Bind (local) == -1)
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }
    }

    m_socket->SetRecvCallback (MakeCallback (&LpuApp::HandleRead, this));
}

void 
LpuApp::StopApplication (void)
{
    NS_LOG_FUNCTION (this);

    if (m_socket)
    {
        m_socket->Close ();
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void LpuApp::HandleRead (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address from;
    
    while ((packet = socket->RecvFrom (from)))
    {
        if (packet->GetSize () == 0) break; 

        m_totalRxPackets++;
        m_totalRxBytes += packet->GetSize ();
        Time now = Simulator::Now ();

        if (m_totalRxPackets > 1) {
            Time jitter = now - m_lastRxTime;
            m_jitterTrace (jitter);
        }
        m_lastRxTime = now;
        
        // Fire telemetry hook before stripping the header
        m_rxTrace (packet, from);

        uint32_t priority = 2; // Default fallback to OP
        uint32_t nodeId = 0;
        uint32_t seqNum = 0;
        
        // FIX: Strict bounds check prevents ns3::Buffer assertion crash 
        // when tiny Wi-Fi background packets (like ARP) hit the socket.
        if (packet->GetSize() >= 15) {
            WbanTelemetryHeader header;
            if (packet->RemoveHeader(header) > 0) {
                priority = header.GetPriority();
                nodeId = header.GetNodeId();
                seqNum = header.GetSeqNum();
                
                // Hard-cap priority at 2 (OP) to protect the parser
                if (priority > 2) priority = 2;

                // Calculate and trace exact End-to-End Delay
                Time txTime = Time(header.GetTxTime());
                Time delay = now - txTime;
                m_delayTrace(delay);
            }
        }

        ProcessPayload (packet);

        // UPDATED: Terminal log now outputs Node and Seq for granular lifecycle tracking
        NS_LOG_INFO ("[T=" << now.GetSeconds() << "s] Rx | LPU (Node 1) Received Packet " 
                     << m_totalRxPackets << " | Node: " << nodeId << " | Seq: " << seqNum 
                     << " | Size: " << packet->GetSize () << " bytes | QoS Type: " << GetQosName(priority));
    }
}

void 
LpuApp::ProcessPayload (Ptr<Packet> packet)
{
    // Packets are destroyed naturally as they fall out of scope, simulating consumption.
    NS_LOG_FUNCTION (this << packet);
}

} // namespace ns3