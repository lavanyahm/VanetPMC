#include "ns3/vector.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include <iostream>
#include "ns3/animation-interface.h"
#include "ns3/vanetRouting.h"

#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"

  #include "ns3/netanim-module.h"

#include "ns3/random-variable-stream.h"

#include "ns3/rng-seed-manager.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleOcb");



static void results(vanetRouting *vanet)
{
	double	pdr = double(vanet->recv_count) / vanet->send_count * 100;
	double	delay = 0;
	double	jitter_sum = 0;
	for(int i=0;i<vanet->recv_count;i++)
	{
		delay += vanet->txDelay[i];

		if(i > 0)
		{
			jitter_sum += (vanet->rxTime[i] - vanet->rxTime[i-1]);
		}		
	}

	double	avgDelay = delay / vanet->recv_count;
	double	jitter = jitter_sum / (vanet->recv_count-1);

	double	throughput = vanet->bytes*8 / (vanet->pend - vanet->pinit);
	double	goodput = vanet->bytes*8 / avgDelay;

	cout<<"Simulation Results"<<endl;
	cout<<"Pkt_send:\t"<<vanet->send_count<<endl;
	cout<<"Pkt_recv:\t"<<vanet->recv_count<<endl;
	cout<<"PDR:\t\t"<<pdr<<endl;
	cout<<"avgDelay:\t"<<avgDelay<<endl;
	cout<<"jitter:\t\t"<<jitter<<endl;
	cout<<"Throughput:\t"<<throughput<<endl;
	cout<<"Goodput:\t"<<goodput<<endl;
	cout<<"Overheads:\t"<<vanet->routingOverhead<<endl;
	cout<<"NRO:\t\t"<<(double(vanet->routingOverhead) / vanet->recv_count)<<endl;

	double	tp = vanet->get_TP();
	double	tn = vanet->get_TN();
	double	fp = vanet->get_FP();
	double	fn = vanet->get_FN();

	double	tpr = tp / (tp + fn);
	double	tnr = tn / (tn + fp);
	double	precision = tp / (tp+fp);
	double	missrate = fn / (fn+tp);
	double	fpr = fp / (fp+tn);
	double	detectionDelay = vanet->get_DD();

	cout<<"TruePositiveRate:\t"<<tpr<<endl;
	cout<<"TrueNegativeRate:\t"<<tnr<<endl;
	cout<<"Accuracy:\t\t"<<precision<<endl;
	cout<<"MissRate:\t\t"<<missrate<<endl;
	cout<<"FalsePositiveRate:\t"<<fpr<<endl;
	cout<<"DetectionDelay:\t\t"<<detectionDelay<<endl;
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
	              uint32_t pktCount, Time pktInterval )

{

  if (pktCount > 0)
	{
      cout<<" LHM Generate Traffic";
	socket->Send (Create<Packet> (pktSize));
	Simulator::Schedule (pktInterval, &GenerateTraffic,
	            socket, pktSize,pktCount - 1, pktInterval);
	}
	else
	{
	socket->Close ();
	}
}

