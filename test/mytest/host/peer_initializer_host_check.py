"""Compile the production peer snapshot initializer with missing-field warnings as errors."""
from pathlib import Path
import subprocess
import sys
import tempfile

here=Path(__file__).resolve().parent
repo=here.parents[2]
source_path='services/service/src/ipshare/nearlink_ipshare_service.cpp'
old_ref='062248018f936f2ec4cddb9919e8080e47b233c4'
def body(text):
 start=text.index('void NearlinkIpShareService::RefreshPeerLinksLocked()')
 end=text.index('{',start)+1
 depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}')
  end+=1
 return text[start:end]
prefix=r'''
#include <cassert>
#include <memory>
#include <vector>
#include "nearlink_ipshare_status.h"
namespace OHOS::Nearlink {
class NearlinkIpShareService {public:
 struct GatewayPeer {uint64_t epoch=12;int32_t selectedMode=3;bool releasing=false,active=true;};
 bool gatewayAny_=true;
 std::vector<std::unique_ptr<GatewayPeer>> gatewayPeers_;
 NearlinkIpShareStatus status_;
 void RefreshPeerLinksLocked();
};
'''
suffix=r'''
}
int main(){
 using namespace OHOS::Nearlink;
 NearlinkIpShareService service;service.status_.requestedMode=NearlinkIpShareMode::DUAL_STACK;
 service.gatewayPeers_.resize(2);service.gatewayPeers_[1]=std::make_unique<NearlinkIpShareService::GatewayPeer>();
 service.RefreshPeerLinksLocked();assert(service.status_.peerLinks.size()==1);
 const auto &link=service.status_.peerLinks.front();
 assert(link.slot==1&&link.generation==12&&link.selectedMode==3&&link.ifaceName=="sleip0");
 assert(link.active&&!link.releasing&&link.ipv4Address.empty()&&link.ipv4LeaseLifetime==0&&link.ipv6Addresses.empty());
 assert(service.status_.ifaceName=="sleip0"&&service.status_.selectedMode==NearlinkIpShareMode::DUAL_STACK);
 service.gatewayAny_=false;service.RefreshPeerLinksLocked();assert(service.status_.peerLinks.empty());
}
'''
with tempfile.TemporaryDirectory(prefix='peer-initializer-') as directory:
 out=Path(directory)
 def compile_code(text,name):
  source=out/(name+'.cpp');source.write_text(prefix+body(text)+suffix,encoding='utf-8')
  exe=out/(name+'.exe')
  result=subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'g++','-std=c++17','-Wall','-Wextra',
   '-Werror=missing-field-initializers','-I'+str(here/'stubs'),'-I'+str(repo/'ipc_parcel/parcel'),str(source),
   str(repo/'ipc_parcel/parcel/nearlink_ipshare_status.cpp'),'-o',str(exe)],capture_output=True)
  return result,exe
 old=subprocess.check_output(['git','-C',str(repo),'show',old_ref+':'+source_path]).decode('utf-8')
 negative,_=compile_code(old,'before')
 assert negative.returncode!=0 and b'missing' in negative.stderr.lower(),negative.stderr
 current,exe=compile_code((repo/source_path).read_text(encoding='utf-8'),'after')
 if current.returncode:raise RuntimeError(current.stderr.decode('utf-8',errors='replace'))
 subprocess.run([str(exe)],check=True)
 print('peer snapshot initializer=PASS: old production code rejected by -Werror=missing-field-initializers; corrected code compiled and executed')
