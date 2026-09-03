#ifndef WBAN_CONFIG_H
#define WBAN_CONFIG_H

#include "ns3/core-module.h"
#include <string>
#include <vector>
#include <array>
#include <cstdint>

using namespace ns3;

// ============================================================================
// 1. MAC & TSN SCHEDULING ENUMS
// ============================================================================

/**
 * \brief TIER-2 PACKET CLASSIFICATION (Dynamic per-packet)
 * Used to tag generated packets via SocketPriorityTag. The downstream TasQueueDisc 
 * Classify() method strictly relies on this 4-level enum to sort packets into the 
 * correct GCL queues, which is mandatory for guard band protection to work.
 */
enum QosPriority {
    QOS_CP = 0,   // Critical Data Packet (Emergency / High Urgency)
    QOS_RP = 1,   // Reliability Data Packet (Vital Medical Alarms)
    QOS_DP = 2,   // Delay-Sensitive Data Packet (Continuous Bio-Signals like ECG/EEG)
    QOS_OP = 3,   // Ordinary Data Packet (Routine Sensors / Environmental Data)
    QOS_NONE = 4  // Infrastructure Sinks / Coordinators (No Data Generation)
};

/**
 * \brief TIER-1 SENSOR HIERARCHY (Static per-node)
 * Dictates how the WbanCentralScheduler prioritizes the physical node 
 * during the Managed Access Phase (MAP) allocation at T=0.
 */
enum SensorHierarchy {
    NODE_CRITICAL_STREAM = 0, // Priority 0: Guaranteed MAP slots (e.g., ECG, EEG)
    NODE_HIGH_EVENT = 1,      // Priority 1: EAP Preemption slots (e.g., Alarms, SpO2)
    NODE_ROUTINE = 2,         // Priority 2: Round-Robin MAP slots (e.g., Temperature)
    NODE_INFRASTRUCTURE = 3   // Priority 3: Coordinator / LPU Sink (No TDMA slots required)
};

// ============================================================================
// 2. UNIFIED WBAN NODE BLUEPRINT (Standard IEEE 802.15.4 Constraints)
// ============================================================================
/**
 * \brief Configuration structure defining physical, traffic, energy, and GTS slot
 * properties for every node in the Wireless Body Area Network.
 */
struct WbanNodeConfig {
    uint32_t nodeId;
    std::string name;
    Vector position;
    bool isCoordinator;
    
    // Physical Layer Configuration
    double txPowerDbm;
    double rxSensitivityDbm;
    uint8_t channel;
    
    // Application Traffic Specification
    uint32_t maxPayloadSize;
    double payloadJitter;
    
    double applicationIntervalSec;
    double intervalJitter;
    
    // Distribution ratios for dynamic packet generation [CP, RP, DP, OP]
    std::array<double, 4> trafficRatios; 
    
    double initialEnergyJoules;

    // --- MAC LAYER GTS SCHEDULING ---
    // Number of IEEE 802.15.4 superframe time slots requested
    uint8_t requestedGtsSlots;
    
    // Defines the node's static hierarchical standing for the Central Scheduler
    SensorHierarchy baseHierarchy; 
};

// ============================================================================
// 3. GLOBAL TOPOLOGY DECLARATION
// ============================================================================
// Global node configuration registry
extern const std::vector<WbanNodeConfig> WBAN_NETWORK;

#endif // WBAN_CONFIG_H
