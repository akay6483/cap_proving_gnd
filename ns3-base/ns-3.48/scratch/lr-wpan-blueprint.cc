/*
 * Sample LR-WPAN node blueprint for testing ns-3.
 * Adapted from lr-wpan-data.cc.
 */
#include "ns3/constant-position-mobility-model.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/packet.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;

static void
DataIndication(McpsDataIndicationParams params, Ptr<Packet> p)
{
    std::cout << "Blueprint Node Received Packet! Size: " << p->GetSize()
              << " | LQI: " << static_cast<uint16_t>(params.m_mpduLinkQuality)
              << " | RSSI: " << static_cast<int16_t>(params.m_rssi) << " dBm\n";
}

static void
DataConfirm(McpsDataConfirmParams params)
{
    std::cout << "Data Confirm Callback Status = " << static_cast<uint16_t>(params.m_status) << "\n";
}

static void
StateChangeNotification(std::string context,
                         Time now,
                         PhyEnumeration oldState,
                         PhyEnumeration newState)
{
    std::cout << "Node " << context << " state change at " << now.As(Time::S) << " from "
              << LrWpanHelper::LrWpanPhyEnumerationPrinter(oldState) << " to "
              << LrWpanHelper::LrWpanPhyEnumerationPrinter(newState) << "\n";
}

int
main(int argc, char* argv[])
{
    std::cout << "--- Starting LR-WPAN Blueprint Node Simulation ---\n";

    // Create 2 nodes, and a NetDevice for each one
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    Ptr<LrWpanNetDevice> dev0 = CreateObject<LrWpanNetDevice>();
    Ptr<LrWpanNetDevice> dev1 = CreateObject<LrWpanNetDevice>();

    // Each device must be attached to the same channel
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> propModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    
    channel->AddPropagationLossModel(propModel);
    channel->SetPropagationDelayModel(delayModel);

    dev0->SetChannel(channel);
    dev1->SetChannel(channel);

    // Add devices to nodes
    n0->AddDevice(dev0);
    n1->AddDevice(dev1);

    // Set MAC addresses
    dev0->GetMac()->SetExtendedAddress(Mac64Address("00:00:00:00:00:00:00:01"));
    dev1->GetMac()->SetExtendedAddress(Mac64Address("00:00:00:00:00:00:00:02"));
    dev0->GetMac()->SetShortAddress(Mac16Address("00:01"));
    dev1->GetMac()->SetShortAddress(Mac16Address("00:02"));

    // Trace state changes in the PHY layer
    dev0->GetPhy()->TraceConnect("TrxState", std::string("0 (Sender)"), MakeCallback(&StateChangeNotification));
    dev1->GetPhy()->TraceConnect("TrxState", std::string("1 (Receiver)"), MakeCallback(&StateChangeNotification));

    // Mobility setup
    Ptr<ConstantPositionMobilityModel> mobility0 = CreateObject<ConstantPositionMobilityModel>();
    mobility0->SetPosition(Vector(0, 0, 0));
    n0->AggregateObject(mobility0);

    Ptr<ConstantPositionMobilityModel> mobility1 = CreateObject<ConstantPositionMobilityModel>();
    mobility1->SetPosition(Vector(0, 10, 0)); // 10 meters apart
    n1->AggregateObject(mobility1);

    // Callbacks
    dev0->GetMac()->SetMcpsDataConfirmCallback(MakeCallback(&DataConfirm));
    dev0->GetMac()->SetMcpsDataIndicationCallback(MakeCallback(&DataIndication));
    
    dev1->GetMac()->SetMcpsDataConfirmCallback(MakeCallback(&DataConfirm));
    dev1->GetMac()->SetMcpsDataIndicationCallback(MakeCallback(&DataIndication));

    // Schedule sending data from dev0 to dev1 (50 bytes of dummy data at t=0s)
    Ptr<Packet> p0 = Create<Packet>(50);
    McpsDataRequestParams params;
    params.m_dstPanId = 0;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = Mac16Address("00:02");
    params.m_msduHandle = 0;
    params.m_txOptions = TX_OPTION_ACK;

    Simulator::ScheduleWithContext(1, Seconds(1.0), &LrWpanMac::McpsDataRequest, dev0->GetMac(), params, p0);

    // Schedule sending data back from dev1 to dev0 (60 bytes of dummy data at t=3s)
    Ptr<Packet> p2 = Create<Packet>(60);
    params.m_dstAddr = Mac16Address("00:01");
    Simulator::ScheduleWithContext(2, Seconds(3.0), &LrWpanMac::McpsDataRequest, dev1->GetMac(), params, p2);

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "--- Simulation Finished Successfully ---\n";
    return 0;
}
