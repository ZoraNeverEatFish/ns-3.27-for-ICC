/*
 * tcp-iccG.cc
 *
 *  Created on: 12 23 2018
 *      Author: root
 *
 *  ICC-G variant: find_fm() reimplemented with the Goertzel algorithm,
 *  removing the FFTW dependency. All other ICC logic is unchanged.
 */

 //update 'RTT reaction' : 2020.6.15 by lhy
 //update default lamda and cycle, correct theta regulation : 2020.7.04 by lhy
 //add switch for running on datacenter : 2020.7.05 by lhy
 //ver 1.0	based on Current Rate ------ 2020.7.14 by lhy
 //ver 1.1	Modify RTT changes and no-PDCC flows detection with both time and frequency domain : 2020.8.14 by lhy
 /*
Update by lhy 2020.10.15
1. Add dynamic fm threshold
2. Add dynamic RTTmin time window
3. Move safe region judgement before Qamp updating
*/
 /*
Update by lhy 2020.11.11
1. Add adaptive Bd
*/

#include "tcp-iccG.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/log.h"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpIccG");
NS_OBJECT_ENSURE_REGISTERED (TcpIccG);

TypeId
TcpIccG::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpIccG")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpIccG> ()
    .SetGroupName ("Internet")
    .AddAttribute ("m_lamuda", "the target packt queue in the buffer",
                   DoubleValue (2),
                   MakeDoubleAccessor (&TcpIccG::m_lamuda),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("cwnd_gain", "coverting gain of cwnd and rate",
                   DoubleValue (1),
                   MakeDoubleAccessor (&TcpIccG::cwnd_gain),
                   MakeDoubleChecker<double> ())
   .AddAttribute ("Bd", "the target packt queue in the buffer",
				  DoubleValue (10),
				  MakeDoubleAccessor (&TcpIccG::Bd),
				  MakeDoubleChecker<double> ())
  .AddAttribute ("Rc", "the target packt queue in the buffer",
				 DoubleValue (30),
				 MakeDoubleAccessor (&TcpIccG::Rc),
				 MakeDoubleChecker<double> ())
   .AddAttribute ("isPeriodicDCModify", "the mode of adjusting cwnd PeriodicDC or PeriodicDCModify",
		   	   	      BooleanValue (false),
					  MakeBooleanAccessor (&TcpIccG::isPeriodicDCModify),
					  MakeBooleanChecker ())
   .AddAttribute ("Qtarget", "the mode of adjusting cwnd PeriodicDC or PeriodicDCModify",
					  TimeValue (Seconds(0.0024)),
					  MakeTimeAccessor (&TcpIccG::Qtarget),
					  MakeTimeChecker ())
   .AddAttribute ("assitPra", "the asist pramete ",
					 DoubleValue (10),
					 MakeDoubleAccessor (&TcpIccG::assitPra),
					 MakeDoubleChecker<double> ())
   .AddAttribute ("cycle", "cycle ",
					 DoubleValue (3.0),
					 MakeDoubleAccessor (&TcpIccG::cycle),
					 MakeDoubleChecker<double> ())
	.AddAttribute ("isDC", "If works in datacenter ",
					 BooleanValue (false),
					 MakeBooleanAccessor (&TcpIccG::isDC),
					 MakeBooleanChecker())
	.AddAttribute ("isCom", "If works in datacenter ",
					 BooleanValue (false),
					 MakeBooleanAccessor (&TcpIccG::isCom),
					 MakeBooleanChecker ())
  ;
  return tid;
}

TcpIccG::TcpIccG (void)
  : TcpNewReno (),
    m_lamuda (1),
	default_lamuda (1),
	detectedLamda (1),
	cur_Rate(-1),
	cwnd_gain(1),
	ack_interval(0.0),
	pre_ack_time(Time (0.0)),
    m_theta(1),
	m_srtt (Time::Max ()),
    m_cntRtt (0),
    m_doingPeriodicDCNow (true),
    m_begSndNxt (0),
	m_RTTmin (Time::Max ()),
	recordTimeMin (Time (0.0)),
	m_RTTstanding (Time::Max ()),
	m_oldRTTstanding(Time::Max ()),
	recordTimeStanding (Time (0.0)),
	oldCwndpkts(1.0),
	newCwndpkts(1.0),
	oldAckTime(0),
	oldAckSeq(0),
	pra(1),
	QdInc(0),
	CwndInc(0),
	old_Qd(0),
	old_Cwnd(0),
	oldRtt1(Time::Min ()),
	oldRtt2(Time::Min ()),
	oldDirect(true),
	newDirect(true),
	times(0),
	isFirstTime(true),
	isPeriodicDCModify(false),
	stable_state(false),
	first_periodic(true),
	Qtarget(Time(Seconds(0.0024))),
	isSlowStart(true),
	m_RTTmax(Time (0.0)),
	glo_RTTmax(Time (0.0)),
	RTTminop(0),
	Qop(0),
	ifcompete(false),
	tempEmpty(false),
	defaultMode(true),
	RTTCountForEmpty(0),
 	RTTCountForMax(0),
	tempMax(Time (0.0)),
	tempMin(Time::Max ()),
	nearEmpty(true),
	probe_ceil(true),
	assitPra(0),
	Epra(2),
	oldEpra(1),
	v0(1),
	RTTCountFormin(0),
	NEpra(1),
	RTTCount(0),
   isBiTheta(false),
   ssTheta(1),
   sstarget(5),
   Smax(8),
   recordTimeEmpty (Time (0.0)),
   directChange(false),
   RTTforCountDirectChange(0),
   ForEmpty(2),
   oldCwndMid(1.0),
   m_RTTq(Time::Max ()),
   ackCount(0),
   intervalPoint(0.0),
   srttPoint(0.0),
   sum(0.0),
   targetRate(0),
    Bd(10),
	defaultBd(10),
    Rc(6),
    cycle(3.0),
	isDC(false),
	default_cycle(3.0),
	oldFm(-500),
	vals(),
	pre_Qdave(0),
	curAm0(0),
	preAm0(0),
        pktLostTime(Time (0.0)),
	g_n(0),
	g_fs(0.0),
	g_N(0),
	g_initialized(false),
	g_dcQd(0.0),
	g_dcCw(0.0)
{
  NS_LOG_FUNCTION (this);
  std::fill(g_freqs, g_freqs + G_BINS, 0.0);
  std::fill(g_s1,    g_s1    + G_BINS, 0.0);
  std::fill(g_s2,    g_s2    + G_BINS, 0.0);
  std::fill(g_c1,    g_c1    + G_BINS, 0.0);
  std::fill(g_c2,    g_c2    + G_BINS, 0.0);
  std::fill(g_coeff, g_coeff + G_BINS, 0.0);
}

