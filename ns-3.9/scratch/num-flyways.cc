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
#include "ns3/measured-2d-antenna.h"
#include <time.h>

#define FTH_setupflows_MAXLINELEN 1000

using namespace std;
using namespace ns3;

int maxSet = 0;
bool cca = false;
double x; 

NS_LOG_COMPONENT_DEFINE ("flyway-sim-full");

Ipv4AddressHelper address;
InternetStackHelper stack;
FlywaysTopoHelper *fth;

char topoFile[1000];
double gain;

double baselineNoisedBm = -81;
double rssiThreshold;

struct Flyway 
{
    int from;
    int to;
};

#define NUM_TRIALS 100000

#define NUM_THRESHOLDS 1
double mcsLimits[] = {-53}; //{-63, -58, -47};
double rates_SC[] =  {0.385, 0.77, 0.9625, 1.155, 1.54, 1.925, 2.31, 2.5025, 3.08, 3.85, 4.62, 5.005, 5.775, 6.93, 7.5075, 9.24, 10.01};
double mcsLimits_SC[] = {-68, -67, -65, -64, -63, -62, -61, -59, -55, -54, -53, -52, -51, -50, -49, -48, -47};
double mcsLimits_OFDM[] = {-66, -64, -63, -62, -60, -58, -56, -54, -53, -51, -49, -47};
double rates_OFDM[] =  {.693, .86625, 1.386, 1.73250, 2.079, 2.772, 3.465, 4.158, 4.5045, 5.1975, 6.237, 6.75675};
#define NUM_GAINS 1
double gainValues[] =  {23};

vector < Flyway > flyways;
vector < double > flywayRates;
vector < double > lastFeasible;
vector < int  > nodes;
vector < int  >::iterator nodesIT;
map <int, bool> nodesWithFlyways;

double FindRateForRSSI(double rssi)
{
    double rate = 0;
    for (int i = 0; i < 12; i++) 
    {
        if (rssi > mcsLimits_OFDM[i]) 
            rate = rates_OFDM[i];
    }
    return rate;
}

void PointAllNowhere()
{
    for (int i = 0; i < fth->GetNumFlyways(); ++i)
    {
            fth->PointNowhere(i, 0);    /* Assumes only 1 dongle! */
    }
}


