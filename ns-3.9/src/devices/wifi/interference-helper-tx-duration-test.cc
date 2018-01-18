/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 CTTC
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include <ns3/object.h>
#include <ns3/log.h>
#include <ns3/test.h>
#include <iostream>
#include "interference-helper.h"
#include "wifi-phy.h"

NS_LOG_COMPONENT_DEFINE ("InterferenceHelperTxDurationTest");

namespace ns3 {

class InterferenceHelperTxDurationTest : public TestCase 
{
public:
  InterferenceHelperTxDurationTest ();
  virtual ~InterferenceHelperTxDurationTest ();
  virtual bool DoRun (void);

private:

  /** 
   * Check if the payload tx duration returned by InterferenceHelper
   * corresponds to a known value of the pay
   * 
   * @param size size of payload in octets (includes everything after the PLCP header)
   * @param payloadMode the WifiMode used 
   * @param knownPlcpLengthFieldValue the known value of the Length field in the PLCP header
   * 
   * @return true if values correspond, false otherwise
   */
  bool CheckPayloadDuration(uint32_t size, WifiMode payloadMode,  uint32_t knownDurationNanoSeconds);

  /** 
   * Check if the overall tx duration returned by InterferenceHelper
   * corresponds to a known value of the pay
   * 
   * @param size size of payload in octets (includes everything after the PLCP header)
   * @param payloadMode the WifiMode used 
   * @param preamble the WifiPreamble used
   * @param knownPlcpLengthFieldValue the known value of the Length field in the PLCP header
   * 
   * @return true if values correspond, false otherwise
   */
  bool CheckTxDuration(uint32_t size, WifiMode payloadMode,  WifiPreamble preamble, uint32_t knownDurationNanoSeconds);
  
};


InterferenceHelperTxDurationTest::InterferenceHelperTxDurationTest ()
  : TestCase ("InterferenceHelper TX Duration")
{
}


InterferenceHelperTxDurationTest::~InterferenceHelperTxDurationTest ()
{
}

bool
InterferenceHelperTxDurationTest::CheckPayloadDuration(uint32_t size, WifiMode payloadMode, uint32_t knownDurationNanoSeconds)
{
  uint32_t calculatedDurationNanoSeconds = InterferenceHelper::GetPayloadDurationNanoSeconds (size, payloadMode);  
  if (calculatedDurationNanoSeconds != knownDurationNanoSeconds)
    {
      std::cerr << " size=" << size
                << " mode=" << payloadMode 
                << " known=" << knownDurationNanoSeconds
                << " calculated=" << calculatedDurationNanoSeconds
                << std::endl;
      return false;
    }
  return true;
}

bool
InterferenceHelperTxDurationTest::CheckTxDuration(uint32_t size, WifiMode payloadMode, WifiPreamble preamble, uint32_t knownDurationNanoSeconds)
{
  uint32_t calculatedDurationNanoSeconds = InterferenceHelper::CalculateTxDuration (size, payloadMode, preamble).GetNanoSeconds ();  
  if (calculatedDurationNanoSeconds != knownDurationNanoSeconds)
    {
      std::cerr << " size=" << size
                << " mode=" << payloadMode 
                << " preamble=" << preamble
                << " known=" << knownDurationNanoSeconds
                << " calculated=" << calculatedDurationNanoSeconds
                << std::endl;
      return false;
    }
  return true;
}

bool 
InterferenceHelperTxDurationTest::DoRun (void)
{
  bool retval = true;

  // IEEE Std 802.11-2007 Table 18-2 "Example of LENGTH calculations for CCK"
  retval = retval
    && CheckPayloadDuration (1023, WifiPhy::GetDsssRate11Mbps (), 744000)
    && CheckPayloadDuration (1024, WifiPhy::GetDsssRate11Mbps (), 745000)
    && CheckPayloadDuration (1025, WifiPhy::GetDsssRate11Mbps (), 746000)
    && CheckPayloadDuration (1026, WifiPhy::GetDsssRate11Mbps (), 747000);


  // Similar, but we add PLCP preamble and header durations
  // and we test different rates.
  // The payload durations for modes other than 11mbb have been
  // calculated by hand according to  IEEE Std 802.11-2007 18.2.3.5 
  retval = retval
    && CheckTxDuration (1023, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_SHORT, 744000 + 96000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_SHORT, 745000 + 96000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_SHORT, 746000 + 96000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_SHORT, 747000 + 96000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 744000 + 192000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 745000 + 192000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 746000 + 192000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 747000 + 192000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_SHORT, 1488000 + 96000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_SHORT, 1490000 + 96000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_SHORT, 1491000 + 96000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_SHORT, 1493 + 96000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_LONG, 1488000 + 192000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_LONG, 1490000 + 192000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_LONG, 1491000 + 192000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate5_5Mbps (), WIFI_PREAMBLE_LONG, 1493000 + 192000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_SHORT, 4092000 + 96000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_SHORT, 4096000 + 96000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_SHORT, 4100000 + 96000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_SHORT, 4104000 + 96000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_LONG, 4092000 + 192000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_LONG, 4096000 + 192000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_LONG, 4100000 + 192000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate2Mbps (), WIFI_PREAMBLE_LONG, 4104000 + 192000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_SHORT, 8184000 + 96000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_SHORT, 8192000 + 96000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_SHORT, 8200000 + 96000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_SHORT, 8208000 + 96000)
    && CheckTxDuration (1023, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_LONG, 8184000 + 192000)
    && CheckTxDuration (1024, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_LONG, 8192000 + 192000)
    && CheckTxDuration (1025, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_LONG, 8200000 + 192000)
    && CheckTxDuration (1026, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_LONG, 8208000 + 192000);

  // values from http://mailman.isi.edu/pipermail/ns-developers/2009-July/006226.html
  retval = retval && CheckTxDuration (14, WifiPhy::GetDsssRate1Mbps (), WIFI_PREAMBLE_LONG, 304000);

  // values from
  // http://www.oreillynet.com/pub/a/wireless/2003/08/08/wireless_throughput.html
  retval = retval 
    && CheckTxDuration (1536, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 1310000)
    && CheckTxDuration (76, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 248000)
    && CheckTxDuration (14, WifiPhy::GetDsssRate11Mbps (), WIFI_PREAMBLE_LONG, 203000)
    && CheckTxDuration (1536, WifiPhy::GetOfdmRate54Mbps (), WIFI_PREAMBLE_LONG, 248000)
    && CheckTxDuration (76, WifiPhy::GetOfdmRate54Mbps (), WIFI_PREAMBLE_LONG, 32000)
    && CheckTxDuration (14, WifiPhy::GetOfdmRate54Mbps (), WIFI_PREAMBLE_LONG, 24000);

  return (!retval);
}

class TxDurationTestSuite : public TestSuite
{
public:
  TxDurationTestSuite ();
};

TxDurationTestSuite::TxDurationTestSuite ()
  : TestSuite ("devices-wifi-tx-duration", UNIT)
{
  AddTestCase (new InterferenceHelperTxDurationTest);
}

TxDurationTestSuite g_txDurationTestSuite;
} //namespace ns3 

