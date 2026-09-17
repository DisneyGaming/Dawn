"""Recover Gateway authored identities and presentation data from installed packages."""
import json,struct as s,re,hashlib
from functools import lru_cache
from pathlib import Path
import package_read as p
ROOT=p.ROOT
OUT=ROOT/'build/coo/gateway-research'
u32=lambda b,o:s.unpack_from('<I',b,o)[0]
u64=lambda b,o:s.unpack_from('<Q',b,o)[0]
i64=lambda b,o:s.unpack_from('<q',b,o)[0]

def array(b,o,stride,kind=None):
 n=u64(b,o)
 if not n:return []
 h=o+8+i64(b,o+8)
 assert 0<=h<=len(b)-16 and u64(b,h)==n,(hex(o),n,hex(h))
 if kind is not None:assert u32(b,h+8)==kind,(hex(o),hex(u32(b,h+8)),hex(kind))
 start=h+16;assert n<100000 and start+n*stride<=len(b)
 return list(range(start,start+n*stride,stride))

@lru_cache(None)
def strings(tag):
 cls,b=p.read(tag)
 if cls!=0x80809A88:return {}
 keys=[u32(b,o) for o in array(b,8,4,0x80800070)]
 cls,eng=p.read(u32(b,24))
 if cls!=0x80809A8A:return {}
 rows=array(eng,8,32,0x80809A90)
 assert len(rows)==len(keys)
 result={}
 for key,o in zip(keys,rows):
  at=o+8+i64(eng,o+8);n=s.unpack_from('<H',eng,o+20)[0];bias=s.unpack_from('<H',eng,o+24)[0]
  assert 0<=at<=at+n<=len(eng)
  result[key]=''.join(chr(ord(c)+bias) for c in eng[at:at+n].decode('utf-8'))
 return result

def dialogue():
 tag=0x80F1FC9E;_,b=p.read(tag)
 roots={u32(b,o):o+8+i64(b,o+8) for o in array(b,24,16,0x80808D19)}
 starts=sorted(roots.values())+[len(b)]
 rows=[]
 for index,o in enumerate(array(b,8,8,0x80808D18)):
  selector=u32(b,o);start=roots[selector];end=starts[starts.index(start)+1]
  texts=[]
  for at in range(start,end-7,4):
   container,key=s.unpack_from('<II',b,at)
   if not 0x80F1FC00<=container<=0x80F1FD00:continue
   text=strings(container).get(key)
   if text and text not in texts:texts.append(text)
  rows.append({'row':index,'selector':f'{selector:08X}','durationMs':round(s.unpack_from('<f',b,o+4)[0]*1000),'texts':texts})
 return rows

def objectives():
 _,b=p.read(0x80F4741F);out=[]
 for index,o in enumerate(array(b,8,40,0x80804F74)):
  v=array(b,o+16,32,0x80804F76)
  out.append({'row':index,'event':f'{u32(b,o):08X}','text':[[strings(u32(b,a+j)).get(u32(b,a+j+4),'') for j in (0,8,16,24)] for a in v]})
 return out

