"""Generate full ambush source wire fixtures with the native reflection oracle."""
import importlib.util,json,struct
from pathlib import Path
from package_read import ROOT
from generate_beyond_infinity_ai import generate
spec=importlib.util.spec_from_file_location('oracle',ROOT/'build/scot-panoptes-native-graph-20260905/authority/verify_authority_schema.py')
o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
data=json.loads((ROOT/'build/coo/beyond-infinity-research/native-bindings.json').read_text())
_,report=generate()
lines=['// Independent reflected 80807EC9 fixtures, generation17, active counts.','#pragma once','#include <array>','#include <cstdint>','namespace beyond_ai_wire_fixture {']
for j in report['joins']:
 source=next(s for s in data['sources'] if s['registry']==0x0FF26BCC and s['slot']==j['source'])
 b=o.defaults(0x80807EC9);n=len(source['categories']);o.set32(b,0x2C,n)
 for i in range(n):o.set32(b,0x30+4*i,1)
 b[0x74:0x79]=bytes([0,0,255,255,255])
 for off,value in [(0x7C,17),(0x80,0),(0x84,0),(0xA8,0),(0xAC,0),(0xB0,-1),(0xB4,j['row']),(0xB8,17)]:o.set32(b,off,value)
 b[0xBC]=1;b[0xBD]=0
 struct.pack_into('<IBBH',b,0x98,source['ruleRegistry'],66,0,source['ruleSlot'])
 struct.pack_into('<IBBH',b,0,0x0FF26BCC,3,0,0)
 count,packed=o.encode(0x80807EC9,b)
 assert count==641+32*(n-1)
 lines.append('inline constexpr std::array<std::uint8_t,%d> source%d{%s};'%(len(packed),j['source'],','.join('0x%02X'%x for x in packed)))
lines+=['}','']
(ROOT/'Sunrise/unit/fixtures/beyond_infinity/ai_wire.h').write_text('\n'.join(lines))
print('PASS: nine source wire fixtures encoded from native reflection')
