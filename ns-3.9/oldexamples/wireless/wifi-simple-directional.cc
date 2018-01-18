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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("WifiSimpleInterference");

using namespace ns3;

void ReceivePacket (Ptr<Socket> socket)
{
  Address addr;
  socket->GetSockName (addr);
  InetSocketAddress iaddr = InetSocketAddress::ConvertFrom (addr);
  NS_LOG_UNCOND ("Received one packet!  Socket: " << iaddr.GetIpv4 () << " port: " << iaddr.GetPort ());
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate11Mbps");
//  std::string phyMode ("VHTMCS3");
  uint32_t PpacketSize = 1000; // bytes
  bool verbose = false;

  // these are not command line arguments for this version
  uint32_t numPackets = 1;
  double interval = 1.0; // seconds
  double startTime = 10.0; // seconds
  double distanceToRx = 100.0; // meters

  /* Command line argument parser setup */
  CommandLine cmd;
  cmd.AddValue ("PpacketSize", "size of application packet sent", PpacketSize);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.Parse (argc, argv);

  /* Global params: no fragmentation, no RTS/CTS, fixed rate for all packets */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
		  StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
		  StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode));


  /**** WifiHelper is a meta-helper: it helps creates helpers ****/
  WifiHelper wifi;
  /* Turn on logging? */
  if (verbose) {
    wifi.EnableLogComponents ();
  }
  /* Use 802.11b for now */
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  /* Configure the frequency based on the standard */
  double freq;
  if (wifi.GetStandard() == WIFI_PHY_STANDARD_80211b)
    freq = 2.4e9;
  else if (wifi.GetStandard() == WIFI_PHY_STANDARD_80211a)
    freq = 5.4e9;
  else if (wifi.GetStandard() == WIFI_PHY_STANDARD_80211ad)
    freq = 59.4e9;
  else
    NS_FATAL_ERROR("unsupported phy standard");



  /**** Set up Channel ****/
  YansWifiChannelHelper wifiChannel ;
  /* Simple propagation delay model */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* Friis model with standard-specific wavelength */
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel",
		  "Lambda", DoubleValue(3e8/freq));



  /**** Allocate a default Adhoc Wifi MAC ****/
  /* Add a non-QoS upper mac */
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  /* Set it to adhoc mode */
  wifiMac.SetType ("ns3::AdhocWifiMac");



  /**** SET UP ALL NODES ****/
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  /* Nodes will be added to the channel we set up earlier */
  wifiPhy.SetChannel (wifiChannel.Create ());
  /* Set CCA threshold for all nodes to 0dBm=1mW; i.e. don't carrier sense */
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (0.0) );
  /* Set default algorithm for all nodes to be constant rate */
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                   "ControlMode",StringValue(phyMode));
  /* Give all nodes omni antennas of 7 dBi gain */
  wifiPhy.SetAntenna ("ns3::ConeAntenna",
		  "GainDbi", DoubleValue(7),
		  "Beamwidth", DoubleValue(ConeAntenna::GainDbiToBeamwidth(7)));
  /* Make two nodes and set them up with the phy and the mac */
  NodeContainer c;
  c.Create (2);
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);



  /**** Set up node positions ****/
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (distanceToRx, 0.0, 0.0));
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
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address("10.1.1.1"), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress remote = InetSocketAddress(
		  Ipv4Address("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  /* Convert packet interval to time object */
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (startTime), &GenerateTraffic, 
				  source, PpacketSize, numPackets,
				  interPacketInterval);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