TcpIccG::TcpIccG (const TcpIccG& sock)
  : TcpNewReno (sock),
	m_lamuda (sock.m_lamuda),
	default_lamuda (1),
	detectedLamda (1),
	cur_Rate(-1),
	cwnd_gain(1),
	ack_interval(0.0),
	pre_ack_time(Time (0.0)),
	m_theta (sock.m_theta),
	m_srtt (sock.m_srtt),
    m_cntRtt (sock.m_cntRtt),
	m_doingPeriodicDCNow (true),
    m_begSndNxt (0),
	m_RTTmin (Time::Max ()),
	recordTimeMin (Time (0.0)),
	m_RTTstanding (Time::Max ()),
	m_oldRTTstanding(Time::Max ()),
	recordTimeStanding (Time (0.0)),
	oldCwndpkts(1.0),
	newCwndpkts(1.0),
	oldAckTime(0),
	oldAckSeq(0),
	pra(1),
	QdInc(0),
	CwndInc(0),
	old_Qd(0),
	old_Cwnd(0),
	oldRtt1(Time::Min()),
	oldRtt2(Time::Min()),
	oldDirect(true),
	newDirect(true),
	times(0),
	isFirstTime(true),
	isPeriodicDCModify(false),
	stable_state(false),
	first_periodic(true),
	Qtarget(Time(Seconds(0.0024))),
	isSlowStart(true),
	m_RTTmax(Time (0.0)),
	glo_RTTmax(Time (0.0)),
	RTTminop(0),
	Qop(0),
	ifcompete(false),
	tempEmpty(false),
	defaultMode(true),
	RTTCountForEmpty(0),
 	RTTCountForMax(0),
	tempMax(Time (0.0)),
	nearEmpty(true),
	probe_ceil(true),
	assitPra(0),
	Epra(4),
	oldEpra(4),
	v0(1),
	RTTCountFormin(0),
	NEpra(1),
	RTTCount(0),
   isBiTheta(false),
   ssTheta(1),
   sstarget(5),
   Smax(4),
   recordTimeEmpty (Time (0.0)),
   directChange(false),
   RTTforCountDirectChange(0),
   ForEmpty(2),
   oldCwndMid(1.0),
   m_RTTq(Time::Max ()),
   ackCount(0),
   intervalPoint(0.0),
   srttPoint(0.0),
   sum(0.0),
   targetRate(0),
   cycle(3.0),
   isDC(false),
   default_cycle(3.0),
   oldFm(-500),
   vals(),
   pre_Qdave(0),
   curAm0(0),
	preAm0(0),
	g_n(0),
	g_fs(0.0),
	g_N(0),
	g_initialized(false),
	g_dcQd(0.0),
	g_dcCw(0.0)
{
  NS_LOG_FUNCTION (this);
  std::fill(g_freqs, g_freqs + G_BINS, 0.0);
  std::fill(g_s1,    g_s1    + G_BINS, 0.0);
  std::fill(g_s2,    g_s2    + G_BINS, 0.0);
  std::fill(g_c1,    g_c1    + G_BINS, 0.0);
  std::fill(g_c2,    g_c2    + G_BINS, 0.0);
  std::fill(g_coeff, g_coeff + G_BINS, 0.0);
}

