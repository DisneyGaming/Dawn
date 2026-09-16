"""Replay native close and cooldown branches. No live-process access or writes.

The native owner callbacks, diagnostics and clock are fixtures. The channel-close
routine and timer branch execute from the pinned executable. The adjusted clock
value comes from the compiled C++ production policy's regression executable.
"""
import hashlib
from pathlib import Path
import re
import subprocess
import sys
from unicorn import UC_HOOK_CODE, UcError
from unicorn.x86_const import *
from mercury_streaming_native import Native, BASE, DATA, IMAGE_SHA256

ROOT = Path(__file__).resolve().parents[2]
image = (ROOT/'build/baseline-02fc2c30/destiny2_unpacked.bin').read_bytes()
assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256
run = subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True)
stamp = int(re.search(r'native_fixture_stamp=(\d+)', run.stdout)[1])
repeat_stamp = int(re.search(r'repeat_fixture_stamp=(\d+)', run.stdout)[1])
assert stamp == 153734
assert repeat_stamp == stamp
n = Native(image)
manager, config, channel = DATA, DATA+0x180000, DATA+0xA8
interface, vtable = DATA+0x190000, DATA+0x191000
n.put(manager+0x28, config, 'Q')
n.put(config+0xC, 10000)
n.put(channel+0x3040, 4)
n.put(channel+0x1D18, 2)
n.put(channel+0x1D1C, 9)
n.put(channel+0x3150, interface, 'Q')
n.put(interface, vtable, 'Q')
n.put(channel+0x3068, 1, 'B')
n.put(channel+0x306A, 0x1FF, 'H')
n.put(BASE+0x26B3920, 0, 'B')
callbacks = []
for i in range(9):
    n.put(manager+0x38+i*8, interface, 'Q')
for offset in (0x20, 0x80, 0x98, 0xC0):
    target = 0x1000+offset
    n.put(vtable+offset, BASE+target, 'Q')
    n.stubs[target] = lambda offset=offset: callbacks.append(offset) or 0
n.stubs[0x35DC50] = lambda: 0
n.stubs[0x2FE650] = lambda: 163484
n.stubs[0x187C480] = lambda: None
n.stubs[0x187E862] = lambda: n.uc.mem_write(n.arg(0), bytes(n.arg(2))) or n.arg(0)
try:
    n.call(0x17CC1A0, manager, 0, 1)
except UcError:
    print('fixture failure RIP', hex(n.uc.reg_read(UC_X86_REG_RIP)),
          'return', hex(n.get(n.uc.reg_read(UC_X86_REG_RSP), 'Q')),
          'calls', [hex(x) for x in n.calls[-12:]])
    raise
assert n.get(channel+0x3040) == 1
assert n.get(channel+0x3048, 'Q') == 163484
assert n.get(channel+0x3089, 'B') == 1
assert callbacks.count(0x20) == 9
assert callbacks.count(0x98) >= 1
native_after = bytes(n.uc.mem_read(channel, 0x41F0))
n.put(channel+0x3048, stamp, 'Q')
patched_after = bytes(n.uc.mem_read(channel, 0x41F0))
assert all(a == b for i, (a, b) in enumerate(zip(native_after, patched_after))
           if not 0x3048 <= i < 0x3050)
# The real update can close with resetOwners=false, then arm retry on a second
# reset while already waiting. Exercise both original calls, not a guessed state.
n.put(channel+0x3040, 4)
n.put(channel+0x3089, 0, 'B')
owners_before = callbacks.count(0x20)
n.call(0x17CC1A0, manager, 0, 0)
assert n.get(channel+0x3040) == 1 and n.get(channel+0x3089, 'B') == 0
assert callbacks.count(0x20) == owners_before
n.call(0x17CC1A0, manager, 0, 1)
assert n.get(channel+0x3040) == 1 and n.get(channel+0x3089, 'B') == 1
assert callbacks.count(0x20) == owners_before+9
n.put(channel+0x3048, repeat_stamp, 'Q')

outcome = []
def stop(uc, address, size, data):
    if address in (BASE+0x1802D9A, BASE+0x1803189):
        outcome.append(address == BASE+0x1802D9A)
        uc.emu_stop()
n.uc.hook_add(UC_HOOK_CODE, stop)
for stored, threshold in ((163484, 10000), (stamp, 250)):
    for elapsed in (0, 1, 100, 249, 250, 251, 9999, 10000, 10001):
        outcome.clear()
        for register, value in ((UC_X86_REG_RSI, channel), (UC_X86_REG_R13, manager),
                                (UC_X86_REG_R14, 0), (UC_X86_REG_RDX, 163484+elapsed),
                                (UC_X86_REG_RBX, stored)):
            n.uc.reg_write(register, value)
        n.uc.emu_start(BASE+0x1802D6A, BASE+0x180318A, count=100)
        assert outcome == [elapsed >= threshold], (stored, elapsed, outcome)
print('PASS native clean-close: all 9 owner resets preserved, native waiting/retry state retained')
print('PASS native repeated reset: normal close followed by waiting-state owner reset covered')
print('PASS native cooldown: original waits 10000 ms; C++ policy releases at 250 ms; only timestamp changes')
