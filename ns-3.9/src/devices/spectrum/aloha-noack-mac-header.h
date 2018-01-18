/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009, 2010 CTTC
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


#ifndef ALOHA_NOACK_MAC_HEADER_H
#define ALOHA_NOACK_MAC_HEADER_H

#include <ns3/header.h>
#include <ns3/mac48-address.h>
#include <ns3/address-utils.h>

namespace ns3 {

class AlohaNoackMacHeader : public Header
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;

  void SetSource (Mac48Address source);
  void SetDestination (Mac48Address destination);
  Mac48Address GetSource () const;
  Mac48Address GetDestination () const;

private:
  Mac48Address m_source;
  Mac48Address m_destination;
};



} // namespace ns3

#endif /* ALOHA_NOACK_MAC_HEADER_H */
