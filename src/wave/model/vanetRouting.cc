 
#include "vanetRouting.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include <stdlib.h>

namespace ns3 {

using namespace std;

#define max(a,b)        ( (a) > (b) ? (a) : (b) )
#define min(a,b)        ( (a) < (b) ? (a) : (b) )
#define abs(a)	        ( (a) >= (0) ? (a) : (-a) )

NS_LOG_COMPONENT("VanetRoutingProtocol");
#include ".file"

struct vanet_head vanetRouting::vanethead_ = { 0 };
int	vanetRouting::cluster_formed;
double	vanetRouting::cluster_time;

vanetRouting::myList vanetRouting::list_;//??
vanetRouting::myList	vanetRouting::Jlist_;
vanetRouting::myList	vanetRouting::Mlist_;
vanetRouting::myList	vanetRouting::Dlist_;

int	vanetRouting::uid;

int	vanetRouting::send_count;
int	vanetRouting::recv_count;
int	vanetRouting::bytes;
double	vanetRouting::txDelay[10000];
double	vanetRouting::rxTime[10000];
int	vanetRouting::routingOverhead;
double	vanetRouting::pinit;
double	vanetRouting::pend;

int	vanetRouting::fp;
int	vanetRouting::fn;
int	vanetRouting::tp;
int	vanetRouting::tn;

double	vanetRouting::launchTime;
double	vanetRouting::detectionTime;
double	vanetRouting::detectionCount;

vanetRouting::vanetRouting() : beaconTimer(Timer::CANCEL_ON_DESTROY), minitTimer(Timer::CANCEL_ON_DESTROY), movementTimer(Timer::CANCEL_ON_DESTROY), ptimer(Timer::CANCEL_ON_DESTROY), priorityTimer(Timer::CANCEL_ON_DESTROY), clusteringTimer(Timer::CANCEL_ON_DESTROY), transitionTimer(Timer::CANCEL_ON_DESTROY), dataTimer(Timer::CANCEL_ON_DESTROY), rtimer(Timer::CANCEL_ON_DESTROY)
{
	dpktcount = 0;
	index = -1;
	node_ = NULL;
	insert(&(vanetRouting::vanethead_));

	txCount = 0;
	rxCount = 0;

	alpha = 0;
	beta = 0;
	gamma = 0;

	cluster_run = 0;
	status = INITIAL;
	cluster_time = -1;
	cluster_formed = 0;
	TryConnectionCH = 0;
	uid = 0;
	send_count = 0;
	recv_count = 0;
	routingOverhead = 0;

	pinit = -1;
	pend = -1;

	fp = fn = tp = tn = 1;
	sybil = 0;
	launchTime = 0;
	detectionTime = 0;
	detectionCount = 0;
}

void	vanetRouting::initializeBeaconTimer()
{
	if(sybil == 1)
	{
		Mlist_.add(index);
	}
	beaconTimer.SetFunction (&vanetRouting::beaconTransmission, this);
	double	startTime = 0;
	startTime = Random(0,1) * 1000;
	beaconTimer.Schedule (MilliSeconds (startTime));
}

void	vanetRouting::recv(Ptr<Packet> p)
{
	//cout<<" vanetRouting Recv ";
	rxCount++;
	if(p->vanetPacket == VANET)
	{
		if(p->vanetType == BEACON)
		{
			recvBeacon(p);
		}
		else if(p->vanetType == PRIORITY)
		{
			recvPriority(p);
		}
		else if(p->vanetType == CLUSTER)
		{
			recvClusterAnnouncement(p);
		}
		else if(p->vanetType == JOIN_REQ)
		{
			recvJoinReq(p);
		}
		else if(p->vanetType == JOIN_RSP)
		{
			recvJoinRsp(p);
		}
		else if(p->vanetType == MERGING)
		{
			recvClusterMerging(p);
		}
		else if(p->vanetType == LEAVE)
		{
			recvLeaveMessage(p);
		}
		else if(p->vanetType == SYBIL_MSG)
		{
			recvSybilDetectionMsg(p);
		}
	}
	else
	{
		handleRoutingData(p);
	}
}

void	vanetRouting::recvBeacon(Ptr<Packet> p)
{
//	cout<<"Vehicle "<<index<<" recv Beacon From "<<p->SenderID<<" at "<<CURRENT_TIME<<endl;
	nbList_.add(p->SenderID,p->X,p->Y,p->speed);
	rss_table_.add(p->SenderID,p->RSSI);

	if(node_->proposed == 1)
	{
		sybilDetection(p);
	}

	updateNBRList();
}

void	vanetRouting::updateNBRList()
{
	double	x1 = get_x();
	double	y1 = get_y();

	for(int i=0;i<nbList_.count;i++)
	{
		nsaddr_t nid = nbList_.nodeid[i];
		double	x2 = get_x(nid);
		double	y2 = get_y(nid);
		double	s = get_speed(nid);

		double	dist = distance(x1,y1,x2,y2);

		if(dist > node_->coverage)
		{
			nbList_.remove(nid);
			i--;
		}
		else
		{
			nbList_.add(nid,x2,y2,s);
		}
	}
}

void	vanetRouting::beaconTransmission()
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = BEACON;

	packet->X	=	get_x();
	packet->Y	=	get_y();
	packet->speed	=	get_speed();
	packet->pSize	=	HDR_LEN;

	packet->SenderID = index;
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
	ScheduleNextBeacon();

	if(sybil == 1)
	{
		if(launchTime == 0)
		{
			launchTime = CURRENT_TIME;
		}
		generateSybilMessage();
	}

	if(cluster_run == 0)
	{
		cluster_run = 1;
		priorityTimer.SetFunction (&vanetRouting::priority_estimation, this);
		double	nextInterval = (1 * 1000);
		priorityTimer.Schedule (MilliSeconds (nextInterval));
	}
}

void	vanetRouting::generateSybilMessage()
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = BEACON;

	packet->X	=	get_x();
	packet->Y	=	get_y();
	packet->speed	=	get_speed();
	packet->pSize	=	HDR_LEN;

	packet->SenderID =	SybilID();
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;
	//cout<<"broadcasting cybil"<<endl;
	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
}

nsaddr_t	vanetRouting::SybilID()
{
	int nNodes = NodeList::GetNNodes();
	nsaddr_t rnd = (nsaddr_t)Random(0,nNodes);
	while(rnd == index)
	{
		rnd = (nsaddr_t)Random(0,nNodes);
	}

	return	rnd;
}

