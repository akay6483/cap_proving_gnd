#include "wban-traffic-generator.h"

// ============================================================================
// CONSTRUCTOR
// Initializes states, mathematically sanitizes user inputs, and builds the CDF.
// ============================================================================
WbanTrafficGenerator::WbanTrafficGenerator(uint32_t nodeId, 
                                           double intervalSec, 
                                           uint32_t maxPayload, 
                                           double activeProbability,
                                           std::array<double, 4> trafficRatios)
    : m_nodeId(nodeId), 
      m_intervalSec(intervalSec), 
      m_maxPayloadSize(maxPayload), 
      m_activeProbability(activeProbability),
      m_trafficRatios(trafficRatios),
      m_isSink(false),
      m_dist(0.0, 1.0),
      m_sizeDist(0, 100) 
{
    // Deterministic seeding ensures that given the same node ID, the sequence
    // of packet generations will be mathematically identical across simulation runs.
    m_rng.seed(1337 + m_nodeId); 

    // ------------------------------------------------------------------------
    // SANITIZATION 1: Is this node a Sink (Coordinator)?
    // ------------------------------------------------------------------------
    if (m_activeProbability < 1e-6) {
        m_isSink = true;
        return; // Fast-exit. We don't need to build a CDF for a node that never sends.
    }

    // ------------------------------------------------------------------------
    // SANITIZATION 2: Medical Profile Validation & CDF Construction
    // ------------------------------------------------------------------------
    double totalRatio = 0.0;
    for (double r : m_trafficRatios) {
        totalRatio += r;
    }

    // Edge Case Trap: The user configured the node to be active (activeProb > 0), 
    // but accidentally set all traffic ratios to {0,0,0,0}.
    // Fix: We force all traffic to be OP (Ordinary) to prevent division by zero.
    if (totalRatio < 1e-6) {
        m_trafficRatios = {0.0, 0.0, 0.0, 1.0};
        totalRatio = 1.0;
    }

    // Build the Cumulative Distribution Function (Roulette Wheel).
    // By dividing by totalRatio, we perfectly normalize the user's config to 1.0, 
    // resolving floating-point memory overages and protecting the RNG bounds.
    double cumulative = 0.0;
    for (size_t i = 0; i < 4; ++i) {
        cumulative += (m_trafficRatios[i] / totalRatio);
        m_cumulativeProbabilities[i] = cumulative;
    }
}

// ============================================================================
// METHOD: GenerateNextPacketType
// Evaluates the decoupled, two-step stochastic probabilities.
// ============================================================================
TrafficClass WbanTrafficGenerator::GenerateNextPacketType() {
    
    // Fast-fail for nodes that are strictly listeners
    if (m_isSink) {
        return CLASS_NONE; 
    }

    // ------------------------------------------------------------------------
    // STEP 1: THE ACTIVATION CHECK (Will the node send a packet?)
    // ------------------------------------------------------------------------
    // Roll the dice to see if the sensor wakes up and generates data on this tick.
    double activationRoll = m_dist(m_rng);
    
    if (activationRoll > m_activeProbability) {
        return CLASS_NONE; // The sensor remains idle on this tick.
    }

    // ------------------------------------------------------------------------
    // STEP 2: THE TYPE CHECK (What kind of packet will it send?)
    // ------------------------------------------------------------------------
    // We only reach this code if the sensor IS generating a packet.
    // It is critical to roll a NEW random number here to maintain statistical 
    // independence from the activation roll.
    double typeRoll = m_dist(m_rng);

    for (size_t i = 0; i < 4; ++i) {
        // If the sensor supports this type (ratio > 0) and the roll falls 
        // into this CDF bucket, return it.
        if (m_trafficRatios[i] > 0.0 && typeRoll <= m_cumulativeProbabilities[i]) {
            return static_cast<TrafficClass>(i);
        }
    }
    
    // ------------------------------------------------------------------------
    // SAFETY NET: Floating-Point Epsilon Failure
    // ------------------------------------------------------------------------
    // If typeRoll is 0.9999999999999 and the final CDF bucket was stored as 
    // 0.9999999999998, the above loop might fall through. We iterate backwards 
    // (using a signed integer 'i' to prevent underflow) and return the first 
    // valid, natively supported priority class.
    for (int i = 3; i >= 0; --i) {
        if (m_trafficRatios[i] > 0.0) return static_cast<TrafficClass>(i);
    }
    
    // Compiler demand: Must return a TrafficClass. Should theoretically never execute.
    return CLASS_OP;
}

// ============================================================================
// METHOD: GetPayloadSize
// Computes byte size based exclusively on the sensor's physical hardware limits.
// Independent of the QoS Priority class.
// ============================================================================
uint32_t WbanTrafficGenerator::GetPayloadSize() {
    
    // Generate a variance integer between 0 and 100
    uint32_t variance = m_sizeDist(m_rng); 
    
    // Calculate a realistic hardware payload:
    // Base size is 80% of the maximum payload allowed by this sensor.
    uint32_t lowerBound = (m_maxPayloadSize * 80) / 100;
    
    // Add a randomized jitter between 0% and 20% to simulate dynamic streams.
    // (Dividing by 10000 normalizes the 20 * variance multiplication)
    uint32_t jitter = (m_maxPayloadSize * 20 * variance) / 10000;
    
    uint32_t finalSize = lowerBound + jitter;

    // Hard physics bounds: Ensure the size never drops to 0 (which would 
    // crash the ns-3 socket) and strictly enforce the hardware's MTU limit.
    if (finalSize == 0) finalSize = 1;
    if (finalSize > m_maxPayloadSize) finalSize = m_maxPayloadSize;

    return finalSize;
}

// ============================================================================
// METHOD: GetInterval
// Returns the exact configured polling rate.
// ============================================================================
double WbanTrafficGenerator::GetInterval() const {
    return m_intervalSec;
}