/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "error-rate-model.h"
#include <math.h>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ErrorRateModel);

TypeId ErrorRateModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ErrorRateModel")
    .SetParent<Object> ()
    ;
  return tid;
}

double 
ErrorRateModel::CalculateSnr (WifiMode txMode, double ber) const
{
  /* This is a very simple binary search */
  double lowDb, highDb, precisionDb, middle, middleDb;
  /*
   * The "dB" quantities are actually dB/10, but we would just be multiplying
   * and dividing, so skip it
   */
  lowDb = log10(1e-10);
  highDb = log10(1e10);
  precisionDb = 0.01 / 10; /* Accurate to 0.01 dB */
  do
    {
      NS_ASSERT (highDb >= lowDb);
      middleDb = (lowDb + highDb) / 2;
      middle = pow(10,middleDb);
      if ((1 - GetChunkSuccessRate (txMode, middle, 1)) > ber) 
        {
	  lowDb = middleDb;
        } 
      else 
        {
	  highDb = middleDb;
        }
    } while (highDb - lowDb > precisionDb) ;
  return middle;
}

} // namespace ns3
