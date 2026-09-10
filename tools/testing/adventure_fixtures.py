"""Read installed Mercury adventure bindings into an isolated fixture directory.

No live process IO. Requires the local read-only research package reader.
Usage: python adventure_fixtures.py OUTPUT --reader D:/Sunrise-work/scripts
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import shutil
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--reader', required=True, type=Path)
a = p.parse_args()
sys.path.insert(0, str(a.reader))
from pkg import Reader
r = Reader()
a.output.mkdir(parents=True, exist_ok=True)
manifest = []
tags = [0x81327D63, 0x81327CF0, 0x80F5B993, 0x80C0127D,
        0x80F5B960, 0x80F5B956, 0x80F5B959, 0x80F5B95D,
        0x80F5B963,0x80F5B966,0x80F5B969,0x80F5B96C,0x80F5B96F,
        0x80F5B972,0x80F5B975,0x80F5B978,0x80F5B97B,0x80F5B97E,
        0x80F5B981,0x80F5B984]
for tag in tags:
    b, cls = r.read_tag(tag)
    if b is None:
        raise RuntimeError(f'Missing tag {tag:08X}')
    (a.output / f'{tag:08X}.bin').write_bytes(b)
    manifest.append(dict(tag=f'{tag:08X}', cls=f'{cls:08X}', bytes=len(b),
                         sha256=hashlib.sha256(b).hexdigest()))
bindings = []
for tag in tags[4:7]:
    b, _ = r.read_tag(tag)
    assert struct.unpack_from('<I', b, 0x614)[0] == 0x80804CFC
    assert struct.unpack_from('<I', b, 0x578)[0] == 0x808099D8
    assert struct.unpack_from('<I', b, 0x580)[0] == 0x80C0127D
    bindings.append(dict(descriptor=f'{tag:08X}',
        position=struct.unpack_from('<3f', b, 0x5A0),
        quaternion=struct.unpack_from('<4f', b, 0x590),
        kind=struct.unpack_from('<I', b, 0x620)[0],
        selector=f'{struct.unpack_from("<I", b, 0x628)[0]:08X}'))
(a.output / 'manifest.json').write_text(json.dumps(manifest, indent=2))
(a.output / 'bindings.json').write_text(json.dumps(bindings, indent=2))
shutil.copyfile(Path(__file__).resolve().parents[2] / 'Sunrise/unit/fixtures/adventure_mercury_flags.json',
                a.output / 'adventure_mercury_flags.json')
print(f'Exported {len(manifest)} read-only installed tags to {a.output}')
