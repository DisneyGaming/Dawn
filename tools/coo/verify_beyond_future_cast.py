"""Execute native Future actor fallback/binding/cleanup with stubbed external services.

This edits emulator memory only. It does not access or modify the running game.
"""
import sys,struct,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/coo'))
from package_read import read
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import *
IMAGE=0x140000000;CHILD=0x420000000;STACK=0x500000000;STOP=0x600000000
DIR=0x400000000;ROWS=0x400001000;DATA=0x410000000;PACKAGE=DATA+0x10000;OBJECTS=0x430000000
image=(ROOT/'destiny2_unpacked.bin').read_bytes();_,graph=read(0x80EC0872)
p32=lambda x:struct.pack('<I',x);p64=lambda x:struct.pack('<Q',x)
results=[]
def case(fallback=True,fail_factory=False,external=False):
 u=Uc(UC_ARCH_X86,UC_MODE_64)
 for a,n in [(IMAGE,(len(image)+4095)&~4095),(DIR,0x2000),(DATA,0x30000),(CHILD,0x10000),(STACK,0x20000),(STOP,0x1000),(OBJECTS,0x200000)]:u.mem_map(a,n)
 u.mem_write(IMAGE,image);u.mem_write(PACKAGE,graph);u.mem_write(CHILD,graph[0x90:0x2648])
 def w32(a,v):u.mem_write(a,p32(v))
 def w64(a,v):u.mem_write(a,p64(v))
 def r32(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def r64(a):return struct.unpack('<Q',u.mem_read(a,8))[0]
 w64(IMAGE+0x2439C70,DIR);w64(DIR,ROWS);w64(ROWS+8,DATA);w32(ROWS+0x30,0x10000);w32(ROWS+0x34,0)
 w32(CHILD,1);w64(CHILD+8,0x2648);w32(CHILD+0x24,0x22F9EA0C);w32(CHILD+0x2C,0x100)
 w64(IMAGE+0x1FA5F80,DIR+0x100);w32(DIR+0x100,0x2222)
 w64(IMAGE+0x1FA60A0,DIR+0x108);w32(DIR+0x108,0x1111)
 w64(IMAGE+0x1F93428,OBJECTS);w32(IMAGE+0x1F93430,0x100)
 w32(OBJECTS+0x12300+0x40,0xffffffff);w32(OBJECTS+0x12300+0x44,0xffffffff)
 w32(PACKAGE+0x2964,0x80EC1113 if fallback else 0xffffffff)
 trace=[];factory_count=0
 def stub_return(value=0):
  u.reg_write(UC_X86_REG_RAX,value);sp=u.reg_read(UC_X86_REG_RSP);u.reg_write(UC_X86_REG_RIP,r64(sp));u.reg_write(UC_X86_REG_RSP,sp+8)
 def hook(uc,address,size,_):
  nonlocal factory_count
  if address==STOP:u.emu_stop();return
  rva=address-IMAGE;c=u.reg_read(UC_X86_REG_RCX);d=u.reg_read(UC_X86_REG_RDX);e=u.reg_read(UC_X86_REG_R8)
  if rva==0x4B2570:
   assert d==0x80EC1113 and r64(c)==0 and r32(c+0x18)==0xffffffff and r64(c+0x20)==0x800 and r64(c+0x28)==0x10 and r64(c+0x30)==0
   w64(c,0x40);w32(c+0x40,d);trace.append('initialize');stub_return(1)
  elif rva==0x4B7340:
   assert (d,e)==(0x1111,0x2222);trace.append('enrich');stub_return(c+0xd0)
  elif rva==0x4B4640:
   assert d==0x22F9EA0C;w32(c+0x34,0x100);trace.append('owner');stub_return()
  elif rva==0x597ED0:
   assert c==CHILD;u.mem_write(d,bytes(range(32)));trace.append('pose');stub_return(d)
  elif rva==0x56D990:
   assert r32(d)==0x80EC1113 and r32(d+0x34)==0x100 and bytes(u.mem_read(d+0x10,32))==bytes(range(32))
   assert r32(d+0x5c)==0x43fa0000 and u.mem_read(d+0x68,1)[0]==0x69 and u.mem_read(d+0xa2,1)[0]==1
   w32(c,0xffffffff if fail_factory else 0x123);factory_count+=1;trace.append('factory');stub_return(c)
  elif rva==0x351C90:
   assert d==0x123;w32(c,0xabcdef);w32(c+4,d);trace.append('weak');stub_return(c)
  elif rva==0x584040:
   assert c==0x123 and r32(d)==0xFE68952B and e==0x100;trace.append('associate');stub_return(1)
  elif rva==0x5589F0:w32(d,0x123);stub_return(d)
  elif rva==0x1151750:
   assert r64(c)==1 and r32(r64(c+8))==0x123;trace.append('visibility');stub_return()
  elif rva==0x352310:
   w32(d,r32(c+4));stub_return(d)
  elif rva==0x584580:
   assert c==0x123 and r32(d)==0xFE68952B and e==0x100;trace.append('unassociate');stub_return()
  elif rva==0x56A8F0:
   assert c==0x123;trace.append('destroy');stub_return()
  elif rva==0x187C480:stub_return()
 u.hook_add(UC_HOOK_CODE,hook)
 def fault(uc,access,address,size,value,_):
  print('fault',hex(u.reg_read(UC_X86_REG_RIP)-IMAGE),hex(address),access,'trace',trace);return False
 u.hook_add(UC_HOOK_MEM_INVALID,fault)
 def execute(rva,rcx=CHILD,rdx=0,r8=0):
  sp=STACK+0x1fef8;w64(sp,STOP);u.reg_write(UC_X86_REG_RSP,sp);u.reg_write(UC_X86_REG_RCX,rcx);u.reg_write(UC_X86_REG_RDX,rdx);u.reg_write(UC_X86_REG_R8,r8)
  u.emu_start(IMAGE+rva,STOP,count=20000)
  assert u.reg_read(UC_X86_REG_RIP)==STOP
 if external:
  execute(0x58D430,rdx=0,r8=0x123)
  assert u.mem_read(CHILD+0x29c,2)==b'\0\1'
 else:
  execute(0x5902C0)
  if fallback and not fail_factory:
   assert r64(CHILD+0x290)==(0x123<<32)|0xabcdef # replaced below
   assert u.mem_read(CHILD+0x29c,2)==b'\1\1'
   execute(0x5902C0);assert factory_count==1
 execute(0x58E630)
 assert r64(CHILD+0x290)==0xffffffffffffffff and u.mem_read(CHILD+0x29c,2)==b'\0\0'
 assert ('destroy' in trace)==(fallback and not fail_factory and not external)
 results.append(dict(fallback=fallback,failedFactory=fail_factory,externalActor=external,trace=trace))
for args in [(False,False,False),(True,False,False),(True,True,False),(False,False,True)]:case(*args)
out=ROOT/'build/coo/beyond-infinity-future-cast-research/native-emulation-results.json'
out.parent.mkdir(parents=True,exist_ok=True)
inputs={'unpackedImageSha256':hashlib.sha256(image).hexdigest(),'nativeFunctions':{}}
for name,start,end in [('fallback',0x5902C0,0x5905A8),('binding',0x58D380,0x58D53A),('cleanup',0x58E630,0x58E782),('allocator',0x4AAF90,0x4AB020)]:
 inputs['nativeFunctions'][name]={'rva':hex(start),'end':hex(end),'sha256':hashlib.sha256(image[start:end]).hexdigest()}
for file,label in [('destiny2.exe','gameExeSha256'),('steam_api64.dll','installedDllAtProofSha256')]:
 p=ROOT/file
 if p.exists():inputs[label]=hashlib.sha256(p.read_bytes()).hexdigest()
out.write_text(json.dumps({'inputs':inputs,'cases':results},indent=2))
print('PASS original5902C0 and58E630: missing/no-fallback, owned fallback, failed factory, external actor preservation')
