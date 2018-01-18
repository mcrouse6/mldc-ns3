/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/mobility-module.h"
#include "ns3/contrib-module.h"
#include "ns3/wifi-module.h"
#include "ns3/cone-antenna.h"
#include "ns3/packet-sink.h"
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("WifiSimpleInterference");

using namespace ns3;

vector< Ptr<PacketSink> > sinks;
vector<double> last;
time_t last_t;

NodeContainer c;
NetDeviceContainer devices;
Ipv4InterfaceContainer ipv4InterfaceContainer; 

double loc[][3] = {
      {0, 0, 0}, 
      {0, 1, 0},
      {3, 3, 0},
      {3, 4, 0}
};

void Progress ()
{
    Time now = Simulator::Now ();
    static time_t now_t=0;
    time(&now_t);
    cout << "Current time is " << now.GetSeconds() << " [real: " << now_t << " +" << now_t-last_t << "]" << endl;
    last_t = now_t;
    for (int i=0; i < (int)sinks.size(); i++) {
        if (sinks[i]) {
            double cur = sinks[i]->GetTotalRx() * (double)8/1e6;
            cout << "\t 1:" << cur << " [+" << cur-last[i] << "]" << endl;
            last[i] = cur;
        }
    }
    Simulator::Schedule (Seconds (1), Progress);
}

void SetTCPFlow (int from, int to, double startTime)
{
    Ptr<Node> sourceNode = c.Get(from);
    Ptr<Node> sinkNode = c.Get(to);
    Ipv4Address sinkAddr = ipv4InterfaceContainer.Get(to).first->GetAddress( ipv4InterfaceContainer.Get(to).second, 0).GetLocal();

    /* Receiver */
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), 9999));
    ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sinks.push_back(sink);
    last.push_back(0);
    sinkApp.Start (Seconds(0));

    /* Sender */
    OnOffHelper src ("ns3::TcpSocketFactory", Address ());
    AddressValue dest (InetSocketAddress(sinkAddr, 9999));
    src.SetAttribute ("Remote", dest);
    ApplicationContainer srcApp;
    srcApp.Add (src.Install(sourceNode));
    srcApp.Start (Seconds(startTime));
}


double GetAngle(double x1, double y1, double x2, double y2)
{
     double dx = x2-x1; 
     double dy = y2-y1;

     return atan2(dy, dx);
}

void PointAntenna(Ptr<WifiNetDevice> wnd, double angle)
{
  Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy>(wnd->GetPhy());
  Ptr<ConeAntenna> ca = DynamicCast<ConeAntenna>(phy->GetAntenna());
  ca->SetAzimuthAngle(angle);
}


void PointAtEachOther(int a, int b)
{

  double angle = GetAngle(loc[a][0], loc[a][1], loc[b][0], loc[b][1]);
  Ptr<WifiNetDevice> first = DynamicCast<WifiNetDevice>(devices.Get(a));
  PointAntenna(first, angle);
  Ptr<WifiNetDevice> second = DynamicCast<WifiNetDevice>(devices.Get(b));
  angle = GetAngle(loc[b][0], loc[b][1], loc[a][0], loc[a][1]);
  PointAntenna(second, angle);
}