TcpIccG::~TcpIccG (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpIccG::Fork (void)
{
  return CopyObject<TcpIccG> (this);
}

void TcpIccG::Send(Ptr<TcpSocketBase> tsb, Ptr<TcpSocketState> tcb,
                  SequenceNumber32 seq, bool isRetrans) {

  NS_LOG_FUNCTION(this);



  // If retransmission, start sequence (PktsAcked() finds end of sequence).
  if (isRetrans) {
    NS_LOG_LOGIC(this << "  Starting retrans sequence: " << seq);
  }else {

	  cwndHistory.insert(std::pair<SequenceNumber32,uint32_t>(tcb->m_nextTxSequence,tcb->m_cWnd));
	  ackTimeHistory.insert(std::pair<SequenceNumber32,Time>(tcb->m_nextTxSequence,Simulator::Now ()));
	  ackSeqHistory.insert(std::pair<SequenceNumber32,SequenceNumber32>(tcb->m_nextTxSequence,tcb->m_lastAckedSeq));
  }
  NS_LOG_DEBUG("debug:Pacing "<< this <<" now: "<< (double)1.0*Simulator::Now ().GetInteger()/1000000000 << " send_seq " << seq << " Pacing_pkts "<< tsb->pacingQueueBytes()/tcb->m_segmentSize<<" Inflight "<<tsb->BytesInFlight()/tcb->m_segmentSize << " Pacing_rate "<< tcb->GetPacingRate() << " now(ns) " <<  Simulator::Now ().GetInteger() % 1000000 << " with "<< tcb->m_congState << " " << isRetrans);
}

void
TcpIccG::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt)
{
	NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
	//NS_LOG_INFO(" PktsAcked "<<tcb->m_cWnd );
//	std::chrono::high_resolution_clock::time_point beginTime = std::chrono::high_resolution_clock::now();



	if (rtt.IsZero ())
	{
		return;
	}
	if(Time::Min()==oldRtt2){
		//pktLostTime = Simulator::Now();
		oldRtt2=Simulator::Now ();
		oldRtt1=Simulator::Now ();
		nearEmpty=false;
		
	}
	Time RTT=this->GetrttInst();
	//Time RTT=min(this->GetrttInst(),rtt);
	//Time RTT=rtt;
	//init lamda 2020.7.04 by lhy
	if(oldFm==-500)
	{
		default_cycle=cycle;
		default_lamuda=m_lamuda;
		detectedLamda = default_lamuda;
		defaultBd=Bd;
		if(cur_Rate<0)// init Rate 2020.7.14 by lhy
		{
			cur_Rate = tcb->m_cWnd*8 / RTT.GetSeconds();// in bps
			cur_Rate /=1000000; // in Mbps
		}
	}
	//2020.7.04 by lhy

        double Bdelay=Bd*0.001;// convert to (s)
        double Bw=Rc*100/12;// convert to (Mbps)

	int64_t nowToInt = Simulator::Now ().GetInteger();
	//int64_t recordTimeMinToInt = recordTimeMin.GetInteger();

	//double cycle=3.0;
	//double Qd = m_RTTstanding.GetSeconds() - m_RTTmin.GetSeconds();
	double Qd = RTT.GetSeconds() - m_RTTmin.GetSeconds(); // use instantaneous RTT to caculate Qd
	Qd = std::max(0.0,Qd);

	//judge packet loss
	/*(bool pkt_loss=false;
	SequenceNumber32 L_seq=tcb->m_lastAckedSeq;
	if(ackTimeHistory.count(L_seq) != 0)
	{
		for(auto x : ackTimeHistory)
		{
			if (x.first > L_seq) break;
			if (x.first < L_seq) pkt_loss=true;
			ackTimeHistory.erase(x.first);
		}
	} */
	
	//update RTTmin
	Time tempRTT = RTT;
	while(!vals.empty() && vals.back().second > tempRTT)vals.pop_back();
	vals.push_back(make_pair(nowToInt,tempRTT));
	while(vals.size()>1 && (vals.front().first + std::max(Seconds (assitPra).GetInteger(),int64_t(100*m_RTTmin.GetSeconds()))) < nowToInt)vals.pop_front();// dynamic RTTmin time window by lhy 2020.10.15
	m_RTTmin=std::min(vals.front().second,m_RTTmin);
	
	
	/* if(nowToInt > recordTimeMinToInt + Seconds (assitPra).GetInteger()) {
		m_RTTmin = Time::Max ();
	}
	if (RTT <= m_RTTmin) {
		recordTimeMin = Simulator::Now ();

	} */
	
	
	//m_RTTmin = std::min (m_RTTmin, RTT);
   sum=sum+RTT.GetSeconds();

   ackCount++;
   /* if(tcb->m_lastAckedSeq.GetValue()/1448.0 <=2){
	   //m_lamuda=6/m_RTTmin.GetSeconds();
	   
   } */
	
	//update Tau
	ack_interval = std::min(Simulator::Now ().GetSeconds()-pre_ack_time.GetSeconds(),10*m_RTTmin.GetSeconds());// 0829
	pre_ack_time = Simulator::Now ();
	
   //record Qd, Cwnd information
   if(Simulator::Now ().GetSeconds()>=oldRtt2.GetSeconds()){
	   if(Qd>=old_Qd)QdInc++;
	   if(tcb->m_cWnd>old_Cwnd)CwndInc++;
	   old_Qd=Qd;
	   old_Cwnd=tcb->m_cWnd;
	   QdArray.push_back(Qd);
	   CwndArray.push_back(tcb->m_cWnd);
	   GoertzelFeed(Qd, (double)tcb->m_cWnd);
	   m_RTTmax=max(RTT,m_RTTmax);
	   tempMin=min(RTT,tempMin);
	   if(RTT<=m_RTTmin){
		   nearEmpty=true;
	   }
	   if(RTT>=glo_RTTmax)
	   {
		   probe_ceil=true;
		   glo_RTTmax=RTT;
	   }

   }
  // NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" Record "<<oldRtt2.GetSeconds()<<" 2 "<<tempMin.GetSeconds()<<" "<<RTT.GetSeconds());


        double amplitudFm=0.0;
        double Qave=(sum/ackCount);
        double fm=5/cycle;
		curAm0 = Qave;

	double f0Am=0,simiFactor=0;
	double fmth=1/(20*m_RTTmin.GetSeconds());// fm threshold by lhy 2020.10.15
	//update lamda and cycle
	if(Simulator::Now ().GetSeconds() > oldRtt2.GetSeconds() + cycle){
		// find_fm(&fm,&amplitudFm,&f0Am,QdArray,QdArray.size()/cycle,CwndArray,&simiFactor);

		// ── Re-arm Goertzel for the NEXT cycle (true online detection) ──
		// Fix the candidate band around 1/(5*RTTmin) and the per-sample
		// coefficients from the just-measured ack rate (samples/cycle, using
		// the still-current `cycle`), reset the IIR state, and set the DC
		// estimates to this cycle's means (subtracted from each incoming
		// sample next cycle). The next cycle's ACKs then accumulate
		// incrementally via GoertzelFeed; the boundary find_fm simply reads
		// the accumulated state — no buffer replay.
		if (!QdArray.empty())
		{
			double mq = 0.0, mc = 0.0;
			for (size_t i = 0; i < QdArray.size(); ++i) { mq += QdArray[i]; mc += CwndArray[i]; }
			g_dcQd = mq / QdArray.size();
			g_dcCw = mc / CwndArray.size();
			GoertzelInit(QdArray.size() / cycle, (int)QdArray.size());
		}
		else
		{
			g_initialized = false;   // force bootstrap on the next cycle
			g_n           = 0;
		}
		find_fm(&fm,&amplitudFm,&f0Am,QdArray,QdArray.size()/cycle,CwndArray,&simiFactor);

		amplitudFm*=2;
		double simiD=abs(double(1.0*(QdInc-CwndInc)))/QdArray.size();
		NS_LOG_DEBUG("debug:Interval_Arrive "<< this <<" now: "<< (double)1.0*nowToInt/1000000000<<" fm: "<< fm << " similarity: " << simiFactor/QdArray.size() << " simiD: "<<simiD<<" Bd "<<Bd << " AM0 " << curAm0 << " Qave " << Qave);
		if(isCom && (simiFactor/QdArray.size() > 0.01 || simiD > 0.7) && !isDC)//combining frequency and time domain : 2020.8.15 by lhy
		{
			/* while(!vals.empty())vals.pop_back();
			vals.push_back(make_pair(nowToInt,RTT));
			m_RTTmin=vals.front().second; */
			//int a = 1;
			//Bd =2*(m_RTTmax.GetMilliSeconds()-m_RTTmin.GetMilliSeconds());// adaptive Bd : 2020.11.11 by lhy
			//Bd = 1000*defaultBd;
			Bd = defaultBd*(1.0*(double)(m_RTTmax.GetSeconds()-RTTminop)/Qop);
			Bd = max(Bd,defaultBd);
			ifcompete=true;
			NS_LOG_DEBUG("debug:Compete "<< this <<" now: "<< (double)1.0*nowToInt/1000000000<<" RTTmax: "<< m_RTTmax.GetSeconds() << " RTTminop: " << RTTminop << " Qop: "<<Qop<<" Bd "<<Bd);
			//m_theta = 1;
		}
		else if(true && fm>0 && oldFm-std::min(5*fmth,2.0)<=fm&&fm<=oldFm+std::min(5*fmth,2.0) && curAm0>=0.9*preAm0 && curAm0<=1.1*preAm0){// restrain stable state by lhy 2021.9.2
			//if(!first_periodic)Bd=defaultBd;
			/* Bd /= 2;
			Bd = std::max(defaultBd,Bd); // modify 2021.08.26 by lhy */
			if(ifcompete) ifcompete=false;
			else 
			{
				cycle=5/fm;
				if(isDC)cycle=2/fm;// small cycle for Datacenter by lhy 2020.7.09
				cycle=min(20*Qave,cycle);
				double Qdamp,L_gain=1.0;
				
				//update Qdamp : 2020.8.18 by lhy
				if(!(nearEmpty || probe_ceil))pre_Qdave=Qave;// by lhy 2020.10.13
				Qdamp=std::max(m_RTTmax.GetSeconds()-pre_Qdave,pre_Qdave-tempMin.GetSeconds());
				if((m_RTTmax-tempMin).GetSeconds()/2>Qdamp)
				{
					Qdamp=(m_RTTmax-tempMin).GetSeconds()/2;
				}
				else
				{
					Qave=pre_Qdave;
				}
				
				/* if(nearEmpty)
				{
					if(m_RTTmax.GetSeconds()>pre_Qdave)Qave=pre_Qdave;
					Qdamp=m_RTTmax.GetSeconds()-Qave;
				}
				else if(pkt_loss)
				{
					if(tempMin.GetSeconds()<pre_Qdave)Qave=pre_Qdave;
					Qdamp=Qave-tempMin.GetSeconds();
				}
				else Qdamp=(m_RTTmax-tempMin).GetSeconds()/2; */
				
				//if(stable_state) L_gain=(pre_Qdave-m_RTTmin.GetSeconds())/Qdamp;
				double C_N=(m_lamuda*RTT.GetSeconds())/(Qdamp);
				if(Bd==defaultBd && first_periodic && Qdamp>=0.001*RTT.GetSeconds())//0915
				{
					//m_lamuda=min(0.1*C_N,Bdelay/Qave*C_N);// for fairness: 0917 by lhy
					//m_lamuda=0.5*Bdelay/Qave*C_N;// for fairness: 0917 by lhy
					/* m_lamuda = min(0.5 * Bdelay/Qave * Bw * std::log(Bdelay/(Qave-m_RTTmin.GetSeconds())),10*m_lamuda);// for fairness: 1106 by lhy
					if(m_lamuda<=0) m_lamuda=default_lamuda;*/
					first_periodic=false;
				}
				else if(Bd==defaultBd){ //&& Qdamp>=0.001*RTT.GetSeconds()){
					//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" max-Min "<<(m_RTTmax-tempMin).GetSeconds()<<" lamuda "<<m_lamuda<<" CwndC "<<m_lamuda* m_RTTmin.GetSeconds());
					//double s=0.25/fm;
					
					/* double C_N=(m_lamuda*RTT.GetSeconds())/(Qdamp); //in Mbps
					double k=std::pow(2.71828,C_N/Bw); 
					m_lamuda=min(max(L_gain*(C_N)*(Bdelay/k)/RTT.GetSeconds(),0.1*m_lamuda),10*m_lamuda);//in Mbps */
					
					//m_lamuda=min(max(L_gain*(Qave-m_RTTmin.GetSeconds())/Qdamp*m_lamuda,0.1*m_lamuda),10*m_lamuda); // 0901
					
					if(nearEmpty){
						detectedLamda = m_lamuda;
						m_lamuda/=2;
					}
					else m_lamuda+=0.2*detectedLamda;
					
					//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" C_N "<<C_N<<" k "<<k<<" ^x "<<0.001*1.5*C_N/Rc<<" CN_k "<<C_N/k<<" lamuda "<<m_lamuda);
				}
				else 
				{
					//m_lamuda=default_lamuda; //m_lamuda=1; //2020.7.04 modified by lhy
					m_lamuda=m_lamuda;
				}
				
				if(Bd==defaultBd)
				{
					Qop = Qave-m_RTTmin.GetSeconds();
					RTTminop = m_RTTmin.GetSeconds();
					//RTTminop = m_RTTmax.GetSeconds();
				}
				
				Bd=defaultBd;
				NS_LOG_DEBUG("debug:Interval_Arrive "<< this <<" now: "<< (double)1.0*nowToInt/1000000000 << " C/N: "<< C_N << " Qdamp: "<<Qdamp<<" pre_Qdave: "<<pre_Qdave<<" Qave: "<<Qave-m_RTTmin.GetSeconds()<<" L_gain: "<<L_gain<<" lamda: "<<m_lamuda << " Pmax: "<< m_RTTmax.GetSeconds() << " Pmin: " << tempMin.GetSeconds() << " clear? " << nearEmpty<< " Am0: "<< curAm0 << " detectedLamda: "<< detectedLamda);
			stable_state=true;
			}
		}else{
			
			//Bd=defaultBd;
			cycle = 20*Qave;
			m_lamuda = detectedLamda;
			
			//cycle=default_cycle; //cycle=1; //2020.7.04 modified by lhy
			stable_state=false;
			first_periodic=true;
			
			//m_lamuda=default_lamuda; // 2020.09.15
			//m_lamuda=1;
		}
		
		/* if(nearEmpty){
			m_lamuda=0.5*m_lamuda;
		}else{
			if(Qdamp>0){
				m_lamuda=(Bdelay*m_lamuda/Qdamp+m_lamuda)/2; 
			}else{				
				m_lamuda=m_lamuda+1;
			}

		} */

		oldFm=fm;
		
	    //m_lamuda=1;
		//cycle=10/fm;
		//cycle=1;

		oldRtt2=Simulator::Now ();
	   m_RTTmax=Time::Min ();

	   tempMin=Time::Max ();

	   m_RTTmax=max(RTT,m_RTTmax);
	   tempMin=min(RTT,tempMin);
	   nearEmpty=false;
	   probe_ceil=false;
	   pre_Qdave=Qave;
	   preAm0=curAm0;// restrain stable state by lhy 2021.9.2

		//m_delta=2;
	  sum=0.0;
	  ackCount=0;

	  QdArray.clear();
	  CwndArray.clear();
	  QdInc=0;
	  CwndInc=0;
	  // NB: Goertzel state was already re-armed right after find_fm above,
	  // so the next cycle's ACKs feed into a freshly-reset accumulator.
	}

	//update m_RTTstanding
	int64_t recordTimeStandingToInt = recordTimeStanding.GetInteger();
	if(nowToInt > recordTimeStandingToInt + rtt.GetInteger()/2){

		m_RTTstanding = Time::Max ();
		m_RTTstanding = std::min (m_RTTstanding, RTT);
		oldCwndMid=tcb->m_cWnd;
		m_oldRTTstanding = m_RTTstanding;
		//m_RTTstanding = Time::Max ();
		recordTimeStanding = Simulator::Now ();
		srttPoint = Simulator::Now ().GetSeconds();
	}else{
		srttPoint=0;
		m_RTTstanding = std::min (m_RTTstanding, RTT);
	}


	//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" TR "<<targetRate<<" Qd "<<Qd<<" Bw "<<Bw<<" ln "<<std::log(Bdelay/Qd));
	targetRate =Bw * std::log(Bdelay/Qd);// in Mbps
	//Bdelay+=0;Bw+=0;
	//targetRate = 2/Qd;
	//double currentRate = tcb->m_cWnd/ m_RTTstanding.GetSeconds();// Byte/s
	//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" TR "<<targetRate<<" currentRate ");
   	intervalPoint=0;
    	// update theta
	//if (tcb->m_lastAckedSeq >= m_begSndNxt)// RTT hits
	if(Simulator::Now ().GetSeconds() > oldRtt1.GetSeconds() + RTT.GetSeconds())
	{
		// A periodicDC cycle has finished, we do Copa cwnd adjustment every RTT.
		intervalPoint=Simulator::Now ().GetSeconds();


		//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()	<<" CwndList[0] "<<CwndList[0]);
		bool accelate=true;
		for(int i=1;i<=2;i++){
			if(CwndList[i-1]>CwndList[i]){
				//accelate=false;	 
			}				
		}
		
		if(CwndList[2]>cur_Rate){
			newDirect=false;
		}else{
			newDirect=true;
		}
		if(oldDirect==newDirect)UpDirectCount++;
		else UpDirectCount=0;
		if(UpDirectCount>=2){ // >2 -> >=2 for accelate at 3rd rtt, modified by lhy 2020.7.09
			accelate=true;	
		}else{
			accelate=false;
		}
		oldDirect=newDirect;
			
		

		if(accelate){
			m_theta=std::min(2*m_theta,(cur_Rate)/(m_lamuda));

			//m_theta=((double)tcb->m_cWnd/tcb->m_segmentSize)/m_lamuda;
			m_theta=std::max(1.0,m_theta);
		}
		else{
			m_theta=1;
		}

		//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" accelate "<<accelate<<" m_theta "<<m_theta);



		for(int i=1;i<=2;i++){
			CwndList[i-1]=CwndList[i];			
		}
		CwndList[2]=cur_Rate;
		

		m_begSndNxt = tcb->m_nextTxSequence;
		oldRtt1=Simulator::Now ();
		oldCwndpkts=newCwndpkts;
		oldDirect=newDirect;

	}
	//tcb->SetPacingRate(cur_Rate);
	
//	std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
//	std::chrono::duration<double, std::milli> fp_ms =endTime - beginTime;
//    std::cout <<this << "Time is " << fp_ms.count()<< " ms" << endl;


	//if (!m_doingPeriodicDCNow)m_theta=1; // 0829
	
	
	/* if(false && Bd > defaultBd && Simulator::Now () > pktLostTime + RTT && (tcb->m_congState == TcpSocketState::CA_LOSS  || tcb->m_congState == TcpSocketState::CA_RECOVERY)){
                    cur_Rate = cur_Rate * 0.7;
					pktLostTime = Simulator::Now ();
	 }
	 else{
		double Qd = RTT.GetSeconds() - m_RTTmin.GetSeconds();
		targetRate =Bw* std::log(Bdelay/Qd);
		double factor = m_RTTmin.GetSeconds()/0.05; 
		factor = m_RTTmin.GetSeconds();
		
		factor=1; 


		if(cur_Rate <= targetRate){
			cur_Rate=std::min(cur_Rate+factor*m_lamuda*m_theta*ack_interval/RTT.GetSeconds(),10*cur_Rate);
		}else {
			m_theta=1;
			cur_Rate=std::max(cur_Rate-factor*m_lamuda*m_theta*ack_interval/RTT.GetSeconds(),0.5*cur_Rate);
		}
	 }
	 cur_Rate = max(cur_Rate, 0.2 * (1.0*tcb->m_segmentSize*8/1000000) / m_RTTmin.GetSeconds());//add lower boundary for rate by lhy 12.23
	 tcb->SetPacingRate(cur_Rate);
	 if(Bd==defaultBd)tcb->m_cWnd = cwnd_gain*cur_Rate*m_RTTmin.GetSeconds()*1000000/8;//in Byte
	 else tcb->m_cWnd = cwnd_gain*cur_Rate*RTT.GetSeconds()*1000000/8;
	 if(tcb->m_cWnd < tcb->m_segmentSize*2)tcb->m_cWnd=tcb->m_segmentSize*2; */

	NS_LOG_INFO(this<<" rttI "<<this->GetrttInst()<<" Stand "<<m_RTTstanding
	  <<" Old "<<m_oldRTTstanding<<" RTTmin "<<m_RTTmin
	  <<" Qd "<<Qd<<" srttPoint "<<srttPoint
	  <<" now "<<(double)1.0*nowToInt/1000000000<<" timePoint(ns) "<< nowToInt % 1000000
	  <<" cwnd "<<tcb->m_cWnd/1448.0<<" CRate "<<cur_Rate
	  <<" Trate "<<targetRate<<" pRate "<<tcb->GetPacingRate()
	  <<" lamuda "<<m_lamuda<<" theta "<<m_theta
	  <<" lsAck "<<tcb->m_lastAckedSeq.GetValue()
	  <<" newDirect "<<newDirect<<" ackCount "<<ackCount<<" rtt "<<rtt<<" sum "<<sum<<" Qave "<<sum/ackCount
	  <<" Rc "<<Rc<<" Bd "<<Bd
	  <<" cycle "<<cycle<<" targetTheta "<<Qave*ackCount*1500/cycle/1448.0<<" target "<<ackCount*1500/cycle
	  <<" fm "<<oldFm
	  <<" amplitudFm "<<amplitudFm<<" Amf0 "<<f0Am
	  <<" m_begSndNxt "<<m_begSndNxt<<" nextTxSeq "<<tcb->m_nextTxSequence
	  <<" maximal "<<m_RTTmax.GetSeconds()<<" minimal "<<tempMin.GetSeconds() << " Ack_interval "<<ack_interval<<" step "<<m_lamuda*m_theta*ack_interval/RTT.GetSeconds()<<" ifPDCC "<<m_doingPeriodicDCNow << " TcpState " << tcb->m_congState);



}
void
TcpIccG::PktsAckedAPeriodicDC (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt)
{
	NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

}

