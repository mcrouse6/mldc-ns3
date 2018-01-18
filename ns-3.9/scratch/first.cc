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

#include <stdio.h>
#include <stdlib.h>

#include "ns3/core-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

#define FTH_setupflows_MAXLINELEN 1000

struct Flyway 
{
    int from;
    int to;
    int dongleNumber;
    int channelNumber;
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

bool CompareLoad (Load a, Load b) { return (a.size < b.size);}

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
  int ts;
  int size;
  char rate[100];
  char key[1000];
  while(fgets(line, FTH_setupflows_MAXLINELEN, flows) != NULL ) 
  {
      linelen = strlen(line); 
      line[--linelen]=0;
      sscanf(line, "%d %d %d %s %d %s", 
              &ts,
              &from,
              &to,
              prot,
              &size, 
              rate);
      if (from == to || ts != time)
          continue;
      if (ts > time)
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

void GenerateFlywaysForShortFlows(const char *filename, int now)
{
    //Time t = Simulator::Now();
    //int now = (int)floor(t.GetSeconds());
    flyways.clear();
    ReadLoad(filename, now);
    for (int i = 0; i < (int)load.size(); i++) 
    {
        printf ("%d => %d %.1f\n", load[i].from, load[i].to, (double)load[i].size/1000000.0);
        if (load[i].size > 1000000000) 
        {
            Flyway f;
            f.from = load[i].from;
            f.to = load[i].to;
            f.dongleNumber = 0;
            f.channelNumber = 0;
            flyways.push_back(f);
        }
    }
}

int 
main (int argc, char *argv[])
{
  GenerateFlywaysForShortFlows("flow.dat", 0);
  return 0;
}
