#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/random-variable.h"
#include "ns3/qbb-net-device.h"
#include "ns3/ipv4-end-point.h"
#include "rdma-cc.h"
#include "ns3/seq-ts-header.h"
#include "ns3/rdma-driver.h"
#include "ns3/rdma-hw.h"
#include "ns3/rdma-queue-pair.h"
#include "ns3/ppp-header.h"
#include <stdlib.h>
#include <stdio.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RdmaCC");
NS_OBJECT_ENSURE_REGISTERED (RdmaCC);

TypeId
RdmaCC::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RdmaCC")
    .SetParent<Application> ()
    .AddConstructor<RdmaCC> ()
    .AddAttribute ("CommOP",
                    "Operation of collective communication",
                    UintegerValue (0),
                    MakeUintegerAccessor (&RdmaCC::m_op),
                    MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("RankID",
                   "Rank ID",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaCC::m_rank),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Algorithm",
                    "algorithm",
                    UintegerValue (0),
                    MakeUintegerAccessor (&RdmaCC::m_alg),
                    MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("StepStu",
                    "status of current step",
                    UintegerValue (1),
                    MakeUintegerAccessor (&RdmaCC::m_stepStu),
                    MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("IP",
                   "Source IP",
                   Ipv4AddressValue ("0.0.0.0"),
                   MakeIpv4AddressAccessor (&RdmaCC::m_ip),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("Port",
                   "Source Port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaCC::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ChunkSize",
                    "The size of each chunk",
                    UintegerValue (0),
                    MakeUintegerAccessor (&RdmaCC::m_chunkSize),
                    MakeUintegerChecker<uint64_t> ()) 
    .AddAttribute ("ChunkNum",
                    "The number of chunks to send",
                    UintegerValue (1),
                    MakeUintegerAccessor (&RdmaCC::m_chunkNum),
                    MakeUintegerChecker<uint16_t> ())               
	.AddAttribute ("PriorityGroup", 
                   "The priority group of this flow",
				    UintegerValue (0),
				    MakeUintegerAccessor (&RdmaCC::m_pg),
				    MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Window",
                   "Bound of on-the-fly packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaCC::m_win),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BaseRtt",
                   "Base Rtt",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaCC::m_baseRtt),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("TimesDetect",
                   "Times of detecting per step",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RdmaCC::m_agent_times),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

RdmaCC::RdmaCC ()
{
//   NS_LOG_FUNCTION_NOARGS ();
}

RdmaCC::~RdmaCC ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void RdmaCC::SetRank(uint16_t rank)
{
    m_rank = rank;
}

void RdmaCC::SetLocal (Ipv4Address ip, uint16_t port)
{
  m_ip = ip;
  m_port = port;
}

void RdmaCC::AddCommGroup (Ipv4Address ip, uint16_t port)
{
  Comm comm;
  comm.ip = ip;
  comm.port = port;
  m_comm.push_back(comm);
}

void RdmaCC::SetChunk(uint64_t size, uint16_t num){
    m_chunkSize = size;
    m_chunkNum = num;
}

void RdmaCC::SetAlg(uint8_t alg){
    m_alg = alg;
}

void RdmaCC::SetControl(uint32_t win, uint64_t baseRtt, uint16_t pg){
    m_win = win;
    m_baseRtt = baseRtt;
    m_pg = pg;
}

Ipv4Address RdmaCC::GetIP(){
    return m_ip;
}

uint16_t RdmaCC::GetPort(){
    return m_port;
}

uint32_t RdmaCC::GetSendStep(){
    return m_sendStep;
}

uint32_t RdmaCC::GetRecvStep(){
    return m_recvStep;
}

int RdmaCC::GetPrevRank(){
    return m_prevRank[m_recvStep - 1];
}

void RdmaCC::SetIP2APPCb(Callback<Ptr<RdmaCC>, Ipv4Address> cb){
    m_ip2app = cb;
}

void RdmaCC::Send(uint16_t distRank){
    
    // NS_LOG_FUNCTION("Rank: " << m_rank << "  SendStep: m_sendStep = " << m_sendStep << ", m_recvStep = " << m_recvStep);
    // get RDMA drive
    Ptr<RdmaDriver> rdma = GetNode()->GetObject<RdmaDriver>();
    if(m_alg == 2)
        rdma->AddQueuePair(m_packetSize[m_sendStep - 1], m_pg, m_ip, m_comm[distRank].ip, m_port, m_comm[distRank].port, m_win, m_baseRtt, MakeCallback(&RdmaCC::SendChunkFinish, this));
    else
        rdma->AddQueuePair(m_chunkSize, m_pg, m_ip, m_comm[distRank].ip, m_port, m_comm[distRank].port, m_win, m_baseRtt, MakeCallback(&RdmaCC::SendChunkFinish, this));
}

void RdmaCC::FinishRecvStep(){
    NS_LOG_FUNCTION_NOARGS ();
    m_recvStep++;
    SendStep();
}

void RdmaCC::SendChunkFinish(){
    NS_LOG_FUNCTION_NOARGS ();

    if(m_alg == 0){
        // do nothing
        return;
    }

    Ptr<RdmaCC> dstCC = m_ip2app(m_comm[m_nextRank[m_sendStep - 1]].ip);
    dstCC->FinishRecvStep();

    m_stepStu = 1;
    SendStep();
}

void RdmaCC::Scatter(uint16_t rootRank){
    NS_LOG_FUNCTION_NOARGS ();

    m_op = 1;

    if(m_rank == rootRank){
        for(uint16_t i = 0; i < m_comm.size(); i++){
            if(i != rootRank){
                Send(i);
            }
        }
    }
}

void RdmaCC::Gather(uint16_t rootRank){
    NS_LOG_FUNCTION_NOARGS ();
    m_op = 2;
    if(m_rank != rootRank){
        for(uint32_t i = 0; i < m_comm.size(); i++){
            Send(rootRank);
        }
    }
}

void RdmaCC::AlltoAll(){
    NS_LOG_FUNCTION_NOARGS ();
    m_op = 3;
    for(uint32_t i = 0; i < m_comm.size(); i++){
        if(i != m_rank){
            Send(i);
        }
    }
}

void RdmaCC::SendStep(){
    // NS_LOG_FUNCTION_NOARGS ();

    while(m_recvStep <= m_nextRank.size() && m_prevRank[m_recvStep - 1] == -1){
        m_recvStep++;
    }

    while(m_sendStep < m_recvStep && m_nextRank[m_sendStep] == -1){
        m_sendStep++;
    }

    SendNotification(); // CC NPA

    if(m_stepStu == 1 && m_recvStep > m_sendStep){
        m_stepStu = 0;
        m_sendStep++; 

        if(m_sendStep > m_nextRank.size()){
            //TODO  application finished
            return;
        }

        SetAgent(); // CC NPA
        Send(m_nextRank[m_sendStep - 1]);
    }
}

void RdmaCC::Allgather(){
    // NS_LOG_FUNCTION_NOARGS ();

    m_op = 7;

    if(m_alg == 1){
        // ring
        uint32_t commLen = m_comm.size();
        for(uint32_t i = 0; i < (commLen - 1); i++){
            m_prevRank.push_back((m_rank - 1 + commLen) % commLen);
            m_nextRank.push_back((m_rank + 1) % commLen);
        }

        return;
    }

    NS_ASSERT_MSG(0, "The algorithm is not supported");

    //TODO
}

void RdmaCC::Allreduce(){
    // NS_LOG_FUNCTION_NOARGS ();

    m_op = 4;

    if(m_alg == 1){
        // ring
        uint32_t commLen = m_comm.size();
        for(uint32_t i = 0; i < 2 * (commLen - 1); i++){
            m_prevRank.push_back((m_rank - 1 + commLen) % commLen);
            m_nextRank.push_back((m_rank + 1) % commLen);
        }

        return;
    }

    if(m_alg == 2){
        // halving and doubling

        // align
        uint32_t p = m_comm.size();
        uint32_t pow = 1;
        for(uint32_t i = p >> 1; i; i >>= 1){
            pow <<= 1;
        }
        uint32_t r = p - pow;
        if(r != 0 && m_rank < 2*r){
            m_prevRank.push_back(m_rank ^ 1);
            m_nextRank.push_back(m_rank ^ 1);
            m_packetSize.push_back(m_chunkSize * p / 2);

            if(m_rank & 1){
                m_prevRank.push_back(-1);
                m_nextRank.push_back(m_rank ^ 1);
                m_packetSize.push_back(m_chunkSize * p / 2); 
            }
            else{
                m_prevRank.push_back(m_rank ^ 1);
                m_nextRank.push_back(-1);
                m_packetSize.push_back(0);
            }
        }

        if(r != 0 && m_rank >= 2*r){
            m_prevRank.push_back(-1); m_prevRank.push_back(-1);
            m_nextRank.push_back(-1); m_nextRank.push_back(-1);
            m_packetSize.push_back(0); m_packetSize.push_back(0);
        }

        std::vector<uint32_t> mapRank(p);  // index rank; value newRank
        for(uint32_t i = 0; i < p; i++){
            mapRank[i] = i;
        }

        if(r != 0){
            for(uint32_t i = 0; i < 2*r; i++){
                if(i % 2 ==0) mapRank[i] = i / 2;
                else mapRank[i] = i / 2 + pow;
            }
        }

        std::vector<uint32_t> rmapRank(p);  // index newRank; value rank
        for(uint32_t i = 0; i < p; i++){
            rmapRank[mapRank[i]] = i;
        }

        // halving and doubling
        for(uint32_t i = 1; i < pow; i <<= 1){
            if(mapRank[m_rank] < pow){
                if((mapRank[m_rank] & i) == 0){
                    m_prevRank.push_back(rmapRank[mapRank[m_rank] + i]);
                    m_nextRank.push_back(rmapRank[mapRank[m_rank] + i]);
                }
                else{
                    m_prevRank.push_back(rmapRank[mapRank[m_rank] - i]);
                    m_nextRank.push_back(rmapRank[mapRank[m_rank] - i]);
                }
                m_packetSize.push_back(m_chunkSize * p / 2 / i);
            }
            else{
                m_prevRank.push_back(-1);
                m_nextRank.push_back(-1);
                m_packetSize.push_back(0);
            }
        }

        for(uint32_t i = pow / 2; i >= 1; i >>= 1){
            if(mapRank[m_rank] < pow){
                if((mapRank[m_rank] & i) == 0){
                    m_prevRank.push_back(rmapRank[mapRank[m_rank] + i]);
                    m_nextRank.push_back(rmapRank[mapRank[m_rank] + i]);
                }
                else{
                    m_prevRank.push_back(rmapRank[mapRank[m_rank] - i]);
                    m_nextRank.push_back(rmapRank[mapRank[m_rank] - i]);
                }
                m_packetSize.push_back(m_chunkSize * p / 2 / i);
            }
            else{
                m_prevRank.push_back(-1);
                m_nextRank.push_back(-1);
                m_packetSize.push_back(0);
            }
        }

        // align
        if(r != 0 && m_rank < 2*r){
            if(m_rank & 1){
                m_prevRank.push_back(m_rank ^ 1);
                m_nextRank.push_back(-1);
                m_packetSize.push_back(0); 
            }
            else{
                m_prevRank.push_back(-1);
                m_nextRank.push_back(m_rank ^ 1);
                m_packetSize.push_back(m_chunkSize * p);
            }
        }

        if(r != 0 && m_rank >= 2*r){
            m_prevRank.push_back(-1);
            m_nextRank.push_back(-1);
            m_packetSize.push_back(0);
        }

        return;
    }

    NS_ASSERT_MSG(0, "The algorithm is not supported");

    //TODO
}


void RdmaCC::StartApplication (void)
{
    NS_ASSERT_MSG(m_comm.size() > 1, "The number of ranks should be greater than 1");
    NS_ASSERT(m_comm.size() <= 30000);

    NS_ASSERT_MSG(m_ip2app.IsNull() == false, "IP2APP callback should be set");

    // NS_LOG_FUNCTION_NOARGS();

    if (m_alg != 3){
        NS_ASSERT_MSG(m_chunkNum == 1, "The chunk number should be 1");
    }

    NS_ASSERT_MSG(m_op > 3 && m_alg != 0, "Start cllocative communication");

    m_recvStep = 1;
    m_sendStep = 0;

    SendStep();
}

void RdmaCC::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
//   m_node->DeleteApplication(this);
  Application::DoDispose ();
}

void RdmaCC::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();
  //TODO: reset the application
  
}

// CC NPA
void RdmaCC::SendNotification(){
    NS_LOG_FUNCTION_NOARGS ();
    
    NS_ASSERT(m_sendStep <= m_recvStep);

    if(m_sendStep < m_recvStep){ // not waiting
        return;
    }

    NS_ASSERT(m_prevRank[m_sendStep - 1] != -1);

    NS_LOG_FUNCTION("Rank: " << m_rank << "  SendNotification: m_sendStep = " << m_sendStep);

    Ptr<Packet> p = Create<Packet>(0);
    
    CustomHeader notifHdr(CustomHeader::L4_Header);
    notifHdr.l3Prot = 0xF9;
    notifHdr.notif.sport = m_port;
    notifHdr.notif.dport = m_comm[m_prevRank[m_sendStep - 1]].port;
    notifHdr.notif.step = m_sendStep;
    p->AddHeader(notifHdr);
    
    Ipv4Header head;
    head.SetSource(m_ip);
    head.SetDestination(m_comm[m_prevRank[m_sendStep - 1]].ip);
    head.SetProtocol(0xF9);
    head.SetTtl(64);
    head.SetPayloadSize(p->GetSize());
    p->AddHeader(head);

    PppHeader ppp;
    ppp.SetProtocol(0x0021);
    p->AddHeader(ppp);

    Ptr<RdmaHw> rdmaHw = GetNode()->GetObject<RdmaDriver>()->m_rdma;

    auto &v = rdmaHw->m_rtTable[m_comm[m_prevRank[m_sendStep - 1]].ip.Get()];  ////
    union{
        struct {
                uint32_t sip, dip;
                uint16_t sport, dport;
            };
            char c[12];
    } buf;
    buf.sip = m_ip.Get();
    buf.dip = m_comm[m_prevRank[m_sendStep - 1]].ip.Get();
    buf.sport = m_port;
    buf.dport = m_comm[m_prevRank[m_sendStep - 1]].port;
    uint32_t nic_idx = v[Hash32(buf.c, 12) % v.size()];

    Ptr<QbbNetDevice> dev = rdmaHw->m_nic[nic_idx].dev;

    dev->RdmaEnqueueHighPrioQ(p);
    dev->TriggerTransmit();
}

// CC NPA
void RdmaCC::SetAgent(){
    NS_LOG_FUNCTION_NOARGS ();

    Ptr<RdmaHw> rdmaHw = GetNode()->GetObject<RdmaDriver>()->m_rdma;
    rdmaHw->m_agent_step = 0;

    if(rdmaHw->m_detectSteps.count(m_sendStep)){
        rdmaHw->m_agent_step = m_sendStep;
        rdmaHw->m_detectSteps.erase(m_sendStep);
    }
}

} // Namespace ns3
