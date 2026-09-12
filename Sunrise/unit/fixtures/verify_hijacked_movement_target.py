"""Offline proof of Hijacked's kind9 target mismatch; no game-process access."""
from pathlib import Path
import hashlib
import json
import re
import struct as S
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP, UC_X86_REG_RIP

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/coo'))
import package_read

IMAGE = (ROOT / 'destiny2_unpacked.bin').read_bytes()
SHA = '63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
assert hashlib.sha256(IMAGE).hexdigest() == SHA
BASE, RAM = 0x7FF618070000, 0x10000000
u = Uc(UC_ARCH_X86, UC_MODE_64)
u.mem_map(BASE, (len(IMAGE) + 0xFFF) & ~0xFFF)
u.mem_write(BASE, IMAGE)
u.mem_map(RAM, 0x10000)
reference, output, stack, stop = RAM + 0x100, RAM + 0x200, RAM + 0xF008, RAM + 0xFFF0
lookups = []
lookup_success = False

def put(address, fmt, *values):
    u.mem_write(address, S.pack(fmt, *values))

def get(address, fmt):
    return S.unpack(fmt, u.mem_read(address, S.calcsize(fmt)))

def hook(uc, address, size, user):
    if address == stop:
        uc.emu_stop()
    elif address == BASE + 0x4EA140:
        # Registry lookup is a boundary. The original resolver must admit the
        # reference type before this is reached; type47 never reaches it.
        lookups.append((get(uc.reg_read(UC_X86_REG_RCX), '<IBBh'), uc.reg_read(UC_X86_REG_RDX)))
        if lookup_success:
            put(uc.reg_read(UC_X86_REG_R8), '<IIQ', 0x1234, 0x8080834D, 0)
        sp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, get(sp, '<Q')[0])
        uc.reg_write(UC_X86_REG_RSP, sp + 8)
        uc.reg_write(UC_X86_REG_RAX, int(lookup_success))

u.hook_add(UC_HOOK_CODE, hook)
cases = 0

def resolve(kind, slot=54, registry=0x153E22CD, success=False):
    global lookup_success, cases
    lookup_success = success
    lookups.clear()
    put(reference, '<IBBh', registry, kind, 0, slot)
    put(output, '<I', 0xBADCAFE)
    put(stack, '<Q', stop)
    for register, value in ((UC_X86_REG_RCX, output), (UC_X86_REG_RDX, reference), (UC_X86_REG_RSP, stack)):
        u.reg_write(register, value)
    u.emu_start(BASE + 0x4FFEC0, stop, count=1000)
    assert u.reg_read(UC_X86_REG_RAX) == output
    cases += 1
    return get(output, '<I')[0], list(lookups)

for kind in range(256):
    result, calls = resolve(kind)
    assert result == 0xFFFFFFFF
    assert bool(calls) == (kind in (48, 58)), (kind, calls)
    if calls:
        assert calls[0][1] == int(kind == 48)
for kind in (47, 48, 58, 255):
    assert resolve(kind, -1) == (0xFFFFFFFF, [])
assert resolve(48, 55, 0x95FB2E01, True)[0] == 0x1234
for registry in (0x153E22CD, 0x95FB2E01, 0x811C9DC5, 0):
    for slot in (0, 54, 57, 32767):
        assert resolve(47, slot, registry) == (0xFFFFFFFF, [])

package_hashes = {}
def package(tag):
    cls, blob = package_read.read(tag)
    assert cls == 0x80809C36
    package_hashes[f'{tag:08X}'] = hashlib.sha256(blob).hexdigest()
    return blob
member = package(0x80B42323)
assert S.unpack_from('<IBBh', member, 0xB88) == (0x153E22CD, 2, 0, 22)
assert S.unpack_from('<II', member, 0xB9C) == (0x80807DA2, 0x80807DA1)
assert S.unpack_from('<IBBh', member, 0xBC0) == (0x153E22CD, 1, 0, 21)
points = package(0x80B421AA)
destinations = []
for tag, slot, offset in ((0x80B423BD, 54, 0x270), (0x80B423C3, 57, 0x2A0)):
    locator = package(tag)
    assert S.unpack_from('<IBBh', locator, 0x270) == (0x153E22CD, 47, 0, slot)
    assert S.unpack_from('<I', points, offset + 4)[0] == slot
    destinations.append({'slot': slot, 'type': 47, 'position': S.unpack_from('<3f', points, offset + 32)})

catalog = (ROOT / 'Sunrise/src/state/activity/hijacked/catalog.h').read_text()
rows = re.findall(r'\{"([^"]+)",\{0x([0-9A-F]+)U,0x([0-9A-F]+)U,(\d+),(\d+)\}', catalog)
assert len(rows) > 100
paths = [(name, registry, int(kind), int(slot)) for name, registry, tag, kind, slot in rows if int(kind) in (48, 58)]
assert paths == [('ps_echoes_dropship_fallen01_entry', '3E9B74F3', 58, 101),
                 ('ps_echoes_dropship_fallen01_exit', '3E9B74F3', 58, 102)]
result = {'imageSha256': SHA, 'nativeResolverCases': cases, 'catalogAssets': len(rows),
          'nativeEntry': '4FFEC0 -> 4E2990; only types48/58 reach4EA140',
          'member': '153E22CD/type2/22; schema80807DA1; parent153E22CD/type1/21',
          'bossPointTargets': destinations, 'catalogPaths': paths, 'packageSha256': package_hashes,
          'gameProcessAccess': False,
          'limits': 'Tests original target type admission and packaged references. Registry lookup is a boundary. Does not prove absence of every possible alternative native movement protocol; no unregistered path is invented.'}
out = ROOT / 'build/coo/hijacked-research/native-target-feasibility.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
