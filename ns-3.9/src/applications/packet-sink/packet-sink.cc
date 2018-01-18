/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "packet-sink.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PacketSink");
NS_OBJECT_ENSURE_REGISTERED (PacketSink);

TypeId 
PacketSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSink")
    .SetParent<Application> ()
    .AddConstructor<PacketSink> ()
    .AddAttribute ("Local", "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&PacketSink::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PacketSink::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSink::m_rxTrace))
    ;
  return tid;
}

PacketSink::PacketSink ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
  m_expected = -1;
}

PacketSink::~PacketSink()
{
  NS_LOG_FUNCTION (this);
}

uint64_t PacketSink::GetTotalRx() const
{
  return m_totalRx;
}

Time PacketSink::WhenFinished() 
{
  return m_whenFinished;
}

double PacketSink::TotalDelay() 
{
  return  m_whenFinished.GetSeconds() - m_whenStarted.GetSeconds();
}

void PacketSink::SetExpected(uint64_t expected)
{
  m_expected = expected;
}

bool PacketSink::IsFinished()
{
    if (m_expected <= 0)
        return false;
    else if (m_expected <= m_totalRx)
        return true;
    else
        return false;
}

void PacketSink::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSink::StartApplication()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode(), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
      m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback(&PacketSink::HandleRead, this));
  m_socket->SetAcceptCallback (
            MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
            MakeCallback(&PacketSink::HandleAccept, this));
  m_socket->SetCloseCallbacks (
            MakeCallback(&PacketSink::HandlePeerClose, this),
            MakeCallback(&PacketSink::HandlePeerError, this));
  m_whenStarted = Simulator::Now();
}

void PacketSink::StopApplication()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty()) //these are accepted sockets, close them
  {
    Ptr<Socket> acceptedSocket = m_socketList.front();
    m_socketList.pop_front();
    //std::cout << "calling close on acceped " << acceptedSocket << std::endl;
    acceptedSocket->Close();
  }
  if (m_socket) 
    {
      //std::cout << "calling close on listen " << m_socket << std::endl;
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void PacketSink::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while (packet = socket->RecvFrom (from))
    {
      if (packet->GetSize() == 0)
        { //EOF
	  break;
        }
      if (InetSocketAddress::IsMatchingType (from))
        {
          InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
          m_totalRx += packet->GetSize();
          NS_LOG_INFO ("Received " << packet->GetSize() << " bytes from " << 
                       address.GetIpv4() << " [" << address << "]" 
                       << " total Rx " << m_totalRx);
          if (m_expected > 0 && m_totalRx >= m_expected) {
              //std::cout << "==> stopping the receiver" <<  " " << m_expected << " " << m_totalRx << " " << packet->GetSize() << std::endl;
              m_whenFinished = Simulator::Now ();
              StopApplication();
          }
        }    
      m_rxTrace (packet, from);
    }
}

void PacketSink::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_INFO("PktSink, peerClose");
}
 
void PacketSink::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_INFO("PktSink, peerError");
}
 

void PacketSink::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback(&PacketSink::HandleRead, this));
  m_socketList.push_back (s);
}

} // Namespace ns3
