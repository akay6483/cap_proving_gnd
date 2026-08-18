#include "wban-traffic-generator.h"
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
      m_trafficRatios(trafficRatios),
      m_probDist(0.0, 1.0),
      m_sizeDist(0.0, 1.0), 
      m_jitterDist(-1.0, 1.0) 
{
    m_rng.seed(1337 + m_nodeId);

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

TrafficClass WbanTrafficGenerator::GenerateNextPacketType() {
    double typeRoll = m_probDist(m_rng);

    for (size_t i = 0; i < 4; ++i) {
        if (m_trafficRatios[i] > 0.0 && typeRoll <= m_cumulativeProbabilities[i]) {
            return static_cast<TrafficClass>(i);
        }
    }
    
    for (int i = 3; i >= 0; --i) {
        if (m_trafficRatios[i] > 0.0) {
            return static_cast<TrafficClass>(i);
        }
    }
    
    return CLASS_OP;
}

double WbanTrafficGenerator::GetInterval() { 
    if (m_intervalSec <= 0.0) {
        return 0.0;
    }

    if (m_intervalJitter > 0.0) {
        double delta = m_jitterDist(m_rng) * m_intervalJitter;
        double finalInterval = m_intervalSec * (1.0 + delta);
        return std::max(0.001, finalInterval);
    }
    
    return m_intervalSec; 
}

uint32_t WbanTrafficGenerator::GetPayloadSize() {
    // Corrected Edge Case: Sinks natively generate 0 byte packets.
    if (m_maxPayloadSize == 0) {
        return 0;
    }

    // Pure deterministic sizes (e.g., Blood Pressure, Temperature)
    if (m_payloadJitter <= 0.0) {
        return m_maxPayloadSize;
    }
    
    // Dynamically scale the hardware floor based on configured variance.
    double lowerBound = m_maxPayloadSize * (1.0 - m_payloadJitter);
    
    // Determine exact jitter burst volume
    double varianceRoll = m_sizeDist(m_rng); 
    double jitterVolume = m_maxPayloadSize * m_payloadJitter * varianceRoll;
    
    uint32_t finalSize = static_cast<uint32_t>(lowerBound + jitterVolume);

    // Hard physics bounds
    finalSize = std::max(1u, finalSize);
    finalSize = std::min(m_maxPayloadSize, finalSize);

    return finalSize;
}