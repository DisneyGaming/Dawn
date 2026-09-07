"""Verify scoped mainland sources against package identities and native-reflected wire bytes."""
import argparse,hashlib,importlib.util,json,re,struct
from generate_gateway_ai import assignment
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/'Sunrise/unit/fixtures/gateway_mainland_wire.h'

def generate():
 image=(ROOT/'destiny2_unpacked.bin').read_bytes()
 assert hashlib.sha256(image).hexdigest()=='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
 spec=importlib.util.spec_from_file_location('mainland_oracle',ROOT/'build/scot-panoptes-native-graph-20260905/authority/verify_authority_schema.py')
 oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 authored=json.loads((ROOT/'build/coo/gateway-research/gateway-authored-bindings.json').read_text())
 sources={s['slot']:s for s in authored['sources'] if s['registry']==0x4B946B28 and 45<=s['slot']<=64}
 group=next(g for g in authored['groups'] if g['registry']==0x4B946B28)
 text=(ROOT/'Sunrise/src/state/activity/gateway/traversal_catalog.h').read_text()
 rows=re.findall(r'\{(\d+),kMainlandRegistry,0x([0-9A-F]+)U,0x([0-9A-F]+)U,(\d+),(\d+),(\d+),(\d+),(true|false)\}',text)
 rows=[row for row in rows if 45<=int(row[0])<=64]
 assert len(rows)==len(sources)==19
 actors=0
 for row in rows:
  slot,tag,offset,rule,count,categories,cohort,required=row;s=sources[int(slot)]
  native=next(n for n in group['slots'] if n['slotTypes']==1 and n['slotIndices']==int(slot))
  assert (int(tag,16),int(offset,16),int(rule),int(categories))==(s['tag'],s['offset'],s['ruleSlot'],len(s['categories']))
  assert (native['descriptorTags'],native['descriptorOffsets'],native['componentClasses'],native['authSchemas'])==(s['tag'],s['offset'],0x80809A3B,0x80807EC9)
  assert s['ruleSlot']==65535 or s['ruleRegistry']==0x4B946B28
  assert int(count)==int(categories) and int(categories) in (1,2)
  assert (int(cohort),required)==((8,'true') if 49<=int(slot)<=60 else (7,'false'))
  actors+=int(count)
 fixtures={}
 for slot in (47,49,63):
  s=sources[slot];data=oracle.defaults(0x80807EC9);n=len(s['categories'])
  group,row=assignment(0x4B946B28,slot)
  struct.pack_into('<IBBH',data,0,0x4B946B28,3,0,group)
  oracle.set32(data,0x2C,n)
  for i in range(n):oracle.set32(data,0x30+4*i,1)
  data[0x74:0x79]=bytes([0,0,255,255,255])
  for off,value in [(0x7C,7),(0x80,0),(0x84,0),(0xA8,0),(0xAC,0),(0xB0,-1),(0xB4,row),(0xB8,7)]:oracle.set32(data,off,value)
  data[0xBC]=1;data[0xBD]=0
  if s['ruleSlot']!=65535:struct.pack_into('<IBBH',data,0x98,s['ruleRegistry'],66,0,s['ruleSlot'])
  fixtures[f'source{slot}']=oracle.encode(0x80807EC9,data)
 header=['// Generated from native reflection by verify_gateway_mainland_bindings.py.','#pragma once','#include <array>','#include <cstdint>','namespace gateway_mainland_wire_fixture {']
 for name,(size,data) in fixtures.items():
  header.append(f'inline constexpr unsigned {name}Bits={size};')
  header.append(f'inline constexpr std::array<std::uint8_t,{len(data)}> {name}{{'+','.join(f'0x{x:02X}' for x in data)+'};')
 header.append('}')
 evidence={'registry':'4B946B28','sources':19,'actors':actors,'fixtures':{name:{'bits':size,'hex':data.hex()} for name,(size,data) in fixtures.items()},'nativeValidation':'pending','policy':'One actor per authored category; 12 outskirts actors optional for gate clearance, 15 central defenders required. No mainland marching tactical group.'}
 return '\n'.join(header)+'\n',evidence

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
 header,evidence=generate()
 if args.check:assert FIXTURE.read_bytes()==header.encode(), 'Mainland wire fixtures changed'
 else:
  FIXTURE.write_bytes(header.encode())
  out=ROOT/'build/coo/gateway-final-cannon-research/mainland-native.json';out.write_text(json.dumps(evidence,indent=2)+'\n')
 print('Verified 19 mainland sources, 27 requested actors, and 3 independent native wire fixtures.',flush=True)
if __name__=='__main__':main()
