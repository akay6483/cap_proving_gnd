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
// 1. MAC & TSN SCHEDULING ENUMS (Updated to Modern Literature Standards)
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
    double applicationIntervalSec;   
    double maxDutyCycle;         
    
    double activeProbability;
    std::array<double, 4> trafficRatios; 
    
    double initialEnergyJoules;  
};

// =========================================================================================================================================
// LEGEND:
// { ID, "Name", Vector, Coord?, Env, Tx(dBm), Rx(dBm), PHY, MaxPayload(B), Int(s), Duty, Prob_Active, [CP, RP, DP, OP], Energy(J) }
// =========================================================================================================================================

const std::vector<WbanNodeConfig> WBAN_HETEROGENEOUS_NETWORK = {
    
    // 0. Coordinator (Waist) - WBAN/Wi-Fi Bridge. Generates no packets natively.
    {0, "Coordinator",   Vector(0.0, 0.0, 0.0),    true,  ENV_ON_BODY, 0.0, -119.20, WbanPhyOption::NB_2400_MHZ_242_9,   0, 0.0,    1.0,  0.00, {0.00, 0.00, 0.00, 0.00}, 50.0},
    
    // 1. LPU / Smartphone (Pocket) - The Inter-BAN Sink. Wi-Fi only. Generates no WBAN sensor data.
    {1, "LPU_Sink",      Vector(0.5, 0.0, 0.0),    false, ENV_ON_BODY, 20.0, -90.00, WbanPhyOption::NB_2400_MHZ_242_9,   0, 0.0,    1.0,  0.00, {0.00, 0.00, 0.00, 0.00}, 36000.0},

    // 2. EEG (Head)
    {2, "EEG",           Vector(0.0, 0.0, 0.7),    false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_242_9, 255, 0.047, 0.10, 0.90, {0.05, 0.00, 0.85, 0.10}, 10.0},
    
    // 3. Hearing Aid (Right Ear)
    {3, "HearingAid",    Vector(0.1, 0.0, 0.65),   false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_242_9, 255, 0.010, 0.50, 0.80, {0.00, 0.00, 0.90, 0.10}, 10.0},
    
    // 4. Temperature (Chest)
    {4, "Temperature",   Vector(0.15, 0.05, 0.4),  false, ENV_ON_BODY,-10.0, -117.95, WbanPhyOption::NB_863_MHZ_101_2,   16, 60.0,  0.01, 0.05, {0.20, 0.00, 0.00, 0.80},  5.0},
    
    // 5. ECG (Heart)
    {5, "ECG",           Vector(-0.1, 0.1, 0.4),   false, ENV_ON_BODY,-10.0, -119.20, WbanPhyOption::NB_402_MHZ_151_8,  200, 0.50,  0.10, 1.00, {0.05, 0.00, 0.85, 0.10}, 10.0},
    
    // 6. Blood Pressure (Arm)
    {6, "BloodPressure", Vector(0.3, 0.0, 0.3),    false, ENV_ON_BODY,-10.0, -117.95, WbanPhyOption::NB_863_MHZ_101_2,   32, 30.0,  0.01, 0.10, {0.20, 0.70, 0.00, 0.10},  5.0},
    
    // 7. Insulin Pump (Implant)
    {7, "InsulinPump",   Vector(0.0, 0.1, 0.05),   false, ENV_IN_BODY,-15.0, -119.20, WbanPhyOption::NB_402_MHZ_75_9,    16, 1.0,   0.01, 0.02, {0.80, 0.10, 0.00, 0.10}, 20.0},
    
    // 8. SpO2 (Wrist)
    {8, "SpO2",          Vector(-0.4, 0.1, 0.0),   false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_121_4, 128, 0.032, 0.05, 0.50, {0.05, 0.85, 0.00, 0.10}, 10.0},
    
    // 9. Accelerometer (Thigh)
    {9, "Accelerometer", Vector(0.15, 0.1, -0.4),  false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_121_4,  64, 0.05,  0.05, 0.20, {0.10, 0.00, 0.00, 0.90},  5.0},
    
    // 10. EMG (Calf)
    {10, "EMG",          Vector(-0.15, 0.1, -0.7), false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2360_MHZ_242_9, 255, 0.006, 0.10, 0.60, {0.00, 0.00, 0.90, 0.10}, 10.0},
    
    // 11. Motion Sensor (Ankle)
    {11, "MotionSensor", Vector(-0.15, 0.1, -0.9), false, ENV_ON_BODY,-10.0, -113.97, WbanPhyOption::NB_2400_MHZ_121_4,  64, 0.05,  0.05, 0.10, {0.00, 0.00, 0.00, 1.00},  5.0}
};

#endif // WBAN_CONFIG_H