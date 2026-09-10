"""Recover one Cabal ambient area's native bindings, without enabling a policy.

The existing inventory identifies the area. This focused extractor follows its
source choices, tactical tasks, firing areas, monitor and named spawn points.
Unknown request counts, source-to-row scheduling and renewal remain explicit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from mercury_faction_battle_catalog import source_choices, tactical_rows, rows


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def u64(data, offset):
    return struct.unpack_from('<Q', data, offset)[0]


def text_at(data, offset):
    target = offset + struct.unpack_from('<q', data, offset)[0]
    return data[target:data.index(0, target)].decode('utf-8')


def fnv1(text):
    value = 0x811C9DC5
    for char in text.encode():
        value = ((value * 16777619) ^ char) & 0xFFFFFFFF
    return value


def extract(reader):
    records = {}
    key = 0x2571C34D

    def read(tag, expected):
        data, cls = reader.read_tag(tag)
        if cls != expected:
            raise ValueError(f'{tag:08X}: expected {expected:08X}, got {cls:08X}')
        records[tag] = {'tag': f'{tag:08X}', 'class': f'{cls:08X}', 'bytes': len(data),
                        'sha256': hashlib.sha256(data).hexdigest().upper()}
        return data

    obj = read(0x80F5B513, 0x80809462)
    assert u32(obj, 12) == key
    sources = []
    for slot, tag, entity in ((0, 0x80F5B4F7, 0x80C1A52D), (1, 0x80F5B4FA, 0x80C19B1F)):
        data = read(tag, 0x80809C36)
        choice = source_choices(data)
        at = choice['definition_offset']
        assert data[at + 48:at + 56] == struct.pack('<IHH', key, 1, slot)
        assert data[at + 0x98:at + 0xA8] == struct.pack('<IHHIHH', key, 66, 8, key, 66, 9)
        assert len(choice['categories']) == 1
        category = choice['categories'][0]
        assert category['all_six_choices_identical']
        assert all(len(v) == 1 and v[0]['entity'] == f'{entity:08X}' and v[0]['weight'] == 1
                   for v in category['variants'])
        sources.append({'slot': slot, 'definition': f'{tag:08X}', 'category': category['key'],
                        'variants': category['variants'], 'primary_rule_slot': 8,
                        'fallback_rule_slot': 9, 'initial_request_count': None,
                        'retail_tactical_row': None})

    objective = read(0x80F5B50A, 0x80809C36)
    tactical = tactical_rows(objective)
    assert [r['tasks'][0]['slot'] for r in tactical['rows']] == [5, 18, 7, 6]
    assert fnv1('engaged') == 0xB29C8BF8 and fnv1('unengaged') == 0x4B54FC7B
    for row in tactical['rows']:
        assert len(row['tasks']) == 1 and row['tasks'][0]['registry'] == f'{key:08X}'
        row['hash_matches'] = next((word for word in ('engaged', 'unengaged')
                                    if fnv1(word) == int(row['group_hash'], 16)), None)

    firing = read(0x80F5B4FD, 0x80809C36)
    placement_names = {}
    for at in rows(firing, 0x1F0, 0x80809A6D, 24):
        registry, kind, slot = struct.unpack_from('<IBxH', firing, at + 8)
        assert registry == key and kind in (44, 45)
        placement_names[(kind, slot)] = text_at(firing, at)
    areas = {}
    for at in rows(firing, 0x200, 0x80808354, 128):
        slot = u32(firing, at + 16)
        assert text_at(firing, at + 8) == placement_names[(44, slot)]
        areas[u64(firing, at)] = {'guid': f'{u64(firing, at):016X}', 'slot': slot,
                                'name': placement_names[(44, slot)]}
    sets = {}
    for at in rows(firing, 0x210, 0x80808350, 32):
        slot = u32(firing, at + 12)
        members = []
        for member in rows(firing, at + 16, 0x80808352, 16):
            area = areas[u64(firing, member)]
            members.append({**area, 'native_flag': u32(firing, member + 12)})
        sets[slot] = {'slot': slot, 'name': placement_names[(45, slot)], 'areas': members}
    for row in tactical['rows']:
        row['firing_area_set'] = sets[row['tasks'][0]['slot']]

    monitor = read(0x80F5B504, 0x80809C36)
    assert monitor[0x248:0x250] == struct.pack('<IHH', key, 30, 4)
    assert monitor[0x270:0x278] == struct.pack('<IHH', key, 60, 15)
    volume = read(0x80F5B4F1, 0x80809C36)
    gp, pp = 0xA70, 0xA40
    assert u32(volume, gp - 8) == 0x808099D0 and u32(volume, pp - 8) == 0x80809A6D
    assert volume[pp + 8:pp + 16] == volume[gp + 12:gp + 20] == struct.pack('<IHH', key, 60, 15)
    assert u32(volume, pp + 16) == u32(volume, gp + 20) == 15
    vertices = [list(struct.unpack_from('<4f', volume, at))
                for at in rows(volume, gp + 0xD0, 0x80800094, 16)]
    triangles = [list(volume[at:at + 3]) for at in rows(volume, gp + 0xE0, 0x80809B92, 3)]
    assert len(vertices) == 6 and len(triangles) == 4
    assert all(v[3] == 1 for v in vertices) and all(i < 6 for t in triangles for i in t)

    def named_placements(tag):
        data = read(tag, 0x808099D6)
        return {u64(data, at + 0x70): {'guid': f'{u64(data, at + 0x70):016X}',
                  'placement_offset': at, 'entity_definition': f'{u32(data, at):08X}',
                  'rotation': list(struct.unpack_from('<4f', data, at + 16)),
                  'position': list(struct.unpack_from('<3f', data, at + 32))}
                for at in rows(data, 8, 0x808099D8, 144)}

    point_maps = {0x80F5B9A4: named_placements(0x80F5B9A4),
                  0x80F5B4F5: named_placements(0x80F5B4F5)}
    primary_guids = [0x3175CA7E8643C2A9, 0xD028E6A9CBB47BA6, 0xC54292CEE0277A33]
    fallback_guids = [0x5656AC7B97C49F01]
    bindings = []
    for rule, expected, descriptor, map_tag, wrapper, owner, owner_key in (
            (0x80F5B4E8, primary_guids, 0x410, 0x80F5B9A4, 0x80F5B9A5, 0x80F5B9C1, 0x4A3E4900),
            (0x80F5B4EE, fallback_guids, 0x378, 0x80F5B4F5, 0x80F5B4F6, 0x80F5B513, key)):
        data = read(rule, 0x80809C36)
        guids = [u64(data, at) for at in rows(data, 0x210, 0x80809840, 72)]
        assert guids == expected
        assert [u64(data, at) for at in rows(data, descriptor, 0x80809845, 8)] == guids
        wrap = read(wrapper, 0x80809468)
        assert len(wrap) == 32 and u32(wrap, 8) == map_tag
        owner_data = read(owner, 0x80809462)
        assert u32(owner_data, 12) == owner_key
        # Exact component-list traversal, not an untyped substring match.
        wrapper_refs = []
        for group in rows(owner_data, 0x38, 0x80809464, 24):
            wrapper_refs.extend(u32(owner_data, at) for at in rows(owner_data, group + 8, 0x80809466, 4))
        assert wrapper_refs.count(wrapper) == 1
        bindings.append({'rule_definition': f'{rule:08X}', 'map_placements': f'{map_tag:08X}',
                         'map_wrapper': f'{wrapper:08X}', 'owner_object': f'{owner:08X}',
                         'owner_registry': f'{owner_key:08X}',
                         'points': [point_maps[map_tag][guid] for guid in guids]})
    for tag in (0x80BFDDC8, 0x80F74364, 0x80F4B3B3):
        read(tag, 0x80809C0F)

    return {'area': 'pf_lighthouse_ca_cannon_forest_a', 'registry': f'{key:08X}',
            'object': '80F5B513', 'bubble': 15, 'sources': sources,
            'objective': {'definition': '80F5B50A', 'slot': 2, **tactical},
            'monitor': {'definition': '80F5B504', 'slot': 4, 'target_slot': 15,
                        'target_definition': '80F5B4F1', 'target_name': text_at(volume, gp),
                        'vertices_xyzw': vertices, 'triangles': triangles,
                        'extrusion': struct.unpack_from('<f', volume, gp + 0xF0)[0]},
            'spawn_rule_bindings': bindings, 'records': list(records.values()),
            'native_consumers': {'named_lookup': 'A1F8D0: compares name to sobject+90',
                'point_resolution': '4E6630 -> A20730 -> A1F8D0; 4E25D0 obtains spawn-point interface',
                'source_request': '4E8FB0 copies authority+30 (length+2C) to source+650',
                'source_tactical': '4E2A90 copies objective/row from authority+00/+B4',
                'generation_reset': '4E9550: independent original-instruction evidence file'},
            'policy': {'initial_request_vector': None, 'source_to_tactical_rows': None,
                       'retail_rule_selection': None, 'retail_activation_predicate': None,
                       'renewal_trigger': None, 'native_quiescence_barrier': None,
                       'activation_supported': False},
            'limits': ['Named placement bindings are not Boolean conditions or population counts.',
                       'The primary point owner is a separate registry; do not activate its other sources.',
                       'Hash matches and authored firing areas do not prove host source-to-row selection.',
                       'Package presence does not prove the corresponding named object is live.',
                       'No registry/profile mutation, process memory access, client injection or live test.']}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reader-dir', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    sys.path.insert(0, str(args.reader_dir))
    from pkg import Reader
    result = extract(Reader())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Cabal area: two sources, four tactical rows, one native occupancy volume; '
          'primary/fallback points joined to exact owning map placements. Retail policy unresolved.')


if __name__ == '__main__':
    main()
