"""Prove the Cabal primary points' owner and native construction boundary.

Package joins are strict. Optional native execution uses only private Unicorn
memory and an archived image. Entity construction, registry services and named
object retirement remain explicit boundaries; no host population policy is made.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from mercury_faction_battle_catalog import rows


def word(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def extract(reader):
    assets = {}

    def read(tag, expected):
        data, actual = reader.read_tag(tag)
        assert actual == expected, (hex(tag), hex(actual), hex(expected))
        assets[tag] = data
        return data

    scenario = read(0x80F4696A, 0x80809994)
    bubbles = rows(scenario, 80, 0x8080924D, 24)
    bubble = bubbles[15]
    assert word(scenario, bubble) == 0xA83A9175
    states = rows(scenario, bubble + 8, 0x8080924F, 76)
    assert len(states) == 1
    state = states[0]
    assert scenario[state] == 1 and scenario[state + 12] == 1
    assert word(scenario, state + 68) == 0x80F46AD5
    entry = read(0x80F46AD5, 0x8080925B)
    assert word(entry, 16) == 15 and word(entry, 20) == 0x80F46C89
    registry = read(0x80F46C89, 0x8080925E)
    objects = rows(registry, 24, 0x80809260, 4)
    assert len(objects) == 68
    assert word(registry, objects[3]) == 0x80F5B513
    assert word(registry, objects[41]) == 0x80F5B9C1

    owner = read(0x80F5B9C1, 0x80809462)
    assert word(owner, 12) == 0x4A3E4900
    groups = []
    for group in rows(owner, 56, 0x80809464, 24):
        wrappers = []
        for member in rows(owner, group + 8, 0x80809466, 4):
            tag = word(owner, member)
            wrapper = read(tag, 0x80809468)
            wrappers.append({'tag': f'{tag:08X}',
                             'named_list': f'{word(wrapper, 8):08X}'})
        groups.append({'index': struct.unpack_from('<i', owner, group)[0],
                       'wrappers': wrappers})
    assert [g['index'] for g in groups] == [-1, 15]
    assert [len(g['wrappers']) for g in groups] == [4, 6]
    assert groups[0]['wrappers'][0] == {'tag': '80F5B9A5', 'named_list': '80F5B9A4'}
    assert all(w['named_list'] == 'FFFFFFFF' for g in groups
               for w in g['wrappers'] if w['tag'] != '80F5B9A5')

    named = read(0x80F5B9A4, 0x808099D6)
    placements = []
    for ordinal, at in enumerate(rows(named, 8, 0x808099D8, 144)):
        placements.append({'index': ordinal, 'offset': at,
            'entity': f'{word(named, at):08X}',
            'guid': f'{struct.unpack_from("<Q", named, at + 112)[0]:016X}',
            'placement_flags_68_69': named[at + 104:at + 106].hex(),
            'position': list(struct.unpack_from('<3f', named, at + 32))})
    assert len(placements) == 19
    assert [placements[i]['guid'] for i in (0, 5, 6)] == [
        '3175CA7E8643C2A9', 'D028E6A9CBB47BA6', 'C54292CEE0277A33']
    result = {'scenario': '80F4696A', 'bubble_ordinal': 15,
        'bubble_hash': 'A83A9175', 'state_offset': state,
        'state_map_index_field': word(scenario, state + 28),
        'entry': '80F46AD5', 'registry_list': '80F46C89',
        'registry_array_descriptor': 24, 'area_index': 3, 'owner_index': 41,
        'owner_key': '4A3E4900', 'owner_object': '80F5B9C1',
        'component_groups': groups, 'named_placements': placements,
        'required_primary_point_indices': [0, 5, 6],
        'records': [{'tag': f'{tag:08X}', 'bytes': len(data),
                     'sha256': hashlib.sha256(data).hexdigest().upper()}
                    for tag, data in assets.items()]}
    return result, assets


def native_cases(native_kit, assets, expected):
    sys.path.insert(0, str(native_kit))
    import verify_member_lifecycle_offline as native
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RCX,
        UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9)
    B, M, D, G = native.B, native.M, native.D, native.G

    def case(path):
        bubble_only = path == 'bubble_components'
        removing = path == 'registry_removal'
        u = native.machine()
        table = 0x70000000
        u.mem_map(table, 0x4000000)

        def p32(at, value):
            u.mem_write(at, struct.pack('<I', value & 0xFFFFFFFF))

        def p64(at, value):
            u.mem_write(at, struct.pack('<Q', value))

        def address(tag):
            return table + (tag & 0x1FFF) * 0x1000

        p64(B + 0x2439C70, G)
        p64(G, G + 0x100)
        for tag, data in assets.items():
            if tag < 0x80F5B000:
                continue
            assert len(data) < 0x1000
            shift = ((tag - (1 << 32)) >> 13) & 0xFFFFFFFF
            bank = ((shift | 0xFFC0000) >> 18) & (shift & 0xFFFF)
            descriptor = G + 0x100 + bank * 64
            p64(descriptor + 8, table)
            p32(descriptor + 48, 0x1000)
            u.mem_write(address(tag), data)
        p32(M, 0x80F4696A)
        p32(D, 0x4A3E4900)
        constructions = []
        removals = []
        wrappers = []

        def boundary(e, ip, size, user):
            rva = ip - B
            a = e.reg_read(UC_X86_REG_RCX)
            b = e.reg_read(UC_X86_REG_RDX)
            if rva == 0x3CACA0:  # qualified object lookup is the package proof above
                p32(b, 0x80F5B9C1)
                e.reg_write(UC_X86_REG_RAX, b)
            elif rva == 0x4EA330:
                e.reg_write(UC_X86_REG_RAX, 0)
            elif rva == 0x3F8B60:
                e.reg_write(UC_X86_REG_RAX, 1)
            elif rva == 0x4EF930:
                p32(M + 0x100, 1)
                e.reg_write(UC_X86_REG_RAX, M + 0x100)
            elif rva == 0x428900:
                wrappers.append(next(f'{tag:08X}' for tag in assets if address(tag) == a))
                p32(b, 0)
                e.reg_write(UC_X86_REG_RAX, 1)
            elif rva == 0x4EF740:
                e.reg_write(UC_X86_REG_RAX, 1)
            elif rva == 0x4EFDB0:
                p32(a, 1)
                e.reg_write(UC_X86_REG_RAX, a)
            elif rva == 0x569D10:
                assert a == 0x80F5B9A4
                removals.append(f'{a:08X}')
            elif rva in (0xA44BB0, 0x3BCCD0, 0x187CB30, 0x3BCCA0,
                          0x3FBB40, 0x4F0FD0, 0x4EFAF0, 0x3FB1B0,
                          0x3F8FE0, 0x4EA480):
                # The fixed private stack is already mapped; the CRT guard-page
                # probe preserves RAX, as the native caller requires.
                pass
            elif rva == 0x575690:
                tag = e.reg_read(UC_X86_REG_R8) & 0xFFFFFFFF
                index = e.reg_read(UC_X86_REG_R9) & 0xFFFFFFFF
                assert tag == 0x80F5B9A4
                assert b == address(tag) + expected[index]['offset']
                constructions.append({'list': f'{tag:08X}', 'index': index,
                                      'guid': expected[index]['guid']})
                p32(a, 0xFFFFFFFF)  # observe a constructor attempt, not a live entity
                e.reg_write(UC_X86_REG_RAX, a)
            else:
                raise AssertionError(hex(rva))
            native.ret(e)

        for rva in (0x3CACA0, 0x4EA330, 0x3F8B60, 0x4EF930, 0x428900,
                    0x4EF740, 0xA44BB0, 0x3BCCD0, 0x187CB30, 0x575690,
                    0x4EFDB0, 0x569D10, 0x3BCCA0, 0x3FBB40, 0x4F0FD0,
                    0x4EFAF0, 0x3FB1B0, 0x3F8FE0, 0x4EA480):
            u.hook_add(UC_HOOK_CODE, boundary, begin=B + rva, end=B + rva)
        u.reg_write(UC_X86_REG_RCX, M)
        u.reg_write(UC_X86_REG_RDX, D)
        native.execute(u, 0x3CA800 if removing else (0x3CBF90 if bubble_only else 0x3CBBE0))
        assert len(wrappers) == (6 if bubble_only else 10), wrappers
        assert [c['index'] for c in constructions] == ([] if bubble_only or removing else list(range(19)))
        assert removals == (['80F5B9A4'] if removing else [])
        return {'path': path, 'wrapper_visits': wrappers,
                'constructor_attempts': constructions, 'named_list_removal_requests': removals,
                'passed': True}

    return {'image_sha256': hashlib.sha256(native.code).hexdigest().upper(),
            'original_consumers': ['3CBBE0', '3CBF90', '3CA800', '4E2850', '4E28A0', '569F00'],
            'cases': [case(path) for path in ('whole_registry', 'bubble_components', 'registry_removal')],
            'limits': ['Private emulation; no game process or client mutation.',
                'Registry lookup/services, stack probe, entity construction and list removal are explicit fixtures.',
                'Constructor attempts do not prove eligible or successful live named objects.',
                'The list removal request does not prove completed native entity retirement.',
                'No source requests, host counts, row selection or renewal policy are recovered.']}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reader-dir', type=Path, required=True)
    ap.add_argument('--native-kit', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    sys.path.insert(0, str(args.reader_dir))
    from pkg import Reader
    result, assets = extract(Reader())
    if args.native_kit:
        result['native'] = native_cases(args.native_kit, assets, result['named_placements'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Owner path verified: whole-registry construction visits 19 named placements; '
          'bubble-only construction skips them; removal targets the same list. Host policy unresolved.'
          if args.native_kit else 'Package owner path verified; native cases not requested.')


if __name__ == '__main__':
    main()