void	vanetRouting::ScheduleNextBeacon()
{
	beaconTimer.SetFunction (&vanetRouting::beaconTransmission, this);
	double	nextInterval = ((BEACON_INTERVAL + Random(0,1)) * 1000);
	beaconTimer.Schedule (MilliSeconds (nextInterval));
}

double   vanetRouting::Random(double a,double b)
{
    if (b == a) return a;
    else if (a < b) return (b - a) * ((float)rand() / RAND_MAX) + a;
    return 0;
}

double   vanetRouting::Random(double b)
{
    return Random(0,b);
}

double	vanetRouting::get_x(nsaddr_t n)
{
	Ptr<Node> node = NodeList::GetNode (n);
	return get_x(node);
}

double	vanetRouting::get_y(nsaddr_t n)
{
	Ptr<Node> node = NodeList::GetNode (n);
	return get_y(node);
}

double	vanetRouting::get_x()
{
	Ptr<Node> node = NodeList::GetNode (index);
	return get_x(node);
}

double	vanetRouting::get_y()
{
	Ptr<Node> node = NodeList::GetNode (index);
	return get_y(node);
}

double	vanetRouting::get_speed(nsaddr_t n)
{
	Ptr<Node> node = NodeList::GetNode (n);
	return node->speed;
}

double	vanetRouting::get_speed()
{
	Ptr<Node> node = NodeList::GetNode (index);
	return node->speed;
}

double	vanetRouting::get_x(Ptr<Node> mn)
{
      Ptr<Node> object = mn;
      Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
      NS_ASSERT (position != 0);
      Vector pos = position->GetPosition ();
      return pos.x;
}

double	vanetRouting::get_y(Ptr<Node> mn)
{
      Ptr<Node> object = mn;
      Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
      NS_ASSERT (position != 0);
      Vector pos = position->GetPosition ();
      return pos.y;
}

Ptr<Node>	vanetRouting::GetNodeWithAddress (Ipv4Address ipv4Address)
{
  nsaddr_t nNodes = NodeList::GetNNodes ();
  for (nsaddr_t i = 0; i < nNodes; ++i)
    {
      Ptr<Node> node = NodeList::GetNode (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      nsaddr_t ifIndex = ipv4->GetInterfaceForAddress (ipv4Address);
      if (ifIndex != -1)
        {
          return node;
        }
    }
  return 0;
}

Ipv4Address	vanetRouting::GetNodeAddress (nsaddr_t nid)
{
      Ptr<Node> node = NodeList::GetNode (nid);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ipv4Address ifIndex = ipv4->GetAddress (1, 0).GetLocal ();
      return	ifIndex;
}

double	vanetRouting::distance(double x1,double y1,double x2,double y2)
{
	return	sqrt(pow(x1-x2,2)+pow(y1-y2,2));
}

void	vanetRouting::SchedulePacket(vanetRouting *vanet,Ptr<Packet> packet,double delay)
{
	vanet->BufferPacket(packet,delay);
}

void	vanetRouting::handlePacket()
{
	Ptr<Packet> packet = ptimer.p[0];
	recv(packet);

	if(ptimer.pcount > 1)
	{
		for(int i=0;i<(ptimer.pcount-1);i++)
		{
			ptimer.p[i] = ptimer.p[i+1];
			ptimer.delay[i] = ptimer.delay[i+1];
		}

		ptimer.pcount--;
		ptimer.sort();

		double	nxtPeriod = ptimer.delay[0];
		double	delay = (nxtPeriod - CURRENT_TIME) * 1000;

		if(delay >= 0)
		{
			ptimer.Cancel ();
			ptimer.Schedule (MilliSeconds (delay));
		}
		else
		{
			cout<<"Bug"<<endl;
		}
	}
	else
	{
		ptimer.pcount = 0;
		ptimer.run = 0;
		ptimer.Cancel ();
	}
}

void	vanetRouting::BufferPacket(Ptr<Packet> packet,double d)
{
	if(ptimer.run == 0)
	{
		ptimer.run = 1;
		double	nxtTime = CURRENT_TIME + d;

		ptimer.p[ptimer.pcount] = packet;
		ptimer.delay[ptimer.pcount] = nxtTime;
		ptimer.pcount++;
		ptimer.sort();

		double	nxtPeriod = ptimer.delay[0];
		double	delay = (nxtPeriod - CURRENT_TIME) * 1000;

		if(delay >= 0)
		{
			ptimer.Cancel ();
			ptimer.SetFunction (&vanetRouting::handlePacket, this);
			ptimer.Schedule (MilliSeconds (delay));
		}
		else
		{
			cout<<"Bug"<<endl;
		}
	}
	else
	{
		ptimer.run = 1;
		double	nxtTime = CURRENT_TIME + d;

		ptimer.p[ptimer.pcount] = packet;
		ptimer.delay[ptimer.pcount] = nxtTime;
		ptimer.pcount++;
		ptimer.sort();

		ptimer.Cancel ();

		double	nxtPeriod = ptimer.delay[0];
		double	delay = (nxtPeriod - CURRENT_TIME) * 1000;

		if(delay >= 0)
		{
			ptimer.SetFunction (&vanetRouting::handlePacket, this);
			ptimer.Schedule (MilliSeconds (delay));
		}
		else
		{
			cout<<"Bug"<<endl;
		}
	}
}

void	vanetRouting::priority_estimation()
{
	performPriority_estimation();

	priorityTimer.SetFunction (&vanetRouting::priority_estimation, this);
	double	nextInterval = (CLUSTER_INTERVAL * 1000);
	priorityTimer.Cancel();
	priorityTimer.Schedule (MilliSeconds (nextInterval));
}

void	vanetRouting::performPriority_estimation()
{
	if(nbList_.count == 0)
		return	;

	double L = node_->L;
	double fc = nbList_.count;
	double D_neigh = getSameLaneNodes();

	double N_follow = D_neigh + fc;
	double R = node_->coverage;

	double d_ =  2 * R / D_neigh;
	double t_del = tdel;
	double v = (d_ - L) / t_del;

	double df = 1./(txCount / CURRENT_TIME);
	double dr = 1./(rxCount / CURRENT_TIME);

	double ETX = 1./(df*dr);
	double LLT = 0;

	double mx = 0;
	double my = 0;

	double nx = 0;
	double ny = 0;

	for(int i=0;i<nbList_.count;i++)
	{
		if(i == 0)
		{
			mx = nbList_.x[i];
			my = nbList_.y[i];

			nx = nbList_.x[i];
			ny = nbList_.y[i];
		}
		else
		{
			mx = max(mx,nbList_.x[i]);
			my = max(my,nbList_.y[i]);

			nx = min(nx,nbList_.x[i]);
			ny = min(ny,nbList_.y[i]);
		}
	}

	double	del_px = mx - nx;
	double	del_py = my - ny;

	double	del_vx = del_px * v;
	double	del_vy = del_py * v;

	double	d_sum = 0;
	for(int i=0;i<nbList_.count;i++)
	{
		double dist = distance(get_x(),get_y(),nbList_.x[i],nbList_.y[i]);
		d_sum += dist;
	}

	double	d = d_sum / nbList_.count;
	double	d2 = d * d;

	double	del2_vx = del_vx * del_vx;
	double	del2_vy = del_vy * del_vy;

	LLT = sqrt((abs((d2 * (del2_vx+del2_vy) - pow((del_px*del_vy-del_py*del_vx),2))))) / (del2_vx+del2_vy) - (del_px*del_vx-del_py*del_vy)/(del2_vx+del2_vy);

	double PRI = alpha * 1./N_follow + beta * ETX + gamma * LLT;
	node_->priority = PRI;
	if(PRI > 0)
	{
		cout<<"Vehicle: "<<index<<" Priority: "<<PRI<<endl;
		AnnouncePriority(PRI);
	}
}

void	vanetRouting::AnnouncePriority(double priority)
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = PRIORITY;

	packet->priority = priority;

	packet->SenderID = index;
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = index;
	packet->pSize	=	HDR_LEN;


	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;

	clusteringTimer.SetFunction (&vanetRouting::clustering, this);
	clusteringTimer.Schedule (MilliSeconds (1000));
}

