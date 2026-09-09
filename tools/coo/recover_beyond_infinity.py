"""Recover Beyond Infinity evidence from installed packages, without changing runtime files."""
import hashlib
import json
import re
import struct
from pathlib import Path

import package_read as packages
from extract_gateway_bindings import array, strings, u32, i64, sources
from extract_deadly_trial_bindings import walk, groups

OUT = packages.ROOT / 'build/coo/beyond-infinity-research'
SCENARIO = 0x80F46015
BANK = 0x80F1FDF7


def recover():
    result = walk(SCENARIO)
    result['groups'] = groups(result)
    result['sources'] = sources(result['groups'])
    result['dialogue'] = []
    _, bank = packages.read(BANK)
    roots = {u32(bank, o): o + 8 + i64(bank, o + 8) for o in array(bank, 24, 16)}
    starts = sorted(roots.values()) + [len(bank)]
    for row, o in enumerate(array(bank, 8, 8)):
        selector = u32(bank, o)
        start = roots[selector]
        end = starts[starts.index(start) + 1]
        texts = []
        for at in range(start, end - 7, 4):
            container, key = struct.unpack_from('<II', bank, at)
            if not 0x80F1E000 <= container < 0x80F20000:
                continue
            text = strings(container).get(key)
            if text and text not in texts:
                texts.append(text)
        result['dialogue'].append({'row': row, 'selector': selector,
            'durationMs': round(struct.unpack_from('<f', bank, o + 4)[0] * 1000), 'texts': texts})
    tags = set(range(0x80F46000, 0x80F4649D))
    tags.add(BANK)
    tags.update(s['descriptorTags'] for g in result['groups'] for s in g['slots'] if s['descriptorTags'] != 0xFFFFFFFF)
    result['tags'] = []
    result['objectives'] = []
    result['volumes'] = []
    for tag in sorted(tags):
        cls, data = packages.read(tag)
        names = [m.group().decode() for m in re.finditer(rb'[a-z][a-z_0-9.\[\]]{8,}', data)]
        result['tags'].append({'tag': tag, 'class': cls, 'sha256': hashlib.sha256(data).hexdigest(), 'names': names})
        if cls == 0x80804F72:
            for index, at in enumerate(array(data, 8, 40)):
                result['objectives'].append({'tag': tag, 'row': index, 'event': u32(data, at),
                    'texts': [[strings(u32(data, a + j)).get(u32(data, a + j + 4), '')
                               for j in (0, 8, 16, 24)] for a in array(data, at + 16, 32)]})
        for at in range(12, len(data) - 0x114, 4):
            if struct.unpack_from('<H', data, at + 4)[0] != 60:
                continue
            base = at - 12
            name_at = base + i64(data, base)
            if not 0 <= name_at < len(data):
                continue
            name = data[name_at:].split(b'\0', 1)[0].decode(errors='replace')
            if not re.fullmatch(r'[a-z_0-9.\[\]]+', name):
                continue
            try:
                vertices = [struct.unpack_from('<3f', data, o) for o in array(data, base + 0xD0, 16, 0x80800094)]
            except (AssertionError, struct.error):
                continue
            result['volumes'].append({'tag': tag, 'offset': base, 'registry': u32(data, at),
                'slot': struct.unpack_from('<H', data, at + 6)[0], 'name': name,
                'min': struct.unpack_from('<3f', data, base + 0xB0),
                'max': struct.unpack_from('<3f', data, base + 0xC0), 'vertices': vertices})
    result['provenance'] = {'method': 'Installed package and version-52 cache extraction; no live observations',
        'scenario': f'{SCENARIO:08X}', 'dialogueBank': f'{BANK:08X}',
        'cacheSha256': hashlib.sha256((packages.ROOT / 'Sunrise/cache/build_data.bin').read_bytes()).hexdigest()}
    validate(result)
    return result


def validate(result):
    # Identity checks fail closed before a different campaign mission can be exported.
    assert result['scenario'] == SCENARIO and result['packageHash'] == 0x03632571
    expected = [0x29BFCE5A, 0x2FDA4350, 0xB1AD777D, 0x9225AEF3,
                0xD60FA7DF, 0xBE5F8E8B, 0x142EC956, 0x64B46F54,
                0x5569523A, 0x5AD156F5, 0xE2AC714F]
    assert [row['event'] for row in result['objectives']] == expected
    assert len(result['dialogue']) == 49
    assert 'Osiris saw something' in result['dialogue'][0]['texts'][0]
    assert result['dialogue'][48]['texts'][0] == 'Ikora! You there?'
    assert len({g['registry'] for g in result['groups']}) == len(result['groups'])
    assert any(s['name'] == 'map_generator_sensor' and s['slotTypes'] == 37
               for g in result['groups'] for s in g['slots'])
    for group in result['groups']:
        assert len({s['slotIndices'] for s in group['slots']}) == len(group['slots'])
    for volume in result['volumes']:
        assert len(volume['vertices']) >= 3
        assert all(low <= high for low, high in zip(volume['min'], volume['max']))


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Compare fresh extraction to saved evidence without writing.')
    args = parser.parse_args()
    result = recover()
    target = OUT / 'native-bindings.json'
    if args.check:
        assert json.loads(target.read_text(encoding='utf-8')) == json.loads(json.dumps(result)), 'Saved extraction differs from installed packages.'
        print('PASS: saved evidence matches current installed packages and cache')
        return
    OUT.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    for key in ('groups', 'sources', 'dialogue', 'objectives', 'volumes', 'tags'):
        print(key, len(result[key]))
    for row in result['objectives']:
        print('objective', f"{row['event']:08X}", row['texts'])


if __name__ == '__main__':
    main()
