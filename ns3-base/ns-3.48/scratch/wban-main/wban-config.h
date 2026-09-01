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
 * \brief Quality of Service (QoS) Priority Traffic Classes
 * Used to tag generated packets for Traffic Control (QueueDisc) sorting.
 */
enum QosPriority {
    QOS_CP = 0,   // Critical Data Packet (Emergency / High Urgency)
    QOS_RP = 1,   // Reliability Data Packet (Vital Medical Alarms)
    QOS_DP = 2,   // Delay-Sensitive Data Packet (Continuous Bio-Signals like ECG/EEG)
    QOS_OP = 3,   // Ordinary Data Packet (Routine Sensors / Environmental Data)
    QOS_NONE = 4  // Infrastructure Sinks / Coordinators
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
    
    std::array<double, 4> trafficRatios; // Distribution ratios for [CP, RP, DP, OP]
    
    double initialEnergyJoules;

    // --- MAC LAYER GTS SCHEDULING ---
    // Number of IEEE 802.15.4 superframe time slots (~30.72 ms each) requested via MlmeGtsRequest
    uint8_t requestedGtsSlots;
};

// ============================================================================
// 3. GLOBAL TOPOLOGY DECLARATION
// ============================================================================
// Global node configuration registry (renamed from WBAN_HETEROGENEOUS_NETWORK)
extern const std::vector<WbanNodeConfig> WBAN_NETWORK;

#endif // WBAN_CONFIG_H
