"""Recover native Gateway tactical rows and reconstruct source joins by authored placement.
Provider identities and firing bounds are native data. The choice between valid
rows is host policy: nearest firing bounds, then nearest area center, then row.
"""
import argparse,json,math,re,struct
from functools import lru_cache
from pathlib import Path
from package_read import read,ROOT
from extract_gateway_bindings import array
OUT=ROOT/'build/coo/gateway-ai-lattice'
TARGET=ROOT/'Dawn/src/state/activity/gateway/ai_bindings.h'
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
u64=lambda b,o:struct.unpack_from('<Q',b,o)[0]
i64=lambda b,o:struct.unpack_from('<q',b,o)[0]

@lru_cache(None)
def recover():
 authored=json.loads((ROOT/'build/coo/gateway-research/gateway-authored-bindings.json').read_text())
 groups={}
 for tag,registry,allowed in [(0x80F470D9,0x85742F3E,(5,6,7)),(0x80F46EB5,0x4B946B28,(16,17,18,19,20,21,22))]:
  _,b=read(tag)
  areas={u64(b,p):{'slot':u32(b,p+16),'min':struct.unpack_from('<3f',b,p+32),'max':struct.unpack_from('<3f',b,p+48)} for p in array(b,0x200,128,0x80808354)}
  providers={u32(b,p+12):[areas[u64(b,q)] for q in array(b,p+16,16)] for p in array(b,0x210,32,0x80808350)}
  group=next(g for g in authored['groups'] if g['registry']==registry)
  for slot in group['slots']:
   if slot['slotTypes']!=3 or slot['slotIndices'] not in allowed:continue
   assert slot['componentClasses']==0x80808348 and slot['authSchemas']==0x80807F0C
   _,g=read(slot['descriptorTags']);rows=[]
   for i,p in enumerate(array(g,slot['descriptorOffsets']+0x88,40)):
    ps=[]
    for q in array(g,p+16,40):
     reg,kind,index=struct.unpack_from('<IHH',g,q+32)
     assert (reg,kind)==(registry,45) and index in providers
     ps.append({'slot':index,'areas':providers[index]})
    assert ps;rows.append({'row':i,'providers':ps})
   groups[registry,slot['slotIndices']]={'registry':registry,'group':slot['slotIndices'],'definition':slot['descriptorTags'],'offset':slot['descriptorOffsets'],'name':slot['name'],'rows':rows}
 # Authored placement records carry their spawn-rule GUID at +0x70 and XYZ at +0x20.
 # Include inline source rules and external rules in the Gateway package range.
 points={}
 for tag in range(0x80F46D00,0x80F47431):
  try:_,b=read(tag)
  except (KeyError,ValueError,AssertionError):continue
  for match in re.finditer(re.escape(struct.pack('<II',0x304,0)),b):
   p=match.start()-0x68
   if p<0 or p%16 or p+0x78>len(b):continue
   xyz=struct.unpack_from('<3f',b,p+0x20)
   if u32(b,p+0x2C)!=0x3F800000 or not all(math.isfinite(v) and abs(v)<10000 for v in xyz):continue
   points.setdefault(u64(b,p+0x70),[]).append({'tag':tag,'offset':p,'position':xyz})
 joins=[]
 for source in authored['sources']:
  reg,slot=source['registry'],source['slot']
  if reg==0x85742F3E and 206<=slot<=232:
   groupSlot=5 if slot<=212 else 6 if slot<=222 else 7
  elif reg==0x4B946B28 and 45<=slot<=64:groupSlot=16 if 49<=slot<=60 else 17
  elif reg==0x4B946B28 and 65<=slot<=106:groupSlot=22 if slot<=76 else 21 if slot<=88 else 18 if slot<=92 else 19 if slot<=98 else 20
  else:continue
  _,b=read(source['tag'])
  if source['ruleSlot']==65535:
   records=array(b,0x770,0x50,0x80809840)
   assert len(records)==1
   guid=u64(b,records[0])
  else:
   group=next(g for g in authored['groups'] if g['registry']==reg)
   rule=next(s for s in group['slots'] if s['slotTypes']==66 and s['slotIndices']==source['ruleSlot'])
   _,ruleBytes=read(rule['descriptorTags']);guid=u64(ruleBytes,0x3C0)
  positions=points.get(guid,[])
  if reg==0x4B946B28 and slot>=65 and source['ruleSlot']!=65535:
   # Native external rules may have several placements. Use their mean for the
   # task assignment policy; never interpret alternative placements as actors.
   headers=[m.start() for m in re.finditer(re.escape(struct.pack('<I',0x80809845)),ruleBytes)]
   assert len(headers)==1
   h=headers[0];n=u64(ruleBytes,h-8);assert 1<=n<=32 and h+8+n*8<=len(ruleBytes)
   guids=[u64(ruleBytes,h+8+i*8) for i in range(n)]
   assert all(g in points for g in guids)
   positions=[p for g in guids for p in points[g]]
  assert positions,(source['name'],hex(guid))
  unique=sorted(set(tuple(p['position']) for p in positions))
  xyz=tuple(sum(p[i] for p in unique)/len(unique) for i in range(3))
  ranked=[]
  for row in groups[reg,groupSlot]['rows']:
   candidates=[]
   for provider in row['providers']:
    for area in provider['areas']:
     d=sum(max(a-x,0,x-z)**2 for x,a,z in zip(xyz,area['min'],area['max']))
     center=sum((x-(a+z)/2)**2 for x,a,z in zip(xyz,area['min'],area['max']))
     candidates.append((d,center))
   ranked.append((*min(candidates),row['row']))
  ranked.sort();row=ranked[0][2]
  assert row<24
  joins.append({'registry':reg,'source':slot,'group':groupSlot,'row':row,'name':source['name'],'guid':f'{guid:016X}','placements':positions,'rankedRows':ranked})
 assert len(joins)==82
 return {'groups':list(groups.values()),'joins':joins,'policy':__doc__}

