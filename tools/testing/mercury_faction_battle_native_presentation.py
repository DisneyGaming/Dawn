"""Verify presentation call edges against an existing offline image dump.

This is read-only evidence extraction, not an injector or live receipt probe.
The dump's identity and function digests are saved with the result. Native
queue delivery remains distinct from a visible, owner-correlated HUD receipt.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys


CALLS = {
    # Extents come from full-decomp's contiguous function bodies. The image's
    # .pdata can describe only the first chained unwind fragment (D7EA90:10B).
    0xD7EA90: (367, (0x4A94B0, 0xD7D8C0, 0xD83390)),
    0xD83390: (705, (0x5041A0, 0xD83720)),
    0xD83720: (1417, (0xD7D8C0, 0xD7D970, 0xBE9A70)),
    0xD84BD0: (1027, (0x13210D0,)),
    0x13210D0: (402, (0x131C930,)),
    0x131C930: (277, (0x131F180,)),
    0x131B300: (406, (0x131D760,)),
    0x15ECD40: (82, (0x15ECBA0, 0x15ED5B0)),
    0x15E47D0: (480, (0x15F0F80,)),
    0x15E6040: (1503, (0x1276DE0,)),
}


def extract(image):
    from capstone.x86 import X86_OP_IMM
    result = {'image_directory': image.dir, 'base': f'{image.base:X}',
              'functions': {}, 'native_delivery_and_receipt_qualified': False}
    for source, (size, expected) in CALLS.items():
        span = image.func(source)
        if span is None or span[0] != source:
            raise ValueError(f'{source:X}: missing exact function range')
        code = image.read(source, size)
        calls = [(instruction.address - image.base, instruction.operands[0].imm - image.base)
                 for instruction in image.md.disasm(code, image.base + source)
                 if instruction.mnemonic == 'call' and instruction.operands[0].type == X86_OP_IMM]
        if not set(expected).issubset({target for _, target in calls}):
            raise ValueError(f'{source:X}: expected native call edge absent')
        result['functions'][f'{source:X}'] = {
            'bytes': len(code), 'sha256': hashlib.sha256(code).hexdigest(),
            'verified_calls': [{'site': f'{site:X}', 'target': f'{target:X}'}
                               for site, target in calls if target in expected]}
    # Boot data contains the resolved shared-tag catalog and predicate sentinel.
    for address, expected in ((0x1F8DC94, 0x80806485), (0x1F8DC98, 0x80B9E5E3),
                              (0x1F92700, 0x1124697D)):
        if image.u32(address) != expected:
            raise ValueError(f'{address:X}: initialized catalog/predicate mismatch')
    for slot, function in ((0x1C21E38, 0xD84BD0), (0x1C94D38 + 13 * 8, 0x15E5FB0),
                           (0x1C94C70 + 13 * 8, 0x15E6040)):
        if image.u64(slot) != image.base + function:
            raise ValueError(f'{slot:X}: presentation virtual method mismatch')
    result['resolved_catalog'] = '80B9E5E3'
    result['recipient_any_predicate'] = '1124697D'
    result['queue_record_bytes'] = 0x858
    result['receipt_limit'] = ('The queue can evict or filter messages. Owner-correlated native '
                               'widget acceptance and successful localization are not yet observed.')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', type=Path, default=Path('D:/Dawn-work/scripts'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir.resolve()))
    from exedump import Image
    report = extract(Image('boot'))
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Verified {len(report["functions"])} native functions; no live receipt asserted.')


if __name__ == '__main__':
    main()