void
TcpIccG::EnablePeriodicDC (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  m_doingPeriodicDCNow = true;
  m_begSndNxt = tcb->m_nextTxSequence;
  m_cntRtt = 0;
}

void
TcpIccG::DisablePeriodicDC ()
{
  NS_LOG_FUNCTION (this);

  m_doingPeriodicDCNow = false;
}

void
TcpIccG::CongestionStateSet (Ptr<TcpSocketState> tcb,
                              const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
  if (newState == TcpSocketState::CA_OPEN)
    {
	  EnablePeriodicDC (tcb);
    }
  else
    {
		//Bd=defaultBd;//2020.11.11 by lhy
		NS_LOG_DEBUG("debug:switching "<< this <<" now: "<< (double)1.0*Simulator::Now ().GetInteger()/1000000000 << " Entering " << newState);
	  if (newState == TcpSocketState::CA_RECOVERY){
		  //NS_LOG_INFO("debug:switching "<< this <<" now: "<< (double)1.0*Simulator::Now ().GetInteger()/1000000000 << " Entering Recovery");
	      if(!defaultMode){
	    	  //m_lamuda=std::max(m_lamuda/2,2.0);
	      }else{
	    	  //m_lamuda=2.0;
	      }
		  //NS_LOG_LOGIC(this<<" delta "<<m_lamuda);
	  }
	  //NS_LOG_INFO(this<<" Debug_L: Now:"<< Simulator::Now() << " Cwnd_old:"<<tcb->m_cWnd);
	  DisablePeriodicDC ();
    }
}
void
TcpIccG::ExitRecovery (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  NS_LOG_DEBUG(this<<"Debug_L: without adjust window");


}

