/**
 * @Author: nanerlee
 * @Date:   2017-05-23T21:51:30+08:00
 * @Email:  nanerlee@qq.com
 * @Last modified by:   nanerlee
 * @Last modified time: 2017-05-24T10:12:44+08:00
 * @Copyright: Copyright (c) by NanerLee. All Rights Reserved.
 */

#ifndef TRACEROUTE_H
#define TRACEROUTE_H

#include "ns3/application.h"
#include "ns3/icmpv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace ns3 {

class Socket;

class Traceroute : public Application
{
  public:
    // static TypeId GetTypeId (void);
    Traceroute();
    virtual ~Traceroute();
    void Setup(uint32_t index, Ipv4Address address);

  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void Check();
    void Receive();
    void Send();
    void OutputToFile();

    Ptr<Socket> socket;
    Ipv4Address remote;
    uint32_t    remote_index;
    uint8_t     ttl;
    uint16_t    seq;
    bool        done;
    int         num;  // number of check for one send
    std::map<uint16_t, std::string> result;
};

/*TypeId Traceroute :: GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Traceroute")
    .SetParent<Application> ()
    .SetGroupName("Internet-Apps")
    .AddConstructor<V4Ping> ()
    .AddAttribute ("Remote",
                   "The address of the machine we want to traceroute.",
                   Ipv4AddressValue (),
                   MakeIpv4AddressAccessor (&Traceroute :: remote),
                   MakeIpv4AddressChecker ())
    ;
  return tid;
}*/

Traceroute::Traceroute() : socket(0), ttl(0), seq(0), done(false), num(0)
{
}

Traceroute::~Traceroute()
{
}

void Traceroute::Setup(uint32_t index, Ipv4Address address)
{
    remote_index = index;
    remote       = address;
}

void Traceroute::StartApplication(void)
{
    socket = Socket::CreateSocket(
        m_node, TypeId::LookupByName("ns3::Ipv4RawSocketFactory"));
    socket->SetAttribute("Protocol", UintegerValue(1));  // use icmp protocol
    InetSocketAddress src = InetSocketAddress(Ipv4Address::GetAny(), 0);
    socket->Bind(src);
    InetSocketAddress dst = InetSocketAddress(remote, 0);
    socket->Connect(dst);

    Send();
}

void Traceroute::Send()
{
    ++seq;
    if (seq > 16)
    {
        done = true;
        Simulator::Schedule(Simulator::Now() + Seconds(0.1), &Traceroute::StopApplication, this);
        return;
    }

    // construct Icmpv4Echo packet
    Ptr<Packet> p = Create<Packet>();
    Icmpv4Echo  echo;
    echo.SetSequenceNumber(seq);
    echo.SetIdentifier(0);
    uint8_t* data = new uint8_t[56];
    for (uint32_t i = 0; i < 56; ++i)
    {
        data[i] = 0;
    }
    Ptr<Packet> dataPacket = Create<Packet>((uint8_t*)data, 56);
    echo.SetData(dataPacket);
    p->AddHeader(echo);
    Icmpv4Header header;
    header.SetType(Icmpv4Header::ECHO);
    header.SetCode(0);
    if (Node::ChecksumEnabled())
    {
        header.EnableChecksum();
    }
    p->AddHeader(header);
    ++ttl;
    socket->SetIpTtl(ttl);  // set TTL value
    socket->Send(p);
    delete[] data;

    Simulator::Schedule(Seconds(0.1), &Traceroute::Check, this);
}

void Traceroute::Check()
{
    ++num;
    if (socket->GetRxAvailable() > 0 && num <= 10)  // receive data
    {
        Receive();
        num = 0;
        if (done != true)
        {
            Send();
        }
        else
        {
            Simulator::Schedule(
                Simulator::Now() + Seconds(0.1), &Traceroute::StopApplication, this);
        }
    }
    else if (socket->GetRxAvailable() == 0 && num <= 10)  // timeout
    {
        Simulator::Schedule(Seconds(0.1), &Traceroute::Check, this);
    }
    else if (socket->GetRxAvailable() == 0 && num > 10)
    {
        num = 0;
        result.insert(make_pair(seq, "*.*.*.*"));  // not response
        Send();
    }
}

void Traceroute::Receive()
{
    Address           from;
    Ptr<Packet>       p        = socket->RecvFrom(0xffffffff, 0, from);
    InetSocketAddress realFrom = InetSocketAddress::ConvertFrom(from);

    // handle packet
    Ipv4Header ipv4;
    p->RemoveHeader(ipv4);  // remove IPv4 header
    Icmpv4Header icmp;
    p->RemoveHeader(icmp);                              // remove ICMP header
    if (icmp.GetType() == Icmpv4Header::TIME_EXCEEDED)  // is TIME_EXCEEDED?
    {
        Icmpv4TimeExceeded timeout;
        p->RemoveHeader(timeout);  // remove TIME_EXCEEDED header
        uint8_t payload[8];
        timeout.GetData(payload);

        std::ostringstream oss;
        realFrom.GetIpv4().Print(oss);   // get IPv4 address
        uint32_t echo_seq = payload[7];  // get echo_seq(generally echo < 10)
        if (echo_seq == seq)
        {
            result.insert(make_pair(echo_seq, oss.str()));
        }
    }
    else if (icmp.GetType() == Icmpv4Header::ECHO_REPLY)  // is ECHO_REPLY?
    {
        Icmpv4Echo echo;
        p->RemoveHeader(echo);  // remove ECHO_REPLY header
        result.insert(make_pair(echo.GetSequenceNumber(), oss.str()));
        done = true;
    }
}

// print the traceroute data to file.
void Traceroute::OutputToFile()
{
    std::ostringstream oss;
    oss << "Traceroute-" << (m_node->GetId()) << "-" << remote_index << ".txt";
    std::string filename = oss.str();

    std::ofstream ofs;
    ofs.open(filename.c_str());
    std::map<uint16_t, std::string>::iterator end   = result.end();
    std::map<uint16_t, std::string>::iterator begin = result.begin();

    // ofs << "# traceroute from " << m_node ->GetId() << " to " << remote_index
    // << "(" << remote << ")" <<" :"<< endl;
    ofs << "# trace: " << m_node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()
        << " -> " << remote << '\n';

    while (begin != end)
    {
        // ofs << begin -> first << '\t'
        ofs << begin->second << '\n';
        ++begin;
    }
    ofs.close();
}

void Traceroute::StopApplication(void)
{
    socket->Close();
    OutputToFile();
}

}  // end of namespace

#endif