void	vanetRouting::recvPriority(Ptr<Packet> p)
{
	double	prio = p->priority;
	int pos = nbList_.check(p->SenderID);
	if(pos != -1)
	{
		nbList_.p[pos] = prio;
	}
}

double	vanetRouting::getSameLaneNodes()
{
	int	nbc = 0;
	double	y1 = get_y();

	for(int i=0;i<nbList_.count;i++)
	{
		if(nbList_.y[i] == y1)
			nbc++;
	}

	return	nbc;
}

void	vanetRouting::clusterHeadSelection()
{
	cout<<"Clustering process at "<<CURRENT_TIME<<endl;

	int	nNodes = NodeList::GetNNodes();

	for(int i=0;i<nNodes;i++)
	{
		infoTable_.add(i,get_x(i),get_y(i),get_speed(i),get_priority(i));
	}

	list_.count = 0;
	Jlist_.count = 0;
	nodeList tmpList_ = infoTable_;

	while(tmpList_.count > 0)
	{
		nsaddr_t nid = -1;
		double	max_prob = -1;
		for(int i=0;i<tmpList_.count;i++)
		{
			int pos = infoTable_.check(tmpList_.nodeid[i]);
			if(pos != -1)
			{
				if(i == 0 || infoTable_.p[i] > max_prob)
				{
					max_prob = infoTable_.p[i];
					nid = tmpList_.nodeid[i];
				}
			}
		}

		nsaddr_t list[tmpList_.count];
		int	lcount = 0;

		int	p1 = tmpList_.check(nid);
		for(int i=0;i<tmpList_.count;i++)
		{
			double dist = distance(tmpList_.x[i],tmpList_.y[i],tmpList_.x[p1],tmpList_.y[p1]);
			if(dist < node_->coverage)
			{
				list[lcount++] = tmpList_.nodeid[i];
			}
		}

		for(int i=0;i<lcount;i++)
		{
			tmpList_.remove(list[i]);
		}
		list_.add(nid);
	}

	if(list_.count > 0)
	{
		cout<<"Formed Clusters: "<<list_.count<<"::clusterHeads: ";
		for(int i=0;i<list_.count;i++)
		{
			cout<<list_.nodeid[i]<<" ";
		}
		cout<<endl;
	}
}

void	vanetRouting::clustering()
{
	TryConnectionCH = 0;
	if(cluster_time == -1)
	{
		clusterHeadSelection();
		clusterTransition();
		cluster_formed = 1;
		cluster_time = CURRENT_TIME + CLUSTER_INTERVAL;
	}
	else
	{
		if(cluster_formed == 0)
		{
			clusterHeadSelection();
			clusterTransition();
			cluster_formed = 1;
			cluster_time = CURRENT_TIME + CLUSTER_INTERVAL;
		}
		else
		{
			if(CURRENT_TIME > cluster_time)
			{
				clusterHeadSelection();
				clusterTransition();
				cluster_formed = 1;
				cluster_time = CURRENT_TIME + CLUSTER_INTERVAL;
			}
			else
			{
				nsaddr_t nid = node_->GetId();
				if(Jlist_.check(nid) == -1)
				{
					Jlist_.add(nid);
					clusterTransition();
				}
			}
		}
	}
}

double	vanetRouting::get_priority(nsaddr_t n)
{
	int pos = nbList_.check(n);
	if(pos != -1)
	{
		if(nbList_.p[pos] > 0)
			return	nbList_.p[pos];
	}

	Ptr<Node> node = NodeList::GetNode (n);
	return node->priority;
}

void	vanetRouting::clusterTransition()
{
	status = INITIAL;
	char desc[CLEN];
	sprintf(desc, "%d", node_->GetId());
	anim->UpdateNodeColor(node_,0,255,0);
	if(list_.check(index) != -1)
	{
		anim->UpdateNodeColor(node_,255,255,0);
		sprintf(desc,"H");
		status = HEAD;
		sendCHAnnouncement();		
	}
	else
	{
		anim->UpdateNodeColor(node_,0,0,255);
		TryConnectionCH = 0;
	}
	anim->UpdateNodeDescription(node_,desc);
}

void	vanetRouting::sendCHAnnouncement()
{
	txCount++;

	cout<<"Vehicle "<<index<<" Send CH Announcement"<<endl;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = CLUSTER;
	memberlist_.count = 0;

	packet->X	=	get_x();
	packet->Y	=	get_y();
	packet->speed	=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize	=	HDR_LEN;


	packet->SenderID = index;
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;

	transitionTimer.SetFunction (&vanetRouting::StateChecking, this);
	double	nextInterval = (1 * 1000);
	transitionTimer.Cancel();
	transitionTimer.Schedule (MilliSeconds (nextInterval));
}

void	vanetRouting::StateChecking()
{
	if(TryConnectionCH == 0)
	{
		if(status == HEAD)
		{
			if(memberlist_.count == 0)
			{
				TryConnectionCH = 1;
				ConnectTOCM();
			}
		}
	}
	transitionTimer.Cancel();
}

