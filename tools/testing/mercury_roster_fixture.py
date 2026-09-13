"""Build isolated roster test inputs from the installed cache and native packages.

Missing ambient groups are decoded from actual package descriptors. This does
not edit the live cache. New DLL provenance causes the production cache rebuild.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/mercury-validation/fixtures')
    args = parser.parse_args()
    # Share the independently checked native descriptor traversal. Its main raid
    # extraction is not invoked and this path does not require the decomp archive.
    path = ROOT / 'docs/raids/eater-of-worlds/tools/extract_eater_of_worlds.py'
    spec = importlib.util.spec_from_file_location('native_package_inventory', path)
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    data = native.CACHE.read_bytes()
    counts, offsets = native.cache_domains(data)
    size = native.DOMAIN_SIZES[native.ROSTER_DOMAIN]
    start = offsets[native.ROSTER_DOMAIN]
    rows = [data[start + i * size:start + (i + 1) * size] for i in range(counts[native.ROSTER_DOMAIN])]
    present = {struct.unpack_from('<I', row)[0] for row in rows}
    catalog = (ROOT / 'Sunrise/src/state/activity/coo/mercury_ambient_catalog.h').read_text()
    candidates = re.findall(r'\{"mercury_freeroam",0x80F4696A,0x([0-9A-F]+),0x([0-9A-F]+),', catalog)
    if len(candidates) != 37:
        raise ValueError('Expected 37 installed ambient groups')
    supplemental = []
    for key, tag in candidates:
        key, tag = int(key, 16), int(tag, 16)
        if key in present:
            continue
        group = native.recover_group(tag)
        if int(group['registryKey'], 16) != key:
            raise ValueError('Native group key differs')
        descriptors = group['descriptors']
        row = bytearray(size)
        struct.pack_into('<IIH', row, 0, key, tag, len(descriptors))
        # Packed RosterGroupRecord v54: each array has 1280 entries.
        fields = [('slotTypes', 10, 1), ('slotFlags', 1290, 1), ('slotIndices', 2570, 2),
                  ('descriptorTags', 5130, 4), ('descriptorOffsets', 10250, 4),
                  ('componentClasses', 15370, 4), ('senseSchemas', 20490, 4), ('authSchemas', 25610, 4)]
        for i, d in enumerate(descriptors):
            sense, auth = int(d['senseSchema'], 16), int(d['authSchema'], 16)
            values = [d['type'], (sense != 0xFFFFFFFF) + 2 * (auth != 0xFFFFFFFF), d['index'],
                      int(d['sourceTag'], 16), d['sourceOffset'], int(d['componentClass'], 16), sense, auth]
            for (_, offset, width), value in zip(fields, values):
                row[offset + i * width:offset + (i + 1) * width] = value.to_bytes(width, 'little')
        rows.append(row)
        supplemental.append({'key': f'{key:08X}', 'tag': f'{tag:08X}', 'descriptors': descriptors})
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'groups.bin').write_bytes(struct.pack('<I', len(rows)) + b''.join(rows))
    start, size = offsets[native.SCENARIO_DOMAIN], native.DOMAIN_SIZES[native.SCENARIO_DOMAIN]
    wanted = {'mercury_freeroam', 'ia_ascend_future', 'ia_horde_future', 'ia_intercept_present'}
    found = set()
    for i in range(counts[native.SCENARIO_DOMAIN]):
        row = data[start + i * size:start + (i + 1) * size]
        name = row[:40].split(b'\0')[0].decode()
        if name in wanted:
            (args.output / (name + '.bin')).write_bytes(row)
            found.add(name)
    if found != wanted:
        raise ValueError('Missing required scenario')
    (args.output / 'provenance.json').write_text(json.dumps({
        'cacheSha256': hashlib.sha256(data).hexdigest(), 'cacheVersion': native.CACHE_VERSION,
        'method': 'isolated cache fixture supplemented by installed package descriptor traversal',
        'supplementalGroups': supplemental,
    }, indent=2) + '\n')
    print(f'Fixture: {len(rows)} groups; {len(supplemental)} recovered from native packages; live cache unchanged.')


if __name__ == '__main__':
    main()
