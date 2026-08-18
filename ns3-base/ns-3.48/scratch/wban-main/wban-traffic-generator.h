#ifndef WBAN_TRAFFIC_GENERATOR_H
#define WBAN_TRAFFIC_GENERATOR_H

#include <cstdint>
#include <array>
#include <random>
#include "wban-config.h" 

enum TrafficClass {
    CLASS_CP = 0,
    CLASS_RP = 1,
    CLASS_DP = 2,
    CLASS_OP = 3,
    CLASS_NONE = 4  
};

class WbanTrafficGenerator {
public:
    WbanTrafficGenerator(uint32_t nodeId, 
                         double intervalSec, 
                         uint32_t maxPayload, 
                         double payloadJitter,
                         double intervalJitter,
                         std::array<double, 4> trafficRatios);

    TrafficClass GenerateNextPacketType();
    uint32_t GetPayloadSize();
    double GetInterval();

private:
    uint32_t m_nodeId;
    double m_intervalSec;
    uint32_t m_maxPayloadSize;
    double m_payloadJitter;
    double m_intervalJitter; 
    std::array<double, 4> m_trafficRatios;
    
    // --- C++11 Stochastic Engine Components ---
    std::mt19937 m_rng;                                  
    std::uniform_real_distribution<double> m_probDist;   // For CDF priority rolls
    std::uniform_real_distribution<double> m_sizeDist;   // For payload jitter multipliers [0.0, 1.0)
    std::uniform_real_distribution<double> m_jitterDist; // For temporal variance multipliers [-1.0, 1.0]

    std::array<double, 4> m_cumulativeProbabilities; 
};

#endif // WBAN_TRAFFIC_GENERATOR_H