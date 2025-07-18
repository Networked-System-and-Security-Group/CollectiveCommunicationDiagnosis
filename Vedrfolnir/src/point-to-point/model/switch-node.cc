#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "switch-node.h"
#include "qbb-net-device.h"
#include "ppp-header.h"
#include "ns3/int-header.h"
#include <cmath>
#include <vector>

namespace ns3 {

TypeId SwitchNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchNode")
    .SetParent<Node> ()
    .AddConstructor<SwitchNode> ()
	.AddAttribute("EcnEnabled",
			"Enable ECN marking.",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
			MakeBooleanChecker())
	.AddAttribute("CcMode",
			"CC mode.",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ccMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("AckHighPrio",
			"Set high priority for ACK/NACK or not",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("MaxRtt",
			"Max Rtt of the network",
			UintegerValue(9000),
			MakeUintegerAccessor(&SwitchNode::m_maxRtt),
			MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

SwitchNode::SwitchNode(){
	m_ecmpSeed = m_id;
	m_node_type = 1;
	m_mmu = CreateObject<SwitchMmu>();
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPktSize[i] = m_lastPktTs[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_u[i] = 0;
	
	//RDMA NPA init
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < epochNum; j++)
			for (uint32_t k = 0; k < flowEntryNum; k++)
				memset(&m_flowTelemetryData[i][j][k].flowTuple, 0, sizeof(FiveTuple));
	for (uint32_t j = 0; j < epochNum; j++)
		for (uint32_t k = 0; k < pCnt; k++){
				m_portTelemetryData[j][k].enqQdepth = 0;
				m_portTelemetryData[j][k].pfcPausedPacketNum = 0;
				m_portTelemetryData[j][k].lastTimeStep = 0;
				m_portTelemetryData[j][k].packetNum = 0;
			}
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++){
			m_portToPortBytes[i][j] = 0;
			for(uint32_t k = 0; k < portToPortSlot; k++)
				m_portToPortBytesSlot[i][j][k] = 0;
		}
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPollingEpoch[i] = 0;
	m_slotIdx = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastEventID[i] = 0;
}

int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch){
	// look up entries
	auto entry = m_rtTable.find(ch.dip);

	// no matching entry
	if (entry == m_rtTable.end())
		return -1;

	// entry found
	auto &nexthops = entry->second;

	// pick one next hop based on hash
	union {
		uint8_t u8[4+4+2+2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ch.sip;
	buf.u32[1] = ch.dip;
	if (ch.l3Prot == 0x6)
		buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
	else if (ch.l3Prot == 0x11)
		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
	else if (ch.l3Prot == 0xFA)
		buf.u32[2] = ch.polling.sport | ((uint32_t)ch.polling.dport << 16);
	else if (ch.l3Prot == 0xF9)
		buf.u32[2] = ch.notif.sport | ((uint32_t)ch.notif.dport << 16);

	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
	return nexthops[idx];
}

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldPause(inDev, qIndex)){
		device->SendPfc(qIndex, 0);
		m_mmu->SetPause(inDev, qIndex);
	}
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldResume(inDev, qIndex)){
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void SwitchNode::OutputTelemetry(uint32_t port, uint32_t inport, bool isSignal){
	int epoch = GetEpochIdx();
	fprintf(fp_telemetry,"epoch now\n\n");
	if(isSignal){
		fprintf(fp_telemetry,"traffic meter form port %d to port %d\n", inport, port);
		fprintf(fp_telemetry, "portToPortBytes\n");
		fprintf(fp_telemetry, "%d\n\n", m_portToPortBytes[inport][port]);
	}
	fprintf(fp_telemetry,"port telemetry data for port %d\n", port);
	fprintf(fp_telemetry, "enqQdepth pfcPausedPacketNum packetNum\n");
	fprintf(fp_telemetry, "%d ", m_portTelemetryData[epoch][port].enqQdepth);
	fprintf(fp_telemetry, "%d ", m_portTelemetryData[epoch][port].pfcPausedPacketNum);
	fprintf(fp_telemetry, "%d\n\n", m_portTelemetryData[epoch][port].packetNum);

	fprintf(fp_telemetry,"flow telemetry data for port %d\n", port);
	fprintf(fp_telemetry, "flowIdx srcIp dstIp srcPort dstPort protocol minSeq maxSeq packetNum enqQdepth pfcPausedPacketNum\n");
	for(int i = 0; i < flowEntryNum; i++){
		if(m_flowTelemetryData[port][epoch][i].flowTuple.srcIp != 0 && Simulator::Now().GetTimeStep() - m_flowTelemetryData[port][epoch][i].lastTimeStep <= epochTime * (epochNum - 1)){
			fprintf(fp_telemetry, "%d ", i);
			fprintf(fp_telemetry, "%08x ", m_flowTelemetryData[port][epoch][i].flowTuple.srcIp);
			fprintf(fp_telemetry, "%08x ", m_flowTelemetryData[port][epoch][i].flowTuple.dstIp);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.srcPort);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.dstPort);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.protocol);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].minSeq);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].maxSeq);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].packetNum);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].enqQdepth);
			fprintf(fp_telemetry, "%d\n", m_flowTelemetryData[port][epoch][i].pfcPausedPacketNum);
		}
	}

	epoch = (epoch + epochNum - 1) % epochNum;

	fprintf(fp_telemetry,"\nepoch last\n\n");
	fprintf(fp_telemetry,"port telemetry data for port %d\n", port);
	fprintf(fp_telemetry, "enqQdepth pfcPausedPacketNum packetNum\n");
	fprintf(fp_telemetry, "%d ", m_portTelemetryData[epoch][port].enqQdepth);
	fprintf(fp_telemetry, "%d ", m_portTelemetryData[epoch][port].pfcPausedPacketNum);
	fprintf(fp_telemetry, "%d\n\n", m_portTelemetryData[epoch][port].packetNum);

	fprintf(fp_telemetry,"flow telemetry data for port %d\n", port);
	fprintf(fp_telemetry, "flowIdx srcIp dstIp srcPort dstPort protocol minSeq maxSeq packetNum enqQdepth pfcPausedPacketNum\n");
	for(int i = 0; i < flowEntryNum; i++){
		if(m_flowTelemetryData[port][epoch][i].flowTuple.srcIp != 0 && Simulator::Now().GetTimeStep() - m_flowTelemetryData[port][epoch][i].lastTimeStep <= epochTime * epochNum){
			fprintf(fp_telemetry, "%d ", i);
			fprintf(fp_telemetry, "%08x ", m_flowTelemetryData[port][epoch][i].flowTuple.srcIp);
			fprintf(fp_telemetry, "%08x ", m_flowTelemetryData[port][epoch][i].flowTuple.dstIp);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.srcPort);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.dstPort);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].flowTuple.protocol);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].minSeq);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].maxSeq);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].packetNum);
			fprintf(fp_telemetry, "%d ", m_flowTelemetryData[port][epoch][i].enqQdepth);
			fprintf(fp_telemetry, "%d\n", m_flowTelemetryData[port][epoch][i].pfcPausedPacketNum);
		}
	}
	fprintf(fp_telemetry,"\n");
	fflush(fp_telemetry);
}

void SwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch){
	//RDMA NPA : signal packet parse
	if (ch.l3Prot == 0xFB){
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		int event_id = ch.signal.eventID;

		fprintf(fp_telemetry,"time %lld\n", Simulator::Now().GetTimeStep());
		fprintf(fp_telemetry,"\nsignal\n\n");
		for (uint32_t idx = 0; idx < pCnt; idx++){
			if(m_portToPortBytes[inDev][idx] > 0){
				OutputTelemetry(idx, inDev, true);
				if(m_portTelemetryData[GetEpochIdx()][idx].pfcPausedPacketNum > 0 || m_portTelemetryData[(GetEpochIdx() + 1) % epochNum][idx].pfcPausedPacketNum > 0 || DynamicCast<QbbNetDevice>(m_devices[idx])->GetEgressPaused(3)){
					if(event_id > m_lastEventID[idx] + 500000 || m_lastEventID[idx] == 0){
						m_lastEventID[idx] = event_id;
					}else{
						continue;
					}
					DynamicCast<QbbNetDevice>(m_devices[idx])-> SendSignal(event_id);
				}
			}
		}
		fprintf(fp_telemetry,"end\n\n");
		return;	
	}
	//RDMA NPA : polling packet parse 
	else if(ch.l3Prot == 0xFA){
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		int idx = GetOutDev(p, ch);
		int event_id = ch.polling.eventID;
		if(m_portTelemetryData[GetEpochIdx()][idx].pfcPausedPacketNum > 0 || m_portTelemetryData[(GetEpochIdx() + 1) % epochNum][idx].pfcPausedPacketNum > 0 || DynamicCast<QbbNetDevice>(m_devices[idx])->GetEgressPaused(3)){
			if(event_id > m_lastEventID[idx] + 500000 || m_lastEventID[idx] == 0){
				m_lastEventID[idx] = event_id;
				DynamicCast<QbbNetDevice>(m_devices[idx])-> SendSignal(event_id);
			}
		}
		fprintf(fp_telemetry,"time %lld\n", Simulator::Now().GetTimeStep());
		fprintf(fp_telemetry,"\npolling\n\n");
		OutputTelemetry(idx, inDev, false);
		fprintf(fp_telemetry,"end\n\n");
	}

	int idx = GetOutDev(p, ch);
	if (idx >= 0){
		NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(), "The routing table look up should return link that is up");

		// determine the qIndex
		uint32_t qIndex;
		if (ch.l3Prot == 0xFA || ch.l3Prot == 0xF9 || ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  //QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}else{
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
		}

		// admission control
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		if (qIndex != 0){ //not highest priority
			if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize()) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize())){			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
			}else{
				return; // Drop
			}
			CheckAndSendPfc(inDev, qIndex);
		}

		// RDMA NPA
		if(qIndex != 0){
			if((Simulator::Now().GetTimeStep() / (epochTime / portToPortSlot)) % portToPortSlot != m_slotIdx){
				m_slotIdx = (Simulator::Now().GetTimeStep() / (epochTime / portToPortSlot)) % portToPortSlot;
				for(uint32_t inDev = 0; inDev < pCnt; inDev++){
					for(uint32_t outDev = 0; outDev < pCnt; outDev++){
						m_portToPortBytes[inDev][outDev] -= m_portToPortBytesSlot[inDev][outDev][m_slotIdx];
						m_portToPortBytesSlot[inDev][outDev][m_slotIdx] = 0;
					}
				}
			}
			m_portToPortBytesSlot[inDev][idx][m_slotIdx] += p->GetSize();
			m_portToPortBytes[inDev][idx] += p->GetSize(); 

			FiveTuple fiveTuple{
				.srcIp = ch.sip,
				.dstIp = ch.dip,
				.srcPort = ch.l3Prot == 0x06 ? ch.tcp.sport : ch.udp.sport,
				.dstPort = ch.l3Prot == 0x06 ? ch.tcp.dport : ch.udp.dport,
				.protocol = (uint8_t)ch.l3Prot
			};
			uint32_t epochIdx = GetEpochIdx();
			uint32_t flowIdx = FiveTupleHash(fiveTuple);
			auto &entry = m_flowTelemetryData[idx][epochIdx][flowIdx];
			bool newEntry = Simulator::Now().GetTimeStep() - entry.lastTimeStep > epochTime * (epochNum - 1);
			if (entry.flowTuple == fiveTuple && !newEntry){
				uint32_t seq = ch.l3Prot == 0x06 ? ch.tcp.seq : ch.udp.seq;
				if(seq < entry.minSeq){
					entry.minSeq = seq;
				}
				if(seq > entry.maxSeq){
					entry.maxSeq = seq;
				}
				entry.packetNum++;
				if(DynamicCast<QbbNetDevice>(m_devices[idx])->GetEgressPaused(qIndex)){
					entry.pfcPausedPacketNum++;
				}else{
					entry.enqQdepth += m_mmu->egress_queue_length[idx][qIndex] - 1;
				}
				entry.lastTimeStep = Simulator::Now().GetTimeStep();
			} else{
				entry.flowTuple = fiveTuple;
				entry.minSeq = entry.maxSeq = ch.l3Prot == 0x06 ? ch.tcp.seq : ch.udp.seq;
				entry.packetNum = 1;
				entry.pfcPausedPacketNum = 0;
				if(DynamicCast<QbbNetDevice>(m_devices[idx])->GetEgressPaused(qIndex)){
					entry.pfcPausedPacketNum++;
				}else{
					entry.enqQdepth = m_mmu->egress_queue_length[idx][qIndex] - 1;
				}
				entry.lastTimeStep = Simulator::Now().GetTimeStep();
			}

			auto &portEntry = m_portTelemetryData[epochIdx][idx];
			bool newPortEntry = Simulator::Now().GetTimeStep() - portEntry.lastTimeStep > epochTime * (epochNum - 1);
			if (!newPortEntry){
				portEntry.enqQdepth += m_mmu->egress_queue_length[idx][qIndex] - 1;
				portEntry.packetNum++;
				if(DynamicCast<QbbNetDevice>(m_devices[idx])->GetEgressPaused(qIndex)){
					portEntry.pfcPausedPacketNum++;
				}
				portEntry.lastTimeStep = Simulator::Now().GetTimeStep();
			} else{
				portEntry.enqQdepth = m_mmu->egress_queue_length[idx][qIndex] - 1;
				portEntry.pfcPausedPacketNum = 0;
				portEntry.packetNum = 1;
				portEntry.lastTimeStep = Simulator::Now().GetTimeStep();
			}
		}

		m_bytes[inDev][idx][qIndex] += p->GetSize();
		m_devices[idx]->SwitchSend(qIndex, p, ch);
	}else
		return; // Drop
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

