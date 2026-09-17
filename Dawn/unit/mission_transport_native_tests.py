"""Pinned native transport contracts; this is not an installed handoff replacement.

Run: python Dawn/unit/mission_transport_native_tests.py
Only publication/request notification callbacks are stubbed. The goal codec,
reader, ownership checks, change/sequence decisions, getters and cinematic
suppression wrapper execute original instructions from the pinned image.
"""
from pathlib import Path
import hashlib
import struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x7FF618070000
image = (ROOT / "destiny2_unpacked.bin").read_bytes()
assert hashlib.sha256(image).hexdigest() == "63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e", "wrong client build"
u = Uc(UC_ARCH_X86, UC_MODE_64)
u.mem_map(BASE, (len(image) + 4095) & ~4095)
u.mem_write(BASE, image)
HEAP = 0x10000000
u.mem_map(HEAP, 0x80000)
stream, value, decoded = HEAP + 0x1000, HEAP + 0x2000, HEAP + 0x3000
prop, session = HEAP + 0x4000, HEAP + 0x10000
stack, stop = HEAP + 0x70000, HEAP + 0x7F000
notifications = []
checks = 0


def put(address, fmt, *values):
    u.mem_write(address, struct.pack(fmt, *values))


def get(address, fmt):
    return struct.unpack(fmt, u.mem_read(address, struct.calcsize(fmt)))


def check(condition):
    global checks
    assert condition
    checks += 1


def notification(machine, address, size, unused):
    if address not in (BASE + 0x17AFC50, BASE + 0x17ADD00):
        return
    notifications.append((address - BASE, machine.reg_read(UC_X86_REG_RCX)))
    rsp = machine.reg_read(UC_X86_REG_RSP)
    ret = get(rsp, "<Q")[0]
    machine.reg_write(UC_X86_REG_RSP, rsp + 8)
    machine.reg_write(UC_X86_REG_RIP, ret)


u.hook_add(UC_HOOK_CODE, notification)


def call(rva, rcx=0, rdx=0, r8=0):
    u.reg_write(UC_X86_REG_RSP, stack - 8)
    put(stack - 8, "<Q", stop)
    u.reg_write(UC_X86_REG_RCX, rcx)
    u.reg_write(UC_X86_REG_RDX, rdx)
    u.reg_write(UC_X86_REG_R8, r8)
    u.emu_start(BASE + rva, stop, count=10000)
    return u.reg_read(UC_X86_REG_RAX)


# Registry0: original registration pairs encoder17BE8B0/decoder17BE860,
# each decoded value12 bytes. Verify the exact instructions that register them.
check(image[0x17BAE67:0x17BAE6E] == bytes.fromhex("48 8d 15 42 3a 00 00"))
check(image[0x17BAE84:0x17BAE8B] == bytes.fromhex("48 8d 05 d5 39 00 00"))
for goal in [-1, 0, 28, 29, 38, 62]:
    for reason in [-1, 0, 7, 58, 309, 510]:
        for sequence in [0, 1, 127, 254, 255]:
            u.mem_write(stream, bytes(0x60))
            put(value, "<iiB3x", goal, reason, sequence)
            call(0x17BE8B0, stream, value)
            expected = ((goal + 1) << 17) | ((reason + 1) << 8) | sequence
            check(get(stream + 0x24, "<I")[0] == 23)
            check(get(stream + 0x28, "<Q")[0] == expected)
            # Original reader consumes its MSB-aligned accumulator directly.
            u.mem_write(stream, bytes(0x60))
            put(stream + 0x28, "<Q", expected << 41)
            u.mem_write(decoded, b"\xA5" * 12)
            check(call(0x17BE860, stream, decoded) & 255 == 1)
            check(get(decoded, "<iiB") == (goal, reason, sequence))
            check(get(stream + 0x24, "<I")[0] == 23)
            check(bytes(u.mem_read(decoded + 9, 3)) == b"\xA5" * 3)

# The property grants authority only to session states6..9. The actual
# registry0 constructor sets request permission0, so non-authority cannot
# silently turn a replicated goal update into a local request.
put(prop + 0x18, "<Q", session)
put(prop + 0x24, "<I", 0)
for state in range(12):
    put(session + 0x1AEF8, "<I", state)
    check(bool(call(0x178DAC0, prop) & 255) == (6 <= state <= 9))
    put(prop + 0x140, "<B", 1)
    put(prop + 0x148, "<iiB3x", 38, 0, 255)
    notifications.clear()
    accepted = call(0x17BD0F0, prop, 28, 309) & 255
    check(bool(accepted) == (6 <= state <= 9))
    if accepted:
        check(get(prop + 0x148, "<iiB") == (28, 309, 1))
        check(notifications == [(0x17AFC50, prop)])
        for _ in range(100):
            check(call(0x17BD0F0, prop, 28, 309) & 255 == 1)
        check(get(prop + 0x150, "<B")[0] == 1 and len(notifications) == 1)
        check(call(0x17B4A60, prop) & 0xFFFFFFFF == 28)
        check(call(0x17B4A90, prop) & 0xFFFFFFFF == 309)
    else:
        check(get(prop + 0x148, "<iiB") == (38, 0, 255))
        check(not notifications)

# Missing presence or sequence0 is not an actionable native goal.
for presence, sequence in [(0, 1), (1, 0), (0, 0)]:
    put(prop + 0x140, "<B", presence)
    put(prop + 0x150, "<B", sequence)
    check(call(0x17B4A60, prop) & 0xFFFFFFFF == 0xFFFFFFFF)
    check(call(0x17B4A90, prop) & 0xFFFFFFFF == 0xFFFFFFFF)

# A permission-enabled non-host request is distinct from replicated authority.
# This fixture exercises the original alternate branch, not a production grant.
put(session + 0x1AEF8, "<I", 4)
put(prop + 0x24, "<I", 2)
put(prop + 0x140, "<B", 1)
put(prop + 0x148, "<iiB3x", 38, 0, 255)
notifications.clear()
check(call(0x17BD0F0, prop, 28, 309) & 255 == 1)
check(get(prop + 0x148, "<iiB") == (38, 0, 255))
check(get(prop + 0x154, "<iiB") == (28, 309, 1))
check(notifications == [(0x17ADD00, prop)])

# The shipping wrapper has no data/replication input: it calls return-false.
# A server property cannot replace this detour merely by assigning a flag.
for residue in [0, 1, 255, 0xFFFFFFFF]:
    u.reg_write(UC_X86_REG_RAX, residue)
    check(call(0xC24490) & 255 == 0)

print(f"PASS: {checks} original native transport checks; 23-bit goal codec, ownership, sequence, replay, presence, request separation and cinematic wrapper")
