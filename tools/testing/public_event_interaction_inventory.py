"""Verify the single authored Mercury rally interaction from installed packages.

This extracts definitions only. Native behavior is separately verified by the
offline original-code tests; this tool never opens the running game.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--re-root', type=Path, default=Path('D:/Sunrise-work'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.re_root / 'scripts'))
    from pkg import Reader
    from arrays import resolve_descriptor
    reader = Reader()
    assets = {}

    def read(tag, expected):
        raw, cls = reader.read_tag(tag)
        assert raw and cls == expected, (hex(tag), cls)
        assets[f'{tag:08X}'] = dict(package_class=f'{cls:08X}', size=len(raw),
                                  sha256=hashlib.sha256(raw).hexdigest().upper())
        return raw

    def u32(raw, offset):
        return struct.unpack_from('<I', raw, offset)[0]

    def qword(raw, offset):
        return struct.unpack_from('<q', raw, offset)[0]

    def localized(container, wanted):
        raw = read(container, 0x80809A88)
        count, hashes, cls = resolve_descriptor(raw, 8)
        assert cls == 0x80800070
        index = next(i for i in range(count) if u32(raw, hashes + 4*i) == wanted)
        text = read(u32(raw, 24), 0x80809A8A)
        nparts, parts, cls = resolve_descriptor(text, 8)
        ncombos, combos, cls2 = resolve_descriptor(text, 72)
        assert cls == 0x80809A90 and cls2 == 0x80809A8E and index < ncombos
        combo = combos + index*16
        first, n = combo + qword(text, combo), qword(text, combo+8)
        assert 0 < n <= 64 and first >= parts and (first-parts) % 32 == 0
        assert (first-parts)//32 + n <= nparts
        result = []
        for i in range(n):
            part = first+i*32
            start = part+8+qword(text, part+8)
            size, shift = struct.unpack_from('<H', text, part+20)[0], struct.unpack_from('<H', text, part+24)[0]
            assert 0 <= start <= len(text) and size <= len(text)-start
            result.append(''.join(chr(ord(c)+shift) for c in text[start:start+size].decode('utf-8')))
        return ''.join(result).split('\0')[0]

    source = read(0x80F5BF33, 0x80809C36)
    source_runtime = 16+qword(source,16)
    source_definition = qword(source,source_runtime+8)
    assert u32(source,source_runtime-4) == 0x80809927 and source_definition == 0x4C8
    assert u32(source,source_definition+0x38) == 15 and source[source_definition+0x94] == 0
    count, visual, cls = resolve_descriptor(source,source_definition+0x58)
    assert (count,cls) == (1,0x808099D8) and u32(source,visual) == 0x80BEF9E6
    entity = read(0x80BEF9E6,0x80809C0F)
    n, start, cls = resolve_descriptor(entity,0x10)
    assert n == 11 and cls == 0x80809C04
    components, controllers = [], []
    for i in range(n):
        tag = u32(entity,start+i*12)
        raw = read(tag,0x80809C36)
        runtime = 16+qword(raw,16)
        component_class = u32(raw,runtime-4)
        components.append(dict(tag=f'{tag:08X}',component_class=f'{component_class:08X}'))
        if component_class == 0x80804FB0:
            controllers.append((tag,raw,runtime))
    assert len(controllers) == 1 and controllers[0][0] == 0x815ABCED
    _, controller, runtime = controllers[0]
    definition = qword(controller,runtime+8)
    assert definition == 0x388 and u32(controller,runtime+4) == 0x80804FB2
    setup = 8+qword(controller,8)
    assert u32(controller,setup-4) == 0x80804FB1 and u32(controller,setup+0x10) == 0x811C9DC5
    assert u32(controller,definition+0x60) == 5
    assert controller[definition+0x209] == 1 and controller[definition+0x328] == 0
    assert u32(controller,definition+0x138) == 0x811C9DC5 and u32(controller,definition+0x32C) == 0x811C9DC5
    assert u32(controller,definition+0x1F0) == 0x80BEF944
    count, predicate, cls = resolve_descriptor(controller,definition+0x128)
    assert (count,cls) == (1,0x80809316) and controller[predicate] == 1
    leaf = predicate+8+qword(controller,predicate+8)
    assert u32(controller,leaf-4) == 0x80804D71
    label_hash = u32(controller,definition+0x70)
    container = u32(controller,definition+0x80)
    assert (label_hash,container) == (0xF1D98B09,0x80BFC7D1)
    label = localized(container,label_hash)
    grant = read(0x80BEF944,0x8080941E)
    path = b'content\\public_events_d2\\rally_point\\rally_beacon\\rally_beacon_grant_ability_energy_hopon.pattern.tft'
    assert path in grant
    count, root, cls = resolve_descriptor(grant,8)
    assert (count,cls) == (1,0x80809420)
    assert u32(grant,root+8+qword(grant,root+8)-4) == 0x80804E31
    count, children, cls = resolve_descriptor(grant,0x58)
    assert (count,cls) == (4,0x80809420)
    report = dict(live_access=False,passed=True,source='80F5BF33',registry='85C38F77',slot=0,bubble=15,
        entity='80BEF9E6',components=components,sole_interaction='815ABCED',
        interaction=dict(auto_register=True,action_kind=5,
            native_range=struct.unpack_from('<f',controller,definition+0x64)[0],
            native_hold_value=struct.unpack_from('<f',controller,definition+0x6C)[0],
            label_hash=f'{label_hash:08X}',container=f'{container:08X}',label=label,
            authored_mode=0,scoped_override='absent',
            predicate=dict(type='80804D71',inverted=True,raw=controller[leaf:leaf+8].hex())),
        native_grant=dict(pattern='80BEF944',authored_path=path.decode(),children=count),assets=assets,
        limits=['The authored typed predicate stays native; its truth value is not fabricated.',
                'Grant children are not interpreted as reward quantities or event completion.',
                'Original native code, not package names, establishes enable/disable and use behavior.'])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Rally interaction inventory passed: {len(assets)} assets; label={label!r}; one controller.')


if __name__ == '__main__':
    main()
