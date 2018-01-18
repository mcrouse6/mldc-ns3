#define NAME "flyway-sim"

#include <string>

#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/flyways-topo-helper.h"
#include "ns3/ipv4.h"
#include "ns3/packet-sink.h"
#include <time.h>

#define FTH_setupflows_MAXLINELEN 1000

using namespace std;
using namespace ns3;

double x; 

vector< Ptr<PacketSink> > sinks;
vector<double> last;

NS_LOG_COMPONENT_DEFINE ("flyway-sim-full");

Ipv4AddressHelper address;
InternetStackHelper stack;
FlywaysTopoHelper *fth;

time_t epoch;
time_t start;
double maxDelay = 0;
char *topoFile;
char *flowFile;

struct Flyway 
{
    int fromNodeId;
    int toNodeId;
    int dongleNumber;
    int channel;
    double gain;
};

void SetTCPFlow (int from, int to, double startTime)
{
    Ptr<Node> sourceNode = fth->GetNode(from);
    Ptr<Node> sinkNode = fth->GetNode(to);
    Ipv4Address sinkAddr = fth->GetIpv4Address(to);

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

void Progress ()
{
    time_t realTime = time(NULL);
    double diff = difftime(realTime, epoch);
    double sinceStart = difftime(realTime, start);
    epoch = realTime;
    Time now = Simulator::Now (); 

    cout << "Time: [Sim=" << now.GetSeconds() << ", WallClock=" << sinceStart << ", WallDiff=" << diff << "]" << endl;

    cout << "\t";
    for (int i=0; i < (int)sinks.size(); i++) {
        double cur = 8.0*(sinks[i]->GetTotalRx()/1e6);
        cout << i << ":" << cur << " [+" << cur-last[i] << "]" << endl;
        last[i] = cur;
    }
    cout << endl;
    Simulator::Schedule (Seconds (1), Progress);
}

void AddFlyway(Flyway f)
{
    int fromNodeId = f.fromNodeId;
    int toNodeId = f.toNodeId;
    int dongleNumber = f.dongleNumber;
    double gain = f.gain; 
    int channel = f.channel; 

    cout << "======> Adding flyway from " << fromNodeId << " to " << toNodeId << endl;

    // Turn on the wireless interfaces
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, fromNodeId, dongleNumber, true);
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, toNodeId, dongleNumber, true);

    // Set channels.
    fth->ChangeChannel(fromNodeId, dongleNumber, channel);
    fth->ChangeChannel(toNodeId, dongleNumber, channel);

    // Sset anetnna gain
    fth->SetAntennaGain(fromNodeId, dongleNumber, gain);
    fth->SetAntennaGain(toNodeId, dongleNumber, gain);

    // Point antenna.
    fth->PointAtEachOther(fromNodeId, dongleNumber, toNodeId, dongleNumber);

    // Set up static routes.
    
    // Get Nodes
    Ptr<Node> fromNode = fth->GetNode(fromNodeId);
    Ptr<Node> toNode = fth->GetNode(toNodeId);

    // Get ipv4 objects.
    Ptr<Ipv4> fromIpv4 = fromNode->GetObject<Ipv4>();
    Ptr<Ipv4> toIpv4 = toNode->GetObject<Ipv4>();

    // Get the wired and wireless interfaces on both nodes.
    uint32_t fromInterfaces[2];
    fromInterfaces[0] = fth->GetWiredUplinks(fromNodeId).Get(0)->GetIfIndex();
    fromInterfaces[1] = fth->GetFlywayLinks(fromNodeId).Get(dongleNumber)->GetIfIndex();

    uint32_t toInterfaces[2];
    toInterfaces[0] = fth->GetWiredUplinks(toNodeId).Get(0)->GetIfIndex();
    toInterfaces[1] = fth->GetFlywayLinks(toNodeId).Get(dongleNumber)->GetIfIndex();

    // The destination is the wired address of the toNode.
    Ipv4Address dest = toIpv4->GetAddress(toInterfaces[0], 0).GetLocal();

    // Now, set up next hops.
    Ipv4Address gateways[2]; 
    gateways[0] = Ipv4Address::GetZero (); // For p2p links, we do not need to specify a netx hop: there is only one.
    gateways[1] = toIpv4->GetAddress(toInterfaces[1], 0).GetLocal(); // For flyway link, we need the address of the flyway dongle.

    // how do we want to split the traffic?
    double fraction[] = {0, 1};

    // Set up routes.
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> fromStaticRouting = ipv4RoutingHelper.GetStaticRouting(fromIpv4);
    fromStaticRouting->AddFractionalHostRouteTo(dest, gateways, fromInterfaces, fraction, 2);
}