int main (int argc, char *argv[])
{
  std::string phyMode;
  std::string errorModel;
  bool verbose = false;
  double energyDetectDbm;
  double slowdown = 1;
  double dbiGain = 10;

  /* Command line argument parser setup */
  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("dbiGain", "gain (in dbi) of the antennas", dbiGain);
  cmd.Parse (argc, argv);

  // these are not command line arguments for this version
  enum WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ad;

  /* Configure the frequency based on the standard */
  double freq;
  if (standard == WIFI_PHY_STANDARD_80211b) {
    freq = 2.4e9;
    phyMode = "DsssRate1Mbps";
    errorModel = "ns3::NistErrorRateModel";
    energyDetectDbm = -96;
  } else if (standard == WIFI_PHY_STANDARD_80211a) {
    freq = 5.4e9;
    phyMode = "OfdmRate6Mbps";
    errorModel = "ns3::NistErrorRateModel";
    energyDetectDbm = -96;
  } else if (standard == WIFI_PHY_STANDARD_80211ad) {
    freq = 59.4e9;
    phyMode = "VHTMCS3";
    errorModel = "ns3::SensitivityModel60GHz";
    energyDetectDbm = -77;
  } else
    NS_FATAL_ERROR("unsupported phy standard");

  /* Global params: no fragmentation, no RTS/CTS, fixed rate for all packets */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
		  StringValue ("120000"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
		  StringValue ("120000"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode));

  /* Set up slowdown */
  if (slowdown < 1 || slowdown > 10000)
    NS_FATAL_ERROR ("slowdown " << slowdown << " unsupported [1, 10000]");
  InterferenceHelper::SlowdownFactor = slowdown;


  /**** WifiHelper is a meta-helper: it helps creates helpers ****/
  WifiHelper wifi;
  wifi.SetStandard (standard);

  /* Turn on logging? */
  if (verbose) {
    wifi.EnableLogComponents ();
  }


 /**** Set up Channel ****/
  YansWifiChannelHelper wifiChannel ;
  /* Simple propagation delay model */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* Friis model with standard-specific wavelength */
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel",
		  "Lambda", DoubleValue(3e8/freq));



  /**** Allocate a default Adhoc Wifi MAC ****/
  /* Add a QoS upper mac */
  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default ();
  /* Set it to flyways adhoc mode */
  wifiMac.SetType ("ns3::FlywaysWifiMac");
  /* Enable packet aggregation */
  wifiMac.SetBlockAckThresholdForAc(AC_BE, 4);
  /* Parametrize */
  wifiMac.SetMsduAggregatorForAc (AC_BE, "ns3::MsduStandardAggregator", 
                                     "MaxAmsduSize", UintegerValue (65535));

  /**** SET UP ALL NODES ****/
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  /* Nodes will be added to the channel we set up earlier */
  wifiPhy.SetChannel (wifiChannel.Create ());
  /* All nodes transmit at 10 dBm == 10 mW, no adaptation */
  wifiPhy.Set ("TxPowerStart", DoubleValue(10.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(10.0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  /* Sensitivity model includes implementation loss and noise figure */
  wifiPhy.Set ("RxNoiseFigure", DoubleValue(0));
  /* Set CS thresholds for all nodes */
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (energyDetectDbm) );
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (energyDetectDbm-3) );
  /* Set default algorithm for all nodes to be constant rate */
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                   "ControlMode",StringValue(phyMode));

  /* Give all nodes cone antennas of specified dBi gain */
  wifiPhy.SetAntenna ("ns3::ConeAntenna", "Beamwidth", DoubleValue(ConeAntenna::GainDbiToBeamwidth(dbiGain)));

  /* Set the phy layer error model */
  wifiPhy.SetErrorRateModel (errorModel);

  /* Make four nodes and set them up with the phy and the mac */
  c.Create (4);
  devices = wifi.Install (wifiPhy, wifiMac, c);


  /**** Set up node positions ****/
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (int i = 0; i < 4; i++) {
        positionAlloc->Add (Vector (loc[i][0], loc[i][1], loc[i][2]));
  }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);
  

  /**** Give all nodes an IP Stack ****/
  InternetStackHelper internet;
  internet.Install (c);
  /* And their WiFi interfaces IP addresses in 10.1.1.0 range */
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4InterfaceContainer = ipv4.Assign (devices);

  /**** Set up TCP ****/
  Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue(1e9));
  Config::SetDefault ("ns3::OnOffApplication::OnTime",
		  RandomVariableValue(ConstantVariable(1)));
  Config::SetDefault ("ns3::OnOffApplication::DataRate",
		  DataRateValue(DataRate("1000Mb/s")));
  Config::SetDefault ("ns3::OnOffApplication::OffTime",
		  RandomVariableValue(ConstantVariable(0)));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1460));

  PointAtEachOther(0, 1);
  SetTCPFlow(0, 1, 0.001);
  PointAtEachOther(2, 3);
  SetTCPFlow(2, 3, 0.01);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  cout << "Set up global routing." << endl;
  Simulator::Schedule(Seconds(1), Progress);

  Simulator::Stop (Seconds(3.001));
  time(&last_t);
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