void	vanetRouting::recvClusterAnnouncement(Ptr<Packet> p)
{
	nsaddr_t cid = p->SenderID;
	if(status == INITIAL)
	{
		if(TryConnectionCH == 0)
		{
			double	d1 = getDistance(cid);

			double	min_dist = 99999999;
			nsaddr_t tmpCH = -1;
			for(int i=0;i<list_.count;i++)
			{
				nsaddr_t nid = list_.nodeid[i];
				double	d2 = getDistance(nid);

				if(d2 < min_dist)
				{
					min_dist = d2;
					tmpCH = nid;
				}
			}	

			if(tmpCH == cid && d1 <= min_dist)
			{
				headPriority = p->priority;
				ConnectToCH(cid);
			}
		}
	}
	else	//	Check for Intra Cluster Interference
	{
		if(status == HEAD)
		{
			if(Check_for_IntraClusterInterference(cid) == 1)
			{
				if(node_->priority < p->priority)
				{
					PerformClusterMerging(cid);
				}
			}
			else
			{
				if(TryConnectionCH == 1)
				{
					if(infoTable_.count == 0 && memberlist_.count == 0)
					{
						status = ISO_CH;
					}
					else
					{
						JoinOtherCluster();
					}
				}
			}
		}
		else
		{
			if(status == MEMBER && headId != cid)
			{
				if(headPriority < p->priority)
				{
					double	d1 = getDistance(cid);

					double	min_dist = 99999999;
					nsaddr_t tmpCH = -1;
					for(int i=0;i<list_.count;i++)
					{
						nsaddr_t nid = list_.nodeid[i];
						double	d2 = getDistance(nid);

						if(d2 < min_dist)
						{
							min_dist = d2;
							tmpCH = nid;
						}
					}	

					if(tmpCH == cid && d1 <= min_dist)
					{
						headPriority = p->priority;
						SendLeave(headId);
						ConnectToCH(cid);
					}
				}
			}
		}
	}
}

double	vanetRouting::getDistance(nsaddr_t nid)
{
	double	x1 = get_x();
	double	y1 = get_y();

	double	x2 = get_x(nid);
	double	y2 = get_y(nid);

	double	dist = distance(x1,y1,x2,y2);
	return	dist;
}

void	vanetRouting::ConnectToCH(nsaddr_t nid)
{
	status = MEMBER;
	JoinCluster(nid);
}

void	vanetRouting::JoinCluster(nsaddr_t nid)
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = JOIN_REQ;

	cout<<"Vehicle "<<index<<" Send Join Req: "<<nid<<" at "<<CURRENT_TIME<<endl;
	
	packet->X		=	get_x();
	packet->Y		=	get_y();
	packet->speed		=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize		=	HDR_LEN;


	packet->SenderID = index;
	packet->ReceiverID = nid;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = nid;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
}

void	vanetRouting::recvJoinReq(Ptr<Packet> p)
{
	if(memberlist_.count < MEM_LIMIT)
	{
		memberlist_.add(p->SenderID);
		sendJoinResponse(p->SenderID);
		TryConnectionCH = 0;
	}
}

void	vanetRouting::sendJoinResponse(nsaddr_t nid)
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = JOIN_RSP;

	cout<<"Vehicle "<<index<<" Send Join Rsp to: "<<nid<<" at "<<CURRENT_TIME<<endl;
	
	packet->X		=	get_x();
	packet->Y		=	get_y();
	packet->speed		=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize		=	HDR_LEN;


	packet->SenderID = index;
	packet->ReceiverID = nid;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = nid;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
}

void	vanetRouting::recvJoinRsp(Ptr<Packet> p)
{
	if(p->ReceiverID == index)
	{
		status = MEMBER;
		headId = p->SenderID;
		headPriority = p->priority;
		cout<<"Vehicle "<<index<<" become Member of "<<headId<<endl;
	}
}

int	vanetRouting::Check_for_IntraClusterInterference(nsaddr_t nid)
{
	if(status == HEAD)
	{
		if(nbList_.check(nid) != -1)
		{
			return	1;
		}
	}
	return	0;
}

void	vanetRouting::PerformClusterMerging(nsaddr_t nid)
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = MERGING;

	cout<<"Vehicle "<<index<<" Send Cluster Merging to MEM: at "<<CURRENT_TIME<<endl;
	
	packet->X		=	get_x();
	packet->Y		=	get_y();
	packet->speed		=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize		=	HDR_LEN;

	packet->SenderID = index;
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = nid;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
}

void	vanetRouting::recvClusterMerging(Ptr<Packet> p)
{
	if(status == MEMBER && headId == p->SenderID)
	{
		if(nbList_.check(p->previousHop) != -1)
		{
			JoinCluster(p->previousHop);
		}
		else
		{
			status = INITIAL;
		}
	}
}

void	vanetRouting::JoinOtherCluster()
{
	double	min_dist = 99999999;
	nsaddr_t tmpCH = -1;
	for(int i=0;i<list_.count;i++)
	{
		nsaddr_t nid = list_.nodeid[i];
		double	d2 = getDistance(nid);

		if(d2 < min_dist)
		{
			min_dist = d2;
			tmpCH = nid;
		}
	}	

	if(tmpCH != -1)
	{
		ConnectToCH(tmpCH);
	}
}

void	vanetRouting::ConnectTOCM()
{
	TryConnectionCH = 1;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = CLUSTER;
	memberlist_.count = 0;

	packet->X	=	get_x();
	packet->Y	=	get_y();
	packet->speed	=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize	=	HDR_LEN;

	packet->SenderID = index;
	packet->ReceiverID = IP_BROADCAST;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = -1;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;

	transitionTimer.SetFunction (&vanetRouting::StateChecking, this);
	double	nextInterval = (1 * 1000);
	transitionTimer.Cancel();
	transitionTimer.Schedule (MilliSeconds (nextInterval));
}

void	vanetRouting::SendLeave(nsaddr_t nid)
{
	txCount++;

	Ptr<Packet> packet = Create<Packet> ();
	packet->vanetPacket = VANET;
	packet->vanetType = LEAVE;

	cout<<"Vehicle "<<index<<" Send Leave msg: "<<nid<<" at "<<CURRENT_TIME<<endl;
	
	packet->X		=	get_x();
	packet->Y		=	get_y();
	packet->speed		=	get_speed();
	packet->priority	=	node_->priority;
	packet->pSize		=	HDR_LEN;

	packet->SenderID = index;
	packet->ReceiverID = nid;

	packet->previousHop = index;

	packet->TxPort = vanetPort;
	packet->RxPort = vanetPort;

	packet->nbBroadcast = 1;
	packet->Broadcast = 0;
	packet->nextHop = nid;

	SchedulePacketWithoutContext(packet,linkLayerTarget);
	routingOverhead++;
}

