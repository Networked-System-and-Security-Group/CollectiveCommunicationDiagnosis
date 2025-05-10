/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#include "rdma-cc-helper.h"
#include "ns3/rdma-cc.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

namespace ns3 {

RdmaCCHelper::RdmaCCHelper ()
{
}

RdmaCCHelper::RdmaCCHelper (uint16_t rank, Ipv4Address ip, uint16_t port, uint8_t alg, uint64_t size, uint32_t win, uint64_t baseRtt, uint16_t pg)
{
  m_factory.SetTypeId (RdmaCC::GetTypeId ());
  SetAttribute ("RankID", UintegerValue (rank));
  SetAttribute ("IP", Ipv4AddressValue (ip));
  SetAttribute ("Port", UintegerValue (port));
  SetAttribute ("Algorithm", UintegerValue (alg));
  SetAttribute ("ChunkSize", UintegerValue (size));
  SetAttribute ("Window", UintegerValue (win));
  SetAttribute ("BaseRtt", UintegerValue (baseRtt));
  SetAttribute ("PriorityGroup", UintegerValue (pg));
}

void
RdmaCCHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

Ptr<Application>
RdmaCCHelper::Install (Ptr<Node> c)
{
  Ptr<RdmaCC> client = m_factory.Create<RdmaCC> ();
  c->AddApplication (client);
  
  return client;
}

} // namespace ns3
