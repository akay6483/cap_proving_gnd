#include "wban-sensor-app.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet-socket-factory.h" 
#include "ns3/packet-socket-address.h" 
#include "ns3/double.h" 

namespace ns3 {
namespace wban {

NS_LOG_COMPONENT_DEFINE("WbanSensorApp");

// ----------------------------------------------------------------------------
// USER CONFIGURATION: Set the discrete time step limit (k) here.
// The simulation event chain will permanently halt after this many hardware ticks.
// ----------------------------------------------------------------------------
const uint32_t MAX_SIMULATION_TICKS = 1000; 

// ============================================================================
// NS-3 TYPE ID REGISTRATION
// ============================================================================
TypeId WbanSensorApp::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::wban::WbanSensorApp")
        .SetParent<Application>()
        .SetGroupName("Wban")
        .AddConstructor<WbanSensorApp>()
        .AddTraceSource("Tx", "Packet generated and sent down to MAC layer", 
                        MakeTraceSourceAccessor(&WbanSensorApp::m_txTrace),
                        "ns3::Packet::TracedCallback");
    return tid;
}

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================
WbanSensorApp::WbanSensorApp() 
    : m_socket(nullptr), 
      m_ticksCompleted(0) 
{
    m_staggerVar = CreateObject<UniformRandomVariable>();
    m_staggerVar->SetAttribute("Min", DoubleValue(0.000));
    m_staggerVar->SetAttribute("Max", DoubleValue(0.050));
}

WbanSensorApp::~WbanSensorApp() 
{ 
    m_socket = nullptr; 
}

// ============================================================================
// SETUP
// ============================================================================
// FIX: Correctly using the generic 'Address' instead of 'Mac48Address'
void WbanSensorApp::Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator)
{
    m_peerAddress = destAddr;
    m_generator = std::move(generator); 
}

// ============================================================================
// LIFECYCLE: START APPLICATION
// ============================================================================
void WbanSensorApp::StartApplication(void)
{
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        
        PacketSocketAddress local;
        local.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        m_socket->Bind(local);

        PacketSocketAddress remote;
        remote.SetPhysicalAddress(m_peerAddress); // Natively accepts generic Address
        remote.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        m_socket->Connect(remote);
    }

    double startOffset = m_staggerVar->GetValue();
    NS_LOG_INFO("Node " << GetNode()->GetId() << " scheduled first tick at T=" << startOffset << "s");
    m_sendEvent = Simulator::Schedule(Seconds(startOffset), &WbanSensorApp::SendPacket, this);
}

// ============================================================================
// LIFECYCLE: STOP APPLICATION
// ============================================================================
void WbanSensorApp::StopApplication(void)
{
    if (m_sendEvent.IsPending()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}

// ============================================================================
// CORE LOGIC: SEND PACKET
// ============================================================================
void WbanSensorApp::SendPacket()
{
    m_ticksCompleted++;
    if (m_ticksCompleted > MAX_SIMULATION_TICKS) {
        NS_LOG_INFO("Node " << GetNode()->GetId() << " reached maximum ticks (" 
                    << MAX_SIMULATION_TICKS << "). Halting generation.");
        return; 
    }

    TrafficClass type = m_generator->GenerateNextPacketType();

    if (type == CLASS_NONE) {
        NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Node " 
                    << GetNode()->GetId() << " | Tick " << m_ticksCompleted << " | IDLE");
    } 
    else {
        uint32_t payloadSize = m_generator->GetPayloadSize();
        Ptr<Packet> packet = Create<Packet>(payloadSize);

        int bytesSent = m_socket->Send(packet);

        if (bytesSent >= 0) {
            NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Node " 
                        << GetNode()->GetId() << " | Tick " << m_ticksCompleted 
                        << " | SENT " << payloadSize << "B | Priority: " << type);
            
            m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(type));
        }
    }

    ScheduleNextPacket();
}

// ============================================================================
// CORE LOGIC: SCHEDULE NEXT PACKET
// ============================================================================
void WbanSensorApp::ScheduleNextPacket()
{
    double intervalSec = m_generator->GetInterval();
    m_sendEvent = Simulator::Schedule(Seconds(intervalSec), &WbanSensorApp::SendPacket, this);
}

} // namespace wban
} // namespace ns3