void	vanetRouting::recvLeaveMessage(Ptr<Packet> p)
{
	if(status == HEAD && p->ReceiverID == index)
	{
		memberlist_.remove(p->SenderID);
	}
}

void	vanetRouting::initializeDataTransmission(nsaddr_t dst,Ptr<Packet> p,int packetSize,double pkt_interval,double startTime,double stopTime)
{
	dataTimer.dataPacket = p;
	dataTimer.interval = pkt_interval;
	dataTimer.packetSize = packetSize;
	dataTimer.startTime = startTime;
	dataTimer.stopTime = stopTime;
	dataTimer.dst = dst;
	dataTimer.SetFunction (&vanetRouting::dataTransmission, this);
	dataTimer.init = 0;

	dataTimer.startTime *= 1000;
	dataTimer.Schedule (MilliSeconds (startTime));
}

void	vanetRouting::dataTransmission()
{
	send_count++;
	if(pinit == -1)
	{
		pinit = CURRENT_TIME;
	}

	pend = CURRENT_TIME;
	Ptr<Packet> p = dataTimer.dataPacket->Copy();

	p->vanetPacket = DATA_PACKET;
	p->pSize = dataTimer.packetSize + HDR_LEN;

	p->SenderID = index;
	p->ReceiverID = dataTimer.dst;
	p->num_forwards = 0;

	p->previousHop = index;

	p->TxPort = dataPort;
	p->RxPort = dataPort;
	p->uid    = ++uid;
	p->txInitTime	=	CURRENT_TIME;

	if(dataTimer.init == 0)
	{
		cout<<"Data Packet Transmission is Initialized at "<<CURRENT_TIME<<endl;
		dataTimer.init = 1;
	}

	p->nextHop = getClusterBasedForwarder(p,p->SenderID,dataTimer.dst);
	if(p->nextHop != -1)
	{
		cout<<"Node: "<<index<<" forward data to "<<p->nextHop<<" dst "<<p->ReceiverID<<" at "<<CURRENT_TIME<<endl;
		p->nbBroadcast = 1;
		p->Broadcast = 0;
		p->num_forwards++;
		SchedulePacketWithoutContext(p,linkLayerTarget);
	}
	else
	{
		cout<<"Link Unavailble to reach destimation "<<p->ReceiverID<<" from node: "<<index<<" at "<<CURRENT_TIME<<endl;
		nsaddr_t forwarder = check_for_multihop_cluster_forwarding(p);
		if(forwarder == -1)
			EnquePacket(p);
		else
		{
cout<<"Forwarding data to multihop cluster forwarding "<<forwarder<<" to reach destimation "<<p->ReceiverID<<" from node: "<<index<<" at "<<CURRENT_TIME<<endl;
			p->nextHop = forwarder;
			p->nbBroadcast = 0;
			p->Broadcast = 1;
			p->num_forwards++;
			SchedulePacketWithoutContext(p,linkLayerTarget);
		}
	}

	double	nxt_clock_time = CURRENT_TIME + dataTimer.interval;
	if(nxt_clock_time <= dataTimer.stopTime)
	{
		dataTimer.SetFunction (&vanetRouting::dataTransmission, this);
		double	scheduleTime = dataTimer.interval * 1000;
		dataTimer.Schedule (MilliSeconds (scheduleTime));
	}
}

nsaddr_t	vanetRouting::check_for_multihop_cluster_forwarding(Ptr<Packet> p)
{
	nsaddr_t src = p->SenderID;
	nsaddr_t dst = p->ReceiverID;

	if(src == index && p->num_forwards == 0)
	{
		if(status == MEMBER && is_nbr(headId) == 1)
		{
			if(available_in_forwarding_route(dst,headId) == 1)
			{
				cout<<"Packet forwarding to Clusterhead "<<headId<<" from Src "<<index<<" at "<<CURRENT_TIME<<endl;
				return	headId;
			}
		}
	}

	if(status == MEMBER && is_nbr(headId) == 1)
	{
		if(available_in_forwarding_route(dst,headId) == 1)
		{
			cout<<"Packet forwarding to Clusterhead "<<headId<<" from IN "<<index<<" at "<<CURRENT_TIME<<endl;
			return	headId;
		}
	}

	double	x1 = get_x();
	double	y1 = get_y();

	double	dst_x = get_x(dst);
	double	dst_y = get_y(dst);

	double	dist = distance(x1,y1,dst_x,dst_y);
	if(dist < node_->coverage)
	{
		return	dst;
	}

	nsaddr_t opt_for = -1;
	double	opt_dist = 99999999;
	nsaddr_t nNodes = NodeList::GetNNodes ();
	for (nsaddr_t i = 0; i < nNodes; ++i)
	{
		if(i != index)
		{
			double	x2 = get_x(i);
			double	y2 = get_y(i);

			if(node_->proposed == 1 && attackerList.check(i) != -1)
				continue;

			double	nb_dist = distance(x1,y1,x2,y2);
			double	for_dist = distance(x2,y2,dst_x,dst_y);
			if(for_dist < dist)
			{
				double	d_sum = nb_dist + for_dist;
				if(d_sum < opt_dist)
				{
					opt_dist = d_sum;
					opt_for = i;
				}
				else if(d_sum == opt_dist)
				{
					if(get_dist(i) > get_dist(opt_for))
					{
						opt_dist = d_sum;
						opt_for = i;
					}
				}
			}
		}
	}

	return	opt_for;
}


