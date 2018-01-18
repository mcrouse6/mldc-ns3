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

double x; 
int maxSet = 0;

NS_LOG_COMPONENT_DEFINE ("flyway-sim-full");

Ipv4AddressHelper address;
InternetStackHelper stack;
FlywaysTopoHelper *fth;

char topoFile[1000];

#define NUM_THRESHOLDS 1
double mcsThresholds[] = {-58};
double mcsAllThresholds[] = {-68, -67, -65, -64, -63, -62, -61, -59, -55, -54, -53, -52, -51, -50, -49, -48, -47};
double rates[] =  {0.385, 0.77, 0.9625, 1.155, 1.54, 1.925, 2.31, 2.5025, 3.08, 3.85, 4.62, 5.005, 5.775, 6.93, 7.5075, 9.24, 10.01};

#define NUM_GAINS 1
double gainValues[] =  {800,10,23};

#define NUM_TRIALS 1000

double baselineNoisedBm = -81;
double gain = 23;
double rssiThreshold = -68;
uint numChannels = 3;
bool cca = false;

struct Flyway 
{
    int from;
    int dongleFrom;
    int to;
    int dongleTo;
    int channel;
};

vector < Flyway > flyways;
vector < double > flywayRates;
vector < double > lastFeasible;
vector < int > nodes;
map <int, bool> nodesWithFlyways[3];
vector < vector<Flyway> > candidates;
uint dongleCounts[1000];


double FindRateForRSSI(double rssi)
{
    double rate = 0;
    for (int i = 0; i < 17; i++) 
    {
        if (rssi > mcsAllThresholds[i]) 
            rate = rates[i];
    }
    return rate;
}

void PointAllNowhere()
{
    for (int i = 0; i < fth->GetNumFlyways(); ++i)
        for (uint j = 0; j < fth->GetNumDongles(); ++j)
            fth->PointNowhere(i, j);
}

void SetAllGain(double gain)
{
    for (int i = 0; i < fth->GetNumFlyways(); ++i)
        for (uint j = 0; j < fth->GetNumDongles(); ++j)
            fth->SetAntennaGain(i, j, gain);
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

double WorstCaseNoiseFromFlywayToNode(Flyway f, int b, int dongleId)
{
    double worstCaseNoise = 0;
    double noiseFromSender =  fth->CalcSignalStrength(f.from, f.dongleFrom,
            b, dongleId);
    worstCaseNoise += pow(10, noiseFromSender/10);
    double noiseFromReceiver = fth->CalcSignalStrength(f.to, f.dongleTo, 
            b, dongleId);;
    worstCaseNoise += pow(10, noiseFromReceiver/10);
    double worstCaseNoisedBm = 10 * log10(worstCaseNoise);
    return worstCaseNoisedBm;
    
}

bool IsLinkFeasible(int index, double *rate)
{
    Flyway f = flyways[index];
    *rate = 0; 

    double rssi = fth->CalcSignalStrength(f.from, f.dongleFrom,
            f.to, f.dongleTo);

    // Check noise generated by other flyways.
    // We will be super-conservative. We will assume
    // that both *sender* and *receiver* of the other
    // links are active *at the same time*. 
   
    double noiseAtSender = pow (10, baselineNoisedBm/10);
    double noiseAtReceiver = pow (10, baselineNoisedBm/10);
    double noise;
    for (int i = 0; i < (int)flyways.size(); i++) 
    {
        if (i == index) 
            continue;

        if (f.channel != flyways[i].channel)
            continue;
        noise = WorstCaseNoiseFromFlywayToNode(flyways[i], f.from, f.dongleFrom);
        noiseAtSender += pow(10, noise/10.0);
        //printf ("noise from %d=>%d at %d is %f dbM. Total %g\n", flyways[i].from, flyways[i].to, f.from, noise, noiseAtSender);
        noise = WorstCaseNoiseFromFlywayToNode(flyways[i], f.to, f.dongleTo);
        noiseAtReceiver += pow(10, noise/10.0);
        //printf ("noise from %d=>%d at %d is %f dbM. Total %g\n", flyways[i].from, flyways[i].to, f.to, noise, noiseAtReceiver);
    }

    if (noiseAtSender == 0 && noiseAtReceiver == 0)
        return true;

    // Check sender
    if (noiseAtSender != 0) 
    {
        noiseAtSender = 10 * log10(noiseAtSender);
        //printf ("ns=%g\n", noiseAtSender);
        if (cca && noiseAtSender > -77) 
            return false;

        // Can we receive acks?
        double rssi_adjust = rssi - (-1 * baselineNoisedBm) + (-1 * noiseAtSender);
        if (!(rssi_adjust - 3 > rssiThreshold)) {
            return false;
        }
    }

    // Check  receiver.
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
            *rate = FindRateForRSSI(rssi_adjust-3);
            return true;
        }
        else {
            return false;
        }
    }
    printf ("should never come here\n");
    exit(-1);
}