void DropFlyway(Flyway f)
{
    int fromNodeId = f.fromNodeId;
    int toNodeId = f.toNodeId;
    int dongleNumber = f.dongleNumber;

    cout << "======> Removing flyway between " << fromNodeId << " to " << toNodeId << endl;

    // Get Nodes
    Ptr<Node> fromNode = fth->GetNode(fromNodeId);
    Ptr<Node> toNode = fth->GetNode(toNodeId);

    // Get ipv4 objects.
    Ptr<Ipv4> fromIpv4 = fromNode->GetObject<Ipv4>();
    Ptr<Ipv4> toIpv4 = toNode->GetObject<Ipv4>();

    // The destination is the wired address of the toNode.
    uint32_t toInterface;
    toInterface = fth->GetWiredUplinks(toNodeId).Get(0)->GetIfIndex();
    Ipv4Address dest = toIpv4->GetAddress(toInterface, 0).GetLocal();

    // drop flyway
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> fromStaticRouting = ipv4RoutingHelper.GetStaticRouting(fromIpv4);
    fromStaticRouting->RemoveFractionalHostRouteTo(dest);

    // Turn off the wireless interfaces
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, fromNodeId, dongleNumber, false);
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, toNodeId, dongleNumber, false);
}

void SetSimulationDefaults()
{
    /* We want large MTUs */
    Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (65535-8));
    Config::SetDefault ("ns3::WifiNetDevice::Mtu", UintegerValue (65535-8));

    /* Various wireless settings */
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("120000"));
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("VHTMCS3"));
    Config::SetDefault ("ns3::YansWifiPhy::TxPowerStart", DoubleValue(10.0));
    Config::SetDefault ("ns3::YansWifiPhy::TxPowerEnd", DoubleValue(10.0));
    Config::SetDefault ("ns3::YansWifiPhy::TxPowerLevels", UintegerValue(1));
    Config::SetDefault ("ns3::YansWifiPhy::RxNoiseFigure", DoubleValue(0.0));
    Config::SetDefault ("ns3::YansWifiPhy::EnergyDetectionThreshold", DoubleValue (-77));
    Config::SetDefault ("ns3::YansWifiPhy::CcaMode1Threshold", DoubleValue (-77-3));

    /* On-off app is set to write infinite data, in 1MB chunks. Note that the rate of this app is the rate to 
     * socket writes. This should always be greater than the max rate TCP can
     * achieve, so that flow is not starved for data. */
    Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue(0));
    Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue(2e6));
    Config::SetDefault ("ns3::OnOffApplication::OnTime", RandomVariableValue(ConstantVariable(1e6)));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", DataRateValue(DataRate("10Gbps"))); 
    Config::SetDefault ("ns3::OnOffApplication::OffTime", RandomVariableValue(ConstantVariable(0)));

    /* TCP defaults to send large packets. We only send 10K large packets to
     * avoid wireless unfairness. */
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(10000));
    Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue(1e9));
    Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(1e9));
    Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(32));
    Config::SetDefault ("ns3::RttEstimator::MinRTO", TimeValue(Seconds(1)));
}

int 
main (int argc, char **argv)
{
    topoFile = argv[1];

    //LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
    //LogComponentEnable("OnOffApplication", LOG_LEVEL_ALL);
    Config::SetDefault ("ns3::V4Ping::Verbose", BooleanValue(true));

    // Set various default values.
    SetSimulationDefaults();

    // Address base.
    address.SetBase("10.1.0.0", "255.255.0.0"); // link local ip addresses

    // Create topology.
    fth = new FlywaysTopoHelper(topoFile, stack, address);

    // Fill in the default routes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up pings between node 1 and node 11. 
    SetTCPFlow(1, 11, 0.001);

    Simulator::Stop(Seconds(11));

    Flyway f;
    f.fromNodeId = 1; 
    f.toNodeId = 11; 
    f.dongleNumber = 0;
    f.channel = 0;
    f.gain = 100; 

    Simulator::Schedule(Seconds(1), Progress);
    Simulator::Schedule(Seconds(1.001), AddFlyway, f);
    Simulator::Schedule(Seconds(5.001), DropFlyway, f);

    cout << "=======================> Starting simulation <======================" << endl;
    start = epoch = time(NULL);
    Simulator::Run ();

    Simulator::Destroy ();
}