nsaddr_t	vanetRouting::getClusterBasedForwarder(Ptr<Packet> p,nsaddr_t src,nsaddr_t dst)
{
	if(src == index && p->num_forwards == 0)
	{
		if(status == MEMBER && is_nbr(headId) == 1)
		{
			if(available_in_forwarding_route(dst,headId) == 1)
			{
				cout<<"Packet forwarding to Clusterhead "<<headId<<" from Src "<<index<<" at "<<CURRENT_TIME<<endl;
				return	headId;
			}
		}
	}

	if(status == MEMBER && is_nbr(headId) == 1)
	{
		if(available_in_forwarding_route(dst,headId) == 1)
		{
			cout<<"Packet forwarding to Clusterhead "<<headId<<" from IN "<<index<<" at "<<CURRENT_TIME<<endl;
			return	headId;
		}
	}

	double	x1 = get_x();
	double	y1 = get_y();

	double	dst_x = get_x(dst);
	double	dst_y = get_y(dst);

	double	dist = distance(x1,y1,dst_x,dst_y);
	if(dist < node_->coverage)
	{
		return	dst;
	}

	nsaddr_t opt_for = -1;
	double	opt_dist = 99999999;
	nsaddr_t nNodes = NodeList::GetNNodes ();
	for (nsaddr_t i = 0; i < nNodes; ++i)
	{
		if(i != index)
		{
			double	x2 = get_x(i);
			double	y2 = get_y(i);

			double	nb_dist = distance(x1,y1,x2,y2);
			if(nb_dist < node_->coverage)
			{
				if(node_->proposed == 1 && attackerList.check(i) != -1)
					continue;

				double	for_dist = distance(x2,y2,dst_x,dst_y);
				if(for_dist < dist)
				{
					double	d_sum = nb_dist + for_dist;
					if(d_sum < opt_dist)
					{
						opt_dist = d_sum;
						opt_for = i;
					}
					else if(d_sum == opt_dist)
					{
						if(get_dist(i) > get_dist(opt_for))
						{
							opt_dist = d_sum;
							opt_for = i;
						}
					}
				}
			}
		}
	}

	return	opt_for;
}

nsaddr_t	vanetRouting::available_in_forwarding_route(nsaddr_t dst,nsaddr_t head)
{
	double	x1 = get_x();
	double	y1 = get_y();

	double	dst_x = get_x(dst);
	double	dst_y = get_y(dst);

	double	dist = distance(x1,y1,dst_x,dst_y);

	if(dist < node_->coverage)
	{
		return	0;
	}
	else
	{
		double	for_x = get_x(head);
		double	for_y = get_y(head);

		double	dist_for_dst = distance(for_x,for_y,dst_x,dst_y);

		if(dist_for_dst < dist)
		{
			return	1;
		}
	}

	return	0;
}

double	vanetRouting::get_dist(nsaddr_t nbr)
{
	double	x1 = get_x();
	double	y1 = get_y();

	double	dst_x = get_x(nbr);
	double	dst_y = get_y(nbr);

	double	dist = distance(x1,y1,dst_x,dst_y);
	return	dist;
}

int	vanetRouting::is_nbr(nsaddr_t nbr)
{
	double	x1 = get_x();
	double	y1 = get_y();

	double	dst_x = get_x(nbr);
	double	dst_y = get_y(nbr);

	double	dist = distance(x1,y1,dst_x,dst_y);

	if(dist < node_->coverage)
		return	1;

	return	0;
}

void	vanetRouting::EnquePacket(Ptr<Packet> p)
{
	PacketList[dpktcount] = p;
	dpktcount++;
}

void	vanetRouting::handleRoutingData(Ptr<Packet> p)
{
	if(p->ReceiverID == index)
	{
		double	delay = CURRENT_TIME - p->txInitTime;
		bytes += p->pSize;
		txDelay[recv_count] = delay;
		rxTime[recv_count] = CURRENT_TIME;
		recv_count++;
		
		cout<<"Packet reached Destination: "<<p->ReceiverID<<" pid "<<p->uid<<endl;
		return;
	}

	p->previousHop = index;
	p->nextHop = getClusterBasedForwarder(p,p->SenderID,p->ReceiverID);

	if(p->nextHop != -1)
	{
		cout<<"Node: "<<index<<" forward data to "<<p->nextHop<<" dst "<<p->ReceiverID<<" at "<<CURRENT_TIME<<endl;
		p->nbBroadcast = 1;
		p->Broadcast = 0;
		p->num_forwards++;
		SchedulePacketWithoutContext(p,linkLayerTarget);
	}
	else
	{
		cout<<"Link Unavailble to reach destimation "<<p->ReceiverID<<" from node: "<<index<<" at "<<CURRENT_TIME<<endl;
		nsaddr_t forwarder = check_for_multihop_cluster_forwarding(p);
		if(forwarder == -1)
			EnquePacket(p);
		else
		{
cout<<"Forwarding data to multihop cluster forwarding "<<forwarder<<" to reach destimation "<<p->ReceiverID<<" from node: "<<index<<" at "<<CURRENT_TIME<<endl;
			p->nextHop = forwarder;
			p->nbBroadcast = 0;
			p->Broadcast = 1;
			p->num_forwards++;
			SchedulePacketWithoutContext(p,linkLayerTarget);
		}
	}
}