bool IsFlywaySetFeasible ()
{
    flywayRates.clear();
    for (int i = 0; i < (int)flyways.size(); i++) 
    {
        double rate = 0;
        if (!IsLinkFeasible(i, &rate))
            return false;
        else 
            flywayRates.push_back(rate);
    }
    return true;
}

void fPrintFlywayChan (FILE* fp, vector <Flyway> c)
{
    for (unsigned int i=0; i < c.size(); ++i)
    {
        if (i > 0)
            fprintf(fp, ":");
        fprintf(fp, "%d-%d.%d-%d.[%d]", c[i].from, c[i].dongleFrom, c[i].to, c[i].dongleTo, c[i].channel);
    }
}

void PrintFlywayChan (vector <Flyway> c)
{
    fPrintFlywayChan (stdout, c);
}

void fPrintFlyway (FILE* fp, vector <Flyway> c)
{
    for (unsigned int i=0; i < c.size(); ++i)
    {
        if (i > 0)
            fprintf(fp, ":");
        fprintf(fp, "%d.%d", c[i].from, c[i].to);
    }
}

void PrintFlyway (vector <Flyway> c)
{
    fPrintFlyway (stdout, c);
}

bool FindFlywayChannel(Flyway &f)
{
    static int cur_chan = 0;

    for (uint c=0; c < numChannels; ++c)
    { 
        unsigned int d = (c + cur_chan) % numChannels;

        /* Are both nodes free on this channel? */
        if (nodesWithFlyways[d].find(f.from) != nodesWithFlyways[d].end())
            continue;
        if (nodesWithFlyways[d].find(f.to) != nodesWithFlyways[d].end())
            continue;

        f.channel = d;

        /* Is it feasible on this channel? */
        flyways.push_back(f);
        if (!IsFlywaySetFeasible())
        {
            flyways.pop_back();
            f.channel = -1;
            continue;
        }

        nodesWithFlyways[d][f.to] = true;
        nodesWithFlyways[d][f.from] = true;
        break;
    }

    if (f.channel == -1)
        return false;
    cur_chan = (cur_chan+1) % numChannels;
    return true;
}

void RandomizeNodeOrder()
{
    nodes.clear();
    for (uint i=0; i < fth->GetNumToRs(); ++i)
        nodes.push_back(i);
    for (uint i=0; i < fth->GetNumToRs(); ++i)
    {
        uint j = rand() % nodes.size();
        swap(nodes[i], nodes[j]);
    }
}

