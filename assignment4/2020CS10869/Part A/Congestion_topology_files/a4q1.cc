#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("a4q1");

// ===========================================================================
//          _____                _____ 
//   App 1 |     |     10Mbps   |     | Sink 1
//   App 2 |  1  |--------------|  3  | Sink 2
//         |_____|     3ms      |_____| Sink 3
//                                 |   
//                                 |   
//                                 | 9Mbps 
//                                 | 3ms
//                               __|__      
//                              |     |
//                              |  2  | App 3
//                              |_____|
//
// ===========================================================================
//
// Taken from fifth.cc
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

void run_sim(std::string tcp_protocol) {

    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcp_protocol));

    NodeContainer nodes;
    nodes.Create (4);

    PointToPointHelper p2p13;
    PointToPointHelper p2p23;
    p2p13.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p13.SetChannelAttribute ("Delay", StringValue ("3ms"));
    p2p23.SetDeviceAttribute ("DataRate", StringValue ("9Mbps"));
    p2p23.SetChannelAttribute ("Delay", StringValue ("3ms"));

    NodeContainer n13(nodes.Get(1), nodes.Get(3));
    NodeContainer n23(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    auto d13 = p2p13.Install(n13);
    auto d23 = p2p23.Install(n23);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
    d13.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    d23.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i13 = ipv4.Assign (d13);
    ipv4.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcp_sink_port = 8080;
    auto tcp_sink_addr = InetSocketAddress(i13.GetAddress(1), tcp_sink_port);

    PacketSinkHelper tcp_sink_helper(
        "ns3::TcpSocketFactory", 
        InetSocketAddress(Ipv4Address::GetAny(), tcp_sink_port)
    );
    auto tcp_sink_app = tcp_sink_helper.Install(nodes.Get(3));
    tcp_sink_app.Start (Seconds (0.));
    tcp_sink_app.Stop (Seconds (40.));

    auto tcp_sock_1 = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> tcp_app_1 = CreateObject<MyApp> ();
    tcp_app_1->Setup (tcp_sock_1, tcp_sink_addr, 3000, 100000, DataRate ("1.5Mbps"));
    nodes.Get(1)->AddApplication(tcp_app_1);
    tcp_app_1->SetStartTime (Seconds (1.));
    tcp_app_1->SetStopTime (Seconds (20.));

    auto tcp_sock_2 = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> tcp_app_2 = CreateObject<MyApp> ();
    tcp_app_2->Setup (tcp_sock_2, tcp_sink_addr, 3000, 100000, DataRate ("1.5Mbps"));
    nodes.Get(1)->AddApplication(tcp_app_2);
    tcp_app_2->SetStartTime (Seconds (5.));
    tcp_app_2->SetStopTime (Seconds (25.));

    auto tcp_sock_3 = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> tcp_app_3 = CreateObject<MyApp> ();
    tcp_app_3->Setup (tcp_sock_3, tcp_sink_addr, 3000, 100000, DataRate ("1.5Mbps"));
    nodes.Get(2)->AddApplication(tcp_app_3);
    tcp_app_3->SetStartTime (Seconds (15.));
    tcp_app_3->SetStopTime (Seconds (30.));

    AsciiTraceHelper ascii_trace_helper;
    Ptr<OutputStreamWrapper> stream_tcp_1 = 
        ascii_trace_helper.CreateFileStream ("a4q1_c1_"+tcp_protocol.substr(5)+".cwnd");
    tcp_sock_1->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream_tcp_1));
    Ptr<OutputStreamWrapper> stream_tcp_2 = 
        ascii_trace_helper.CreateFileStream ("a4q1_c2_"+tcp_protocol.substr(5)+".cwnd");
    tcp_sock_2->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream_tcp_2));
    Ptr<OutputStreamWrapper> stream_tcp_3 = 
        ascii_trace_helper.CreateFileStream ("a4q1_c3_"+tcp_protocol.substr(5)+".cwnd");
    tcp_sock_3->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream_tcp_3));
    
    Simulator::Stop (Seconds(30.0));
    Simulator::Run ();
    Simulator::Destroy ();
}

int main (int argc, char *argv[]) {

    run_sim("ns3::TcpNewRenoPlus");
    run_sim("ns3::TcpNewReno");
}

