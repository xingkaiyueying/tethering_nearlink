
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
static int opens=0,writes=0,snapshots=0,kernelQueries=0;
static bool cpActive=false,snapshotReady=true;
static uint32_t snapshotIndex=7;
static std::vector<std::array<uint8_t,16>> localAddresses{Lla(1),Global(1)};
static std::function<void()> snapshotHook;
struct Delivery {uint16_t lcid;uint8_t pi;std::vector<uint8_t> packet;};
static std::vector<Delivery> delivered;
extern "C" uint32_t QOSM_TransChannelCreate(const QOSM_TransChannelParams_S*) {return 0;}
extern "C" uint32_t QOSM_TransChannelDestroy(const QOSM_TransChannelReleaseParams_S*) {return 0;}
static int32_t Deliver(uint16_t l,uint8_t t,uint8_t pi,const uint8_t*p,uint16_t n,uint64_t g) {
    if(!NearlinkIpShareService::CanSend(l,t,pi,g))return -1;
    delivered.push_back({l,pi,{p,p+n}});return 0;
}
extern "C" int32_t IposlProfileSendIp(uint16_t l,uint8_t t,uint8_t pi,const uint8_t*p,uint16_t n,uint64_t g) {
    assert(!cpActive && "CP receive must never synchronously repost to its own worker");return Deliver(l,t,pi,p,n,g);
}
extern "C" int32_t IposlProfileSendIpOnCp(uint16_t l,uint8_t t,uint8_t pi,const uint8_t*p,uint16_t n,uint64_t g) {
    assert(cpActive && "TUN sender must hand off to CP");return Deliver(l,t,pi,p,n,g);
}
namespace OHOS::Nearlink {
bool NearlinkIpShareService::CanSend(uint16_t l,uint8_t t,uint8_t pi,uint64_t g) {
    for(const auto& c:g_channelRegistry)if(c->CanSend(l,t,pi,g))return true;
    return false;
}
NearlinkIpShareTun::~NearlinkIpShareTun() {}
int32_t NearlinkIpShareTun::Open(const PacketCallback&callback) {
    fd_=1;callback_=callback;++opens;return 0;
}
void NearlinkIpShareTun::Close(){fd_=-1;}
int32_t NearlinkIpShareTun::Write(const uint8_t*,uint16_t){++writes;return fd_>=0?0:-1;}
bool NearlinkIpShareTun::IsOpen()const{return fd_>=0;}
bool NearlinkIpShareTun::ParseIpv6Evidence(const std::string&,uint32_t,uint8_t*,const std::string&){return false;}
bool NearlinkIpShareTun::IsIpv6AddressUsable(const uint8_t *a,const std::string&){++kernelQueries;return a[0]==0xfe&&a[15]==1;}
bool NearlinkIpShareTun::ReadLocalIpv6Addresses(uint32_t &index,
    std::vector<std::array<uint8_t,16>> &addresses,const std::string &name) {
    assert(name=="sleip0");++snapshots;index=snapshotIndex;addresses=localAddresses;
    auto hook=std::move(snapshotHook);snapshotHook={};if(hook)hook();return snapshotReady;
}
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
static int32_t ReceiveOnCp(NearlinkIpShareLink &link,NearlinkIpShareChannel &source,
    const uint8_t *data,uint16_t size,uint64_t epoch) {
    assert(!cpActive);cpActive=true;int32_t result=link.Receive(source,data,size,epoch);cpActive=false;return result;
}
static void Dhcp(NearlinkIpShareLink&link,NearlinkIpShareChannel&c,uint8_t host,uint8_t message){
    uint8_t packet[300];ProbeDhcp(packet,message,c.clientKey_);
    packet[276]=host;if(message==2||message==5)packet[47]=host;
    if(message==1||message==3)assert(ReceiveOnCp(link,c,packet,300,c.generation_)==0);
    else link.FromTun(packet,300);
}

static void AddressReuseRegression()
{
    auto link=std::make_shared<NearlinkIpShareLink>();assert(link->Open(200)==0);
    auto a=Member(link,10),b=Member(link,11),c=Member(link,12);
    Dhcp(*link,*a,2,1);Dhcp(*link,*a,2,2);Dhcp(*link,*a,2,3);Dhcp(*link,*a,2,5);
    a->leaseExpiry_=std::chrono::steady_clock::now()-std::chrono::seconds(1);
    Dhcp(*link,*b,2,1);Dhcp(*link,*b,2,2);Dhcp(*link,*b,2,3);Dhcp(*link,*b,2,5);
    uint8_t address[]={192,168,77,2};
    assert(!a->OwnsAddress(address,false)&&b->OwnsAddress(address,false));
    assert(link->owners_.at(std::string(reinterpret_cast<const char*>(address),4))==11);
    uint8_t data[1500];ProbeData(data,1,true,gatewayId,b->peer_,0);
    delivered.clear();link->FromTun(data,1500);assert(delivered.size()==1&&delivered[0].lcid==11);
    ProbeData(data,1,false,a->peer_,gatewayId,0);
    assert(ReceiveOnCp(*link,*a,data,1500,10)!=0);
    Dhcp(*link,*c,3,1);Dhcp(*link,*c,3,2);Dhcp(*link,*c,3,3);Dhcp(*link,*c,3,5);
    ProbeData(data,1,false,c->peer_,gatewayId,0);data[15]=3;data[19]=2;
    delivered.clear();assert(ReceiveOnCp(*link,*c,data,1500,12)==0&&delivered.size()==1&&delivered[0].lcid==11);
    // A live lease keeps its owner when another member attempts to claim the address.
    link->UpdateOwnerLocked(std::string(reinterpret_cast<const char*>(address),4),12);
    assert(link->owners_.at(std::string(reinterpret_cast<const char*>(address),4))==11);
    auto ra=Ra();link->FromTun(ra.data(),ra.size());
    auto dad=Dad(Global(10));assert(ReceiveOnCp(*link,*a,dad.data(),dad.size(),10)==0);
    auto first=Echo(Global(10),Global(1));assert(ReceiveOnCp(*link,*a,first.data(),first.size(),10)==0);
    a->ipv6_.Expire(UINT64_MAX);
    b->ipv6_.Reset();
    auto now=std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    assert(b->ipv6_.Authorize(ra.data(),ra.size(),false,gatewayId,now));
    assert(ReceiveOnCp(*link,*b,dad.data(),dad.size(),11)==0);
    assert(ReceiveOnCp(*link,*b,first.data(),first.size(),11)==0);
    auto ip=Global(10);assert(link->owners_.at(std::string(reinterpret_cast<const char*>(ip.data()),16))==11);
    auto reply=Echo(Global(1),Global(10));delivered.clear();link->FromTun(reply.data(),reply.size());
    assert(delivered.size()==1&&delivered[0].lcid==11);
    assert(ReceiveOnCp(*link,*a,first.data(),first.size(),10)!=0);
    link->Close();a->Close();b->Close();c->Close();
    puts("R01 IPv4 lease reuse, IPv6 address expiry/reuse, member/local delivery and live-owner conflict PASS");
}

int main(){
    auto link=std::make_shared<NearlinkIpShareLink>();assert(link->Open(100)==0&&opens==1);
    link->RefreshLocalAddresses(100);assert(snapshots==1&&link->localIpv6_.size()==2);
    auto a=Member(link,2),b=Member(link,3),v4=Member(link,4,1);
    // Identical xid values remain isolated by authenticated client key.
    Dhcp(*link,*a,2,1);Dhcp(*link,*b,3,1);
    delivered.clear();Dhcp(*link,*b,3,2);assert(delivered.size()==1&&delivered[0].lcid==3);
    Dhcp(*link,*a,2,2);Dhcp(*link,*a,2,3);Dhcp(*link,*b,3,3);
    Dhcp(*link,*b,3,5);Dhcp(*link,*a,2,5);
    assert(a->boundIp_==0xc0a84d02&&b->boundIp_==0xc0a84d03);
    uint8_t forged[300];ProbeDhcp(forged,1,b->peer_);
    assert(ReceiveOnCp(*link,*a,forged,300,2)!=0);
    uint8_t data[1500];ProbeData(data,1,false,a->peer_,gatewayId,0);data[19]=3;
    delivered.clear();int before=writes;
    assert(ReceiveOnCp(*link,*a,data,1500,2)==0&&writes==before&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==std::vector<uint8_t>(data,data+1500));
    std::array<uint8_t,10> tombstone{};
    link->transactions_[tombstone]={90,std::chrono::steady_clock::now()-std::chrono::seconds(1)};
    assert(ReceiveOnCp(*link,*a,data,1500,2)==0&&link->transactions_.count(tombstone));
    Dhcp(*link,*b,3,1);assert(!link->transactions_.count(tombstone));
    Dhcp(*link,*b,3,2);Dhcp(*link,*b,3,3);Dhcp(*link,*b,3,5);
    data[15]=3;assert(ReceiveOnCp(*link,*a,data,1500,2)!=0);data[15]=2;
    memset(data+16,255,4);delivered.clear();before=writes;
    assert(ReceiveOnCp(*link,*a,data,1500,2)==0&&writes==before+1&&delivered.size()==1);
    assert(delivered[0].lcid==3); // unconfigured v4 member has no business authorization
    auto ra=Ra();delivered.clear();link->FromTun(ra.data(),ra.size());assert(delivered.size()==2);
    auto da=Dad(Global(2));auto db=Dad(Global(3));
    assert(ReceiveOnCp(*link,*a,da.data(),da.size(),2)==0);
    assert(ReceiveOnCp(*link,*b,db.data(),db.size(),3)==0);
    auto first=Echo(Global(2),Global(1));assert(ReceiveOnCp(*link,*a,first.data(),first.size(),2)==0);
    first=Echo(Global(3),Global(1));assert(ReceiveOnCp(*link,*b,first.data(),first.size(),3)==0);
    // Local delivery uses only the control-plane snapshot; ordinary packets do no /proc/ioctl lookup.
    int reads=snapshots,queries=kernelQueries;
    auto local=Echo(Global(2),Global(1));before=writes;
    for(unsigned i=0;i<100;++i)assert(ReceiveOnCp(*link,*a,local.data(),local.size(),2)==0);
    assert(writes==before+100&&snapshots==reads&&kernelQueries==queries);
    link->localCacheUntil_=std::chrono::steady_clock::now()-std::chrono::seconds(1);
    before=writes;assert(ReceiveOnCp(*link,*a,local.data(),local.size(),2)==0&&writes==before);
    localAddresses={Lla(1)};link->RefreshLocalAddresses(100);
    assert(ReceiveOnCp(*link,*a,local.data(),local.size(),2)==0&&writes==before);
    localAddresses.push_back(Global(1));link->RefreshLocalAddresses(100);
    assert(ReceiveOnCp(*link,*a,local.data(),local.size(),2)==0&&writes==++before);
    snapshotReady=false;link->RefreshLocalAddresses(100);assert(link->localIpv6_.empty());
    snapshotReady=true;snapshotIndex=8;link->RefreshLocalAddresses(100);assert(link->localIpv6_.empty());
    snapshotIndex=7;link->RefreshLocalAddresses(100);assert(link->localIpv6_.size()==2);
    reads=snapshots;link->RefreshLocalAddresses(99);assert(snapshots==reads);
    // A slower older dump cannot overwrite a newer snapshot.
    snapshotHook=[&](){localAddresses={Lla(1)};link->RefreshLocalAddresses(100);};
    link->RefreshLocalAddresses(100);assert(link->localIpv6_.size()==1);
    localAddresses.push_back(Global(1));link->RefreshLocalAddresses(100);
    auto echo=Echo(Global(2),Global(3));delivered.clear();before=writes;
    assert(ReceiveOnCp(*link,*a,echo.data(),echo.size(),2)==0&&writes==before&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==echo);
    auto multicast=Echo(Global(2),Group(1));delivered.clear();before=writes;
    assert(ReceiveOnCp(*link,*a,multicast.data(),multicast.size(),2)==0&&writes==before+1&&delivered.size()==1);
    assert(delivered[0].lcid==3&&delivered[0].packet==multicast);
    echo=Echo(Global(3),Global(2));assert(ReceiveOnCp(*link,*a,echo.data(),echo.size(),2)!=0);
    // Original member ND identity survives relay, and a confirmed owner defeats a newcomer DAD.
    auto duplicate=Dad(Global(2));assert(ReceiveOnCp(*link,*b,duplicate.data(),duplicate.size(),3)==0);
    auto na=Na(Global(2),Group(1));assert(Policy::AddLayer2Option(na,a->peer_));
    auto forgedNa=Na(Global(2),Group(1));assert(Policy::AddLayer2Option(forgedNa,gatewayId));
    assert(ReceiveOnCp(*link,*a,forgedNa.data(),forgedNa.size(),2)!=0);
    delivered.clear();assert(ReceiveOnCp(*link,*a,na.data(),na.size(),2)==0);
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
    assert(ReceiveOnCp(*link,*b,da.data(),da.size(),3)!=0);
    link->Remove(2);a->Close();link->Remove(4);v4->Close();assert(link->tun_.IsOpen());
    auto returning=std::make_shared<NearlinkIpShareChannel>();
    assert(returning->Initialize([](bool,int32_t,uint64_t){})==0);
    assert(returning->SetPeer(a->peer_,0,true,a->peer_,gatewayId,6)==0);
    assert(returning->PrepareMode(1)==0&&returning->EnableMode(1)==0&&link->Add(returning,6));
    returning->lcid_=6;returning->tcid_=6;returning->channelEstablished_=true;
    uint8_t stale[300];ProbeDhcp(stale,1,a->peer_);
    assert(ReceiveOnCp(*link,*returning,stale,300,6)!=0);
    stale[35]=43;assert(ReceiveOnCp(*link,*returning,stale,300,6)==0);
    link->Remove(6);returning->Close();
    auto again=Member(link,5);assert(opens==1);
    snapshotHook=[&](){link->Close();};link->RefreshLocalAddresses(100);
    assert(!link->tun_.IsOpen()&&link->localIpv6_.empty());
    assert(ReceiveOnCp(*link,*again,da.data(),da.size(),5)!=0);
    AddressReuseRegression();
    std::cout<<"shared_link=PASS (CP relay context, cached local delivery/withdrawal/expiry/races, DHCP-only cleanup, DHCP xid collisions, source spoofing, unicast/broadcast, mixed modes, IPv6 relay/DAD, epoch revocation, final member retention)\n";
}
