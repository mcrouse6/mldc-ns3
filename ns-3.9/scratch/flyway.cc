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

NS_LOG_COMPONENT_DEFINE ("flyway-sim-full");

Ipv4AddressHelper address;
InternetStackHelper stack;
FlywaysTopoHelper *fth;

vector < double > last;

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

struct Load
{
    int from;
    int to;
    int size;
};

map < string, int > traffic;
map < string, int >::iterator trafficIT;

vector < Load > load;
vector < Load >::iterator loadIT;

vector < Flyway > flyways;

bool CompareLoad (Load a, Load b) { return (a.size > b.size);}

void ReadLoad(const char *filename, int time)
{
  FILE* flows = fopen(filename, "r"); 
  if ( flows == NULL ) {
      printf("cannot read flow file:\n");
      exit(-1);
  }
  char line[FTH_setupflows_MAXLINELEN];
  int linelen=0;
  int from;
  int to;
  char prot[100];
  float ts;
  int size;
  char rate[100];
  char key[1000];
  load.clear();
  traffic.clear();
  while(fgets(line, FTH_setupflows_MAXLINELEN, flows) != NULL ) 
  {
      linelen = strlen(line); 
      line[--linelen]=0;
      sscanf(line, "%f %d %d %s %d %s", 
              &ts,
              &from,
              &to,
              prot,
              &size, 
              rate);
      int x = (int)floor(ts);
      if (from == to || x != time)
          continue;
      if (x > time)
          break;
      sprintf(key, "%d %d", from, to);
      string mapKey (key);
      if (traffic.find(mapKey) != traffic.end()) {
        traffic[mapKey] = traffic[mapKey] + size;
      }
      else {
        traffic[mapKey] = size;
      }
  }
  fclose(flows);

  load.clear();
  for (trafficIT = traffic.begin(); trafficIT != traffic.end(); trafficIT++) 
  {
      //printf("%s %d\n", it->first.c_str(), it->second);
      struct Load l;
      sscanf(trafficIT->first.c_str(), "%d %d", &l.from, &l.to);
      l.size = trafficIT->second;
      load.push_back(l);
  }
  sort(load.begin(), load.end(), CompareLoad);
}

void PointAllNowhere()
{
	for (int i = 0; i < fth->GetNumFlyways(); ++i)
	{
		fth->PointNowhere(i, 0);	/* Assumes only 1 dongle! */
	}
}

void AddFlyway(Flyway f)
{
    int fromNodeId = f.fromNodeId;
    int toNodeId = f.toNodeId;
    int dongleNumber = f.dongleNumber;
    double gain = f.gain; 

    cout << "======> Adding flyway from " << fromNodeId << " to " << toNodeId << endl;

    // Turn on the wireless interfaces
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, fromNodeId, dongleNumber, true);
    fth->SetFlywayInterfaceUpDown(FW_Node_ToR, toNodeId, dongleNumber, true);

    // Set channels.
    //int channel = f.channel; 
    //fth->ChangeChannel(fromNodeId, dongleNumber, channel);
    //fth->ChangeChannel(toNodeId, dongleNumber, channel);

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

    // Point the antenna straight up so it gets no reception
    fth->PointNowhere(fromNodeId, dongleNumber);
    fth->PointNowhere(toNodeId, dongleNumber);
}

void GenerateNewFlyways()
{
    int count = 0;
    // First, drop all old flyways
    for (int i = 0; i < (int)flyways.size(); i++) 
        DropFlyway(flyways[i]);

    flyways.clear();

    Time t = Simulator::Now();
    int now = (int)floor(t.GetSeconds());
    flyways.clear();
    ReadLoad(flowFile, now);
    for (int i = 0; i < (int)load.size(); i++) 
    {
        if (load[i].size <= 10000 && fth->GetDistance(load[i].from, load[i].to) < 100)
        {
            Flyway f;
            f.fromNodeId = load[i].from;
            f.toNodeId = load[i].to;
            f.dongleNumber = 0;
            f.channel = 1;
            f.gain = 100;
            printf ("%d => %d %d %f\n", load[i].from, load[i].to, load[i].size, fth->GetDistance(load[i].from, load[i].to));
            AddFlyway(f);
            flyways.push_back(f);
            count++;
            if (count == 1)
                break;
        }
    }
    Simulator::Schedule (Seconds (1), GenerateNewFlyways);
}

void Progress ()
{
    time_t realTime = time(NULL);
    double diff = difftime(realTime, epoch);
    double sinceStart = difftime(realTime, start);
    epoch = realTime;
    Time now = Simulator::Now (); 
    cout << "Time: [Sim=" << now.GetSeconds() << ", WallClock=" << sinceStart << ", WallDiff=" << diff << "]" << endl;
    int finished = 0;
    //int x = 0;
    for (int i=0; i < (int)fth->GetNumApps(); i++) {
        if (fth->IsFinished(i)) { 
            //printf ("[%d: %d %.3f] ", i, (int)fth->GetTotalRx(i), fth->TotalDelay(i)*1000);
            finished++;
            x++;
            //if (x % 8 == 0) {printf ("\n");}
            if (fth->TotalDelay(i) > maxDelay) 
                maxDelay =  fth->TotalDelay(i);
        }
    }
    //printf ("\n");
    cout << "\t Total Finished=" << finished << std::endl;
    cout << "\t Max time=" << maxDelay << std::endl;
    Simulator::Schedule (Seconds (1), Progress);
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
    Config::SetDefault ("ns3::RttEstimator::MinRTO", TimeValue(Seconds(0.01)));
    Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue(Seconds(0.01)));
}

int 
main (int argc, char **argv)
{
    //LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
    //LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
    //LogComponentEnable("Ipv4EndPointDemux", LOG_LEVEL_ALL);
    //LogComponentEnable("Ipv4EndPoint", LOG_LEVEL_ALL);
    
    topoFile = argv[1];
    flowFile = argv[2];

    //LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
    //LogComponentEnable("OnOffApplication", LOG_LEVEL_ALL);

    // Set various default values.
    SetSimulationDefaults();

    // Address base.
    address.SetBase("10.1.0.0", "255.255.0.0"); // link local ip addresses

    // Create topology.
    fth = new FlywaysTopoHelper(topoFile, stack, address);
    for (int i = 0; i < fth->GetNumApps(); i++) {
        last.push_back(0);
    }

    // Point all nodes to Nowhere
    PointAllNowhere();

    // Fill in the default routes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Measure throughput every 1 second.
    Simulator::Schedule(Seconds(1), Progress);

    // Generate flyways every second. 
    Simulator::Schedule(Seconds(0), GenerateNewFlyways);
    
    // Stop after 10 seconds.
    Simulator::Stop(Seconds(120));

    cout << "=======================> Starting simulation <======================" << endl;
    start = epoch = time(NULL);
    Simulator::Run ();

    Simulator::Destroy ();
}