"""Compile real S1 sources with mocked platform boundaries. Not a product/device build."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import os

p = argparse.ArgumentParser()
p.add_argument('--cxx', default='g++')
p.add_argument('--cc', default='gcc')
a = p.parse_args()
here = Path(__file__).resolve().parent
subprocess.run([__import__('sys').executable, str(here / 'peer_initializer_host_check.py'), a.cxx], check=True)
subprocess.run([__import__('sys').executable, str(here / 'local_ipv6_cache_host_check.py'), a.cxx], check=True)
repo = here.parents[2]
profile = repo / 'services/stack/src/cp/bal/iposl'
channel = repo / 'services/service/src/ipshare'
with tempfile.TemporaryDirectory(prefix='p2-s1-') as tmp:
    out = Path(tmp)
    shutil.copytree(here / 'stubs', out, dirs_exist_ok=True)
    for source in [*profile.glob('src/*.[ch]'), profile / 'interface/iposl_profile.h',
                   channel / 'nearlink_ipshare_channel.cpp', channel / 'nearlink_ipshare_channel.h',
                   channel / 'nearlink_ipshare_tun.h', channel / 'nearlink_ipshare_link.h', channel / 'nearlink_ipshare_link.cpp', here.parent / 'sleip_probe_packets.h',
                   channel / 'nearlink_ipshare_ipv6.h', channel / 'nearlink_ipshare_ipv6.cpp',
                   repo / 'services/stack/src/cp/bsl/sle/qosm/interface/qosm_trans_channel.h',
                   repo / 'ipc_parcel/parcel/nearlink_ipshare_status.h',
                   repo / 'ipc_parcel/parcel/nearlink_ipshare_status.cpp',
                   repo / 'interfaces/def/nearlink_def.h',
                   repo / 'interfaces/def/nearlink_def_types.h',
                   repo / 'ipc_parcel/interface/nearlink_service_ipc_interface_code.h']:
        shutil.copyfile(source, out / source.name)
    if os.environ.get('P3_OWNER_REUSE_NEGATIVE'):
        link = out / 'nearlink_ipshare_link.cpp'
        text = link.read_text(encoding='utf-8')
        assert text.count('owners_[address] = epoch;') == 1
        link.write_text(text.replace('owners_[address] = epoch;', 'owners_.emplace(address, epoch);'), encoding='utf-8')
    (out / 'arpa').mkdir(exist_ok=True)
    (out / 'arpa/inet.h').write_text('#pragma once\n#include <winsock2.h>\n#include <ws2tcpip.h>\n#undef ERROR\n')
    before_ref = os.environ.get('P3_RESTART_BEFORE_REF')
    if before_ref:
        for name in ('iposl_server.c', 'iposl_client.c'):
            relative = (profile / 'src' / name).relative_to(repo).as_posix()
            payload = subprocess.check_output(['git', '-c', f'safe.directory={repo.as_posix()}',
                '-C', str(repo), 'show', before_ref + ':' + relative])
            (out / name).write_bytes(payload)
    # Codec is C in the stack, while the probe compiles as C++.
    subprocess.run([a.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(out),
                    '-c', str(out/'iposl_codec.c'), '-o', str(out/'codec.o')], check=True)
    for source in sorted(here.glob('*_test.cpp')):
        if os.environ.get('P3_HOST_TEST_FILTER') and source.stem != os.environ['P3_HOST_TEST_FILTER']:
            continue
        if source.stem != 'service_test':
            shutil.copyfile(here / 'stubs/nearlink_ipshare_service.h', out / 'nearlink_ipshare_service.h')
        if source.stem == 'service_test':
            for name in ('nearlink_ipshare_service.cpp', 'nearlink_ipshare_service.h'):
                shutil.copyfile(channel / name, out / name)
            packet_before = os.environ.get('P3_PACKET_GATE_BEFORE_REF')
            if packet_before:
                signature = 'bool NearlinkIpShareService::CanSend('
                def packet_gate(text):
                    start = text.index(signature)
                    end = text.index('{', start) + 1
                    depth = 1
                    while depth:
                        depth += (text[end] == '{') - (text[end] == '}')
                        end += 1
                    return text[start:end]
                path = 'services/service/src/ipshare/nearlink_ipshare_service.cpp'
                old = subprocess.check_output(['git', '-c', f'safe.directory={repo.as_posix()}',
                    '-C', str(repo), 'show', packet_before + ':' + path]).decode('utf-8')
                current = (out / 'nearlink_ipshare_service.cpp').read_text(encoding='utf-8')
                (out / 'nearlink_ipshare_service.cpp').write_text(
                    current.replace(packet_gate(current), packet_gate(old)), encoding='utf-8')
            setup_before = os.environ.get('P3_PEER_SETUP_BEFORE_REF')
            if setup_before:
                signature = 'int32_t NearlinkIpShareService::PrepareMode('
                def body(text):
                    start = text.index(signature)
                    end = text.index('{', start) + 1
                    depth = 1
                    while depth:
                        depth += (text[end] == '{') - (text[end] == '}')
                        end += 1
                    return text[start:end]
                path = 'services/service/src/ipshare/nearlink_ipshare_service.cpp'
                old = subprocess.check_output(['git', '-c', f'safe.directory={repo.as_posix()}',
                    '-C', str(repo), 'show', setup_before + ':' + path]).decode('utf-8')
                current = (out / 'nearlink_ipshare_service.cpp').read_text(encoding='utf-8')
                (out / 'nearlink_ipshare_service.cpp').write_text(current.replace(body(current), body(old)),
                    encoding='utf-8')
        exe = out / (source.stem + '.exe')
        subprocess.run([a.cxx, '-std=c++17', '-pthread', '-I'+str(out), str(source), str(out/'nearlink_ipshare_ipv6.cpp'), *([str(out/'nearlink_ipshare_link.cpp')] if source.stem in ('channel_test','service_test','shared_link_test') else []), '-lws2_32', '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        print(source.stem + '=PASS', flush=True)

    production = (profile / 'src/iposl_profile.c').read_text(encoding='utf-8')
    (out / 'profile_send.inc').write_text(production[production.index('/* The caller and CP task'):], encoding='utf-8')
    exe = out / 'profile_send_test.exe'
    subprocess.run([a.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(out),
                    str(here/'profile_send_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('profile_send_test=PASS (production CP send functions; platform/allocator mocked)', flush=True)

    syntax = out / 'syntax'
    syntax.mkdir()
    (syntax / 'nlstk_log.h').write_text('#define NLSTK_LOG_INFO(...) ((void)0)\n#define NLSTK_LOG_WARN(...) ((void)0)\n#define NLSTK_LOG_ERROR(...) ((void)0)\n')
    (syntax / 'cp_worker.h').write_text('#include <stdint.h>\nuint32_t CP_PostTaskBlocked(void (*cb)(void *),void *arg,void (*freeCb)(void *),int timeout);\n')
    includes = sorted({str(p.parent) for p in (repo/'services/stack').rglob('*.h')})
    for name in ('iposl_client.c', 'iposl_server.c', 'iposl_profile.c'):
        subprocess.run([a.cc, '-std=c11', '-fsyntax-only', '-I'+str(syntax),
                        *['-I'+d for d in includes], str(profile/'src'/name)], check=True)
    print('real_stack_headers_c_syntax=PASS (logging/CP worker platform boundary stubbed)', flush=True)

    subprocess.run([__import__('sys').executable, str(here / 'tun_close_host_check.py'), a.cxx], check=True)
