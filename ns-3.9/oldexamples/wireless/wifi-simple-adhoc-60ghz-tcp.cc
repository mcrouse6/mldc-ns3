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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhoc60GHzTcp");

using namespace ns3;

Ptr<PacketSink> sink;
time_t last_t;
time_t start_t;
double last;
NetDeviceContainer devices;

double loc[][3] = {
      {0, 0, 0}, 
      {1, 0, 0},
};

void Progress ()
{
    Time now = Simulator::Now ();
    static time_t now_t=0;
    time(&now_t);
    cout << "Current time is " << now.GetSeconds() << " [real: " << now_t-start_t << " +" << now_t-last_t << "]" << endl;
    last_t = now_t;
    if (sink) {
        double cur = sink->GetTotalRx() * (double)8/1e6;
        cout << "\t 1:" << cur << " [+" << cur-last << "]" << endl;
        last = cur;
    }
    Simulator::Schedule (Seconds (1), Progress);
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
  double dbiGain = 10;

  // these are not command line arguments for this version
  double distanceToRx = 7.0; // meters
  double ber = 1e-9; /* Rate selection picks highest rate with BER < ber */
  enum WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ad;

  /* Command line argument parser setup */
  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("ber", "ber threshold for rate selection", ber);
  cmd.AddValue ("dist", "distance between nodes", distanceToRx);
  cmd.AddValue ("dbiGain", "gain of the antennas", dbiGain);
  cmd.Parse (argc, argv);

  /* Configure the frequency based on the standard */
  double freq;
  if (standard == WIFI_PHY_STANDARD_80211b) {
    freq = 2.4e9;
    phyMode = "DsssRate1Mbps";
    errorModel = "ns3::NistErrorRateModel";
  } else if (standard == WIFI_PHY_STANDARD_80211a) {
    freq = 5.4e9;
    phyMode = "OfdmRate6Mbps";
    errorModel = "ns3::NistErrorRateModel";
  } else if (standard == WIFI_PHY_STANDARD_80211ad) {
    freq = 59.4e9;
    phyMode = "VHTMCS1";
    errorModel = "ns3::SensitivityModel60GHz";
  } else
    NS_FATAL_ERROR("unsupported phy standard");

  /* Global params: no fragmentation, no RTS/CTS, fixed rate for all packets */
  /* various defaults */
    Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (65535-8));
    Config::SetDefault ("ns3::WifiNetDevice::Mtu", UintegerValue (65535-8));
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("VHTMCS3"));
    Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue(0));
    Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue(1e6));
    Config::SetDefault ("ns3::OnOffApplication::OnTime", RandomVariableValue(ConstantVariable(1e6)));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", DataRateValue(DataRate("4Gbps"))); 
    Config::SetDefault ("ns3::OnOffApplication::OffTime", RandomVariableValue(ConstantVariable(0)));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(32767));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue(2e6));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(2e6));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(32));

  /**** WifiHelper is a meta-helper: it helps creates helpers ****/
  WifiHelper wifi;
  /* Basic setup */
  wifi.SetStandard (standard);

  /* Turn on logging? */
  if (verbose) {
    wifi.EnableLogComponents ();
    LogComponentEnable ("WifiSimpleAdhoc60GHzTcp", LOG_LEVEL_ALL);
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
  /* Set CCA threshold for all nodes to 0dBm=1mW; i.e. don't carrier sense */
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79) );
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79+3) );
  /* Set default algorithm for all nodes to be constant rate */
  wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                "BerThreshold",DoubleValue(ber));
  /* Set the phy layer error model */
  wifiPhy.SetErrorRateModel (errorModel);
  /* Give all nodes cone antennas of specified dBi gain */
  wifiPhy.SetAntenna ("ns3::ConeAntenna", "Beamwidth", DoubleValue(ConeAntenna::GainDbiToBeamwidth(dbiGain)));


  /* Make two nodes and set them up with the phy and the mac */
  NodeContainer c;
  c.Create (2);
  devices = wifi.Install (wifiPhy, wifiMac, c);

  /**** Set up node positions ****/
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  loc[1][0] = distanceToRx;
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
  NS_LOG_DEBUG ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  /**** Set up TCP ****/
  /* Receiver */
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
		  InetSocketAddress (Ipv4Address::GetAny(), 9999));
  ApplicationContainer sinkApp = sinkHelper.Install(c.Get(1));
  sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  sinkApp.Start (Seconds(0.0));
  /* Sender */
  OnOffHelper src ("ns3::TcpSocketFactory", Address ());
  AddressValue dest (InetSocketAddress(Ipv4Address("10.1.1.2"), 9999));
  src.SetAttribute ("Remote", dest);
  ApplicationContainer srcApp;
  srcApp.Add (src.Install(c.Get(0)));
  srcApp.Start (Seconds(0));
  
  PointAtEachOther(0,1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  cout << "Set up global routing." << endl;
  Simulator::Schedule(Seconds(1.0), Progress);

  Simulator::Stop (Seconds(10.001));
  time(&start_t);
  last_t = start_t;
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