void
TcpIccG::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
	NS_LOG_FUNCTION (this << tcb << segmentsAcked);
	NS_LOG_DEBUG("debug:Regularte "<< this <<" now: "<< Simulator::Now () << " pktLostTime: "<< pktLostTime << " tcpState: "<< tcb->m_congState );


          if(Bd > defaultBd && Simulator::Now () > pktLostTime + this->GetrttInst() &&  tcb->m_congState == TcpSocketState::CA_LOSS){
                    cur_Rate = cur_Rate * 0.7;
					tcb->SetPacingRate(cur_Rate);
					tcb->m_cWnd = cwnd_gain*cur_Rate*m_RTTmin.GetSeconds()*1000000/8;//in Byte
					if(tcb->m_cWnd < tcb->m_segmentSize*2)tcb->m_cWnd=tcb->m_segmentSize*2;
					pktLostTime = Simulator::Now ();
		    return;
	 }
          double Bdelay=Bd*0.001;
	  double Bw=Rc*100/12;

	  if (false && !m_doingPeriodicDCNow)
	    {
	      // If Vegas is not on, we follow NewReno algorithm
	      //NS_LOG_LOGIC ("Vegas is not turned on, we follow NewReno algorithm.");
		  //NS_LOG_INFO(this<<" Debug_L: Now:"<< Simulator::Now() << " Cwnd_old:"<<tcb->m_cWnd);
	      TcpNewReno::IncreaseWindow (tcb, segmentsAcked);
		  
		  m_theta=1; // 0829
		  
		  //NS_LOG_INFO(this<<" Debug_L: Now:"<< Simulator::Now() << " Cwnd_new:"<<tcb->m_cWnd);
	      return;
	    }
	
	Time RTT=this->GetrttInst();
	//double Qd = m_RTTstanding.GetSeconds() - m_RTTmin.GetSeconds(); 
	double Qd = RTT.GetSeconds() - m_RTTmin.GetSeconds();

	//double currentRate = (tcb->m_lastAckedSeq.GetValue()-oldseq)/(Simulator::Now ().GetSeconds()-oldtime);
	//double currentRate = tcb->m_cWnd/m_RTTstanding.GetSeconds();

	//double targetRate = m_delta*tcb->m_segmentSize/Qd;
	targetRate =Bw* std::log(Bdelay/Qd);
	//Bdelay+=0;Bw+=0;
	//targetRate=2/Qd;
	double factor = m_RTTmin.GetSeconds()/0.05; 
	factor = m_RTTmin.GetSeconds();
	
	factor=1; 


	if(cur_Rate <= targetRate){
		//NS_LOG_INFO(" currentRate <= targetRate "<<tcb->m_cWnd );
		//tcb->m_cWnd = (double)tcb->m_cWnd + factor*m_lamuda*m_theta*ackTemp*tcb->m_segmentSize;
		//cur_Rate=std::min(cur_Rate+factor*m_lamuda*m_theta*ack_interval/RTT.GetSeconds(),2*cur_Rate);
		cur_Rate=std::min(cur_Rate+factor*m_lamuda*m_theta*ack_interval/RTT.GetSeconds(),10*cur_Rate);
		//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" currentRate <= targetRate "<<tcb->m_cWnd<<" diff "<<m_lamuda*m_theta*ackTemp*tcb->m_segmentSize  );
	}else {
		//NS_LOG_INFO(" currentRate > targetRate "<<tcb->m_cWnd );
		//tcb->m_cWnd = std::max((double)tcb->m_segmentSize, (double)tcb->m_cWnd - factor*m_lamuda*m_theta*ackTemp*tcb->m_segmentSize);
		//if(newDirect)m_theta=1;
		m_theta=1;
		cur_Rate=std::max(cur_Rate-factor*m_lamuda*m_theta*ack_interval/RTT.GetSeconds(),0.5*cur_Rate);
		//NS_LOG_UNCOND(this<<" now "<<Simulator::Now ().GetSeconds()<<" currentRate > targetRate "<<tcb->m_cWnd<<" diff "<<m_lamuda*m_theta*ackTemp*tcb->m_segmentSize );
	}
	cur_Rate = std::max(cur_Rate,0.2 * (1.0*tcb->m_segmentSize*8/1000000) / m_RTTmin.GetSeconds());
	tcb->SetPacingRate(cur_Rate);
	//tcb->SetPacingRate(0);
	tcb->m_cWnd = cwnd_gain*cur_Rate*m_RTTmin.GetSeconds()*1000000/8;//in Byte
	if(tcb->m_cWnd < tcb->m_segmentSize*2)tcb->m_cWnd=tcb->m_segmentSize*2;
}

