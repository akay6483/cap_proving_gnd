#include "wban-lpu-app.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LpuApp");

NS_OBJECT_ENSURE_REGISTERED (LpuApp);

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

void 
LpuApp::HandleRead (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address from;
    
    // Drain the socket buffer completely
    while ((packet = socket->RecvFrom (from)))
    {
        if (packet->GetSize () == 0)
        {
            break; 
        }

        m_totalRxPackets++;
        m_totalRxBytes += packet->GetSize ();

        Time now = Simulator::Now ();

        // 1. Calculate and Trace Jitter (Inter-arrival time)
        if (m_totalRxPackets > 1)
        {
            Time jitter = now - m_lastRxTime;
            m_jitterTrace (jitter);
        }
        m_lastRxTime = now;

        // 2. Trace generic Rx Event
        m_rxTrace (packet, from);

        // 3. Process Custom Payload (DP, RP, OP WBAN Tags)
        ProcessPayload (packet);

        NS_LOG_INFO ("Rx Packet: " << m_totalRxPackets << " | Size: " << packet->GetSize () << " bytes");
    }
}

void 
LpuApp::ProcessPayload (Ptr<Packet> packet)
{
    NS_LOG_FUNCTION (this << packet);
    
    // Default implementation does nothing.
    // Subclass this method to extract WBAN specific headers, DP/RP tags,
    // or calculate precise end-to-end delay if a custom timestamp tag exists.
}

} // namespace ns3