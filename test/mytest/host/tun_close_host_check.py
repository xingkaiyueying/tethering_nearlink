"""Execute production TUN close/read ordering with a deterministic blocked poll double."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[3]
source = repo / 'services/service/src/ipshare/nearlink_ipshare_tun.cpp'
production = source.read_text(encoding='utf-8')
before = os.environ.get('P3_TUN_BEFORE_REF')
if before:
    production = subprocess.check_output(['git', '-c', f'safe.directory={repo.as_posix()}', '-C', str(repo),
        'show', before + ':services/service/src/ipshare/nearlink_ipshare_tun.cpp']).decode()
close = production[production.index('void NearlinkIpShareTun::Close()'):production.index('int32_t NearlinkIpShareTun::Write(')]
read = production[production.index('void NearlinkIpShareTun::ReadLoop()'):production.rindex('} // namespace')]
with tempfile.TemporaryDirectory(prefix='p3-tun-close-') as directory:
    root = Path(directory)
    (root / 'nearlink_ipshare_tun.h').write_text(source.with_suffix('.h').read_text(encoding='utf-8'), encoding='utf-8')
    (root / 'check.cpp').write_text(r'''
#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>
#define private public
#include "nearlink_ipshare_tun.h"
#undef private
using ssize_t = long long;
constexpr size_t IP_SHARE_PACKET_MAX=1500;
constexpr short POLLIN=1;
struct pollfd {int fd;short events,revents;};
std::mutex gate;
std::condition_variable cv;
bool entered=false,released=false;
std::atomic_bool stopped{false},closed{false};
std::atomic_int reads{0},callbacks{0};
bool autoWake=false,batch=false;
int wakeWrites=0,polls=0;
int poll(pollfd*p,int,int) {
 if(batch){++polls;p->revents=POLLIN;return 1;}
 std::unique_lock<std::mutex> lock(gate);entered=true;cv.notify_all();
 cv.wait(lock,[]{return released;});p->revents=POLLIN;return 1;
}
ssize_t read(int,void*p,size_t){++reads;if(batch){std::memset(p,0x45,64);return 64;}return 1;}
ssize_t write(int fd,const void*,size_t n){
 if(autoWake){assert(fd==19);++wakeWrites;{std::lock_guard<std::mutex> lock(gate);released=true;}cv.notify_all();}
 return n;
}
int close(int){closed=true;assert(stopped);return 0;}
void logInfo(const char*text) {if(std::strstr(text,"read loop stopped"))stopped=true;}
template<class... Args> void logInfo(const char*text,Args...){logInfo(text);}
#define HILOGI(...) logInfo(__VA_ARGS__)
#define HILOGE(...) ((void)0)
namespace OHOS::Nearlink {
''' + close + read + r'''
NearlinkIpShareTun::~NearlinkIpShareTun(){Close();}
}
int main(){
 OHOS::Nearlink::NearlinkIpShareTun tun;
 tun.fd_=17;tun.running_=true;tun.callback_=[](const uint8_t*,uint16_t){++callbacks;};
 tun.reader_=std::thread([&]{tun.ReadLoop();});
 {std::unique_lock<std::mutex> lock(gate);cv.wait(lock,[]{return entered;});}
 std::thread cleanup([&]{tun.Close();});
 while(tun.running_.load())std::this_thread::yield();
 {std::lock_guard<std::mutex> lock(gate);released=true;}cv.notify_all();
 cleanup.join();assert(closed && stopped && reads==0 && callbacks==0 && tun.fd_==-1);
 // Production wake descriptor unblocks poll without the old 200 ms idle timeout.
 entered=released=false;stopped=closed=false;autoWake=true;
 OHOS::Nearlink::NearlinkIpShareTun wakeTun;
 wakeTun.fd_=17;wakeTun.wakeRead_=18;wakeTun.wakeWrite_=19;wakeTun.running_=true;
 wakeTun.callback_=[](const uint8_t*,uint16_t){++callbacks;};
 wakeTun.reader_=std::thread([&]{wakeTun.ReadLoop();});
 {std::unique_lock<std::mutex> lock(gate);cv.wait(lock,[]{return entered;});}
 wakeTun.Close();assert(wakeWrites==1 && stopped && closed && reads==0 && callbacks==0);
 // A ready fd carries multiple distinct packets per poll; packet boundaries remain intact.
 autoWake=false;batch=true;stopped=closed=false;
 OHOS::Nearlink::NearlinkIpShareTun batchTun;
 batchTun.fd_=17;batchTun.running_=true;
 batchTun.callback_=[&](const uint8_t*p,uint16_t n){assert(p[0]==0x45 && n==64);if(++callbacks==40)batchTun.running_=false;};
 batchTun.ReadLoop();batchTun.Close();assert(reads==40 && callbacks==40 && polls<40 && stopped);
}
''', encoding='utf-8')
    executable = root / 'check.exe'
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else 'g++', '-std=c++17', '-pthread',
        '-I' + str(root), str(root / 'check.cpp'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('TUN production close/read race: join before fd release, no post-stop read/callback, explicit wake and packet batching PASS')
