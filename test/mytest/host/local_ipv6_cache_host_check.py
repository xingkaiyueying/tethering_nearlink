"""Execute the production kernel-snapshot parser; no Linux/device API is mocked into the parser."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = Path(__file__).resolve().parents[3] / 'services/service/src/ipshare/nearlink_ipshare_tun.cpp'
text = source.read_text(encoding='utf-8')
parser = text[text.index('static bool ParseLocalIpv6Addresses('):text.index('bool NearlinkIpShareTun::ReadLocalIpv6Addresses(')]
with tempfile.TemporaryDirectory(prefix='local-ipv6-') as directory:
    out = Path(directory)
    (out / 'check.cpp').write_text(r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
''' + parser + r'''
int main() {
 std::vector<std::array<uint8_t,16>> addresses;
 auto parse=[&](const std::string &rows){std::istringstream input(rows);return ParseLocalIpv6Addresses(input,7,"sleip0",addresses);};
 const std::string local="fe800000000000000000000000000001 07 40 20 80 sleip0\n";
 const std::string global="20010DB8000000000000000000000001 07 40 00 00 sleip0\n";
 assert(parse(local+global)&&addresses.size()==2&&addresses[0][0]==0xfe&&addresses[1][2]==0x0d);
 for(const char *flags:{"40","08","04","48"}) {
  assert(parse(std::string("fe800000000000000000000000000001 07 40 20 ")+flags+" sleip0\n")&&addresses.empty());
 }
 assert(parse("fe800000000000000000000000000001 08 40 20 80 sleip0\n")&&addresses.empty());
 assert(parse("fe800000000000000000000000000001 07 40 20 80 other0\n")&&addresses.empty());
 assert(parse("zz800000000000000000000000000001 07 40 20 80 sleip0\n")&&addresses.empty());
 assert(parse("")&&addresses.empty());
 assert(!parse(local+"2001 07 40")&&addresses.empty());
 assert(!parse(local+"20010000000000000000000000000001 07 40 00 00 sleip0 extra")&&addresses.empty());
 std::string full;for(unsigned i=0;i<64;++i)full+=local;
 assert(parse(full)&&addresses.size()==64);assert(!parse(full+global)&&addresses.empty());
}
''', encoding='utf-8')
    executable = out / 'check.exe'
    subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(out / 'check.cpp'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('local_ipv6_snapshot=PASS (interface/index, kernel flags, hex, truncation, empty snapshot, capacity)')
