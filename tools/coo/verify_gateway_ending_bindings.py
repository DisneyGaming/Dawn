"""Verify Gateway return/finale package identities and independent native wire fixtures."""
import argparse,hashlib,importlib.util,json,re,struct
from pathlib import Path
from generate_gateway_ai import assignment
from package_read import read
ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/'Dawn/unit/fixtures/gateway_ending_wire.h'
OUT=ROOT/'build/coo/gateway-ending-research'

def generate():
 spec=importlib.util.spec_from_file_location('ending_oracle',ROOT/'build/scot-panoptes-native-graph-20260905/authority/verify_authority_schema.py')
 oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 oracle.SCHEMAS.update({0x8080626B:0x37D8890,0x80807ED9:0x3834870,0x808094E1:0x394F570,0x808094F3:0x390E4D8,0x808094F2:0x3930498,0x808094DF:0x3903DB0})
 authored=json.loads((ROOT/'build/coo/gateway-research/gateway-authored-bindings.json').read_text())
 groups={g['registry']:g for g in authored['groups']}
 sources={s['slot']:s for s in authored['sources'] if s['registry']==0x4B946B28 and 65<=s['slot']<=106}
 rows=re.findall(r'\{(\d+),kMainlandRegistry,0x([0-9A-F]+)U,0x([0-9A-F]+)U,(\d+),(\d+),(\d+),(\d+),(true|false)\}',(ROOT/'Dawn/src/state/activity/gateway/traversal_catalog.h').read_text())
 rows=[r for r in rows if 65<=int(r[0])<=106]
 assert len(rows)==len(sources)==40
 cohorts={}
 for slot,tag,offset,rule,count,categories,cohort,required in rows:
  slot=int(slot);s=sources[slot];n=next(n for n in groups[0x4B946B28]['slots'] if n['slotTypes']==1 and n['slotIndices']==slot)
  assert (int(tag,16),int(offset,16),int(rule),int(categories))==(s['tag'],s['offset'],s['ruleSlot'],len(s['categories']))
  assert (n['descriptorTags'],n['descriptorOffsets'],n['componentClasses'],n['authSchemas'])==(s['tag'],s['offset'],0x80809A3B,0x80807EC9)
  assert int(count)==int(categories) and int(count) in (1,2)
  expected=9 if slot<=76 else 10 if slot<=88 else 11 if slot<=92 else 12 if slot<=98 else 13 if slot==99 else 14
  assert int(cohort)==expected and (required=='true')==(slot>=89)
  cohorts[expected]=cohorts.get(expected,0)+int(count)
 identities={}
 for reg,kind,slot,tag,offset in [(0x4B946B28,4,26,0x80F46F11,0x4C8),(0x4B946B28,4,27,0x80F46F14,0x4C8),(0x4B946B28,4,28,0x80F46F17,0x4C8),(0x4B946B28,4,32,0x80F46F23,0x4C8),(0x4B946B28,4,33,0x80F46F26,0x4C8),(0x4B946B28,23,0,0x80F46EC3,0x278),(0x4B946B28,23,3,0x80F46ECC,0x278),(0x4B946B28,23,4,0x80F46ECF,0x278),(0xBA0B27A0,1,4,0x80F46DDD,0x878),(0xBA0B27A0,43,5,0x80F46DE0,0x368)]:
  s=next(s for s in groups[reg]['slots'] if s['slotTypes']==kind and s['slotIndices']==slot)
  assert (s['descriptorTags'],s['descriptorOffsets'])==(tag,offset)
  _,data=read(tag);identities[f'{reg:08X}/{kind}/{slot}']={'definition':f'{tag:08X}','offset':offset,'sha256':hashlib.sha256(data).hexdigest()}
 _,module=read(0x80F4803C);assert struct.pack('<I',0x80F48026) in module
 _,scene=read(0x80F46DE0);assert struct.pack('<I',0x80EC0ABD) in scene
 _,parent=read(0x80EC0ABC);assert struct.pack('<I',0x80EC0AC6) in parent
 _,child=read(0x80EC0AC5)
 for selector in (0x057FF1E6,0x80A617F1,0x65BDC761,0xDF2E40B0,0x5B932E1B):assert struct.pack('<I',selector) in child
 assert oracle.IMAGE[0x9F0750:0x9F0750+16]==bytes.fromhex('4055564154488dac2460f4ffff4881ec')
 # These instruction bytes recover the resource chain and runtime metadata offsets.
 assert oracle.IMAGE[0x591290:0x591293]==bytes.fromhex('8b524c')
 assert oracle.IMAGE[0x59A401:0x59A404]==bytes.fromhex('8b4e18')
 assert oracle.IMAGE[0x557535:0x557539]==bytes.fromhex('488b4230')
 assert oracle.IMAGE[0x55753D:0x557542]==bytes.fromhex('4863441154')
 fixtures={}
 for slot in (65,69,89,98,99,101):
  s=sources[slot];data=oracle.defaults(0x80807EC9);n=len(s['categories']);group,row=assignment(0x4B946B28,slot)
  struct.pack_into('<IBBH',data,0,0x4B946B28,3,0,group);oracle.set32(data,0x2C,n)
  for i in range(n):oracle.set32(data,0x30+4*i,1)
  data[0x74:0x79]=bytes([0,0,255,255,255])
  for off,value in [(0x7C,7),(0x80,0),(0x84,0),(0xA8,0),(0xAC,0),(0xB0,-1),(0xB4,row),(0xB8,7)]:oracle.set32(data,off,value)
  data[0xBC]=1;data[0xBD]=0
  struct.pack_into('<IBBH',data,0x98,s['ruleRegistry'],66,0,s['ruleSlot'])
  fixtures[f'source{slot}']=oracle.encode(0x80807EC9,data)
 def scene_wire(active):
  data=oracle.defaults(0x8080626B);oracle.set32(data,0,7 if active else -1);oracle.set32(data,8,int(active));oracle.set32(data,0x4C,int(active))
  if active:struct.pack_into('<IBBH',data,12,0xBA0B27A0,1,0,4)
  bits=[]
  def put(value,n):bits.extend((value>>i)&1 for i in reversed(range(n)))
  def walk(tag,base,count=None):
   fields=oracle.descriptor(tag)['fields']
   if count is not None:
    assert len(fields)==1;f=fields[0]
    for i in range(count):
     at=base+i*f['offset']
     if f['kind']==1:walk(f['child'],at)
     else:put(oracle.native_scalar(data,at,f['kind']),f['width'])
    return
   for f in fields:
    assert not f['optional'];at=base+f['offset']
    if f['kind']==1:walk(f['child'],at,struct.unpack_from('<I',data,base)[0] if f['bias']==1 else None)
    else:put((oracle.native_scalar(data,at,f['kind'])+f['bias'])&0xFFFFFFFF,1 if f['kind']==2 else f['width'])
  walk(0x8080626B,0);packed=bytearray((len(bits)+7)//8)
  for i,value in enumerate(bits):packed[i//8]|=value<<(7-i%8)
  return len(bits),bytes(packed)
 fixtures['sceneDormant']=scene_wire(False);fixtures['sceneActive']=scene_wire(True)
 assert fixtures['sceneDormant'][0]==74 and fixtures['sceneActive'][0]==129
 header=['// Generated independently from native reflection by verify_gateway_ending_bindings.py.','#pragma once','#include <array>','#include <cstdint>','namespace gateway_ending_wire_fixture {']
 for name,(size,data) in fixtures.items():
  header.append(f'inline constexpr unsigned {name}Bits={size};')
  header.append(f'inline constexpr std::array<std::uint8_t,{len(data)}> {name}{{'+','.join(f'0x{x:02X}' for x in data)+'};')
 header.append('}')
 evidence={'sources':40,'cohortActors':cohorts,'identities':identities,'fixtures':{name:{'bits':size,'hex':data.hex()} for name,(size,data) in fixtures.items()},'nativeValidation':'pending','policy':'One actor per authored category. Return enemies and final supports do not gate the module; wave1, wave2 and the Module Minotaur require real deaths. Scene owns rows11..15. Completion lifecycle phase6/result1 is a reconstruction pending live HUD verification.'}
 return '\n'.join(header)+'\n',evidence

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args();header,evidence=generate()
 if args.check:assert FIXTURE.read_bytes()==header.encode(),'Ending wire fixtures changed'
 else:
  FIXTURE.write_bytes(header.encode());OUT.mkdir(parents=True,exist_ok=True);(OUT/'ending-native.json').write_text(json.dumps(evidence,indent=2)+'\n')
 print('Verified 40 return/finale sources, 10 object/scene identities and 8 independent native wire fixtures.',flush=True)
if __name__=='__main__':main()
