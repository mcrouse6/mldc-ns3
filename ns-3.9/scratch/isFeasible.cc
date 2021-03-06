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
double mcsLimits[] = {-68, -67, -65, -64, -63, -62, -61, -59, -55, -54, -53, -52, -51, -50, -49, -48, -47};
double rates[] =  {0.385, 0.77, 0.9625, 1.155, 1.54, 1.925, 2.31, 2.5025, 3.08, 3.85, 4.62, 5.005, 5.775, 6.93, 7.5075, 9.24, 10.01};
//double gainValues[] =  {23, 10};

double gain = 23;
double rssiThreshold = -62;

struct Flyway 
{
    int from;
    int to;
};

vector < Flyway > flyways;
vector < double > flywayRates;
vector < int  >::iterator nodesIT;
map <int, bool> nodesWithFlyways;


double FindRateForRSSI(double rssi)
{
    double rate = 0;
    for (int i = 0; i < 17; i++) 
    {
        if (rssi > mcsLimits[i]) 
            rate = rates[i];
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

bool IsLinkFeasible(int index, double *rate)
{
    Flyway f = flyways[index];
    *rate = 0; 

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

bool ReadFlyways()
{
    FILE *fp = fopen(flywayFile, "r");
    char line[2000];
    int linelen;
    if (fp == NULL) 
    {
        printf ("Can't open %s to read\n", flywayFile);
        exit(-1);
    }
    while (fgets(line, 2000, fp) != NULL)
    {
        linelen = strlen(line);
        line[--linelen] = 0;
        Flyway f;
        if (sscanf(line, "%d %d", &f.from, &f.to) == 2)
        {
            if (f.from == f.to) 
            {
                return false;
            }
                        
            if (nodesWithFlyways.find(f.from) != nodesWithFlyways.end()) {
                return false;
            }
        
            if (nodesWithFlyways.find(f.to) != nodesWithFlyways.end()) {
                return false;
            }

            fth->PointAtEachOther(f.from, 0, f.to, 0); 
            double rssi = fth->CalcSignalStrength(f.from, 0, f.to, 0);
        
            if (rssi < rssiThreshold + 3)  {
                return false;
            }
           
            flyways.push_back(f);
        
            if (!IsFlywaySetFeasible()) {
                return false;
            }
            
            nodesWithFlyways[f.from] = true;
            nodesWithFlyways[f.to] = true;
        }
        else 
        {
            break;
        }
    }
    fclose(fp);
    if (flyways.size() == 0)
        return false;
    else
        return true;
}

bool AlreadyAdded(Flyway f)
{
    for (int i = 0; i < (int)flyways.size(); i++) 
    {
        if (flyways[i].from == f.from && flyways[i].to == f.to) 
        {
            return true;
        }
    }
    return false;
}

void CheckFeasibility()
{
    FILE *fp = fopen ("out.dat", "w");
    if (fp == NULL)
    {
        printf("Can't open out.dat for writing\n");
        exit(-1);
    }
            
    flywayRates.clear();
    if (ReadFlyways() == false) 
    {
        fprintf(fp, "false\n");
        fprintf(fp, "FINISH\n");
        fflush(fp);
        fclose(fp);
        return;
    }


    fprintf(fp, "true Rates ");
    for (int i = 0; i < (int) flywayRates.size(); i++) 
    {
        fprintf(fp, " %.3f ", flywayRates[i]);
    }
    fprintf(fp, "\n");


    for (uint s = 0; s < fth->GetNumToRs(); s++) 
    {
        for (uint d = 0; d < fth->GetNumToRs(); d++) 
        {
            Flyway f;
            f.from = s;
            f.to = d;

            if (AlreadyAdded(f))
            {
                fprintf(fp, "No AlreadyAdded %d %d\n", s, d);
                continue;
            }

            if (s == d) 
            {
                fprintf (fp, "No SameEndPoint %d %d\n", s, d);
                continue;
            }
       
            if (nodesWithFlyways.find(s) != nodesWithFlyways.end()) 
            {
                fprintf (fp, "No NodeInUse %d %d\n", s, d);
                continue;
            }
       
            if (nodesWithFlyways.find(d) != nodesWithFlyways.end()) 
            {
                fprintf (fp, "No NodeInUse %d %d\n", s, d);
                continue; 
            }
        
            fth->PointAtEachOther(f.from, 0, f.to, 0); 
            double rssi = fth->CalcSignalStrength(f.from, 0, f.to, 0);
        
            if (rssi < rssiThreshold + 3)  {
                fprintf (fp, "No TooFar %d %d\n", s, d);
                continue;
            }
       
            flyways.push_back(f);
        
            if (!IsFlywaySetFeasible()) 
            {
                fprintf (fp, "No SetInfeasible %d %d\n", s, d);
            }
            else 
            {
                fprintf(fp, "Yes CanAdd %d %d Rates ", s, d);
                for (int i = 0; i < (int) flywayRates.size(); i++) 
                {
                    fprintf(fp, " %.3f ", flywayRates[i]);
                }
                fprintf(fp, "\n");
            }

            flyways.pop_back();

        }
    }
    fprintf(fp, "FINISH\n");
    fflush(fp);
    fclose(fp);
}

int 
main (int argc, char **argv)
{
    CommandLine cmd;
    cmd.AddValue ("topo", "topo file", topoFile);
    cmd.AddValue ("flyways", "flyway file", flywayFile);
    cmd.AddValue ("gain", "antenna gain", gain);
    cmd.AddValue ("min_rssi", "min rssi", rssiThreshold);
    cmd.Parse (argc, argv);

    // Set various default values.
    SetSimulationDefaults();

    // Address base.
    address.SetBase("10.1.0.0", "255.255.0.0"); // link local ip addresses

    // Create topology.
    fth = new FlywaysTopoHelper(topoFile, stack, address);

    // Set up initial state. 
    PointAllNowhere();
    SetAllGain(gain);
    flyways.clear();
    nodesWithFlyways.clear();

    // CheckFeasibility.
    CheckFeasibility();

    exit(0);
}