def groups():
 layout=json.loads((OUT/'cache-layout.json').read_text()); sections=json.loads((OUT/'cache-sections.json').read_text())
 cache=(ROOT/'Dawn/cache/build_data.bin').read_bytes(); assert u32(cache,8)==52
 record=layout['records']['RosterGroupRecord']; region=sections['RosterGroupRecord']
 walk=json.loads((OUT/'gateway-registry-walk.json').read_text())
 tags={int(o['tag'],16):o['array'] for r in walk['regions'] if r['bubble']==15 and r['state']==0 for o in r['objects']}
 out=[]
 for index in range(region['count']):
  start=region['offset']+index*record['size'];b=cache[start:start+record['size']]
  registry,tag,n=s.unpack_from('<IIH',b)
  if tag not in tags:continue
  fields=record['fields'];slots=[]
  for j in range(n):
   item={}
   for name in ['slotTypes','slotFlags','slotIndices','descriptorTags','descriptorOffsets','componentClasses','senseSchemas','authSchemas']:
    offset,size=fields[name];stride=size//1280
    item[name]=int.from_bytes(b[offset+j*stride:offset+(j+1)*stride],'little')
   if item['descriptorTags']!=0xFFFFFFFF:
    _,data=p.read(item['descriptorTags']);off=item['descriptorOffsets']
    assert off+56<=len(data)
    assert u32(data,off+48)==registry and s.unpack_from('<H',data,off+54)[0]==item['slotIndices'],(tag,item)
    nameAt=off+0x50+i64(data,off+0x50)
    item['name']=data[nameAt:].split(b'\0',1)[0].decode(errors='replace') if 0<=nameAt<len(data) else ''
    if not re.fullmatch(r'[\w /.-]{3,160}',item['name']):
     names=re.findall(rb'[a-z][a-z_0-9]{10,}',data)
     item['name']=names[-1].decode() if names else ''
   slots.append(item)
  out.append({'registry':registry,'objectTag':tag,'cacheIndex':index,'topLevel':tags[tag]==0,'slots':slots})
 return out

def volumes():
 out=[]
 for tag,registry in [(0x80F470E5,0x85742F3E),(0x80F46EC0,0x4B946B28),(0x80F46DCD,0xBA0B27A0)]:
  _,b=p.read(tag)
  for at in range(12,len(b)-0x114,4):
   if u32(b,at)!=registry or s.unpack_from('<H',b,at+4)[0]!=60:continue
   base=at-12;nameAt=base+i64(b,base)
   if not 0<=nameAt<len(b):continue
   name=b[nameAt:].split(b'\0',1)[0].decode(errors='replace')
   if not re.fullmatch(r'[a-z_0-9]+',name):continue
   if '_volume' not in name and not name.startswith('tv_'):continue
   try:vertices=[s.unpack_from('<3f',b,o) for o in array(b,base+0xd0,16,0x80800094)]
   except (AssertionError,s.error):continue
   out.append({'registry':registry,'slot':s.unpack_from('<H',b,at+6)[0],'tag':tag,'offset':base,'name':name,
      'min':s.unpack_from('<3f',b,base+0xb0),'max':s.unpack_from('<3f',b,base+0xc0),'vertices':vertices})
 return out

def sources(groups):
 out=[]
 for group in groups:
  for slot in group['slots']:
   if slot['slotTypes']!=1:continue
   tag=slot['descriptorTags'];_,b=p.read(tag);base=slot['descriptorOffsets']+0x68
   assert u32(b,base)==tag and u32(b,base+4)==0x80807EB9
   cats=[]
   for c in array(b,base+64,104,0x80808356):
    selections=[]
    for i in range(6):
     entries=[]
     for e in array(b,c+8+16*i,24,0x80808358):
      at=e+i64(b,e)
      assert 4<=at<=len(b)-4 and u32(b,at-4)==0x808099D8,(hex(tag),hex(at))
      entries.append({'entity':u32(b,at),'nameHash':u32(b,e+8),'weight':u32(b,e+12),'offset':at})
     selections.append(entries)
    cats.append({'category':u32(b,c),'selections':selections})
   ruleRegistry,rulePacked=s.unpack_from('<II',b,base+56)
   out.append({'registry':group['registry'],'slot':slot['slotIndices'],'tag':tag,'offset':slot['descriptorOffsets'],
     'name':slot['name'],'categories':cats,'ruleRegistry':ruleRegistry,'ruleType':rulePacked&255,'ruleSlot':rulePacked>>16})
 return out

def main():
 g=groups();result={'dialogue':dialogue(),'objectives':objectives(),'groups':g,'volumes':volumes(),'sources':sources(g)}
 (OUT/'gateway-authored-bindings.json').write_text(json.dumps(result,indent=2)+'\n')
 print('Recovered',len(g),'groups,',len(result['volumes']),'volumes,',len(result['sources']),'sources')
 for name in ['dialogue','objectives']:
  for row in result[name]: print(name,json.dumps(row))

if __name__=='__main__':main()
