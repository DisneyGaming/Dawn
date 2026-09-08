"""Recover A Deadly Trial's bindings from installed packages; never alter the game."""
import json
import struct as s
from pathlib import Path
import package_read as p
from extract_gateway_bindings import array, strings, u32, u64, i64

ROOT = p.ROOT
OUT = ROOT / 'build/coo/deadly-trial-research'


def walk(tag):
    _, data = p.read(tag)
    regions = []
    for bubble, row in enumerate(array(data, 80, 24)):
        for state, at in enumerate(array(data, row + 8, 76)):
            entry_tag = u32(data, at + 68)
            if entry_tag == 0xffffffff:
                continue
            _, entry = p.read(entry_tag)
            registry_tag = u32(entry, 20)
            _, registry = p.read(registry_tag)
            objects = []
            for kind, offset in enumerate((8, 24, 40)):
                for element in array(registry, offset, 4):
                    object_tag = u32(registry, element)
                    _, obj = p.read(object_tag)
                    objects.append({'tag': object_tag, 'registry': u32(obj, 12), 'array': kind})
            regions.append({'bubble': bubble, 'state': state, 'enabled': bool(data[at]),
                            'bubbleHash': u32(data, row), 'region': u32(entry, 16) * 8,
                            'registryTag': registry_tag, 'objects': objects})
    return {'scenario': tag, 'packageHash': u32(data, 8), 'regions': regions}


def groups(walked):
    layout = json.loads((ROOT / 'build/coo/gateway-research/cache-layout.json').read_text())['records']['RosterGroupRecord']
    section = json.loads((ROOT / 'build/coo/gateway-research/cache-sections.json').read_text())['RosterGroupRecord']
    cache = (ROOT / 'Sunrise/cache/build_data.bin').read_bytes()
    assert u32(cache, 8) == 52
    wanted = {o['tag'] for r in walked['regions'] for o in r['objects']}
    result = []
    for index in range(section['count']):
        start = section['offset'] + index * layout['size']
        data = cache[start:start + layout['size']]
        registry, tag, count = s.unpack_from('<IIH', data)
        if tag not in wanted:
            continue
        slots = []
        for j in range(count):
            slot = {}
            for name in ('slotTypes', 'slotFlags', 'slotIndices', 'descriptorTags', 'descriptorOffsets', 'componentClasses', 'senseSchemas', 'authSchemas'):
                offset, size = layout['fields'][name]
                stride = size // 1280
                slot[name] = int.from_bytes(data[offset + j * stride:offset + (j + 1) * stride], 'little')
            if slot['descriptorTags'] != 0xffffffff:
                _, blob = p.read(slot['descriptorTags'])
                off = slot['descriptorOffsets']
                assert u32(blob, off + 48) == registry
                assert s.unpack_from('<H', blob, off + 54)[0] == slot['slotIndices']
                name_at = off + 0x50 + i64(blob, off + 0x50)
                slot['name'] = blob[name_at:].split(b'\0', 1)[0].decode(errors='replace') if 0 <= name_at < len(blob) else ''
            slots.append(slot)
        result.append({'registry': registry, 'objectTag': tag, 'cacheIndex': index, 'slots': slots})
    return result


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', type=lambda x: int(x, 16), default=0x80B2E043)
    args = parser.parse_args()
    result = walk(args.scenario)
    result['groups'] = groups(result)
    OUT.mkdir(parents=True, exist_ok=True)
    output = OUT / f'{args.scenario:08X}-inventory.json'
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(output)
    for group in result['groups']:
        names = [slot.get('name', '') for slot in group['slots']]
        print(f"{group['registry']:08X} {group['objectTag']:08X} slots={len(names)}", names[:12])


if __name__ == '__main__':
    main()
