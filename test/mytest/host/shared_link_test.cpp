
#include <cassert>
#include <cerrno>
#include <array>
#include <cstring>
#include <mutex>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include "iposl_codec.c"
#include "sleip_probe_packets.h"
#include "ipv6_test_packets.h"
#define private public
#include "nearlink_ipshare_channel.cpp"
#undef private
using namespace OHOS::Nearlink;
static int opens=0,writes=0;
struct Delivery {uint16_t lcid;uint8_t pi;std::vector<uint8_t> packet;};
static std::vector<Delivery> delivered;
extern "C" uint32_t QOSM_TransChannelCreate(const QOSM_TransChannelParams_S*) {return 0;}
extern "C" uint32_t QOSM_TransChannelDestroy(const QOSM_TransChannelReleaseParams_S*) {return 0;}
extern "C" int32_t IposlProfileSendIp(uint16_t l,uint8_t t,uint8_t pi,const uint8_t*p,uint16_t n,uint64_t g) {
    if(!NearlinkIpShareService::CanSend(l,t,pi,g))return -1;
    delivered.push_back({l,pi,{p,p+n}});return 0;
}
namespace OHOS::Nearlink {
bool NearlinkIpShareService::CanSend(uint16_t l,uint8_t t,uint8_t pi,uint64_t g) {
    for(const auto& c:g_channelRegistry)if(c->CanSend(l,t,pi,g))return true;
    return false;
}
NearlinkIpShareTun::~NearlinkIpShareTun() {}
int32_t NearlinkIpShareTun::Open(const PacketCallback&callback,const std::string&name) {
    assert(name=="sleip0");fd_=1;callback_=callback;++opens;return 0;
}
void NearlinkIpShareTun::Close(){fd_=-1;}
int32_t NearlinkIpShareTun::Write(const uint8_t*,uint16_t){++writes;return fd_>=0?0:-1;}
bool NearlinkIpShareTun::IsOpen()const{return fd_>=0;}
bool NearlinkIpShareTun::ParseIpv6Evidence(const std::string&,uint32_t,uint8_t*,const std::string&){return false;}
bool NearlinkIpShareTun::IsIpv6AddressUsable(const uint8_t *a,const std::string&){return a[0]==0xfe&&a[15]==1;}
}
static std::shared_ptr<NearlinkIpShareChannel> Member(std::shared_ptr<NearlinkIpShareLink> link,uint8_t id,uint8_t mode=3){
    auto c=std::make_shared<NearlinkIpShareChannel>();
    uint8_t peer[6]={2,1,2,3,4,id};
    assert(c->Initialize([](bool,int32_t,uint64_t){})==0);
    assert(c->SetPeer(peer,0,true,peer,gatewayId,id)==0);
    assert(c->PrepareMode(mode)==0&&c->EnableMode(mode)==0&&link->Add(c,id));
    c->channelEstablished_=true;c->lcid_=id;c->tcid_=id;
    return c;
}
static void Dhcp(NearlinkIpShareLink&link,NearlinkIpShareChannel&c,uint8_t host,uint8_t message){
    uint8_t packet[300];ProbeDhcp(packet,message,c.clientKey_);
    packet[276]=host;if(message==2||message==5)packet[47]=host;
    if(message==1||message==3)assert(link.Receive(c,packet,300,c.generation_)==0);
    else link.FromTun(packet,300);
}
int main(){
    auto link=std::make_shared<NearlinkIpShareLink>();assert(link->Open(100)==0&&opens==1);
    uint8_t dnsTrace[40]{};dnsTrace[0]=0x45;dnsTrace[9]=17;dnsTrace[23]=53;
    errno=EAGAIN;link->TraceDnsLocked("TEST",dnsTrace,sizeof(dnsTrace),2);
    assert(errno==EAGAIN&&link->traceCount_==1);
    link->TraceDnsLocked("TEST",dnsTrace,sizeof(dnsTrace)-1,2);assert(link->traceCount_==1);
    dnsTrace[23]=54;link->TraceDnsLocked("TEST",dnsTrace,sizeof(dnsTrace),2);assert(link->traceCount_==1);
    dnsTrace[23]=53;
    for(unsigned i=0;i<130;++i)link->TraceDnsLocked("TEST",dnsTrace,sizeof(dnsTrace),2);
    assert(errno==EAGAIN&&link->traceSuppressed_>0);
    auto a=Member(link,2),b=Member(link,3),v4=Member(link,4,1);
    // Identical xid values remain isolated by authenticated client key.
    Dhcp(*link,*a,2,1);Dhcp(*link,*b,3,1);
    delivered.clear();Dhcp(*link,*b,3,2);assert(delivered.size()==1&&delivered[0].lcid==3);
    Dhcp(*link,*a,2,2);Dhcp(*link,*a,2,3);Dhcp(*link,*b,3,3);
    Dhcp(*link,*b,3,5);Dhcp(*link,*a,2,5);
    assert(a->boundIp_==0xc0a84d02&&b->boundIp_==0xc0a84d03);
    uint8_t forged[300];ProbeDhcp(forged,1,b->peer_);
    assert(link->Receive(*a,forged,300,2)!=0);
    uint8_t data[1500];ProbeData(data,1,false,a->peer_,gatewayId,0);data[19]=3;
    delivered.clear();int before=writes;
    assert(link->Receive(*a,data,1500,2)==0&&writes==before&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==std::vector<uint8_t>(data,data+1500));
    data[15]=3;assert(link->Receive(*a,data,1500,2)!=0);data[15]=2;
    memset(data+16,255,4);delivered.clear();before=writes;
    assert(link->Receive(*a,data,1500,2)==0&&writes==before+1&&delivered.size()==1);
    assert(delivered[0].lcid==3); // unconfigured v4 member has no business authorization
    auto ra=Ra();delivered.clear();link->FromTun(ra.data(),ra.size());assert(delivered.size()==2);
    auto da=Dad(Global(2));auto db=Dad(Global(3));
    assert(link->Receive(*a,da.data(),da.size(),2)==0);
    assert(link->Receive(*b,db.data(),db.size(),3)==0);
    auto first=Echo(Global(2),Global(1));assert(link->Receive(*a,first.data(),first.size(),2)==0);
    first=Echo(Global(3),Global(1));assert(link->Receive(*b,first.data(),first.size(),3)==0);
    auto echo=Echo(Global(2),Global(3));delivered.clear();before=writes;
    assert(link->Receive(*a,echo.data(),echo.size(),2)==0&&writes==before&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==echo);
    auto multicast=Echo(Global(2),Group(1));delivered.clear();before=writes;
    assert(link->Receive(*a,multicast.data(),multicast.size(),2)==0&&writes==before+1&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==multicast);
    echo=Echo(Global(3),Global(2));assert(link->Receive(*a,echo.data(),echo.size(),2)!=0);
    // Original member ND identity survives relay, and a confirmed owner defeats a newcomer DAD.
    auto duplicate=Dad(Global(2));assert(link->Receive(*b,duplicate.data(),duplicate.size(),3)==0);
    auto na=Na(Global(2),Group(1));assert(Policy::AddLayer2Option(na,a->peer_));
    auto forgedNa=Na(Global(2),Group(1));assert(Policy::AddLayer2Option(forgedNa,gatewayId));
    assert(link->Receive(*a,forgedNa.data(),forgedNa.size(),2)!=0);
    delivered.clear();assert(link->Receive(*a,na.data(),na.size(),2)==0);
    assert(delivered.size()==1&&delivered[0].packet==na&&delivered[0].lcid==3);
    Policy terminal;auto rb=Ra();assert(terminal.Authorize(rb.data(),rb.size(),false,gatewayId,1));
    assert(terminal.ApplyLocal(Global(2),true,0x40,30,90,1));
    assert(!terminal.Authorize(na.data(),na.size(),false,gatewayId,1));
    assert(terminal.Authorize(na.data(),na.size(),false,gatewayId,1,true));
    assert(!terminal.Authorize(Echo(Global(2),Global(3)).data(),56,true,b->peer_,1));
    Policy many;
    assert(many.Authorize(rb.data(),rb.size(),false,gatewayId,1));
    for (uint8_t id=2;id<34;++id) {
        auto memberNa=Na(Global(id),Group(1));uint8_t identity[6]={2,1,2,3,4,id};
        assert(Policy::AddLayer2Option(memberNa,identity));
        assert(many.Authorize(memberNa.data(),memberNa.size(),false,gatewayId,1,true));
    }
    NearlinkIpSharePeerLink state;state.generation=2;a->ExportAddresses(state);
    assert(state.ipv4Address=="192.168.77.2"&&!state.ipv6Addresses.empty());
    link->Remove(3);b->Close();assert(opens==1&&link->tun_.IsOpen());
    assert(link->Receive(*b,da.data(),da.size(),3)!=0);
    link->Remove(2);a->Close();link->Remove(4);v4->Close();assert(link->tun_.IsOpen());
    auto returning=std::make_shared<NearlinkIpShareChannel>();
    assert(returning->Initialize([](bool,int32_t,uint64_t){})==0);
    assert(returning->SetPeer(a->peer_,0,true,a->peer_,gatewayId,6)==0);
    assert(returning->PrepareMode(1)==0&&returning->EnableMode(1)==0&&link->Add(returning,6));
    returning->lcid_=6;returning->tcid_=6;returning->channelEstablished_=true;
    uint8_t stale[300];ProbeDhcp(stale,1,a->peer_);
    assert(link->Receive(*returning,stale,300,6)!=0);
    stale[35]=43;assert(link->Receive(*returning,stale,300,6)==0);
    link->Remove(6);returning->Close();
    auto again=Member(link,5);assert(opens==1);link->Close();assert(!link->tun_.IsOpen());
    assert(link->Receive(*again,da.data(),da.size(),5)!=0);
    std::cout<<"shared_link=PASS (DHCP xid collisions, source spoofing, unicast/broadcast, mixed modes, IPv6 relay/DAD, epoch revocation, final member retention)\n";
}