void
TcpIccG::PeriodicDCModifyWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked){





}

std::string
TcpIccG::GetName () const
{
  return "TcpIccG";
}

uint32_t
TcpIccG::GetSsThresh (Ptr<const TcpSocketState> tcb,
                       uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  //return std::max (std::min (tcb->m_ssThresh.Get (), tcb->m_cWnd.Get () - tcb->m_segmentSize), 2 * tcb->m_segmentSize);
  return std::max (tcb->m_cWnd.Get () - tcb->m_segmentSize, 2 * tcb->m_segmentSize);//^_^
}

void
TcpIccG::DecreaseWindow(Ptr<TcpSocketState> tcb)
{
	  if(Bd > defaultBd){
					Time RTT=this->GetrttInst();
                    cur_Rate = cur_Rate * 0.7;
					pktLostTime = Simulator::Now ();
					tcb->SetPacingRate(cur_Rate);
					tcb->m_cWnd = cwnd_gain*cur_Rate*RTT.GetSeconds()*1000000/8;
					if(tcb->m_cWnd < tcb->m_segmentSize*2)tcb->m_cWnd=tcb->m_segmentSize*2;
	 }
}


void
TcpIccG::GoertzelInit(double fs, int N)
{
	// Guard: skip if fs or N is nonsensical.
	if (fs <= 0.0 || N < 4) return;

	g_fs = fs;
	g_N  = N;

	// ── Frequency of interest ────────────────────────────────────────
	// ICC's rate-adjustment profile oscillates at the physical frequency
	//   f_center ≈ 1/(5*RTTmin)
	// (the buffer holds ~5 oscillation periods per cycle, since the
	//  controller sets cycle = 5/fm). Rather than spreading the candidate
	//  bins uniformly from the 1.5 Hz lower bound to Nyquist — which wastes
	//  resolution and lets low-frequency trend energy dominate the lowest
	//  bin — we concentrate all G_BINS bins in a narrow band around
	//  f_center so the true oscillation peak is resolved finely.
	double rttmin = m_RTTmin.GetSeconds();
	double f_center;
	if (rttmin > 0.0 && rttmin < 1.0e3)        // valid measured RTTmin
		f_center = 1.0 / (5.0 * rttmin);
	else                                       // fallback: 5 periods/window = 5/cycle = 5*fs/N
		f_center = 5.0 * fs / (double)N;

	// Band = [f_center / G_BAND, f_center * G_BAND], clamped to the
	// admissible range (above the 1.5 Hz trend cutoff, below Nyquist).
	const double G_BAND = 3.0;                  // half-width factor (band spans G_BAND^2 = 9x)
	double f_low  = std::max(1.5,      f_center / G_BAND);
	double f_high = std::min(fs / 2.0, f_center * G_BAND);
	if (f_high <= f_low) f_high = std::min(fs / 2.0, f_low * 2.0); // safety
	if (f_high <= f_low) f_high = f_low * 2.0;                     // last resort

	for (int k = 0; k < G_BINS; ++k)
	{
		double frac = (G_BINS > 1)
		              ? (double)k / (G_BINS - 1)
		              : 0.0;
		// log-spacing within the band centred on f_center
		g_freqs[k] = f_low * std::pow(f_high / f_low, frac);
		double omega = 2.0 * M_PI * g_freqs[k] / fs;
		g_coeff[k]  = 2.0 * std::cos(omega);
	}

	// Reset IIR state
	std::fill(g_s1, g_s1 + G_BINS, 0.0);
	std::fill(g_s2, g_s2 + G_BINS, 0.0);
	std::fill(g_c1, g_c1 + G_BINS, 0.0);
	std::fill(g_c2, g_c2 + G_BINS, 0.0);
	g_n           = 0;
	g_initialized = true;
}