void	vanetRouting::sybilDetection(Ptr<Packet> p)
{
	nsaddr_t nid = p->SenderID;
	int	pos = rss_table_.check(nid);
	if(pos != -1 && rss_table_.count > 1)
	{
		double	r1 = rss_table_.rss[pos];
		double	r2 = rss_table_.rss2[pos];

		if(r1 > 0 && r2 > 0)
		{
			// Z score normalization
			double	sum = 0;
			int	count = 0;
			for(int i=0;i<rss_table_.count;i++)
			{
				if(rss_table_.rss[i] != rss_table_.rss2[i])
				{
					sum += rss_table_.rss[i];
					count++;
					sum += rss_table_.rss2[i];
					count++;
				}
				else
				{
					sum += rss_table_.rss[i];
					count++;
				}
			}

			double	avg = sum / count;
			double	diff_sum = 0;
			int	diff_count = 0;
			for(int i=0;i<rss_table_.count;i++)
			{
				if(rss_table_.rss[i] != rss_table_.rss2[i])
				{
					{
						double	diff = abs((rss_table_.rss[i]-avg));
						diff_sum += diff;
						diff_count++;
					}
					{
						double	diff = abs((rss_table_.rss2[i]-avg));
						diff_sum += diff;
						diff_count++;
					}
				}
				else
				{
					double	diff = abs((rss_table_.rss[i]-avg));
					diff_sum += diff;
					diff_count++;
				}
			}

			double	variance = diff_sum / diff_count;
			double	stddev = sqrt(variance);

			double	RSS0 = abs((r1 - avg)) / (3*stddev);
			double	RSS1 = abs((r2 - avg)) / (3*stddev);

			if(RSS0 == RSS1)
			{
			//	cout<<"RSS0 "<<RSS0<<" RSS1 "<<RSS1<<endl;

				double	dtw[diff_count];
				unsigned int	dt_count = 0;
				for(int i=0;i<rss_table_.count;i++)
				{
					if(rss_table_.rss[i] != rss_table_.rss2[i])
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						double	dt1 = (rss_table_.rss2[i]-avg) / (3*stddev);

						dtw[dt_count++] = dt0;
						dtw[dt_count++] = dt1;
					}
					else
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						dtw[dt_count++] = dt0;
					}
				}

				double	min_dtw = 0;
				double	max_dtw = 0;
				for(unsigned int i=0;i<dt_count;i++)
				{
					if(i == 0)
					{
						min_dtw = max_dtw = dtw[i];
					}
					else
					{
						min_dtw = min(min_dtw,dtw[i]);
						max_dtw = max(max_dtw,dtw[i]);
					}
				}

				if(min_dtw == max_dtw)
					return;

				double	d_dtw_i = RSS0;
				double	d_dtw_j = RSS1;

				double	DTW_i = (d_dtw_i - min_dtw) / (max_dtw-min_dtw);
				double	DTW_j = (d_dtw_j - min_dtw) / (max_dtw-min_dtw);

				double	t_test = abs((DTW_i - DTW_j)) / sqrt(variance);
				if(t_test >= 0)
				{
					for(int i=0;i<rss_table_.count;i++)
					{
						if(rss_table_.rss[i] != rss_table_.rss2[i])
						{
							double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
							double	dt1 = (rss_table_.rss2[i]-avg) / (3*stddev);
	
							double	DT0 = (dt0 - min_dtw) / (max_dtw-min_dtw);
							double	DT1 = (dt1 - min_dtw) / (max_dtw-min_dtw);
	
							if(DTW_i == DT0 || DTW_i == DT1 || DTW_j == DT0 || DTW_j == DT1)
							{
								if(nid != rss_table_.nodeid[i])
								{
									if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
									{
										Dlist_.add(p->previousHop);
										if(attackerList.check(p->previousHop) == -1)
										{
											cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
											attackerAnnouncement(p->previousHop);
										}
									}
								}
							}
							else
							{
								double	diff1 = abs((DTW_i-DT0));
								double	diff2 = abs((DTW_i-DT0));
	
								double	diff3 = abs((DTW_j-DT0));
								double	diff4 = abs((DTW_j-DT0));

								if(diff1 <= min_thresh || diff2 <= min_thresh || diff3 <= min_thresh || diff4 <= min_thresh)
								{
									if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
									{
										Dlist_.add(p->previousHop);
										if(attackerList.check(p->previousHop) == -1)
										{
											cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
											attackerAnnouncement(p->previousHop);
										}
									}
								}
								else if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
								{
									Dlist_.add(p->previousHop);
									if(attackerList.check(p->previousHop) == -1)
									{
										cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
										attackerAnnouncement(p->previousHop);
									}
								}
							}
						}
						else
						{
							double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
							double	DT0 = (dt0 - min_dtw) / (max_dtw-min_dtw);
	
							double	diff1 = abs((DTW_i-DT0));
							double	diff2 = abs((DTW_j-DT0));

							if(diff1 <= min_thresh || diff2 <= min_thresh)
							{
								if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
								{
									Dlist_.add(p->previousHop);
									if(attackerList.check(p->previousHop) == -1)
									{
										cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
										attackerAnnouncement(p->previousHop);
									}
								}
							}							
							else if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
							{
								Dlist_.add(p->previousHop);
								if(attackerList.check(p->previousHop) == -1)
								{
									cout<<"Identity "<<nid<<" is spoofed by "<<p->previousHop<<endl;
									attackerAnnouncement(p->previousHop);
								}
							}
						}
					}
				}
			}
			else
			{
				//cout<<"RSS0 "<<RSS0<<endl;

				double	dtw[diff_count];
				unsigned int	dt_count = 0;
				for(int i=0;i<rss_table_.count;i++)
				{
					if(rss_table_.rss[i] != rss_table_.rss2[i])
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						double	dt1 = (rss_table_.rss2[i]-avg) / (3*stddev);

						dtw[dt_count++] = dt0;
						dtw[dt_count++] = dt1;
					}
					else
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						dtw[dt_count++] = dt0;
					}
				}

				double	min_dtw = 0;
				double	max_dtw = 0;
				for(unsigned int i=0;i<dt_count;i++)
				{
					if(i == 0)
					{
						min_dtw = max_dtw = dtw[i];
					}
					else
					{
						min_dtw = min(min_dtw,dtw[i]);
						max_dtw = max(max_dtw,dtw[i]);
					}
				}

				if(min_dtw == max_dtw)
					return;

				double	d_dtw_i = RSS0;
				double	DTW_i = (d_dtw_i - min_dtw) / (max_dtw-min_dtw);

				for(int i=0;i<rss_table_.count;i++)
				{
					if(rss_table_.rss[i] != rss_table_.rss2[i])
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						double	dt1 = (rss_table_.rss2[i]-avg) / (3*stddev);

						double	DT0 = (dt0 - min_dtw) / (max_dtw-min_dtw);
						double	DT1 = (dt1 - min_dtw) / (max_dtw-min_dtw);

						if(DTW_i == DT0 || DTW_i == DT1)
						{
							if(nid != rss_table_.nodeid[i])
							{
								if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
								{
									Dlist_.add(p->previousHop);
									if(attackerList.check(p->previousHop) == -1)
									{
										cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
										attackerAnnouncement(p->previousHop);
									}
								}
							}							
							else if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
							{
								Dlist_.add(p->previousHop);
								if(attackerList.check(p->previousHop) == -1)
								{
									cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
									attackerAnnouncement(p->previousHop);
								}
							}
						}
						else
						{
							double	diff1 = abs((DTW_i-DT0));
							double	diff2 = abs((DTW_i-DT0));

							if(diff1 <= min_thresh || diff2 <= min_thresh)
							{
								if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
								{
									Dlist_.add(p->previousHop);
									if(attackerList.check(p->previousHop) == -1)
									{
										cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
										attackerAnnouncement(p->previousHop);
									}
								}
							}
							else if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
							{
								Dlist_.add(p->previousHop);
								if(attackerList.check(p->previousHop) == -1)
								{
									cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
									attackerAnnouncement(p->previousHop);
								}
							}
						}
					}
					else
					{
						double	dt0 = (rss_table_.rss[i]-avg) / (3*stddev);
						double	DT0 = (dt0 - min_dtw) / (max_dtw-min_dtw);

						double	diff1 = abs((DTW_i-DT0));
						if(diff1 <= min_thresh)
						{
							if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
							{
								Dlist_.add(p->previousHop);
								if(attackerList.check(p->previousHop) == -1)
								{
									cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
									attackerAnnouncement(p->previousHop);
								}
							}
						}
						else if(p->previousHop == rss_table_.nodeid[i] && p->previousHop != nid)
						{
							Dlist_.add(p->previousHop);
							if(attackerList.check(p->previousHop) == -1)
							{
								cout<<"Identity "<<nid<<" is spoofed by "<<rss_table_.nodeid[i]<<endl;
								attackerAnnouncement(p->previousHop);
							}
						}
					}
				}
			}
		}
	}
}

