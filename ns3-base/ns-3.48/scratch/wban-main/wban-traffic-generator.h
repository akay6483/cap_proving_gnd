#ifndef WBAN_TRAFFIC_GENERATOR_H
#define WBAN_TRAFFIC_GENERATOR_H

#include <cstdint>
#include <array>
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "wban-config.h" // Provides the unified QosPriority enum

class WbanTrafficGenerator {
public:
    WbanTrafficGenerator(uint32_t nodeId, 
                         double intervalSec, 
                         uint32_t maxPayload, 
                         double payloadJitter,
                         double intervalJitter,
                         std::array<double, 4> trafficRatios);

    // Generates the QoS priority of the next packet based on CDF ratios
    QosPriority GenerateNextPacketType();
    
    // Calculates the byte size of the payload, applying jitter if configured
    uint32_t GetPayloadSize();
    
    // Calculates the generation interval, applying temporal jitter if configured
    double GetInterval();

    // Hooks into ns-3's global RNG seed manager to guarantee reproducibility
    int64_t AssignStreams(int64_t stream);

private:
    uint32_t m_nodeId;
    double m_intervalSec;
    uint32_t m_maxPayloadSize;
    double m_payloadJitter;
    double m_intervalJitter; 
    std::array<double, 4> m_trafficRatios;
    std::array<double, 4> m_cumulativeProbabilities; 
    
    // --- ns-3 Native Stochastic Engine Components ---
    ns3::Ptr<ns3::UniformRandomVariable> m_probDist;   // For CDF priority rolls [0.0, 1.0)
    ns3::Ptr<ns3::UniformRandomVariable> m_sizeDist;   // For payload jitter multipliers [0.0, 1.0)
    ns3::Ptr<ns3::UniformRandomVariable> m_jitterDist; // For temporal variance multipliers [-1.0, 1.0]
};

#endif // WBAN_TRAFFIC_GENERATOR_H