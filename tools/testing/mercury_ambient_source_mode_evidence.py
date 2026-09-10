"""Qualify native source suppression modes without live access or renewal.

Original4E4580,4E8270,4E3410 execute against actual Cabal source bytes. Actor
iteration and final request dispatch are explicit fixture boundaries. The latter
records a request only; it never fabricates an admitted actor or cancellation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-kit', type=Path, required=True)
    parser.add_argument('--reader-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path[:0] = [str(args.native_kit), str(args.reader_dir)]
    import verify_member_lifecycle_offline as native
    from pkg import Reader
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9
    resource, cls = Reader().read_tag(0x80F5B4F7)
    assert cls == 0x80809C36
    assert struct.unpack_from('<I', resource, 0x7D0)[0] == 1
    rows = []
    for mode in range(5):
        for requested, consumed, alive, pending in ((9, 5, 0, 0), (9, 5, 2, 1), (9, 5, 2, 3), (9, 5, 0, 4)):
            u = native.machine()
            b, source, authority, assets, directory, actors = native.B, native.M, native.D, native.ASSET, native.G, native.ACTORS

            def put(at, fmt, value):
                u.mem_write(at, struct.pack(fmt, value))

            def integer(at):
                return struct.unpack('<i', u.mem_read(at, 4))[0]

            u.mem_write(assets, resource)
            put(b + 0x2439C70, '<Q', directory)
            put(directory, '<Q', directory + 0x100)
            put(directory + 0x108, '<Q', assets)
            put(directory + 0x130, '<I', 0x10000)
            put(source + 8, '<Q', 0x728)
            put(source + 0x244, '<I', 17)
            put(source + 0x650, '<i', requested)
            put(source + 0x268, '<i', consumed)
            put(source + 0x670, '<i', pending)
            put(authority + 0xBD, '<B', mode)
            put(b + 0x1F9D7F8, '<Q', actors)
            put(b + 0x1F9D800, '<I', 0x100)
            for index in range(alive):
                put(actors + index * 0x100 + 0x60, '<I', 0xFFFFFFFF)
            cursor = 0
            requests = []

            def boundary(machine, address, size, user):
                nonlocal cursor
                rva = address - b
                if rva == 0xA92AE0:
                    cursor = 0
                elif rva == 0xA99540:
                    machine.reg_write(UC_X86_REG_RAX, int(cursor < alive))
                elif rva == 0xA97AA0:
                    put(machine.reg_read(UC_X86_REG_RDX), '<I', cursor)
                elif rva == 0xA99730:
                    cursor += 1
                elif rva == 0xA05E00:
                    machine.reg_write(UC_X86_REG_RAX, 0)
                elif rva == 0x4E2E80:
                    block = machine.reg_read(UC_X86_REG_R8)
                    requests.append(dict(categories=integer(block), count=integer(block + 4),
                                         selection_flag=integer(block + 0x24)))
                    put(machine.reg_read(UC_X86_REG_R9), '<I', 0)
                else:
                    raise AssertionError(hex(rva))
                native.ret(machine)

            for rva in (0xA92AE0, 0xA99540, 0xA97AA0, 0xA99730, 0xA05E00, 0x4E2E80):
                u.hook_add(UC_HOOK_CODE, boundary, begin=b + rva, end=b + rva)
            u.reg_write(UC_X86_REG_RCX, source)
            u.reg_write(UC_X86_REG_RDX, 1)
            u.reg_write(UC_X86_REG_R8, authority)
            native.execute(u, 0x4E4580)
            deficit = max(0, max(0, requested - alive - consumed) - pending)
            expected_consumed = consumed + max(0, requested - alive - consumed) if mode == 4 and deficit else consumed
            assert integer(source + 0x670) == pending
            assert integer(source + 0x268) == expected_consumed
            assert integer(source + 0x244) == 17
            if mode in (0, 2) and deficit:
                assert requests == [dict(categories=1, count=deficit, selection_flag=int(mode == 0))]
            else:
                assert requests == []
            rows.append(dict(mode=mode, requested=requested, consumed_before=consumed, loose_members=alive,
                             pending_before=pending, deficit=deficit, requests=requests,
                             consumed_after=integer(source + 0x268), pending_after=integer(source + 0x670),
                             generation_after=integer(source + 0x244), passed=True))
    result = dict(image_sha256=hashlib.sha256(native.code).hexdigest().upper(),
                  source='80F5B4F7', resource_sha256=hashlib.sha256(resource).hexdigest().upper(),
                  original_consumers=['4E4580', '4E8270', '4E3410'], cases=rows,
                  fixture_boundaries=['A92AE0', 'A99540', 'A97AA0', 'A99730', 'A05E00', '4E2E80'],
                  live_access=False, production_changes=False,
                  conclusion='Modes1/3 suppress dispatch; mode4 consumes outstanding budget but preserves pending births.',
                  limits=['No mode here acknowledges cancellation or source quiescence.',
                          'The authored initial request count and retail mode-selection producer remain unknown.',
                          'No source renewal, native actor admission or live cancellation is claimed.'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'Native source modes: {len(rows)} original-instruction cases passed')


if __name__ == '__main__':
    main()
