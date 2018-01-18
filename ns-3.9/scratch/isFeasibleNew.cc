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

NS_LOG_COMPONENT_DEFINE ("flyway-sim-full");

Ipv4AddressHelper address;
InternetStackHelper stack;
FlywaysTopoHelper *fth;

char topoFile[1000];
char flywayFile[1000];

double baselineNoisedBm = -81;
double mcsLimits_SC[] = {-68, -67, -65, -64, -63, -62, -61, -59, -55, -54, -53, -52, -51, -50, -49, -48, -47};
double mcsLimits_OFDM[] = {-66, -64, -63, -62, -60, -58, -56, -54, -53, -51, -49, -47};
double rates_SC[] =  {0.385, 0.77, 0.9625, 1.155, 1.54, 1.925, 2.31, 2.5025, 3.08, 3.85, 4.62, 5.005, 5.775, 6.93, 7.5075, 9.24, 10.01};
double rates_OFDM[] =  {.693, .86625, 1.386, 1.73250, 2.079, 2.772, 3.465, 4.158, 4.5045, 5.1975, 6.237, 6.75675};
//double gainValues[] =  {23, 10};


uint num_rates;
double *rates;
double *mcsLimits;
double gain = 23;
double rssiThreshold = -62;
uint numChannels = 3;

struct Flyway 
{
    int from;
    int dongleFrom;
    int to;
    int dongleTo;
    int channel;
};

vector < Flyway > flyways;
vector < Flyway > init_flyways;
vector < double > flywayRates;
vector < int  >::iterator nodesIT;
map <int, bool> nodesWithFlyways[3];
vector < vector<Flyway> > candidates;
uint dongleCounts[1000];


