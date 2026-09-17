"""Preserve the failed run and locate Omega's generator in its actual cache."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct

root = Path(__file__).resolve().parents[2]
out = root / 'build/omega-ikora-trigger-fix-20260906'
out.mkdir(parents=True, exist_ok=True)
for name in ['Dawn/logs/dawn.log', 'Dawn/settings.json',
             'Dawn/src/state/build_data/runtime/build_data_catalog_runtime.cpp',
             'Dawn/src/state/build_data/runtime/build_data_roster_runtime.cpp',
             'tools/omega-reference-20260906/port_omega.py']:
    target = out / ('before-' + Path(name).name)
    if not target.exists():
        shutil.copy2(root / name, target)

data = (root / 'Dawn/cache/build_data.bin').read_bytes()
inventory = json.loads((root / 'Dawn/exports/omega_inventory.json').read_text())
first = inventory['groups'][0]
assert first['table_index'] == 0
start = data.index(struct.pack('<IIH', int(first['registry_key'], 16),
                               int(first['object_tag'], 16), first['slot_count']))
record_size = 10 + 1280 * 24
count = struct.unpack_from('<I', data, 92 + 14 * 4)[0]
assert 0 < count <= 2048
generators = []
for index in range(count):
    key, tag, slots = struct.unpack_from('<IIH', data, start + index * record_size)
    assert key and 0 < slots <= 1280
    if key == 0x2763EC97:
        generators.append({'index': index, 'key': f'{key:08X}', 'tag': f'{tag:08X}', 'slots': slots})
assert len(generators) == 1

log = (out / 'before-dawn.log').read_text(errors='replace')
packets = {}
for line in log.splitlines():
    if 'stage=sensor_sense_update ' in line:
        packet = re.search(r' packet=(\d+) ', line)
        payload = re.search(r' hex=([0-9A-F]+)', line)
        if packet and payload and int(packet[1]) in (1, 10):
            packets[packet[1]] = payload[1]
assert packets.keys() == {'1', '10'}
(out / 'packets.json').write_text(json.dumps(packets, indent=2))
report = {'cacheSha256': hashlib.sha256(data).hexdigest(), 'rosterCount': count,
          'generator': generators[0], 'archiveSearchRange': [1006, 1134],
          'startupPacket': 1, 'rejectedApproachPacket': 10}
(out / 'diagnosis.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
