/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage.inria.fr>
 */

#include "system-wall-clock-ms.h"

#include <time.h>

namespace ns3 {

class SystemWallClockMsPrivate {
public:
  void Start (void);
  int64_t End (void);
  int64_t GetElapsedReal (void) const;
  int64_t GetElapsedUser (void) const;
  int64_t GetElapsedSystem (void) const;

private:
  clock_t m_startTime;
  int64_t m_elapsedReal;
  int64_t m_elapsedUser;
  int64_t m_elapsedSystem;
};

void 
SystemWallClockMsPrivate::Start (void)
{
  m_startTime = clock ();
}

int64_t
SystemWallClockMsPrivate::End (void)
{
  //
  // We need to return the number of milliseconds that have elapsed in some
  // reasonably portable way.  The underlying function that we will use returns
  // a number of elapsed ticks.  We can look up the number of ticks per second
  // from the system configuration.
  //
  // Conceptually, we need to find the number of elapsed clock ticks and then
  // multiply the result by the milliseconds per clock tick (or just as easily
  // divide by clock ticks per millisecond).  Integer dividing by clock ticks
  // per millisecond is bad since this number is fractional on most machines
  // and would result in divide by zero errors due to integer rounding.
  //
  // Multiplying by milliseconds per clock tick works up to a clock resolution 
  // of 1000 ticks per second.  If we go  past this point, we begin to get zero
  // elapsed times when millisecondsPerTick becomes fractional and another 
  // rounding error appears.
  //
  // So rounding errors using integers can bite you from two direction.  Since 
  // all of our targets have math coprocessors, why not just use doubles 
  // internally?  Works fine, lasts a long time.
  //
  // If millisecondsPerTick becomes fractional, and an elapsed time greater than 
  // a milliscond is measured, the function will work as expected.  If an elapsed 
  // time is measured that turns out to be less than a millisecond, we'll just 
  // return zero which would, I think, also will be expected.
  //
  static int64_t ticksPerSecond = CLOCKS_PER_SEC;
  static double millisecondsPerTick = 1000. / ticksPerSecond;

  clock_t endTime = clock ();

  double tmp;

  tmp = static_cast<double> (endTime - m_startTime) * millisecondsPerTick;
  m_elapsedReal = static_cast<int64_t> (tmp);

  //
  // Nothing like this in MinGW, for example.
  //
  m_elapsedUser = 0;
  m_elapsedSystem = 0;

  return m_elapsedReal;
}

int64_t
SystemWallClockMsPrivate::GetElapsedReal (void) const
{
  return m_elapsedReal;
}

int64_t
SystemWallClockMsPrivate::GetElapsedUser (void) const
{
  return m_elapsedUser;
}

int64_t
SystemWallClockMsPrivate::GetElapsedSystem (void) const
{
  return m_elapsedSystem;
}
  
SystemWallClockMs::SystemWallClockMs ()
  : m_priv (new SystemWallClockMsPrivate ())
{}

SystemWallClockMs::~SystemWallClockMs ()
{
  delete m_priv;
  m_priv = 0;
}

void
SystemWallClockMs::Start (void)
{
  m_priv->Start ();
}

int64_t
SystemWallClockMs::End (void)
{
  return m_priv->End ();
}

int64_t
SystemWallClockMs::GetElapsedReal (void) const
{
  return m_priv->GetElapsedReal ();
}

int64_t
SystemWallClockMs::GetElapsedUser (void) const
{
  return m_priv->GetElapsedUser ();
}

int64_t
SystemWallClockMs::GetElapsedSystem (void) const
{
  return m_priv->GetElapsedSystem ();
}

}; // namespace ns3