void RunTrial(int trial)
{
    /* Reset all state */
    flyways.clear();
    flywayRates.clear();
    PointAllNowhere();
    SetAllGain(gain);
    for (uint c = 0; c < numChannels; ++c)
        nodesWithFlyways[c].clear();
    memset(dongleCounts, 0, sizeof(dongleCounts));

    /* Randomize things for this trial */
    srand(trial + time(NULL));
    RandomizeNodeOrder();

    int numValid = 0;
    int sameEndPoint = 0;
    int noFreeChannel = 0;
    int noFreeDongle = 0;
    int tooFar = 0;
    int hurtsOthers = 0;
    int total = 0;

    for (uint s = 0; s < nodes.size(); ++s)
    {
        for (uint d = 0; d < nodes.size(); ++d)
        {
            /* Skip flyway if same endpoint */
            if (s == d)
            {
                ++sameEndPoint;
                continue;
            }


            /* Set up this flyway struct */
            Flyway f;
            f.from = nodes[s];
            f.to = nodes[d];
            f.channel = -1;


            /* Are there too many dongles already? */
            if (dongleCounts[nodes[s]] >= fth->GetNumDongles() ||
                dongleCounts[nodes[d]] >= fth->GetNumDongles())
            {
                ++noFreeDongle;
                continue;
            }
            /* Set them otherwise */
            f.dongleFrom = dongleCounts[f.from];
            dongleCounts[f.from]++;
            f.dongleTo = dongleCounts[f.to];
            dongleCounts[f.to]++;


            /* Are they free on the same channel? */
            bool flag = false;
            for (uint c = 0; c < numChannels; ++c)
            {
                if (nodesWithFlyways[c].find(nodes[s]) !=
                        nodesWithFlyways[c].end())
                    continue;
                if (nodesWithFlyways[c].find(nodes[d]) !=
                        nodesWithFlyways[c].end())
                    continue;
                flag = true;
                break;
            }
            if (!flag)
            {
                ++noFreeChannel;
                dongleCounts[f.from]--;
                dongleCounts[f.to]--;
                continue;
            }

            /* Is it strong enough on this channel? */
            fth->PointAtEachOther(f.from, f.dongleFrom, f.to, f.dongleTo);
            double rssi = fth->CalcSignalStrength(f.from, f.dongleFrom, f.to, f.dongleTo);
            if (rssi < rssiThreshold + 3)
            {
                f.channel = -1;
                ++tooFar;
                dongleCounts[f.from]--;
                dongleCounts[f.to]--;
                continue;
            }

            if (FindFlywayChannel(f))
            {
                ++numValid;
                printf("trial=%d flyway=%d (%d=>%d)\n", trial, numValid,
                        f.from, f.to);
                lastFeasible = flywayRates;
            }
            else
            {
                hurtsOthers++;
                dongleCounts[f.from]--;
                dongleCounts[f.to]--;
            }
        }
    }

    total = numValid + sameEndPoint + noFreeDongle + noFreeChannel + hurtsOthers;
    double capacity = 0;
    for (uint i = 0; i < lastFeasible.size(); ++i)
    {
        printf("%.3f,", lastFeasible[i]);
        capacity += lastFeasible[i];
    }
    printf("\n");
    PrintFlywayChan(flyways);
    printf("\n");
    printf("trial=%d gain=%d thresh=%d total=%d valid=%d sameEndPoint=%d noFreeDongle=%d noFreeChannel=%d tooFar=%d hurtsOthers=%d tot=%.1f",
            trial, (int)gain, (int)rssiThreshold, total, numValid, sameEndPoint,
            noFreeDongle, noFreeChannel, tooFar, hurtsOthers, capacity);
    if (capacity > maxSet)
    {
        printf(" BestSoFar");
        maxSet = capacity;
    }
    printf("\n");
    fflush(stdout);
}

void CalcNumFlyways()
{
    for (int t = 0; t < NUM_THRESHOLDS; ++t)
    {
        rssiThreshold = mcsThresholds[t];
        for (int g = 0; g < NUM_GAINS; ++g)
        {
            gain = gainValues[g];
            for (int trial = 0; trial < NUM_TRIALS; ++trial)
            {
                RunTrial(trial);
            }
        }
    }
}

int 
main (int argc, char **argv)
{
    /* Initialize Randomness */
    srand(time(NULL));
    //srand(0);
    
    CommandLine cmd;
    cmd.AddValue ("topo", "topo file", topoFile);
    cmd.AddValue ("numChannels", "num channels", numChannels);
    cmd.AddValue ("cca", "do cca", cca);
    cmd.Parse (argc, argv);

    // Set various default values.
    SetSimulationDefaults();

    // Address base.
    address.SetBase("10.1.0.0", "255.255.0.0"); // link local ip addresses

    // Create topology.
    fth = new FlywaysTopoHelper(topoFile, stack, address);

    /* Verify parameters */
    if (numChannels < 1 || numChannels > 3)
    {
        printf("Error: numChannels must be [1,3]\n");
        exit(0);
    }
    if (numChannels < fth->GetNumDongles())
    {
        printf("Warning: numChannels %d < numDongles %d\n", numChannels, fth->GetNumDongles());
    }

    /* Calculate number of flyways! */
    CalcNumFlyways();

    return 0;
}
