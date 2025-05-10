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
#ifndef RDMA_CC_CLIENT_SERVER_HELPER_H
#define RDMA_CC_CLIENT_SERVER_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/rdma-cc.h"

namespace ns3 {

class RdmaCCHelper
{

public:
  
  RdmaCCHelper ();

  RdmaCCHelper (uint16_t rank, Ipv4Address ip, uint16_t port, uint8_t alg, uint64_t size, uint32_t win, uint64_t baseRtt, uint16_t pg);

  void SetAttribute (std::string name, const AttributeValue &value);

  Ptr<Application> Install (Ptr<Node> c);

private:
  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* RDMA_CLIENT_SERVER_HELPER_H */