uint32_t SwitchNode::FiveTupleHash(const FiveTuple &fiveTuple){
	return EcmpHash((const uint8_t*)&fiveTuple, sizeof(fiveTuple), flowHashSeed) % flowEntryNum;
}

uint32_t SwitchNode::GetEpochIdx(){
	return Simulator::Now().GetTimeStep() / epochTime % epochNum;
}

void SwitchNode::SetEcmpSeed(uint32_t seed){
	m_ecmpSeed = seed;
}

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx){
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable(){
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch){
	SendToDev(packet, ch);
	return true;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p){
	FlowIdTag t;
	p->PeekPacketTag(t);
	if (qIndex != 0){
		uint32_t inDev = t.GetFlowId();
		m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
		if (m_ecnEnabled){
			bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested){
				PppHeader ppp;
				Ipv4Header h;
				p->RemoveHeader(ppp);
				p->RemoveHeader(h);
				h.SetEcn((Ipv4Header::EcnType)0x03);
				p->AddHeader(h);
				p->AddHeader(ppp);
			}
		}
		//CheckAndSendPfc(inDev, qIndex);
		CheckAndSendResume(inDev, qIndex);
	}
	if (1){
		uint8_t* buf = p->GetBuffer();
		if (buf[PppHeader::GetStaticSize() + 9] == 0x11){ // udp packet
			IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
			if (m_ccMode == 3){ // HPCC
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
			}else if (m_ccMode == 10){ // HPCC-PINT
				uint64_t t = Simulator::Now().GetTimeStep();
				uint64_t dt = t - m_lastPktTs[ifIndex];
				if (dt > m_maxRtt)
					dt = m_maxRtt;
				uint64_t B = dev->GetDataRate().GetBitRate() / 8; //Bps
				uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
				double newU;

				/**************************
				 * approximate calc
				 *************************/
				int b = 20, m = 16, l = 20; // see log2apprx's paremeters
				int sft = logres_shift(b,l);
				double fct = 1<<sft; // (multiplication factor corresponding to sft)
				double log_T = log2(m_maxRtt)*fct; // log2(T)*fct
				double log_B = log2(B)*fct; // log2(B)*fct
				double log_1e9 = log2(1e9)*fct; // log2(1e9)*fct
				double qterm = 0;
				double byteTerm = 0;
				double uTerm = 0;
				if ((qlen >> 8) > 0){
					int log_dt = log2apprx(dt, b, m, l); // ~log2(dt)*fct
					int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
					qterm = pow(2, (
								log_dt + log_qlen + log_1e9 - log_B - 2*log_T
								)/fct
							) * 256;
					// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
				}
				if (m_lastPktSize[ifIndex] > 0){
					int byte = m_lastPktSize[ifIndex];
					int log_byte = log2apprx(byte, b, m, l);
					byteTerm = pow(2, (
								log_byte + log_1e9 - log_B - log_T
								)/fct
							);
					// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
				}
				if (m_maxRtt > dt && m_u[ifIndex] > 0){
					int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l); // ~log2(T-dt)*fct
					int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
					uTerm = pow(2, (
								log_T_dt + log_u - log_T
								)/fct
							) / 8192;
					// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
				}
				newU = qterm+byteTerm+uTerm;

				#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else{
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
				#endif

				/************************
				 * update PINT header
				 ***********************/
				uint16_t power = Pint::encode_u(newU);
				if (power > ih->GetPower())
					ih->SetPower(power);

				m_u[ifIndex] = newU;
			}
		}
	}
	m_txBytes[ifIndex] += p->GetSize();
	m_lastPktSize[ifIndex] = p->GetSize();
	m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
}

int SwitchNode::logres_shift(int b, int l){
	static int data[] = {0,0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};
	return l - data[b];
}

int SwitchNode::log2apprx(int x, int b, int m, int l){
	int x0 = x;
	int msb = int(log2(x)) + 1;
	if (msb > m){
		x = (x >> (msb - m) << (msb - m));
		#if 0
		x += + (1 << (msb - m - 1));
		#else
		int mask = (1 << (msb-m)) - 1;
		if ((x0 & mask) > (rand() & mask))
			x += 1<<(msb-m);
		#endif
	}
	return int(log2(x) * (1<<logres_shift(b, l)));
}

} /* namespace ns3 */