void	vanetRouting::attackerAnnouncement(nsaddr_t nid)
{
	if(attackerList.check(nid) == -1)
	{
		Dlist_.add(nid);

		detectionTime += (CURRENT_TIME - launchTime);
		detectionCount++;
		
		attackerList.add(nid);
		txCount++;

		Ptr<Packet> packet = Create<Packet> ();
		packet->vanetPacket = VANET;
		packet->vanetType = SYBIL_MSG;

		packet->X	=	get_x();
		packet->Y	=	get_y();
		packet->speed	=	get_speed();
		packet->pSize	=	HDR_LEN;
	
		packet->SenderID = index;
		packet->ReceiverID = IP_BROADCAST;
	
		packet->previousHop = nid;
	
		packet->TxPort = vanetPort;
		packet->RxPort = vanetPort;
	
		packet->nbBroadcast = 0;
		packet->Broadcast = 1;
		packet->nextHop = -1;

		SchedulePacketWithoutContext(packet,linkLayerTarget);
		routingOverhead++;
	}
}

void	vanetRouting::recvSybilDetectionMsg(Ptr<Packet> p)
{
	if(p->previousHop != index)
	{
		Ptr<Node> node = NodeList::GetNode (p->previousHop);
		anim->UpdateNodeColor(node,0,0,0);
		if(attackerList.check(p->previousHop) == -1)
		{
			attackerList.add(p->previousHop);
			cout<<"Node: "<<index<<" recv Attacker ID: " <<p->previousHop<<" announcement from "<<p->SenderID<<" at "<<CURRENT_TIME<<endl;
		}		
	}
}

void	vanetRouting::initializeMovement()
{
	minitTimer.SetFunction (&vanetRouting::assignMovement, this);
	double	startTime = 1000;
	minitTimer.Schedule (MilliSeconds (startTime));
}

void	vanetRouting::assignMovement()
{
	double	y = get_y();

	if(y == lane_1 || y == lane_3)
	{
		dest_x = 1500;
		dest_y = y;
	}
	else
	{
		dest_x = 0;
		dest_y = y;
	}

	if(movementTimer.run == 0)
	{
		movementTimer.run = 1;
		movementTimer.SetFunction (&vanetRouting::NodeMovement, this);
		movementTimer.Schedule (MilliSeconds (100));
	}
}

void	vanetRouting::NodeMovement()
{
	double	x = get_x();
	double	y = get_y();

	if(x == dest_x && y == dest_y)
	{
		if(movementTimer.vertical == 0)
		{
			movementTimer.vertical = 1;
			if(y == lane_1)
			{
				dest_x = x;
				if(index % 2 == 0)
				{
					dest_y = lane_2;
				}
				else
				{
					dest_y = lane_4;
				}
			}
			else if(y == lane_2)
			{
				dest_x = x;
				if(index % 2 == 0)
				{
					dest_y = lane_1;
				}
				else
				{
					dest_y = lane_3;
				}
			}
			else if(y == lane_3)
			{
				dest_x = x;
				if(index % 2 == 0)
				{
					dest_y = lane_4;
				}
				else
				{
					dest_y = lane_2;
				}
			}
			else if(y == lane_4)
			{
				dest_x = x;
				if(index % 2 == 0)
				{
					dest_y = lane_3;
				}
				else
				{
					dest_y = lane_1;
				}
			}
		}
		else
		{
			movementTimer.vertical = 0;
			if(y == lane_1 || y == lane_3)
			{
				dest_x = 1500;
				dest_y = y;
			}
			else
			{
				dest_x = 0;
				dest_y = y;
			}
		}
		movementTimer.Cancel();
		movementTimer.SetFunction (&vanetRouting::NodeMovement, this);
		movementTimer.Schedule (MilliSeconds (100));
	}
	else
	{
		double	speed = node_->speed;
		double	dist = distance(x,y,dest_x,dest_y);

		double	travel_time = dist / speed;
		double	new_dist = dist / travel_time * 0.1;

		double	new_x = 0;
		double	new_y = 0;

		if(y == dest_y)
		{
			if(x >= dest_x)
			{
				new_x = x - new_dist;
			}
			else
			{
				new_x = x + new_dist;
			}
			new_y = y;
		}
		else if(x == dest_x)
		{
			if(y >= dest_y)
			{
				new_y = y - new_dist;
			}
			else
			{
				new_y = y + new_dist;
			}
			new_x = x;
		}

		assign_position(new_x,new_y);
		movementTimer.Cancel();
		movementTimer.SetFunction (&vanetRouting::NodeMovement, this);
		movementTimer.Schedule (MilliSeconds (100));
	}
}

void	vanetRouting::assign_position(double x,double y)
{
	Ptr<MobilityModel> position = node_->GetObject<MobilityModel> ();
	Vector vector = position->GetPosition ();
	vector.x = x;
	vector.y = y;
	position->SetPosition (vector);
}

void	vanetRouting::initAnalysis()
{
	rtimer.Cancel();
	rtimer.SetFunction (&vanetRouting::resultAnalysis, this);
	rtimer.Schedule (MilliSeconds (5000));
}

void	vanetRouting::resultAnalysis()
{
	cout<<"Analysis at "<<CURRENT_TIME<<endl;

	for(int i=0;i<Dlist_.count;i++)
	{
		nsaddr_t nid = Dlist_.nodeid[i];
		if(Mlist_.check(nid) != -1)
		{
			tp++;
		}
		else
		{
			fn++;
		}
	}

	for(int i=0;i<Mlist_.count;i++)
	{
		nsaddr_t nid = Dlist_.nodeid[i];
		if(Dlist_.check(nid) != -1)
		{
			tn++;
		}
		else
		{
			fp++;
		}
	}

	rtimer.Cancel();
	rtimer.SetFunction (&vanetRouting::resultAnalysis, this);
	rtimer.Schedule (MilliSeconds (5000));

}

int	vanetRouting::get_TP()
{
	return	tp;
}

int	vanetRouting::get_TN()
{
	return	tn;
}

int	vanetRouting::get_FP()
{
	return	fp;
}

int	vanetRouting::get_FN()
{
	return	fn;
}

double	vanetRouting::get_DD()
{
	if(detectionCount > 0)
	{
		return	detectionTime / detectionCount;
	}
	return	CURRENT_TIME;
}

} // namespace ns3
