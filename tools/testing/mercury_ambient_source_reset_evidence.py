"""Exercise original native source reset consumers in private Unicorn memory.

This is a boundary test, not a replacement scheduler or live game controller.
Entity/actor iteration, deletion, clock and tactical-unlink APIs are explicit
fixtures. The source-generation branch and its counter writes execute original
instructions. A generation echo is deliberately NOT treated as quiescence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--native-kit', type=Path, required=True)
    ap.add_argument('--reader-dir', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    sys.path.insert(0, str(args.native_kit))
    sys.path.insert(0, str(args.reader_dir))
    import verify_member_lifecycle_offline as native
    import schema
    from pkg import Reader
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX,
                                  UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP)
    B, M, D, G, ASSET, ACTORS = (native.B, native.M, native.D, native.G,
                                 native.ASSET, native.ACTORS)
    resource, cls = Reader().read_tag(0x80F5B4F7)
    assert cls == 0x80809C36
    assert resource[0x758:0x760] == struct.pack('<IHH', 0x2571C34D, 1, 0)
    assert struct.unpack_from('<I', resource, 0x7D0)[0] == 1

    def p32(u, at, value):
        u.mem_write(at, struct.pack('<I', value & 0xFFFFFFFF))

    def p64(u, at, value):
        u.mem_write(at, struct.pack('<Q', value & 0xFFFFFFFFFFFFFFFF))

    def n32(u, at):
        return struct.unpack('<I', u.mem_read(at, 4))[0]

    def setup():
        u = native.machine()
        u.mem_write(ASSET, resource)
        p64(u, B + 0x2439C70, G)
        p64(u, G, G + 0x100)
        p64(u, G + 0x108, ASSET)
        p32(u, G + 0x130, 0x10000)
        p64(u, M + 8, 0x728)
        table = 0x600000
        u.mem_map(table, 0x400000)
        # Reconstruct immutable primitive reflection records for AB1B60's
        # native default-fill; no source-generation logic is replaced.
        for handle in (0x80800009, *range(0x80800072, 0x80800077)):
            shift = ((handle - (1 << 32)) >> 13) & 0xFFFFFFFF
            index = ((shift | 0xFFC0000) >> 18) & (shift & 0xFFFF)
            desc = G + 0x100 + index * 0x40
            p64(u, desc + 8, table)
            p32(u, desc + 0x30, 0x200)
            rva = schema.find_blob(handle)
            length = struct.unpack_from('<I', native.code, rva)[0]
            raw = bytearray(native.code[rva:rva + length])
            dst = table + (handle & 0x1FFF) * 0x200
            for offset in (0x30, 0x38, 0x40):
                relative = struct.unpack_from('<q', raw, offset)[0]
                if relative:
                    struct.pack_into('<q', raw, offset,
                                     B + rva + offset + relative - (dst + offset))
            u.mem_write(dst, bytes(raw))
        u.mem_map(0x7FFFBE9FC000, 0x1000)

        def crt(e, address, size, user):
            dst, src, count = (e.reg_read(r) for r in
                               (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
            assert count < 0x10000
            raw = (bytes([src & 255]) * count if address == 0x7FFFBE9FC840
                   else bytes(e.mem_read(src, count)))
            e.mem_write(dst, raw)
            e.reg_write(UC_X86_REG_RAX, dst)
            native.ret(e)

        for address in (0x7FFFBE9FC840, 0x7FFFBE9FC3F0):
            u.hook_add(UC_HOOK_CODE, crt, begin=address, end=address)
        return u

    def reset_case(changed, mode, actors, pending):
        u = setup()
        old_generation, new_generation = 17, 18 if changed else 17
        p32(u, M + 0x244, old_generation)
        p32(u, M + 0x264, 1)
        p32(u, M + 0x268, 5)
        p32(u, M + 0x5FC, 1)
        p32(u, M + 0x640, 0xFFFFFFFF)
        p32(u, M + 0x650, 9)
        p32(u, M + 0x670, pending)
        p32(u, D + 0x7C, new_generation)
        u.mem_write(D + 0xBC, bytes([mode]))
        p64(u, B + 0x1F93428, ACTORS + 0x10000)
        p32(u, B + 0x1F93430, 0x100)
        p32(u, B + 0x26BE0E0, (1 << 7) if actors else 0)
        cursor = 0
        calls = []

        def boundary(e, address, size, user):
            nonlocal cursor
            rva = address - B
            if rva == 0xA92AE0:
                cursor = 0
                assert n32(e, e.reg_read(UC_X86_REG_R8)) == 1
            elif rva == 0xA99540:
                e.reg_write(UC_X86_REG_RAX, int(cursor < actors))
            elif rva == 0xA97AA0:
                p32(e, e.reg_read(UC_X86_REG_RDX), 0x12342001)
            elif rva == 0xA99730:
                cursor += 1
            elif rva == 0xA05CD0:
                p32(e, e.reg_read(UC_X86_REG_RCX), 7)
                e.reg_write(UC_X86_REG_RAX, e.reg_read(UC_X86_REG_RCX))
            elif rva == 0xA93E40:
                e.reg_write(UC_X86_REG_RAX, int(actors != 0))
            elif rva == 0x3BC970:
                p64(e, e.reg_read(UC_X86_REG_RCX), 123456)
                e.reg_write(UC_X86_REG_RAX, e.reg_read(UC_X86_REG_RCX))
            elif rva in (0x56A8F0, 0x3BD720):
                assert e.reg_read(UC_X86_REG_RCX) == 7
                calls.append({'rva': f'{rva:X}', 'entity': 7,
                              'native_generation_at_call': n32(e, M + 0x244)})
                # Record the request boundary; do not fabricate completion.
            elif rva == 0x4EC710:
                calls.append({'rva': f'{rva:X}', 'old_row': e.reg_read(UC_X86_REG_R8) & 0xFFFFFFFF,
                              'new_row': n32(e, M + 0x5FC)})
            else:
                raise AssertionError(hex(rva))
            native.ret(e)

        hooks = (0xA92AE0, 0xA99540, 0xA97AA0, 0xA99730, 0xA05CD0,
                 0xA93E40, 0x3BC970, 0x56A8F0, 0x3BD720, 0x4EC710)
        for rva in hooks:
            u.hook_add(UC_HOOK_CODE, boundary, begin=B + rva, end=B + rva)
        before = bytes(u.mem_read(M, 0x6A4))
        u.reg_write(UC_X86_REG_RCX, M)
        u.reg_write(UC_X86_REG_RDX, D)
        native.execute(u, 0x4E9550)
        after = bytes(u.mem_read(M, 0x6A4))
        if changed:
            assert n32(u, M + 0x244) == new_generation
            assert n32(u, M + 0x268) == n32(u, M + 0x670) == 0
            assert n32(u, M + 0x264) == 1
            assert n32(u, M + 0x650) == 9  # reset does not clear target storage
            assert n32(u, M + 0x5FC) == 0xFFFFFFFF
            assert sum(c['rva'] in ('56A8F0', '3BD720') for c in calls) == actors
        else:
            assert before == after and calls == []
        return {'changed': changed, 'cleanup_mode': mode, 'fixture_actors': actors,
                'pending_before': pending, 'pending_after': n32(u, M + 0x670),
                'generation_after': n32(u, M + 0x244), 'consumed_after': n32(u, M + 0x268),
                'requested_storage_after': n32(u, M + 0x650), 'calls': calls,
                'source_bytes_unchanged': before == after, 'passed': True}

    cases = [reset_case(changed, mode, actors, pending)
             for changed in (False, True) for mode in (0, 1)
             for actors, pending in ((0, 0), (0, 3), (1, 0), (1, 3))]
    result = {'passed': True, 'native_image_sha256': hashlib.sha256(native.code).hexdigest().upper(),
              'source_resource': '80F5B4F7', 'resource_sha256': hashlib.sha256(resource).hexdigest().upper(),
              'original_consumers': ['4E9550', '4EC1A0', '4EC410', 'AB25A0', 'AB1B60'],
              'cases': cases,
              'limits': [
                  'Private Unicorn memory only; no game process accessed.',
                  'Actor iteration, entity deletion, clock and tactical unlink are explicit fixtures.',
                  'Only the reset branch/counter writes and native default initialization execute here.',
                  'Nonzero pending counters do not prevent generation reset in this consumer.',
                  'Deletion call return is not simulated actor retirement or network allocation release.',
                  'No quiescence predicate, retail renewal trigger, cancellation barrier or live renewal is proven.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(cases)} original-native reset cases passed; generation acknowledgement is not quiescence')


if __name__ == '__main__':
    main()
