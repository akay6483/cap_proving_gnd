#include "wban-traffic-generator.h"
#include "ns3/double.h"
#include <algorithm> 

WbanTrafficGenerator::WbanTrafficGenerator(uint32_t nodeId, 
                                           double intervalSec, 
                                           uint32_t maxPayload, 
                                           double payloadJitter,
                                           double intervalJitter,
                                           std::array<double, 4> trafficRatios)
    : m_nodeId(nodeId), 
      m_intervalSec(intervalSec), 
      m_maxPayloadSize(maxPayload), 
      m_payloadJitter(payloadJitter),
      m_intervalJitter(intervalJitter),
      m_trafficRatios(trafficRatios)
{
    // 1. Initialize ns-3 Native Stochastic Engine Components
    m_probDist = ns3::CreateObject<ns3::UniformRandomVariable>();
    m_probDist->SetAttribute("Min", ns3::DoubleValue(0.0));
    m_probDist->SetAttribute("Max", ns3::DoubleValue(1.0));

    m_sizeDist = ns3::CreateObject<ns3::UniformRandomVariable>();
    m_sizeDist->SetAttribute("Min", ns3::DoubleValue(0.0));
    m_sizeDist->SetAttribute("Max", ns3::DoubleValue(1.0));

    m_jitterDist = ns3::CreateObject<ns3::UniformRandomVariable>();
    m_jitterDist->SetAttribute("Min", ns3::DoubleValue(-1.0));
    m_jitterDist->SetAttribute("Max", ns3::DoubleValue(1.0));

    // 2. Normalize Traffic Ratios for Cumulative Probabilities
    double totalRatio = 0.0;
    for (double r : m_trafficRatios) {
        totalRatio += r;
    }

    if (totalRatio < 1e-6) {
        m_trafficRatios = {0.0, 0.0, 0.0, 1.0};
        totalRatio = 1.0;
    }

    double cumulative = 0.0;
    for (size_t i = 0; i < 4; ++i) {
        cumulative += (m_trafficRatios[i] / totalRatio);
        m_cumulativeProbabilities[i] = cumulative;
    }
}

QosPriority WbanTrafficGenerator::GenerateNextPacketType() {
    double typeRoll = m_probDist->GetValue();

    for (size_t i = 0; i < 4; ++i) {
        if (m_trafficRatios[i] > 0.0 && typeRoll <= m_cumulativeProbabilities[i]) {
            return static_cast<QosPriority>(i);
        }
    }
    
    // Fallback to the lowest available priority if floating point rounding fails
    for (int i = 3; i >= 0; --i) {
        if (m_trafficRatios[i] > 0.0) {
            return static_cast<QosPriority>(i);
        }
    }
    
    return QOS_OP;
}

double WbanTrafficGenerator::GetInterval() { 
    if (m_intervalSec <= 0.0) {
        return 0.0;
    }

    if (m_intervalJitter > 0.0) {
        double delta = m_jitterDist->GetValue() * m_intervalJitter;
        double finalInterval = m_intervalSec * (1.0 + delta);
        return std::max(0.001, finalInterval);
    }
    
    return m_intervalSec; 
}

uint32_t WbanTrafficGenerator::GetPayloadSize() {
    if (m_maxPayloadSize == 0) {
        return 0;
    }

    if (m_payloadJitter <= 0.0) {
        return m_maxPayloadSize;
    }
    
    double lowerBound = m_maxPayloadSize * (1.0 - m_payloadJitter);
    
    // Roll for size variance
    double varianceRoll = m_sizeDist->GetValue(); 
    double jitterVolume = m_maxPayloadSize * m_payloadJitter * varianceRoll;
    
    uint32_t finalSize = static_cast<uint32_t>(lowerBound + jitterVolume);

    finalSize = std::max(1u, finalSize);
    finalSize = std::min(m_maxPayloadSize, finalSize);

    return finalSize;
}

int64_t WbanTrafficGenerator::AssignStreams(int64_t stream) {
    int64_t currentStream = stream;
    m_probDist->SetStream(currentStream++);
    m_sizeDist->SetStream(currentStream++);
    m_jitterDist->SetStream(currentStream++);
    
    // Return the number of streams consumed (3 in this case)
    return (currentStream - stream);
}