int main (int argc, char *argv[])
{
	std::string phyMode ("OfdmRate6MbpsBW10MHz");
	uint32_t packetSize = 1000; // bytes
	uint32_t numPackets = 1;
	double	interval = 0.1; // seconds
	bool	verbose = false;
	int	n_nodes = 64;
	int	proposed = 1;
	double	coverage = 250;
	double	VehicleLength = 3;
	double	speed = 10;

	double	alpha = 0.3;
	double	beta = 0.4;
	double	gamma = 0.3;

	int	stop_time	=	200;

	CommandLine cmd;

	cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
	cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
	cmd.AddValue ("numPackets", "number of packets generated", numPackets);
	cmd.AddValue ("interval", "interval (seconds) between packets", interval);
	cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
	cmd.AddValue ("n_nodes","Number of nodes in simulation",n_nodes);
	cmd.Parse (argc, argv);
	// Convert to time object
//	Time interPacketInterval = Seconds (interval);
	cout<<"No of packets"<<numPackets<<endl;

	NodeContainer node_;
	node_.Create (n_nodes);

	// The below set of helpers will help us to put together the wifi NICs we want
	YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	Ptr<YansWifiChannel> channel = wifiChannel.Create ();
	wifiPhy.SetChannel (channel);
	// ns-3 supports generate a pcap trace
	wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11);
	NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
	Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
	if (verbose)
	{
		wifi80211p.EnableLogComponents ();      // Turn on all Wifi 802.11p logging
	}

	wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue (phyMode));
	NetDeviceContainer devices = wifi80211p.Install (wifiPhy, waveMac, node_);

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	int set_1 = 16;

	double dist = 50;
	double gap = 300;

	for(int i=0;i<set_1;i++)
	{
		if(i < set_1/2)
		{
			positionAlloc->Add (Vector (dist * (i+1), 100.0, 0.0));	
			
		}
		else
		{
			positionAlloc->Add (Vector (gap + dist * (i+1), 100.0, 0.0));			
		}
	}

	for(int i=0;i<set_1;i++)
	{
		if(i < set_1/2)
		{
			positionAlloc->Add (Vector (1500-(dist * (i+1)), 200.0, 0.0));
		}
		else
		{
			positionAlloc->Add (Vector (1500-(gap + dist * (i+1)), 200.0, 0.0));
		}
	}

	for(int i=0;i<set_1;i++)
	{
		if(i < set_1/2)
		{
			positionAlloc->Add (Vector (dist * (i+1), 900.0, 0.0));
		}
		else
		{
			positionAlloc->Add (Vector (gap + dist * (i+1), 900.0, 0.0));
		}
	}

	for(int i=0;i<set_1;i++)
	{
		if(i < set_1/2)
		{
			positionAlloc->Add (Vector (1500-(dist * (i+1)), 1000.0, 0.0));
		}
		else
		{
			positionAlloc->Add (Vector (1500-(gap + dist * (i+1)), 1000.0, 0.0));
		}
	}

	cout<<"Coverage: "<<coverage<<" Proposed "<<proposed<<endl;
	vanetRouting *vanet_[n_nodes];

	for (int i=0;i<n_nodes;i++)
	{
		//node_ mn = node_ (node_.Get (i));
		Ptr <Node> node = node_.Get (i);
		node->coverage = coverage;
		node->proposed = proposed;
		node->L = VehicleLength;
		vanetRouting *vanet = new vanetRouting();
		vanet->index = i;
		vanet->node_ = node;
		vanet->linkLayerTarget = (void*)(waveMac.Get(i));
		vanet->alpha = alpha;
		vanet->beta = beta;
		vanet->gamma = gamma;
		vanet_[i] = vanet;
		vanet->node_->speed = speed;//?
	}

	//vanet_[28]->sybil = 1;

	for(int i=0;i<n_nodes;i++)
	{
		vanet_[i]->lane_1 = 100.0;//Why this is ?
		vanet_[i]->lane_2 = 200.0;
		vanet_[i]->lane_3 = 900.0;
		vanet_[i]->lane_4 = 1000.0;

		vanet_[i]->initializeMovement();
		vanet_[i]->initializeBeaconTimer();
	}
	
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (node_);

	InternetStackHelper internet;

	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	list.Add (staticRouting, 0);
	internet.SetRoutingHelper (list);
	internet.Install (node_);

	Ipv4AddressHelper ipv4;
	NS_LOG_INFO ("Assign IP Addresses.");
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer i = ipv4.Assign (devices);

	// Traffic
{
		nsaddr_t src = 0;
		nsaddr_t dst = 9;
		double	startTime = 1.0;
		double	stopTime = 50;
#if 0
		TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
		Ptr<Socket> recvSink = Socket::CreateSocket (node_.Get (src), tid);
		InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
		recvSink->Bind (local);
		recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

		Ptr<Socket> source = Socket::CreateSocket (node_.Get (dst), tid);
		InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
		source->SetAllowBroadcast (true);
		source->Connect (remote);
	

	
	Simulator::ScheduleWithContext(source->GetNode ()->GetId(), Seconds(startTime), &GenerateTraffic, source, packetSize, numPackets, interPacketInterval);
#endif
	int flag = 0;
	if(flag == 0)
		{
			vanet_[src]->initializeDataTransmission(dst,Create<Packet>(),packetSize,interval,startTime,stopTime);
		}
}

	double	startTime = 37;
	double	stopTime = 145;
	nsaddr_t src = 25;
	nsaddr_t dst = 36;
{
#if 0
		TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
		Ptr<Socket> recvSink = Socket::CreateSocket (node_.Get (src), tid);
		InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
		recvSink->Bind (local);
		recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

		Ptr<Socket> source = Socket::CreateSocket (node_.Get (dst), tid);
		InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
		source->SetAllowBroadcast (true);
		source->Connect (remote);
	
		int flag = 0;
	
	Simulator::ScheduleWithContext(source->GetNode ()->GetId(), Seconds(startTime), &GenerateTraffic, source, packetSize, numPackets, interPacketInterval);
#endif
	int flag = 0;
	if(flag == 0)
		{
			vanet_[src]->initializeDataTransmission(dst,Create<Packet>(),packetSize,interval,startTime,stopTime);
		}

}
	Simulator::Schedule(Seconds(stop_time), &results, vanet_[0]);
	
	AnimationInterface *anim = new AnimationInterface("animation.xml");
	anim->EnablePacketMetadata (true);
	anim->SetStopTime(Seconds(stop_time));

	for(int i=0;i<n_nodes;i++)
	{
		vanet_[i]->anim = anim;
	}

	vanet_[0]->initAnalysis();

	RngSeedManager::SetSeed (3);  // Changes seed from default of 1 to 3
	RngSeedManager::SetRun (7);   // Changes run number from default of 1 to 7
	Simulator::Stop(Seconds(stop_time));
	Simulator::Run ();
	Simulator::Destroy ();
	return 0;
}
