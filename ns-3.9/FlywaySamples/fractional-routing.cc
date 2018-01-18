/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/ipv4.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

NodeContainer nodes;
NodeContainer n0n1;
NodeContainer n1n2;
NodeContainer n2n3;

NetDeviceContainer d0d1;
NetDeviceContainer d1d2Primary;
NetDeviceContainer d1d2Secondary;
NetDeviceContainer d2d3;

namespace{
        void ChangeRoute (int)
        {
            Time now = Simulator::Now();
            printf("=====> %lf: Seeting up fractional routes. From now on, half of the pings will see extra delay.\n", now.GetSeconds());

            Ptr<Node> n1 = n0n1.Get(1);
            Ptr<Ipv4> i1 = n1->GetObject<Ipv4>();
            
            uint32_t interfaces[] = {2, 3};
            double fraction[] = {0.9, 0.1};

            Ipv4StaticRoutingHelper ipv4RoutingHelper;
            Ptr<Ipv4StaticRouting> s1 = ipv4RoutingHelper.GetStaticRouting(i1);
            s1->AddFractionalHostRouteTo(Ipv4Address("10.1.4.2"), interfaces, fraction, 2);
        }
}
  int 
main (int argc, char *argv[])
{
  Timer dummyTimer;
  Time delay;

  //Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  delay = Time("10s");
  dummyTimer = Timer();
  dummyTimer.SetDelay(delay);
  dummyTimer.SetFunction(&ChangeRoute);
  dummyTimer.Schedule();

  LogComponentEnable("V4Ping", LOG_LEVEL_INFO);

  nodes.Create (4);

  n0n1.Add(nodes.Get(0));
  n0n1.Add(nodes.Get(1));

  n1n2.Add(nodes.Get(1));
  n1n2.Add(nodes.Get(2));

  n2n3.Add(nodes.Get(2));
  n2n3.Add(nodes.Get(3));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper pointToPoint2;
  pointToPoint2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint2.SetChannelAttribute ("Delay", StringValue ("20ms"));

  d0d1 = pointToPoint.Install (n0n1);
  d1d2Primary = pointToPoint.Install (n1n2);
  d1d2Secondary = pointToPoint2.Install (n1n2);
  d2d3 = pointToPoint.Install (n2n3);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper a0a1Helper;
  a0a1Helper.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer a0a1 = a0a1Helper.Assign (d0d1);

  Ipv4AddressHelper a1a2PrimaryHelper;
  a1a2PrimaryHelper.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer a1a2Primary = a1a2PrimaryHelper.Assign (d1d2Primary);

  Ipv4AddressHelper a1a2SecondaryHelper;
  a1a2SecondaryHelper.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer a1a2Secondary = a1a2SecondaryHelper.Assign (d1d2Secondary);

  Ipv4AddressHelper a2a3Helper;
  a2a3Helper.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer a2a3 = a2a3Helper.Assign (d2d3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n3 = nodes.Get(3);

  V4PingHelper pingHelper = V4PingHelper(a2a3.GetAddress(1));
  pingHelper.SetAttribute("Verbose", BooleanValue(true));
  ApplicationContainer ping = pingHelper.Install(n0);
  ping.Start(Seconds(1.0));
  ping.Stop(Seconds(30.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