double FindRateForRSSI(double rssi)
{
    double rate = 0;
    for (uint i = 0; i < num_rates; i++) 
    {
        if (rssi > mcsLimits[i]) 
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
        // CCA
        //if (noiseAtSender > -77) 
        //    return false;

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
        // CCA
        //if (noiseAtReceiver > -77) 
        //    return false;
        
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

bool ReadFlyways()
{
    bool ret;

    FILE *fp = fopen(flywayFile, "r");
    char line[2000];
    if (fp == NULL) 
    {
        printf ("Can't open %s to read\n", flywayFile);
        exit(-1);
    }

    /* Read all flyways */
    while (fgets(line, 2000, fp) != NULL)
    {
        /* Skip comments */
        if (line[0] == '#')
            continue;

        /* Read a flyway */
        Flyway f;
        if (sscanf(line, "%d.%d", &f.from, &f.to) != 2)
        {
            ret = false;
            goto out;
        }
        /* Is this the end of flyways? */
        if (f.from == -1 && f.to == -1)
            break;

        f.channel = -1;
        f.dongleFrom = dongleCounts[f.from];
        dongleCounts[f.from]++;
        f.dongleTo = dongleCounts[f.to];
        dongleCounts[f.to]++;

        if (dongleCounts[f.from] > fth->GetNumDongles() ||
            dongleCounts[f.to] > fth->GetNumDongles())
        {
            printf("%d %d %d\n", dongleCounts[f.from], dongleCounts[f.to], fth->GetNumDongles());
            printf("Returning false 0\n");
            ret = false;
            goto out;
        }


        /* Add this flyway to list */
        init_flyways.push_back(f);
    }
    printf("Read flyways: ");
    PrintFlyway (init_flyways);
    printf("\n");

    /* Read all candidates */
    while (fgets(line, 2000, fp) != NULL)
    {
        /* Skip comments */
        if (line[0] == '#')
            continue;

        vector<Flyway> candidate;
        char *cur = strtok(line, ":");
        while (cur)
        {
            Flyway f;
            if (sscanf(cur, "%d.%d", &f.from, &f.to) != 2)
            {
                ret = false;
                goto out;
            }
            f.channel = -1;
            f.dongleFrom = -1;
            f.dongleTo = -1;
            candidate.push_back(f);
            cur = strtok(NULL, ":");
        }
        candidates.push_back(candidate);
        printf("Read candidate: ");
        PrintFlyway (candidate);
        printf("\n");
    }
    ret = true;

out:
    fclose(fp);
    return ret;
}

bool CheckAddFlyways(vector<Flyway> flyList, bool pop_after)
{
    /* Can we assign channels to the flyways we have? */
    static int cur_chan = 0;
    int old_cur_chan = cur_chan;
    bool ret;
    unsigned int orig_len = flyways.size();
    for (unsigned int i=0; i < flyList.size(); ++i)
    {
        /* Validate basic flyway stuff */
        if (flyList[i].from == flyList[i].to)
        {
            ret = false;
            goto out;
        }

        /* do we need to set dongle ids? */
        if (pop_after)
        {
            assert((flyList[i].dongleFrom == -1) && (flyList[i].dongleTo == -1));

            /* Are we out of dongles? */
            if (dongleCounts[flyList[i].to] >= fth->GetNumDongles() ||
                dongleCounts[flyList[i].from] >= fth->GetNumDongles())
            {
                ret = false;
                goto out;
            }

            /* Set dongle IDs */
            flyList[i].dongleFrom = dongleCounts[flyList[i].from];
            dongleCounts[flyList[i].from]++;
            flyList[i].dongleTo = dongleCounts[flyList[i].to];
            dongleCounts[flyList[i].to]++;
        }

        flyList[i].channel = -1;
        for (uint c=0; c < numChannels; ++c)
        { 
            unsigned int d = (c + cur_chan) % numChannels;

            /* Are both nodes free on this channel? */
            if (nodesWithFlyways[d].find(flyList[i].from) !=
                    nodesWithFlyways[d].end())
                continue;
            if (nodesWithFlyways[d].find(flyList[i].to) !=
                    nodesWithFlyways[d].end())
                continue;

            flyList[i].channel = d;
            /* Is it strong enough on this channel? */
            fth->PointAtEachOther(flyList[i].from, flyList[i].dongleFrom, flyList[i].to, flyList[i].dongleTo);
            double rssi = fth->CalcSignalStrength(flyList[i].from, flyList[i].dongleFrom, flyList[i].to, flyList[i].dongleTo);
            if (rssi < rssiThreshold + 3)
            {
                flyList[i].channel = -1;
                continue;
            }

            /* Is it feasible on this channel? */
            flyways.push_back(flyList[i]);
            if (!IsFlywaySetFeasible())
            {
                flyways.pop_back();
                flyList[i].channel = -1;
                continue;
            }

            nodesWithFlyways[d][flyList[i].to] = true;
            nodesWithFlyways[d][flyList[i].from] = true;
            break;
        }

        if (flyList[i].channel == -1)
        {
            ret = false;
            dongleCounts[flyList[i].from]--;
            dongleCounts[flyList[i].to]--;
            goto out;
        }
        cur_chan = (cur_chan+1) % numChannels;
    }

    /* If we get here, our flyway set is good */
    ret = true;
    PrintFlywayChan(flyways);
    printf("\n");
out:
    /* Are we supposed to pop afterwards? */
    if (pop_after)
    {
        cur_chan = old_cur_chan;
        while (flyways.size() > orig_len)
        {
            Flyway f = flyways.back();
            if (f.channel != -1)
            {
                nodesWithFlyways[f.channel].erase(f.to);
                nodesWithFlyways[f.channel].erase(f.from);
                f.channel = -1;
            }
            dongleCounts[f.from]--;
            dongleCounts[f.to]--;
            f.dongleFrom = -1;
            f.dongleTo = -1;
            flyways.pop_back();
        }
    }

    return ret;
}

bool InitializeFlyways()
{
    return CheckAddFlyways(init_flyways, false);
}

bool CheckCandidate(vector<Flyway> candidate)
{
    return CheckAddFlyways(candidate, true);
}
    
void CheckFeasibility()
{
    FILE *fp = fopen ("out.dat", "w");
    if (fp == NULL)
    {
        printf("Can't open out.dat for writing\n");
        exit(-1);
    }
            
    /* First read the flyways */
    if (!ReadFlyways()) 
    {
        fprintf(fp, "false\n");
        goto out;
    }

    if (!InitializeFlyways())
    {
        fprintf(fp, "false\n");
        goto out;
    }

    /* Now we need to check candidates */
    for (unsigned i = 0; i < candidates.size(); ++i)
    {
        fprintf(fp, "BEGIN CANDIDATE ");
        fPrintFlyway(fp, candidates[i]);
        fprintf(fp, "\n");
        if (CheckCandidate(candidates[i]))
        {
            fprintf(fp, "true Rates ");
            for (unsigned int j = 0; j < flywayRates.size(); j++) 
                fprintf(fp, " %.3f ", flywayRates[j]);
            fprintf(fp, "\n");
        }
        else
        {
            fprintf(fp, "false\n");
        }
        fprintf(fp, "END CANDIDATE ");
        fPrintFlyway(fp, candidates[i]);
        fprintf(fp, "\n");
    }

out:
    fprintf(fp, "FINISH\n");
    fflush(fp);
    fclose(fp);
}

int 
main (int argc, char **argv)
{
    /* Initialize Randomness */
    srand(time(NULL));
    //srand(0);
    bool use_ofdm = 1;
    
    CommandLine cmd;
    cmd.AddValue ("topo", "topo file", topoFile);
    cmd.AddValue ("flyways", "flyway file", flywayFile);
    cmd.AddValue ("gain", "antenna gain", gain);
    cmd.AddValue ("min_rssi", "min rssi [default -62]", rssiThreshold);
    cmd.AddValue ("numChannels", "num channels [default 3]", numChannels);
    cmd.AddValue ("use_ofdm", "use ofdm [default true]", use_ofdm);
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

    if (use_ofdm)
    {
        mcsLimits = mcsLimits_OFDM;
        rates = rates_OFDM;
        num_rates = 12;
    }
    else
    {
        mcsLimits = mcsLimits_SC;
        rates = rates_SC;
        num_rates = 17;
    }

    // Set up initial state. 
    PointAllNowhere();
    SetAllGain(gain);

    // CheckFeasibility.
    CheckFeasibility();

    exit(0);
}