void
TcpIccG::GoertzelFeed(double qd_sample, double cwnd_sample)
{
	if (!g_initialized) return;
	// Remove DC at feed time (estimated from the previous cycle's mean):
	// Goertzel at an arbitrary frequency is not orthogonal to a constant,
	// so a non-zero mean would leak into the low-frequency bins.
	double xq = qd_sample   - g_dcQd;
	double xc = cwnd_sample - g_dcCw;
	for (int k = 0; k < G_BINS; ++k)
	{
		// RTT/Qd channel
		double s  = xq + g_coeff[k] * g_s1[k] - g_s2[k];
		g_s2[k]   = g_s1[k];
		g_s1[k]   = s;

		// cwnd channel
		double sc = xc + g_coeff[k] * g_c1[k] - g_c2[k];
		g_c2[k]   = g_c1[k];
		g_c1[k]   = sc;
	}
	++g_n;
}

void
TcpIccG::find_fm(double *fm,double *am,double *f0am,vector<double> &s,double fs,vector<double> &c,double *simi)
{
	int n = (int)s.size();
	if (n < 2) return;

	// ── 1. DC term (FFT bin 0) of each signal ─────────────────────
	// For the DC term (f=0) we use the arithmetic mean of the signal,
	// matching the original: *f0am = |X[0]| / n  (approximately mean).
	double dc_qd = 0.0, dc_cw = 0.0;
	for (int i = 0; i < n; ++i) { dc_qd += s[i]; dc_cw += c[i]; }
	double mean_qd = dc_qd / n, mean_cw = dc_cw / n;
	*f0am = std::fabs(dc_qd) / n;       // ≈ original *f0am
	double qd_dc_amp = std::fabs(dc_qd);  // used for normalisation below
	double cw_dc_amp = std::fabs(dc_cw);

	// ── 2. Goertzel state ─────────────────────────────────────────
	// Normal (online) path: the IIR state in g_s1/g_s2/g_c1/g_c2 has
	// already been accumulated incrementally over this cycle's ACKs by
	// GoertzelFeed (DC removed at feed time, band fixed at the previous
	// cycle boundary). We just read it below — no buffer replay.
	//
	// Bootstrap path (first cycle only): no online state exists yet, so
	// replay the buffered window once with the mean removed.
	if (!g_initialized)
	{
		g_dcQd = mean_qd;
		g_dcCw = mean_cw;
		GoertzelInit(fs, n);
		for (int i = 0; i < n; ++i)
			GoertzelFeed(s[i], c[i]);
	}

	// ── 3. Extract power at each candidate bin ────────────────────
	// Goertzel power formula:
	//   P = s1^2 + s2^2 - coeff * s1 * s2
	// This equals |X(k)|^2 (unnormalised DFT magnitude squared).
	double best_qd_power = -1.0;
	int    best_k        = 0;

	// Per-bin amplitude arrays (normalised the same way as the FFT version:
	// amp[i] = |X[i]| * 2 / |X[0]|)
	std::vector<double> rtt_amp(G_BINS), cw_amp(G_BINS);

	for (int k = 0; k < G_BINS; ++k)
	{
		// Goertzel magnitude (not squared) for RTT channel
		double p_qd = g_s1[k]*g_s1[k] + g_s2[k]*g_s2[k]
		              - g_coeff[k]*g_s1[k]*g_s2[k];
		p_qd = std::sqrt(std::max(p_qd, 0.0));

		// Goertzel magnitude for cwnd channel
		double p_cw = g_c1[k]*g_c1[k] + g_c2[k]*g_c2[k]
		              - g_coeff[k]*g_c1[k]*g_c2[k];
		p_cw = std::sqrt(std::max(p_cw, 0.0));

		// Normalise: rtt_amp[k] = 2 * |X_rtt(k)| / |X_rtt(0)|
		//            cw_amp[k]  = 2 * |X_cw(k)|  / |X_cw(0)|
		rtt_amp[k] = (qd_dc_amp > 0.0) ? 2.0 * p_qd / qd_dc_amp : 0.0;
		cw_amp[k]  = (cw_dc_amp > 0.0) ? 2.0 * p_cw / cw_dc_amp  : 0.0;

		// Find dominant bin (same constraint as FFT version: freq > 1.5 Hz)
		if (g_freqs[k] > 1.5 && p_qd > best_qd_power)
		{
			best_qd_power = p_qd;
			best_k        = k;
		}
	}

	// ── 4. Fill outputs (identical semantics to the FFT version) ──
	*fm = g_freqs[best_k];              // dominant frequency (Hz)
	*am = (qd_dc_amp > 0.0)
	      ? best_qd_power / qd_dc_amp   // ≈ original *am = qst/n
	      : 0.0;

	// simiFactor: normalised L1 distance between RTT and cwnd spectra
	*simi = 0.0;
	for (int k = 0; k < G_BINS; ++k)
		*simi += std::fabs(rtt_amp[k] - cw_amp[k]);
	// The call-site divides by QdArray.size() — no change needed.
}

} // namespace ns3




