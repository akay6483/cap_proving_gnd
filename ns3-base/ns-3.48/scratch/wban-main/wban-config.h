#ifndef WBAN_CONFIG_H
#define WBAN_CONFIG_H

#include "ns3/core-module.h"
#include "ns3/wban-module.h"
#include <string>
#include <vector>
#include <array>

using namespace ns3;
using namespace ns3::wban;

// ============================================================================
// 1. MAC & TSN SCHEDULING ENUMS (Modern Medical Taxonomy)
// ============================================================================
enum QosPriority {
    QOS_CP = 0,   // Critical Data Packet (Emergencies, Actuation)
    QOS_RP = 1,   // Reliability Data Packet (Vital signs, zero-loss required)
    QOS_DP = 2,   // Delay Data Packet (Continuous waveforms, real-time required)
    QOS_OP = 3,   // Ordinary Data Packet (Routine, battery checks, no strict bounds)
    QOS_NONE = 4  // Sinks/Coordinators
};

enum WbanEnvironment {
    ENV_IN_BODY = 0,    // CM1 / CM2 (Implants)
    ENV_ON_BODY = 1     // CM3 (Wearables)
};

// ============================================================================
// 2. UNIFIED WBAN NODE BLUEPRINT
// ============================================================================
struct WbanNodeConfig {
    uint32_t nodeId;
    std::string name;
    Vector position;
    bool isCoordinator;
    WbanEnvironment environment;     

    double txPowerDbm;
    double rxSensitivityDbm;
    WbanPhyOption phyOption; 
    
    uint32_t maxPayloadSize;         
    double payloadJitter;
    
    double applicationIntervalSec;   
    double intervalJitter;                   
    
    std::array<double, 4> trafficRatios; 
    
    double initialEnergyJoules;      
};

// ==============================================================================================================================================================
// LEGEND:
// { ID, "Name", Vector, Coord?, Env, Tx(dBm), Rx(dBm), PHY, MaxPayload(B), SizeJitter, Int(s), TimeJitter, Duty, [CP, RP, DP, OP], Energy(J) }
// ==============================================================================================================================================================

const std::vector<WbanNodeConfig> WBAN_HETEROGENEOUS_NETWORK = {
    
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    // SINK NODES - Generate no packets natively.
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    {0, "Coordinator",   Vector(0.0, 0.0, 0.0),    true,  ENV_ON_BODY, 0.0, -119.20, WbanPhyOption::NB_2400_MHZ_242_9,   0, 0.00,  0.0, 0.00, {0.00, 0.00, 0.00, 0.00}, 50.0},
    {1, "LPU_Sink",      Vector(0.5, 0.0, 0.0),    false, ENV_ON_BODY, 20.0, -90.00, WbanPhyOption::NB_2400_MHZ_242_9,   0, 0.00,  0.0, 0.00, {0.00, 0.00, 0.00, 0.00}, 36000.0},

    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    // STREAMING SENSORS (0.0 Time Jitter) - Minimal payload jitter due to waveform compression consistency.
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    {2, "EEG",           Vector(0.0, 0.0, 0.7),    false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_242_9, 255, 0.05, 0.25, 0.00, {0.05, 0.00, 0.85, 0.10}, 10.0},
    {3, "HearingAid",    Vector(0.1, 0.0, 0.65),   false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_242_9, 255, 0.15, 0.10, 0.00, {0.00, 0.00, 0.90, 0.10}, 10.0},
    {5, "ECG",           Vector(-0.1, 0.1, 0.4),   false, ENV_ON_BODY,-10.0, -119.20, WbanPhyOption::NB_402_MHZ_151_8,  200, 0.05, 0.50, 0.00, {0.05, 0.00, 0.85, 0.10}, 10.0},
    {8, "SpO2",          Vector(-0.4, 0.1, 0.0),   false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_121_4, 128, 0.02, 1.00, 0.00, {0.05, 0.85, 0.00, 0.10}, 10.0},
    {10, "EMG",          Vector(-0.15, 0.1, -0.7), false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_242_9, 255, 0.05, 0.25, 0.00, {0.00, 0.00, 0.90, 0.10}, 10.0},
    
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    // EVENT-DRIVEN SENSORS (> 0.0 Time Jitter) - Adjusted to realistic clinical intervals for proper representation.
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------
    {4, "Temperature",   Vector(0.15, 0.05, 0.4),  false, ENV_ON_BODY,-10.0, -117.95, WbanPhyOption::NB_863_MHZ_101_2,   16, 0.00,  60.0, 0.15, {0.20, 0.00, 0.00, 0.80},  5.0}, // Sampled every 1 min
    {6, "BloodPressure", Vector(0.3, 0.0, 0.3),    false, ENV_ON_BODY,-10.0, -117.95, WbanPhyOption::NB_863_MHZ_101_2,   32, 0.00, 120.0, 0.05, {0.20, 0.70, 0.00, 0.10},  5.0}, // Sampled every 2 min
    {7, "InsulinPump",   Vector(0.0, 0.1, 0.05),   false, ENV_IN_BODY,-15.0, -119.20, WbanPhyOption::NB_402_MHZ_75_9,    16, 0.00, 120.0, 0.02, {0.80, 0.10, 0.00, 0.10}, 20.0}, // Sampled every 2 min
    {9, "Accelerometer", Vector(0.15, 0.1, -0.4),  false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_121_4,  64, 0.20,  15.0, 0.50, {0.10, 0.00, 0.00, 0.90},  5.0}, // Sampled every 15s
    {11, "MotionSensor", Vector(-0.15, 0.1, -0.9), false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_121_4,  64, 0.25,  10.0, 0.60, {0.00, 0.00, 0.00, 1.00},  5.0}  // Sampled every 10s
};

#endif // WBAN_CONFIG_H