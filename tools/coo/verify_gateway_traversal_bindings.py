"""Offline Gateway native identity/reflection checks and independent wire fixtures."""
import argparse, hashlib, importlib.util, json, re, struct, zipfile
from generate_gateway_ai import assignment
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/coo/gateway-research'
FIXTURE=ROOT/'Dawn/unit/fixtures/gateway_traversal_wire.h'

def generate():
 image=(ROOT/'destiny2_unpacked.bin').read_bytes()
 assert hashlib.sha256(image).hexdigest()=='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
 spec=importlib.util.spec_from_file_location('native_oracle',ROOT/'build/scot-panoptes-native-graph-20260905/authority/verify_authority_schema.py')
 oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 oracle.SCHEMAS.update({0x8080956A:0x38E6350,0x8080956D:0x38E4428,0x8080956B:0x3912B40,0x80809579:0x3788E90,0x8080954B:0x37AAE20})
 reflection={f'{tag:08X}':oracle.descriptor(tag) for tag in oracle.SCHEMAS}
 assert oracle.descriptor(0x8080956D)['fields'][0]['width']==4
 assert oracle.descriptor(0x8080956B)['size']==0x180
 assert oracle.descriptor(0x8080956B)['fields'][0]['raw'][0]==0x30
 assert oracle.descriptor(0x80809579)['fields'][0]['bias']==1
 # ABB27E registers ABA810 with the exact runtime selector80809579.
 base=0x7FF618070000
 pointer=struct.unpack_from('<Q',image,0x1FA26C0)[0]-base
 assert struct.unpack_from('<I',image,pointer)[0]==0x80809579
 assert image[0xABB27E:0xABB285]==bytes.fromhex('488D158BF5FFFF')
 native=json.loads((OUT/'gateway-authored-bindings.json').read_text())
 sources=sorted((s for s in native['sources'] if s['registry']==0x85742F3E),key=lambda s:s['slot'])
 text=(ROOT/'Dawn/src/state/activity/gateway/traversal_catalog.h').read_text()
 rows=re.findall(r'\{(\d+),kTraversalRegistry,0x([0-9A-F]+)U,0x([0-9A-F]+)U,(\d+),(\d+),(\d+),(\d+),(true|false)\}',text)
 assert len(rows)==len(sources)==67
 for row,s in zip(rows,sources):
  slot,tag,offset,rule,count,categories,cohort,required=row
  assert (int(slot),int(tag,16),int(offset,16),int(rule),int(categories))==(s['slot'],s['tag'],s['offset'],s['ruleSlot'],len(s['categories']))
  assert int(count)==int(categories) and (required=='true')==(int(cohort)>0)
 def source_wire(slot):
  s=next(s for s in sources if s['slot']==slot);data=oracle.defaults(0x80807EC9);n=len(s['categories'])
  group,row=assignment(0x85742F3E,slot)
  oracle.set32(data,0x2C,n)
  for i in range(n):oracle.set32(data,0x30+4*i,1)
  data[0x74:0x79]=bytes([0,0,255,255,255])
  for off,value in [(0x7C,7),(0x80,0),(0x84,0),(0xA8,0),(0xAC,0),(0xB0,-1),(0xB4,row),(0xB8,7)]:oracle.set32(data,off,value)
  data[0xBC]=1;data[0xBD]=0
  if s['ruleSlot']!=65535:struct.pack_into('<IBBH',data,0x98,s['ruleRegistry'],66,0,s['ruleSlot'])
  struct.pack_into('<IBBH',data,0,0x85742F3E,3,0,group)
  return oracle.encode(0x80807EC9,data)
 fixtures={f'source{slot}':source_wire(slot) for slot in (16,216,230)}
 # The effect uses the reflected ordinary scoped reference and no dynamic selector.
 data=bytearray(0x70);struct.pack_into('<IBBH',data,0x14,0x85742F3E,34,0,268)
 bits=[]
 def put(value,width):bits.extend((value>>i)&1 for i in reversed(range(width)))
 def scalar(tag,data,base=0):
  for f in oracle.descriptor(tag)['fields']:
   assert not f['optional'];o=base+f['offset']
   if f['kind']==1:scalar(f['child'],data,o)
   elif f['kind']==34:put(0,1)
   else:put((oracle.native_scalar(data,o,f['kind'])+f['bias'])&0xFFFFFFFF,1 if f['kind']==2 else f['width'])
 def pack():
  out=bytearray((len(bits)+7)//8)
  for i,b in enumerate(bits):out[i//8]|=b<<(7-i%8)
  return len(bits),bytes(out)
 scalar(0x8080954B,data);fixtures['effect']=pack();bits.clear()
 put(4,oracle.descriptor(0x8080956D)['fields'][0]['width'])
 for slot in (16,18,20,22):
  put(1,1);put(0x80809579,32);data=bytearray(12);struct.pack_into('<IBBH',data,4,0x85742F3E,1,0,slot);scalar(0x80809579,data)
 fixtures['collection']=pack()
 header=['// Generated independently from native reflection by verify_gateway_traversal_bindings.py.','#pragma once','#include <array>','#include <cstdint>','namespace gateway_wire_fixture {']
 for name,(size,data) in fixtures.items():
  header.append(f'inline constexpr unsigned {name}Bits={size};')
  header.append(f'inline constexpr std::array<std::uint8_t,{len(data)}> {name}{{'+','.join(f'0x{x:02X}' for x in data)+'};')
 header.append('}')
 evidence={'reflection':reflection,'sourceCount':len(sources),'fixtures':{name:{'bits':size,'hex':data.hex()} for name,(size,data) in fixtures.items()},'nativeValidation':'pending','policy':'One actor per category; cohorts and device timing are reconstruction choices. Template alternatives are not spawn counts.'}
 return '\n'.join(header)+'\n',evidence

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
 header,evidence=generate()
 if args.check:assert FIXTURE.read_bytes()==header.encode(), 'Gateway wire fixtures changed'
 else:FIXTURE.write_bytes(header.encode());(OUT/'gateway-traversal-native.json').write_text(json.dumps(evidence,indent=2)+'\n')
 print('Verified67 Gateway sources and5 independent native wire fixtures',flush=True)
if __name__=='__main__':main()
