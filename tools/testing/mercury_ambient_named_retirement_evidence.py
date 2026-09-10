"""Execute original native pool release in private Unicorn memory only.

No process or game access. The heap, slot and free-list are explicit fixtures;
all generation and free-list operations execute original34F790 unchanged.
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
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.native_kit))
    import verify_member_lifecycle_offline as native
    from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX
    rows = []
    for index in (0, 5, 6):
        for generation, mask in ((0, 0xFF), (254, 0xFF), (255, 0xFF), (7, 7)):
            machine = native.machine()
            heap, free_list, backing = native.M, native.M + 0x100, native.ACTORS

            def put(at, fmt, value):
                machine.mem_write(at, struct.pack(fmt, value))

            stride, generation_offset = 0x100, 0xF0
            put(heap, '<Q', free_list)
            put(heap + 8, '<Q', backing)
            put(heap + 0x1C, '<I', generation_offset)
            put(heap + 0x20, '<I', stride)
            put(heap + 0x24, '<I', mask)
            put(heap + 0x28, '<I', 0)  # no external handle-table backlink
            put(heap + 0x38, '<I', 0xFFFFFFFF)  # no linked child heaps
            put(free_list, '<Q', backing)
            put(free_list + 0x10, '<I', stride)
            put(free_list + 0x14, '<I', 0xE0)
            put(free_list + 0x1E, '<H', 0xFFFE)
            at = backing + index * stride + generation_offset
            put(at, '<I', generation)
            handle = (generation << 23) | index
            machine.reg_write(UC_X86_REG_RCX, heap)
            machine.reg_write(UC_X86_REG_RDX, handle)
            native.execute(machine, 0x34F790)
            after = struct.unpack('<I', machine.mem_read(at, 4))[0]
            marker = struct.unpack('<H', machine.mem_read(backing + index * stride + 0xE2, 2))[0]
            assert after == ((generation + 1) & mask)
            assert marker == 0xFEFE
            rows.append(dict(index=index, before=generation, after=after, mask=mask,
                             freed_marker=f'{marker:04X}', passed=True))
    result = dict(native_rva='34F790', image_sha256=hashlib.sha256(native.code).hexdigest().upper(),
                  original_prefix=native.code[0x34F790:0x34F7A0].hex(' ').upper(),
                  live_access=False, external_function_substitutions=[], cases=rows,
                  limits=['Fixtures prove native release arithmetic, not live release timing.',
                          '569D10 removal return alone is not a pool retirement acknowledgement.',
                          'Named-point cleanup does not prove an AI source cancellation barrier.'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'Native named retirement: {len(rows)} original-instruction cases passed')


if __name__ == '__main__':
    main()
