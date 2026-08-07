#ifndef WBAN_TRAFFIC_GENERATOR_H
#define WBAN_TRAFFIC_GENERATOR_H

#include <cstdint>
#include <array>
#include <random>

// ============================================================================
// ENUM: TrafficClass
// Updated to reflect modern WBAN literature taxonomy:
// CP: Critical, RP: Reliability, DP: Delay, OP: Ordinary
// ============================================================================
enum TrafficClass {
    CLASS_CP = 0,
    CLASS_RP = 1,
    CLASS_DP = 2,
    CLASS_OP = 3,
    CLASS_NONE = 4  // Used for idle ticks and sink nodes
};

// ============================================================================
// CLASS: WbanTrafficGenerator
// A standalone mathematical engine. Calculates what type of packet a sensor 
// should generate based on two orthogonal stochastic parameters:
// 1. The probability that the sensor is active on a given tick.
// 2. The medical profile (ratio) of the data it generates when active.
// ============================================================================
class WbanTrafficGenerator {
public:
    /**
     * @brief Constructor for the pure C++ Traffic Generator.
     * @param nodeId Unique node ID used to deterministically seed the RNG.
     * @param intervalSec The rigid hardware polling interval.
     * @param maxPayload Maximum payload size supported by the PHY layer.
     * @param activeProbability The chance (0.0 to 1.0) of generating a packet per tick.
     * @param trafficRatios The distribution of packet types [CP, RP, DP, OP] if active.
     */
    WbanTrafficGenerator(uint32_t nodeId, 
                         double intervalSec, 
                         uint32_t maxPayload, 
                         double activeProbability,
                         std::array<double, 4> trafficRatios);

    /**
     * @brief The core stochastic selection engine.
     * Executes a two-step Monte Carlo simulation (Activation Roll -> Type Roll).
     * @return TrafficClass The selected packet type (or CLASS_NONE if idle).
     */
    TrafficClass GenerateNextPacketType();

    /**
     * @brief Dynamically calculates payload size based on hardware limits.
     * Size is independent of priority classification, simulating realistic
     * sensor data generation (e.g., an ECG waveform fluctuating slightly in size).
     * @return uint32_t Payload size in bytes.
     */
    uint32_t GetPayloadSize();

    /**
     * @brief Returns the rigid hardware clock tick duration.
     * @return double Interval in seconds.
     */
    double GetInterval() const;

private:
    // --- Hardware & Configuration State ---
    uint32_t m_nodeId;
    double m_intervalSec;
    uint32_t m_maxPayloadSize;
    
    // --- Independent Stochastic Parameters ---
    double m_activeProbability;            // Network Load dial (e.g., 0.05)
    std::array<double, 4> m_trafficRatios; // Medical Profile (e.g., {0.20, 0.0, 0.0, 0.80})
    
    // Flag to instantly bypass execution for Coordinator nodes
    bool m_isSink; 

    // --- C++11 Stochastic Engine Components ---
    std::mt19937 m_rng;                                  // Mersenne Twister Generator
    std::uniform_real_distribution<double> m_dist;       // Float generator [0.0, 1.0) for probability rolls
    std::uniform_int_distribution<uint32_t> m_sizeDist;  // Integer generator [0, 100] for payload jitter

    // --- Mechanical Array ---
    // The normalized Cumulative Distribution Function used strictly to execute 
    // the secondary roll (Traffic Type selection).
    std::array<double, 4> m_cumulativeProbabilities; 
};

#endif // WBAN_TRAFFIC_GENERATOR_H