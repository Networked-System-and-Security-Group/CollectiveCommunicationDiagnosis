#ifndef RDMA_CC_CLIENT_H
#define RDMA_CC_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/rdma.h"
#include <vector>

namespace ns3 {

class Socket;
class Packet;

/**
* \ingroup rdmaccclientserver
* \class rdmaCC
* \brief A RDMA client for collective communication.
*
*/
class RdmaCC : public Application
{
public:
  static TypeId GetTypeId (void);

  RdmaCC ();

  virtual ~RdmaCC ();

  void SetRank(uint16_t rank);
  void SetLocal (Ipv4Address ip, uint16_t port);
  void AddCommGroup (Ipv4Address ip, uint16_t port);
  void SetAlg(uint8_t alg);

  Ipv4Address GetIP();
  uint16_t GetPort();

  uint32_t GetSendStep();
  uint32_t GetRecvStep();
  int GetPrevRank();
  
  void SetChunk(uint64_t size, uint16_t num);
  void SetControl(uint32_t win, uint64_t baseRtt, uint16_t pg);
  void SetIP2APPCb(Callback<Ptr<RdmaCC>, Ipv4Address> cb);

  void Send(uint16_t distRank);
  // void Recv(uint16_t srcRank);
  void FinishRecvStep();

  void Scatter(uint16_t rootRank);
  void Gather(uint16_t rootRank);
  void AlltoAll();

  void Allreduce();
  void Broadcast(uint16_t rootRank);
  void Reduce(uint16_t rootRank);
  void Allgather();
  void ReduceScatter();


protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendChunkFinish();
  void SendStep();

  Callback<Ptr<RdmaCC>, Ipv4Address> m_ip2app;

  uint8_t m_op; // 0 send, 1 scatter, 2 gather, 3 alltoall, 4 allreduce, 5 broadcast, 6 reduce, 7 allgather, 8 reduceScatter

  uint16_t m_rank;  //rank  index of comm
  std::vector<int> m_prevRank;
  std::vector<int> m_nextRank;
  std::vector<uint64_t> m_packetSize;

  uint8_t m_alg;  // 0 no alg, 1 ring, 2 halving and doubling, 3 double binary tree
  uint32_t m_recvStep;
  uint32_t m_sendStep;
  uint8_t m_stepStu;  // 0 running, 1 finish

  Ipv4Address m_ip;
  uint16_t m_port;

  struct Comm
  {
    Ipv4Address ip;
    uint16_t port;
  };
  std::vector<Comm> m_comm;

  uint64_t m_chunkSize; // m_chunkSize = total data / m_comm.size
  uint16_t m_chunkNum;
  
  uint32_t m_win; // bound of on-the-fly packets
  uint64_t m_baseRtt; // base Rtt
  uint16_t m_pg; // priority group

  // CC NPA 
  void SendNotification();
  void SetAgent();
  uint32_t m_agent_times; // times of detecting per step
  
};

} // namespace ns3

#endif /* RDMA_CC_CLIENT_H */
