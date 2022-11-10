/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Universita' di Firenze, Italy
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
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

// Network topology
//
//    SRC
//     |1
//     |
//     |1
//     A___
//    2| 3 |
//     |   |
//    1|   |
//     B   |
//    2|   |
//     |   |
//    1|   |
//     C_2_|
//    3|
//     |
//    1|  
//    DST   
//
// A, B, C are RIPng routers.
// A and C are configured with static IP addresses
// SRC and DST will exchange packets.
// All interfaces have cost 1
//

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("a4q2c");

void TearDownLink (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv4> ()->SetDown (interfaceA);
  nodeB->GetObject<Ipv4> ()->SetDown (interfaceB);
}

void RecreateLink (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv4> ()->SetUp (interfaceA);
  nodeB->GetObject<Ipv4> ()->SetUp (interfaceB);
}

void run_sim() {
  Config::SetDefault ("ns3::Rip::SplitHorizon", EnumValue (RipNg::SPLIT_HORIZON));

  NS_LOG_INFO ("Create nodes.");
  Ptr<Node> src = CreateObject<Node> ();
  Ptr<Node> dst = CreateObject<Node> ();
  Ptr<Node> a = CreateObject<Node> ();
  Ptr<Node> b = CreateObject<Node> ();
  Ptr<Node> c = CreateObject<Node> ();
  NodeContainer net_src (src, a);
  NodeContainer net_ab  (a, b);
  NodeContainer net_bc  (b, c);
  NodeContainer net_ac  (a, c);
  NodeContainer net_dst (c, dst);
  NodeContainer routers (a, b, c);
  NodeContainer nodes   (src, dst);

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer ndc_src = csma.Install (net_src);
  NetDeviceContainer ndc_ab  = csma.Install (net_ab );
  NetDeviceContainer ndc_bc  = csma.Install (net_bc );
  NetDeviceContainer ndc_ac  = csma.Install (net_ac );
  NetDeviceContainer ndc_dst = csma.Install (net_dst);

  NS_LOG_INFO ("Create IPv4 and routing");
  RipHelper ripRouting;

  // Rule of thumb:
  // Interfaces are added sequentially, starting from 0
  // However, interface 0 is always the loopback...
  ripRouting.ExcludeInterface (a, 1);
  ripRouting.ExcludeInterface (c, 3);

//  ripRouting.SetInterfaceMetric (c, 3, 10);
//  ripRouting.SetInterfaceMetric (d, 1, 10);

  Ipv4ListRoutingHelper listRH;
  listRH.Add (ripRouting, 0);
//  Ipv4StaticRoutingHelper staticRh;
//  listRH.Add (staticRh, 5);
  
  InternetStackHelper internet;
  internet.SetIpv6StackInstall (false);
  internet.SetRoutingHelper (listRH);
  internet.Install (routers);

  InternetStackHelper internetNodes;
  internetNodes.SetIpv6StackInstall (false);
  internetNodes.Install (nodes);

  // Assign addresses.
  // The source and destination networks have global addresses
  // The "core" network just needs link-local addresses for routing.
  // We assign global addresses to the routers as well to receive
  // ICMPv6 errors.
  NS_LOG_INFO ("Assign IPv4 Addresses.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic1 = ipv4.Assign (ndc_src);

  ipv4.SetBase (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic2 = ipv4.Assign (ndc_ab);

  ipv4.SetBase (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic3 = ipv4.Assign (ndc_bc);

  ipv4.SetBase (Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic4 = ipv4.Assign (ndc_ac);

  ipv4.SetBase (Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic5 = ipv4.Assign (ndc_dst);

  Ptr<Ipv4StaticRouting> staticRouting;
  staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (src->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute ("10.0.0.2", 1 );
  staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (dst->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute ("10.0.4.1", 1 );

  RipHelper routingHelper;

  std::ofstream of("a4q2c.log");
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&of);

  routingHelper.PrintRoutingTableAt (Seconds (70.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (70.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (70.0), c, routingStream);

  routingHelper.PrintRoutingTableAt (Seconds (119.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (119.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (119.0), c, routingStream);

  routingHelper.PrintRoutingTableAt (Seconds (121.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (121.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (121.0), c, routingStream);

  routingHelper.PrintRoutingTableAt (Seconds (140.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (140.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (140.0), c, routingStream);

  routingHelper.PrintRoutingTableAt (Seconds (180.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (180.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (180.0), c, routingStream);

  routingHelper.PrintRoutingTableAt (Seconds (200.0), a, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (200.0), b, routingStream);
  routingHelper.PrintRoutingTableAt (Seconds (200.0), c, routingStream);

  NS_LOG_INFO ("Create Applications.");
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  V4PingHelper ping ("10.0.4.2");

  ping.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping.SetAttribute ("Size", UintegerValue (packetSize));
  ApplicationContainer apps = ping.Install (src);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (200.0));

  // AsciiTraceHelper ascii;
  // csma.EnableAsciiAll (ascii.CreateFileStream ("rip-simple-routing.tr"));
  // csma.EnablePcapAll ("rip-simple-routing", true);

  Simulator::Schedule (Seconds (50), &TearDownLink, a, b, 2, 1);
  Simulator::Schedule (Seconds (50), &TearDownLink, a, c, 3, 2);
  Simulator::Schedule (Seconds (120), &RecreateLink, a, b, 2, 1);
  Simulator::Schedule (Seconds (120), &RecreateLink, a, c, 3, 2);

  /* Now, do the actual simulation. */
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (201.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

int main (int argc, char **argv)
{

    run_sim();
}

