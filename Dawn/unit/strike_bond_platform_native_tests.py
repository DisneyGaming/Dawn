"""Run pinned native type23 consumer106AD60 and position wrapperDF6BD0.
Resource refresh/weak resolution and final renderer setter are isolated stubs;
revision selection, authority gate, snap forwarding and revision writes are real
image instructions. This script never opens or changes a live process.
"""
from pathlib import Path
import hashlib,struct
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
base=0x7FF618070000
image=(Path(__file__).resolve().parents[2]/"destiny2_unpacked.bin").read_bytes()
assert hashlib.sha256(image).hexdigest()=="63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e","wrong client build"
u=Uc(UC_ARCH_X86,UC_MODE_64);u.mem_map(base,(len(image)+4095)&~4095);u.mem_write(base,image)
heap=0x10000000;u.mem_map(heap,0x10000)
device=heap+0x1000;world=heap+0x3000;directory=heap+0x5000;descriptor=heap+0x6000;stack=heap+0xE000;stop=heap+0xF000
calls=[]
def put(a,f,*v):u.mem_write(a,struct.pack(f,*v))
def get(a,f):return struct.unpack(f,u.mem_read(a,struct.calcsize(f)))
def callback(u,a,size,user):
    rva=a-base
    if rva==0xA78470:pass
    elif rva==0x352310:
        out=u.reg_read(UC_X86_REG_RDX);put(out,"<I",1);u.reg_write(UC_X86_REG_RAX,out)
    elif rva==0xDF6C70:
        value=struct.unpack("<f",struct.pack("<I",u.reg_read(UC_X86_REG_XMM1)&0xFFFFFFFF))[0]
        snap=u.reg_read(UC_X86_REG_R8)&255;calls.append((value,snap))
        put(device+0x37C,"<f",value)
        if snap:put(device+0x370,"<f",value)
        # The real snap setter does not directly clear+374; acceptance must
        # depend on native position/target/revision, not this stale derivative.
    elif rva==0x3F9030:pass
    else:return
    rsp=u.reg_read(UC_X86_REG_RSP);ret=get(rsp,"<Q")[0]
    u.reg_write(UC_X86_REG_RSP,rsp+8);u.reg_write(UC_X86_REG_RIP,ret)
u.hook_add(UC_HOOK_CODE,callback)
put(base+0x2439C70,"<Q",directory);put(directory,"<Q",descriptor)
put(descriptor+8,"<Q",device-0x1000);put(descriptor+0x30,"<ii",0x1000,0)
put(world+0x1F4,"<I",1);put(world+0x1CC,"<h",-1);put(world+0x1D4,"<h",-1)
put(device+0x2C,"<I",1);put(device+0x70,"<i",-1);put(device+0x960,"<ii",-1,-1)
put(base+0x26BE0E0,"<I",2)
def command(revision,value,snap):put(world+0x1C0,"<fhB",value,revision,snap)
def tick():
    u.reg_write(UC_X86_REG_RSP,stack-8);put(stack-8,"<Q",stop);u.reg_write(UC_X86_REG_RCX,world)
    u.emu_start(base+0x106AD60,stop,count=10000)
command(128,.5,1);tick()
assert calls==[(.5,1)] and get(device+0x960,"<ii")== (128,0)
for _ in range(1000):tick()
assert len(calls)==1,"same native revision cannot repeat snap"
command(129,.3758,0);tick()
assert len(calls)==2 and calls[-1][1]==0 and get(device+0x370,"<f")[0]==.5
assert get(device+0x960,"<ii")== (129,0),"smooth travel preserves snap counter"
command(128,0.,1);tick();assert len(calls)==2,"stale snap cannot overwrite travel"
command(130,.42,1);tick()
assert len(calls)==3 and calls[-1][1]==1 and get(device+0x960,"<ii")== (130,1)
assert get(device+0x370,"<f")==get(device+0x37C,"<f"),"death snap holds current position"
put(base+0x26BE0E0,"<I",0);command(131,.2,0);tick();assert len(calls)==3,"native entity authority gate preserved"
put(base+0x26BE0E0,"<I",2)
for revision in (0,-1,-32768):command(revision,.1,1);tick()
assert len(calls)==3,"native nonpositive signed16 revisions ignored"
print("Garden platform native106AD60+DF6BD0: snap,1000replays,smooth target,stale revision,death stop,authority gate,signed16 bounds PASS")
