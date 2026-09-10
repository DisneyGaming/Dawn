"""Installed authored bindings and original source callbacks, in private Unicorn RAM.

No game process is opened. Descriptor function pointers are reconstructed from
the archived native method registration selected by the package's method rows.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native-kit', type=Path, required=True)
parser.add_argument('--reader-dir', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
sys.path.insert(0, str(args.reader_dir))
from pkg import Reader
sys.path.insert(0, str(args.native_kit))
import verify_member_lifecycle_offline as n
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

assert hashlib.sha256(n.code).hexdigest().upper() == '8713D15E3D05B26F9E259E02B0F29BC1E000E4B0C62CA2CC87C38597186CC3BD'
R = Reader()
source, source_class = R.read_tag(0x80F5B4F7)
definition = struct.unpack_from('<Q', source, 0x78)[0]
binding_at = definition + 0x58
instance_ref = binding_at + struct.unpack_from('<q', source, binding_at)[0]
descriptor_tag = struct.unpack_from('<I', source, binding_at + 8)[0]
descriptor, descriptor_class = R.read_tag(descriptor_tag)
assert source_class == 0x80809C36 and definition == 0x728
assert instance_ref == definition and descriptor_tag == 0x80FEBA97
assert descriptor_class == 0x80809C54
assert struct.unpack_from('<II', descriptor, 8) == (0x808094CD, 0x808094CC)
assert struct.unpack_from('<II', source, instance_ref) == (0x80F5B4F7, 0x80809A3B)
assert struct.unpack_from('<Q', source, instance_ref + 8)[0] == 0x70
methods = [struct.unpack_from('<II', descriptor, 0x40 + i * 24) for i in range(12)]
assert methods[:3] == [(0x80809A3B, i) for i in (8, 9, 10)]
patched = bytearray(descriptor)
targets = []
for i, (kind, method) in enumerate(methods):
    assert kind == 0x80809A3B
    pointer = struct.unpack_from('<Q', n.code, 0x1C12E20 + method * 16)[0]
    struct.pack_into('<Q', patched, 0x48 + i * 24, pointer)
    targets.append(pointer - n.B)
assert targets[:3] == [0x4E5260, 0x4E8390, 0x4E87A0]

def p(u, address, fmt, value):
    u.mem_write(address, struct.pack(fmt, value))

def read(u, address, fmt):
    return struct.unpack(fmt, u.mem_read(address, struct.calcsize(fmt)))[0]

def setup(consumed=5, pending=1, generation=17):
    u = n.machine()
    u.mem_write(n.ASSET, bytes(patched))
    p(u, n.M, '<Q', n.ASSET)
    p(u, n.M + 8, '<Q', n.D)
    p(u, n.D + 0x244, '<I', generation)
    for category in range(8):
        p(u, n.D + 0x268 + category * 4, '<I', consumed)
        p(u, n.D + 0x670 + category * 4, '<I', pending)
    return u

def invoke(u, rva, category=0):
    u.reg_write(UC_X86_REG_RSP, n.S)
    p(u, n.S, '<Q', n.END)
    u.reg_write(UC_X86_REG_RCX, n.M)
    u.reg_write(UC_X86_REG_RDX, 5)
    u.reg_write(UC_X86_REG_R8, category)
    n.execute(u, rva)

def arithmetic():
    cases = []
    for category in (0, 3, 7):
        for pending in (0, 1, 2):
            for wrapper, consumed_delta in ((0x1840380, 1), (0xB321E0, 0)):
                u = setup(pending=pending)
                before = bytes(u.mem_read(n.D, 0x6B0))
                invoke(u, wrapper, category)
                after = bytes(u.mem_read(n.D, 0x6B0))
                expected = bytearray(before)
                struct.pack_into('<I', expected, 0x268 + category * 4, 5 + consumed_delta)
                struct.pack_into('<I', expected, 0x670 + category * 4, (pending - 1) & 0xFFFFFFFF)
                assert after == bytes(expected)
                cases.append(dict(wrapper=f'{wrapper:X}', category=category,
                                  pending_before=pending,
                                  pending_after=read(u, n.D + 0x670 + category * 4, '<i'),
                                  consumed_delta=consumed_delta, passed=True))
    for generation in (0, 1, 17, 0x7FFFFFFF):
        u = setup(generation=generation)
        invoke(u, 0x1849230)
        assert u.reg_read(UC_X86_REG_RAX) == generation
        cases.append(dict(wrapper='1849230', generation=generation, passed=True))
    return cases

def deferred_job(queued_generation, current_generation, result, pending=1):
    """Original 10D3930 generation gate and completion branches.

    Entity creation is intercepted at 4FF620. Its returned result is a fixture;
    neither creation nor actor attachment is claimed by this test.
    """
    u = n.machine()
    u.mem_write(n.ASSET, bytes(patched))
    instance = n.ASSET + 0x3000
    job, manager, out = n.D, n.M, n.G + 0x800
    p(u, n.B + 0x2439C70, '<Q', n.G)
    p(u, n.G, '<Q', n.G + 0x100)
    p(u, n.G + 0x108, '<Q', n.ASSET)
    p(u, n.G + 0x130, '<I', 0x10000)
    p(u, job, '<I', 0xFFFFFFFF)
    p(u, job + 0x50, '<I', 0)
    p(u, job + 0x58, '<I', 0)
    p(u, job + 0x5C, '<I', 0)
    p(u, job + 0x60, '<Q', 0x3000)
    p(u, job + 0x70, '<I', 0xFFFFFFFF)
    p(u, job + 0x80, '<I', 0)
    p(u, job + 0xA0, '<I', queued_generation)
    p(u, instance + 0x244, '<I', current_generation)
    p(u, instance + 0x268, '<I', 5)
    p(u, instance + 0x670, '<I', pending)
    calls = []
    boundaries = {0x352310, 0x170A0D0, 0xA93920, 0xA10CB0,
                  0x4FF620, 0x9EB640, 0xA05CD0, 0xC7AFA0}
    def boundary(e, address, size, user):
        rva = address - n.B
        a, b, c = [e.reg_read(reg) for reg in
                   (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8)]
        if rva == 0x352310:
            assert a == job + 0x58
            p(e, b, '<I', 1)
            e.reg_write(UC_X86_REG_RAX, b)
        elif rva in (0x170A0D0, 0xA93920, 0xA10CB0):
            pass
        elif rva == 0x4FF620:
            calls.append('entity_creation_boundary')
            p(e, c, '<I', 1)
            p(e, c + 8, '<B', result)
        elif rva == 0x9EB640:
            p(e, b, '<I', 0xFFFFFFFF)
            e.reg_write(UC_X86_REG_RAX, b)
        elif rva == 0xA05CD0:
            p(e, a, '<I', 0xFFFFFFFF)
            e.reg_write(UC_X86_REG_RAX, a)
        elif rva == 0xC7AFA0:
            calls.append('native_job_cleanup_boundary')
        else:
            raise AssertionError(hex(rva))
        n.ret(e)
    for rva in boundaries:
        u.hook_add(UC_HOOK_CODE, boundary, begin=n.B + rva, end=n.B + rva)
    u.reg_write(UC_X86_REG_RCX, manager)
    u.reg_write(UC_X86_REG_RDX, job)
    u.reg_write(UC_X86_REG_R8, out)
    n.execute(u, 0x10D3930)
    matched = queued_generation == current_generation
    expected_consumed = 5 + int(matched and result == 1)
    expected_pending = pending - int(matched and result in (1, 3))
    assert read(u, instance + 0x268, '<I') == expected_consumed
    assert read(u, instance + 0x670, '<I') == expected_pending
    assert ('entity_creation_boundary' in calls) == matched
    assert ('native_job_cleanup_boundary' in calls) == (not matched or result in (1, 3))
    assert read(u, instance + 0x244, '<I') == current_generation
    return dict(queued_generation=queued_generation, current_generation=current_generation,
                creation_result_fixture=result, pending_before=pending, consumed_after=expected_consumed,
                pending_after=expected_pending, calls=calls, passed=True)

def deferred_queue(queued, current):
    """Original 4EB9E0 compare calls the real, authored-bound getter."""
    u = n.machine()
    u.mem_write(n.ASSET, bytes(patched))
    p(u, n.B + 0x2439C70, '<Q', n.G)
    p(u, n.G, '<Q', n.G + 0x100)
    p(u, n.G + 0x108, '<Q', n.ASSET)
    p(u, n.G + 0x130, '<I', 0x10000)
    p(u, n.M + 0x1B94, '<I', 0)
    p(u, n.M + 0x1B98, '<Q', 0x1000)
    p(u, n.M + 0x1BA0, '<I', 0)
    p(u, n.ASSET + 0x1000, '<I', 0)
    p(u, n.ASSET + 0x1008, '<Q', 0x2000)
    p(u, n.ASSET + 0x3000 + 0x244, '<I', current)
    p(u, n.M + 0x28, '<I', queued)
    p(u, n.M + 0x40, '<I', 0)
    p(u, n.M + 0x48, '<I', 0)
    p(u, n.M + 0x50, '<Q', 0x3000)
    p(u, n.M + 0x58, '<I', 0xFFFFFFFF)
    u.mem_map(0x7FFFBE9FC000, 0x1000)
    accepted = []
    def boundary(e, address, size, user):
        rva = address - n.B
        a = e.reg_read(UC_X86_REG_RCX)
        if address == 0x7FFFBE9FC840:
            length = e.reg_read(UC_X86_REG_R8)
            assert length <= 0x3000
            e.mem_write(a, bytes([e.reg_read(UC_X86_REG_RDX) & 255]) * length)
            e.reg_write(UC_X86_REG_RAX, a)
        elif rva == 0xAA14B0:
            pass
        elif rva == 0xAA1190:
            assert a == n.D + 0x10 and e.reg_read(UC_X86_REG_RDX) == n.M + 8
            accepted.append(queued)
            p(e, n.D + 0x18, '<I', 1)
        elif rva == 0xAA4330:
            assert a == n.D + 0x2B0
        else:
            raise AssertionError(hex(rva))
        n.ret(e)
    for target in [0x7FFFBE9FC840, *[n.B + r for r in (0xAA14B0, 0xAA1190, 0xAA4330)]]:
        u.hook_add(UC_HOOK_CODE, boundary, begin=target, end=target)
    u.reg_write(UC_X86_REG_RCX, n.M)
    u.reg_write(UC_X86_REG_RDX, 0)
    u.reg_write(UC_X86_REG_R8, n.D)
    n.execute(u, 0x4EB9E0)
    assert accepted == ([queued] if queued == current else [])
    return dict(queued_generation=queued, current_generation=current,
                retained=bool(accepted), original_getter=True, passed=True)

if __name__ == '__main__':
    result = dict(
        native_image_sha256=hashlib.sha256(n.code).hexdigest().upper(),
        source=dict(tag='80F5B4F7', sha256=hashlib.sha256(source).hexdigest().upper(),
                    definition_offset='728', callback_binding_offset='780',
                    descriptor_field_offset='788', source_instance_offset='70'),
        descriptor=dict(tag='80FEBA97', class_id='80809C54',
                        sha256=hashlib.sha256(descriptor).hexdigest().upper(),
                        interface='808094CD', methods=[dict(kind=f'{k:X}', method=m,
                        native_rva=f'{targets[i]:X}') for i,(k,m) in enumerate(methods)]),
        arithmetic=arithmetic(),
        deferred_jobs=[deferred_job(q, g, result, pending) for q,g,pending in
                       ((17,17,1),(17,18,1),(17,18,0)) for result in (0,1,2,3)],
        deferred_queue=[deferred_queue(q,g) for q,g in
                        ((17,17),(17,18),(18,17),(18,18),(0x7FFFFFFF,1))],
        live_access=False,
        limits=['Function-pointer binding is reconstructed from exact package method rows and archived registrations; the loader is not emulated.',
                '10D3930 executes original generation/completion branches; entity creation results and peripheral helper boundaries are fixtures.',
                'Zero pending followed by either callback underflows to -1. This does not prove any retail path makes that ordering.',
                'No authored initial count, reset timing, actor-retirement barrier or complete all-path renewal policy is inferred.'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Native source callback: {len(result['arithmetic'])} arithmetic, {len(result['deferred_jobs'])} deferred-job and {len(result['deferred_queue'])} deferred-queue cases passed")
