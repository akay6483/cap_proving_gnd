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

enum QosPriority {
    QOS_CP = 0,   // Critical Packet (Emergency / High Urgency / Spikes)
    QOS_RP = 1,   // Reliability Sensitive Packet (Vital Alarms / Continuous Bio-Signals)
    QOS_OP = 2,   // Ordinary Packet (Routine Sensors / Environmental Data)
    QOS_NONE = 3  // Infrastructure Sinks / Coordinators (No Data Generation)
};

enum SensorHierarchy {
    NODE_CRITICAL_STREAM = 0, // Priority 0: Guaranteed MAP slots (e.g., ECG, EEG)
    NODE_HIGH_EVENT = 1,      // Priority 1: EAP Preemption slots (e.g., Alarms, SpO2)
    NODE_ROUTINE = 2,         // Priority 2: Round-Robin MAP slots (e.g., Temperature)
    NODE_INFRASTRUCTURE = 3   // Priority 3: Coordinator / LPU Sink (No TDMA slots required)
};

// ============================================================================
// 2. UNIFIED WBAN NODE BLUEPRINT (Standard IEEE 802.15.4 Constraints)
// ============================================================================
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
    
    // Distribution ratios for dynamic packet generation [CP, RP, OP]
    std::array<double, 3> trafficRatios; 
    
    double initialEnergyJoules;

    // --- MAC LAYER GTS SCHEDULING ---
    uint8_t requestedGtsSlots;
    SensorHierarchy baseHierarchy; 

    // --- BUFFER MANAGEMENT ---
    uint32_t maxBufferSize; // Maximum hardware memory buffer capacity in bytes
};

// ============================================================================
// 3. GLOBAL TOPOLOGY DECLARATION
// ============================================================================
extern const std::vector<WbanNodeConfig> WBAN_NETWORK;

#endif // WBAN_CONFIG_H