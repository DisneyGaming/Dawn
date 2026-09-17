"""Extract Haunted Forest's installed native descriptors; no live process access.

The result is content evidence, not a recovered server program. The optional
header pins the complete start-platform registry for native UE admission.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--re-root', type=Path, default=Path('D:/Dawn-work'))
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--header', type=Path)
    a = p.parse_args()
    sys.path[:0] = [str(a.re_root/'scripts'), str(a.re_root/'mercury_inventory')]
    from pkg import Reader
    from descriptors import descriptors
    from scenario_walk import scenario_objects
    from arrays import find_arrays
    r = Reader()
    objects = scenario_objects(0x81550015)[13]
    report = {'scenario':'81550015','bubble':13,'region':104,'registries':[],
              'limits':['Names and row counts do not recover host timing, wave counts or completion policy.']}
    fixtures = a.output.parent/'fixtures'
    fixtures.mkdir(parents=True, exist_ok=True)
    for tag in objects:
        raw, cls = r.read_tag(tag)
        assert cls == 0x80809462
        key, rows = descriptors(tag)
        item = {'object':f'{tag:08X}','key':f'{key:08X}',
                'sha256':hashlib.sha256(raw).hexdigest().upper(),'slots':[]}
        (fixtures/f'{tag:08X}.bin').write_bytes(raw)
        for ix, ty, component, sense, auth, redirect, definition in rows:
            blob, bc = r.read_tag(definition)
            assert bc == 0x80809C36
            (fixtures/f'{definition:08X}.bin').write_bytes(blob)
            names = [s.decode('ascii') for s in re.findall(rb'[ -~]{7,}',blob)]
            item['slots'].append({'slot':ix,'type':ty,'component':f'{component:08X}',
                'sense':f'{sense:08X}','auth':f'{auth:08X}','redirect':f'{redirect:08X}',
                'definition':f'{definition:08X}','names':names,
                'sha256':hashlib.sha256(blob).hexdigest().upper(),
                'arrays':[{'at':at,'count':v[0],'data':v[1],'class':f'{v[2]:08X}'}
                          for at,v in find_arrays(blob).items()]})
        report['registries'].append(item)
    start = next(x for x in report['registries'] if x['key']=='34D23982')
    expected = {30:(4,'815500A3'),31:(4,'815500A6'),32:(4,'815500A9'),
                109:(30,'81550170'),10:(4,'8155006D'),11:(4,'81550070'),12:(4,'81550073')}
    for ix, value in expected.items():
        row = next(x for x in start['slots'] if x['slot']==ix)
        assert (row['type'], row['definition'])==value
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    if a.header:
        lines = ['#pragma once','#include "registry_admission.h"','',
            'namespace dawn::server::runtime::activity::haunted_forest::mode {',
            '// Generated from installed 81550015 / bubble 13, not the other-build Lua reference.',
            f'inline constexpr std::array<registry::Slot,{len(start["slots"])}> kStartSlots{{{{']
        for row in start['slots']:
            label = row['names'][-1] if row['names'] else 'unnamed'
            lines.append('    {%d,%d,0x%s,0x%s,0x%s,0x%s}, // %s' % (
                row['slot'],row['type'],row['component'],row['sense'],row['auth'],row['definition'],label))
        lines.extend(['}};','inline constexpr std::array<registry::Definition,1> kRegistries{{',
            '    {"infinite_abyss",0x81550015,0x34D23982,0x81550188,0x47EA4CE9,13,kStartSlots},','}};','}'])
        a.header.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(f'{len(objects)} objects; {sum(len(x["slots"]) for x in report["registries"])} native descriptors; start group {len(start["slots"])}.')


if __name__=='__main__':
    main()