void SetAllGain(double gain)
{
        for (int i = 0; i < fth->GetNumFlyways(); i++) 
        {
            fth->SetAntennaGain(i, 0, gain);
        }
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

void InitNodeList()
{
    nodes.clear();
    for (uint i  = 0; i < fth->GetNumToRs(); i++) 
    {
        nodes.push_back(i);
    }
    // Now, randomize.
    for (uint i = 0; i < fth->GetNumToRs(); i++) 
    {
        uint j = rand() % nodes.size();
        swap(nodes[i], nodes[j]);
    }
}

double WorstCaseNoiseFromFlywayToNode(Flyway f, int b)
{
    double worstCaseNoise = 0;
    double noiseFromSender =  fth->CalcSignalStrength(f.from, 0, b, 0);
    worstCaseNoise += pow(10, noiseFromSender/10);
    double noiseFromReceiver = fth->CalcSignalStrength(f.to, 0, b, 0);;
    worstCaseNoise += pow(10, noiseFromReceiver/10);
    double worstCaseNoisedBm = 10 * log10(worstCaseNoise);
    return worstCaseNoisedBm;
    
}

bool IsLinkFeasible(int index, double *rateF, double *rateB)
{
    Flyway f = flyways[index];
    *rateF = *rateB = 0;
    double rssi = fth->CalcSignalStrength(f.from, 0, f.to, 0);

    // Check noise generated by other flyways.
    // We will be super-conservative. We will assume
    // that both *sender* and *receiver* of the other
    // links are active *at the same time*. 
   
    double noiseAtSender = pow (10, baselineNoisedBm/10);
    double noiseAtReceiver = pow (10, baselineNoisedBm/10);
    double noise;
    for (int i = 0; i < (int)flyways.size(); i++) 
    {
        if (i != index) 
        {
            noise = WorstCaseNoiseFromFlywayToNode(flyways[i], f.from);
            noiseAtSender += pow(10, noise/10.0);
            //printf ("noise from %d=>%d at %d is %f dbM. Total %g\n", flyways[i].from, flyways[i].to, f.from, noise, noiseAtSender);
            noise = WorstCaseNoiseFromFlywayToNode(flyways[i], f.to);
            noiseAtReceiver += pow(10, noise/10.0);
            //printf ("noise from %d=>%d at %d is %f dbM. Total %g\n", flyways[i].from, flyways[i].to, f.to, noise, noiseAtReceiver);
        }
    }

    // Check feasible at sender.
    if (noiseAtSender != 0) 
    {
        noiseAtSender = 10 * log10(noiseAtSender);
        //printf ("ns=%g\n", noiseAtSender);
        if (cca && noiseAtSender > -77) 
            return false;

        double rssi_adjust = rssi - (-1 * baselineNoisedBm) + (-1 * noiseAtSender);
        *rateB = FindRateForRSSI(rssi_adjust - 3); 

        if (!(rssi_adjust - 3 > rssiThreshold)) {
            return false;
        }
    }
    else
    {
        *rateB = FindRateForRSSI(1000); 
    }

    // Check feasible at receiver.
    if (noiseAtReceiver != 0) 
    {
        noiseAtReceiver = 10 * log10(noiseAtReceiver);
        //printf ("nr=%g\n", noiseAtReceiver);
        if (cca && noiseAtReceiver > -77) 
            return false;
        
        // Check if we have enough margin  to do what we need to do.
        double rssi_adjust = rssi - (-1 * baselineNoisedBm) + (-1 * noiseAtReceiver);
        /*
        printf ("%d=>%d rssiThresh=%f noiseAtReceiver=%f base=%f needed=%f rssi=%f\n",  
                f.from, 
                f.to, 
                rssiThreshold, 
                noiseAtReceiver, 
                baselineNoisedBm, 
                needed, 
                rssi);
                */
        if (rssi_adjust - 3 > rssiThreshold) {
            *rateF = FindRateForRSSI(rssi_adjust - 3); 
            return true;
        } else {
            return false;
        }
    }
    else  {
        *rateF = FindRateForRSSI(1000); 
        return true;
    }
}

bool IsFlywaySetFeasible (double *rateF, double *rateB)
{
    flywayRates.clear();
    for (int i = 0; i < (int)flyways.size(); i++) 
    {
        if (!IsLinkFeasible(i, rateF, rateB))
            return false;
        else
        {
            flywayRates.push_back(*rateF);
            flywayRates.push_back(*rateB);
        }
    }
    return true;
}

void RunTrial(int trial)
{
    // Point all nodes to Nowhere
    PointAllNowhere();
    // Set gain
    SetAllGain(gain);
    flyways.clear();
    nodesWithFlyways.clear();

    srand(trial + time(NULL));
    InitNodeList();

    int numValid = 0;
    int sameEndPoint = 0;
    int nodeOverlap = 0;
    int tooFar = 0; 
    int hurtsOthers = 0;
    int total  =0;

    for (int s = 0; s < (int)nodes.size(); s++) 
    {
        for (int d = 0; d < (int)nodes.size(); d++) 
        {
            if (s == d) 
            {
                sameEndPoint++;
                continue;
            }
                        
            if (nodesWithFlyways.find(nodes[s]) != nodesWithFlyways.end()) {
                nodeOverlap++;
                continue;
            }
            if (nodesWithFlyways.find(nodes[d]) != nodesWithFlyways.end()) {
                nodeOverlap++;
                continue; 
            }
        
            Flyway f;
            f.from = nodes[s];
            f.to = nodes[d];
        
            fth->PointAtEachOther(f.from, 0, f.to, 0); 
            double rssi = fth->CalcSignalStrength(f.from, 0, f.to, 0);
        
            if (rssi < rssiThreshold + 3)  {
                tooFar++;
                continue;
            }
       
            //printf ("Now considering %d=>%d\n", f.from, f.to);
            flyways.push_back(f);
        
            double rateF, rateB;
            if (IsFlywaySetFeasible(&rateF, &rateB)) 
            {
                numValid++;
                printf ("trial=%d flyway=%d (%d=>%d) rssi=%f, rateF=%f, rateB=%f\n", trial, numValid, f.from, f.to, fth->CalcSignalStrength(f.from, 0, f.to, 0), rateF, rateB); 
                nodesWithFlyways[nodes[s]] = true;
                nodesWithFlyways[nodes[d]] = true;
                lastFeasible = flywayRates;
            }
            else 
            {
                hurtsOthers++;
                flyways.pop_back();
            }
        }
    }
    total = numValid + sameEndPoint + nodeOverlap + tooFar + hurtsOthers;
    double capacity = 0;
    for (int i = 0; i < (int)lastFeasible.size(); i++) 
    {
        printf("%.3f,", lastFeasible[i]);
        capacity += lastFeasible[i];
    }
    printf("\n");
    printf ("trial=%d gain=%d thresh=%d total=%d valid=%d sameEndpoint=%d nodeOverlap=%d tooFar=%d hurtsOthers=%d tot=%f",
                trial,
                (int)gain,
                (int)rssiThreshold,
                total,
                numValid,
                sameEndPoint,
                nodeOverlap,
                tooFar,
                hurtsOthers,
                capacity);
    if (capacity > maxSet) 
    {
        printf (" BestSoFar");
        maxSet = capacity;
    }
    printf("\n");
    fflush(stdout);
}

int 
main (int argc, char **argv)
{
    CommandLine cmd;
    cmd.AddValue ("topo", "topo file", topoFile);
    cmd.AddValue ("cca", "do cca", cca);
    cmd.Parse (argc, argv);

    // Set various default values.
    SetSimulationDefaults();

    // Address base.
    address.SetBase("10.1.0.0", "255.255.0.0"); // link local ip addresses

    // Create topology.
    fth = new FlywaysTopoHelper(topoFile, stack, address);

    for (int t = 0; t < NUM_THRESHOLDS; t++) 
    {
        rssiThreshold = mcsLimits[t];
        for (int g = 0; g < NUM_GAINS; g++) 
        {
            gain = gainValues[g];
            for (int trial = 0; trial < NUM_TRIALS; trial++) 
            {
                RunTrial(trial);
            }
        }
    }
}
