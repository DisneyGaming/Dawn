"""Read verified A Deadly Trial presentation, local volumes, and native spawn sources."""
import json,struct as s,re,hashlib
from pathlib import Path
import package_read as p
from extract_gateway_bindings import array,strings,u32,u64,i64,sources
from extract_deadly_trial_bindings import walk,groups,OUT
SCENARIO=0x80B2E043
BANK=0x80F1F086
OBJECTIVES=0x80B2ED24

def presentation():
 _,b=p.read(BANK)
 roots={u32(b,o):o+8+i64(b,o+8) for o in array(b,24,16)}
 starts=sorted(roots.values())+[len(b)];dialogue=[]
 for index,off in enumerate(array(b,8,8)):
  selector=u32(b,off);start=roots[selector];end=starts[starts.index(start)+1];texts=[]
  for at in range(start,end-7,4):
   container,key=s.unpack_from('<II',b,at)
   if 0x80F1E000<=container<0x80F20000:
    text=strings(container).get(key)
    if text and text not in texts:texts.append(text)
  dialogue.append({'row':index,'selector':selector,'durationMs':round(s.unpack_from('<f',b,off+4)[0]*1000),'texts':texts})
 _,b=p.read(OBJECTIVES);objectives=[]
 for index,off in enumerate(array(b,8,40)):
  objectives.append({'row':index,'event':u32(b,off),'texts':[[strings(u32(b,a+j)).get(u32(b,a+j+4),'') for j in (0,8,16,24)] for a in array(b,off+16,32)]})
 return dialogue,objectives

def local_bindings():
 volumes=[];names=[];placements=[]
 for tag in range(0x80B2E000,0x80B2ED5B):
  cls,b=p.read(tag)
  text=[m.group().decode() for m in re.finditer(rb'[a-z][a-z_0-9.\[\]]{8,}',b)]
  names.append({'tag':tag,'names':text})
  for at in range(12,len(b)-0x114,4):
   registry=u32(b,at)
   if s.unpack_from('<H',b,at+4)[0]!=60:continue
   base=at-12;name_at=base+i64(b,base)
   if not 0<=name_at<len(b):continue
   name=b[name_at:].split(b'\0',1)[0].decode(errors='replace')
   if not re.fullmatch(r'[a-z_0-9.\[\]]+',name):continue
   try:vertices=[s.unpack_from('<3f',b,o) for o in array(b,base+0xD0,16,0x80800094)]
   except (AssertionError,s.error):continue
   volumes.append({'tag':tag,'offset':base,'registry':registry,'slot':s.unpack_from('<H',b,at+6)[0],'name':name,'min':s.unpack_from('<3f',b,base+0xB0),'max':s.unpack_from('<3f',b,base+0xC0),'vertices':vertices})
  for m in re.finditer(re.escape(s.pack('<II',0x304,0)),b):
   o=m.start()-0x68
   if o<0 or o%16 or o+0x78>len(b) or u32(b,o+0x2C)!=0x3F800000:continue
   placements.append({'tag':tag,'offset':o,'guid':u64(b,o+0x70),'position':s.unpack_from('<3f',b,o+0x20)})
 return volumes,names,placements

def main():
 result=walk(SCENARIO);result['groups']=groups(result)
 result['dialogue'],result['objectives']=presentation()
 result['sources']=sources(result['groups'])
 result['volumes'],result['namedTags'],result['placements']=local_bindings()
 result['evidence']={f'{tag:08X}':hashlib.sha256(p.read(tag)[1]).hexdigest() for tag in (SCENARIO,BANK,OBJECTIVES,0x80B2E004,0x81327CF0,0x81327CD8)}
 OUT.mkdir(parents=True,exist_ok=True)
 (OUT/'native-bindings.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print('Recovered',len(result['sources']),'sources,',len(result['volumes']),'volumes,',len(result['placements']),'placements')
 for v in result['volumes']:
  if v['registry'] not in (0xEB7AF018,0xEB7AF01B):print(f"{v['registry']:08X}",v['slot'],v['name'],v['min'],v['max'])
if __name__=='__main__':main()
