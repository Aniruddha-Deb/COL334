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

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/point-to-point-dumbbell.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SeventhScriptExample");

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |    ns-3 TCP    |    |    ns-3 TCP    |
//   +----------------+    +----------------+
//   |    10.1.1.1    |    |    10.1.1.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                5 Mbps, 2 ms
//
//
// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off
// application is not created until Application Start time, so we wouldn't be
// able to hook the socket (now) at configuration time.  Second, even if we
// could arrange a call after start time, the socket is not public so we
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass
// this socket into the constructor of our simple application which we then
// install in the source node.
// ===========================================================================
//
class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
  else
    {
      m_socket->Bind6 ();
    }
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  file->Write (Simulator::Now (), p);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpVegas"));

  NS_LOG_INFO("Creating Nodes");
  NodeContainer nodes;
  nodes.Create(5);

  NS_LOG_INFO("Creating Node Containers");
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));
  NodeContainer n2n4 = NodeContainer(nodes.Get(2), nodes.Get(4));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

  NS_LOG_INFO("Installing Internet Stack");
  InternetStackHelper net;
  net.Install(nodes);

  NS_LOG_INFO("Creating connections");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d0d1 = p2p.Install(n0n1);
  NetDeviceContainer d2d3 = p2p.Install(n2n3);
  NetDeviceContainer d2d4 = p2p.Install(n2n4);
  NetDeviceContainer d1d2 = p2p.Install(n1n2);

  NS_LOG_INFO("Adding error models");
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));

  d0d1.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d0d1.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d1d2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d1d2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d2d3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d2d3.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d2d4.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  d2d4.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  NS_LOG_INFO("Adding IP addresses");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign (d2d3);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i4 = ipv4.Assign (d2d4);
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

  NS_LOG_INFO("Populating Global Routing Tables");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NS_LOG_INFO("Creating Applications");

  uint16_t TCPSinkPort = 8080;
  uint16_t UDPSinkPort = 8081;
  Address TCPSinkAddress = i2i3.GetAddress(0);
  Address TCPAnyAddress = InetSocketAddress (Ipv4Address::GetAny (), TCPSinkPort);
  Address UDPSinkAddress = i2i4.GetAddress(0);
  Address UDPAnyAddress = InetSocketAddress (Ipv4Address::GetAny (), UDPSinkPort);


  PacketSinkHelper TCPSinkHelper ("ns3::TcpSocketFactory", TCPAnyAddress);
  ApplicationContainer TCPSinkApp = TCPSinkHelper.Install (nodes.Get(3));
  TCPSinkApp.Start (Seconds (0.));
  TCPSinkApp.Stop (Seconds (100.));

  Ptr<Socket> TCPSocket = Socket::CreateSocket (nodes.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> TCPApp = CreateObject<MyApp> ();
  TCPApp->Setup (TCPSocket, TCPSinkAddress, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication(TCPApp);
  TCPApp->SetStartTime (Seconds (1.));
  TCPApp->SetStopTime (Seconds (100.));

  PacketSinkHelper UDPSinkHelper ("ns3::UdpSocketFactory", UDPAnyAddress);
  ApplicationContainer UDPSinkApp = UDPSinkHelper.Install (nodes.Get(4));
  UDPSinkApp.Start (Seconds (0.));
  UDPSinkApp.Stop (Seconds (100.));

  Ptr<Socket> UDPSocket = Socket::CreateSocket (nodes.Get(1), UdpSocketFactory::GetTypeId());

  Ptr<MyApp> UDPAppLowRate = CreateObject<MyApp> ();
  UDPAppLowRate->Setup (UDPSocket, UDPSinkAddress, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(1)->AddApplication(UDPAppLowRate);
  UDPAppLowRate->SetStartTime (Seconds (20.));
  UDPAppLowRate->SetStopTime (Seconds (30.));

  Ptr<MyApp> UDPAppHighRate = CreateObject<MyApp> ();
  UDPAppHighRate->Setup (UDPSocket, UDPSinkAddress, 1040, 100000, DataRate ("500Kbps"));
  nodes.Get(1)->AddApplication(UDPAppHighRate);
  UDPAppHighRate->SetStartTime (Seconds (30.));
  UDPAppHighRate->SetStopTime (Seconds (100.));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("a3q3.cwnd");
  TCPSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));

  PcapHelper pcapHelper;
  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("a3q3.pcap", std::ios::out, PcapHelper::DLT_PPP);
  nodes.Get(4)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));

  Simulator::Stop (Seconds (100));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

