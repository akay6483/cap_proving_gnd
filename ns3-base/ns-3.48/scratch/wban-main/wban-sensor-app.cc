#include "wban-sensor-app.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/packet-socket-address.h"
#include "ns3/double.h"

namespace ns3 {
namespace wban {

NS_LOG_COMPONENT_DEFINE("WbanSensorApp");

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

WbanSensorApp::WbanSensorApp() 
    : m_socket(nullptr), 
      m_currentBufferSize(0),
      m_maxPayloadSize(0)
{
    // Initialize random variable for initial application start staggering
    m_staggerVar = CreateObject<UniformRandomVariable>();
    m_staggerVar->SetAttribute("Min", DoubleValue(0.000));
    m_staggerVar->SetAttribute("Max", DoubleValue(0.050));
}

WbanSensorApp::~WbanSensorApp() 
{ 
    m_socket = nullptr; 
}

void WbanSensorApp::Setup(Address destAddr, std::unique_ptr<WbanTrafficGenerator> generator, uint32_t maxPayloadSize)
{
    m_peerAddress = destAddr;
    m_generator = std::move(generator);
    m_maxPayloadSize = maxPayloadSize;
}

void WbanSensorApp::StartApplication(void)
{
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        
        PacketSocketAddress local;
        local.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        m_socket->Bind(local);

        PacketSocketAddress remote;
        remote.SetPhysicalAddress(m_peerAddress);
        remote.SetSingleDevice(GetNode()->GetDevice(0)->GetIfIndex());
        m_socket->Connect(remote);
    }

    // Stagger initial sensor startup to avoid network congestion bursts
    double startOffset = m_staggerVar->GetValue();
    NS_LOG_INFO("Node " << GetNode()->GetId() << " scheduled initial sensor sampling at T=" << startOffset << "s");
    m_sendEvent = Simulator::Schedule(Seconds(startOffset), &WbanSensorApp::ExecuteSamplingCycle, this);
}

void WbanSensorApp::StopApplication(void)
{
    if (m_sendEvent.IsPending()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
    }
}

std::string WbanSensorApp::GetTrafficClassName(TrafficClass c) const
{
    switch (c) {
        case CLASS_CP: return "CP (Critical Packet)";
        case CLASS_RP: return "RP (Reliability Packet)";
        case CLASS_DP: return "DP (Delay Packet)";
        case CLASS_OP: return "OP (Ordinary Packet)";
        default:       return "UNKNOWN";
    }
}

void WbanSensorApp::ExecuteSamplingCycle()
{
    // 1. Independently sample the traffic generator on every tick to preserve configuration ratios
    TrafficClass sampledType = m_generator->GenerateNextPacketType();
    uint32_t sampledSize = m_generator->GetPayloadSize();

    // 2. Push the discrete sample into the local buffer vector and accumulate size
    m_sampleBuffer.push_back({sampledType, sampledSize});
    m_currentBufferSize += sampledSize;

    // 3. Log the generation event for statistical tracking and verification
    NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] Node " 
                << GetNode()->GetId() << " | Sample Generated -> Class: " << GetTrafficClassName(sampledType) 
                << " | Size: " << sampledSize << "B | Buffer Total: " << m_currentBufferSize << "B");

    // 4. Transmission threshold condition: Flush buffer down to MAC layer when static MTU/max size is reached 
    if (m_currentBufferSize >= m_maxPayloadSize) 
    {
        FlushAndTransmitBuffer();
    }

    // 5. Chain the next sampling event to maintain continuous sensor operation
    ScheduleNextSamplingCycle();
}

void WbanSensorApp::FlushAndTransmitBuffer()
{
    if (m_sampleBuffer.empty()) {
        return;
    }

    // Aggregate total payload size and determine batch priority class 
    // (selecting the highest priority class present in the buffer window)
    TrafficClass aggregateClass = CLASS_OP;
    uint32_t totalPayload = 0;

    for (const auto& sample : m_sampleBuffer) {
        totalPayload += sample.size;
        if (sample.type < aggregateClass) {
            aggregateClass = sample.type;
        }
    }

    // Create packet with accumulated payload and transmit via socket
    Ptr<Packet> packet = Create<Packet>(totalPayload);
    int bytesSent = m_socket->Send(packet);

    if (bytesSent >= 0) {
        NS_LOG_INFO("[T=" << Simulator::Now().GetSeconds() << "s] >>> TRANSMIT BATCH: Node " 
                    << GetNode()->GetId() << " sent " << totalPayload 
                    << "B aggregated payload down to MAC layer | Assigned Priority Class: " << GetTrafficClassName(aggregateClass));
        
        m_txTrace(packet, GetNode()->GetId(), static_cast<uint32_t>(aggregateClass));
    }
    
    // Reset buffer state post-transmission
    m_sampleBuffer.clear();
    m_currentBufferSize = 0;
}

void WbanSensorApp::ScheduleNextSamplingCycle()
{
    double interval = m_generator ? m_generator->GetInterval() : 1.0;
    m_sendEvent = Simulator::Schedule(Seconds(interval), &WbanSensorApp::ExecuteSamplingCycle, this);
}

} // namespace wban
} // namespace ns3