def assignment(registry,slot):
 if registry==0x85742F3E and 16<=slot<=202 and (slot-16)%18 in (0,2,4,6):return 24+((slot-16)//18)*18,0
 row=next(j for j in recover()['joins'] if j['registry']==registry and j['source']==slot)
 return row['group'],row['row']

def generate():
 report=recover()
 lines=['// Generated from package tactical groups and placement policy by generate_gateway_ai.py.','#pragma once','#include "traversal_catalog.h"','#include "../coo/native_combatant_authority.h"','namespace dawn::state::activity::gateway {',
 'struct TacticalJoin { std::uint32_t registry; std::uint16_t source,group; std::int8_t row; };','inline constexpr TacticalJoin kTacticalJoins[]{']
 for j in report['joins']:lines.append('    {0x%08XU,%d,%d,%d}, // %s'%(j['registry'],j['source'],j['group'],j['row'],j['name']))
 lines += ['};','[[nodiscard]] constexpr coo::native_combatant::TacticalGroup tactical_group(const Spawn& source) noexcept {',
 '    if(source.registry==kTraversalRegistry && source.cohort==0) { return {source.registry,static_cast<std::uint16_t>(24+((source.source-16)/18)*18),0}; }',
 '    for(const auto& join:kTacticalJoins) { if(join.registry==source.registry && join.source==source.source) { return {join.registry,join.group,join.row}; } }',
 '    return {};','}',
 'static_assert([] { for(const auto& source:kSpawns) { if(tactical_group(source).row<0) { return false; } } return true; }());','}','']
 return '\n'.join(lines),report

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
 header,report=generate()
 if args.check:assert TARGET.read_bytes()==header.encode()
 else:TARGET.write_bytes(header.encode());OUT.mkdir(parents=True,exist_ok=True);(OUT/'tactical-joins.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Verified 82 combat source joins against native tasks and authored placements.')
 if not args.check:
  for j in report['joins']:print(j['source'],j['group'],j['row'],j['rankedRows'][0],j['placements'][0]['position'])
if __name__=='__main__':main()
