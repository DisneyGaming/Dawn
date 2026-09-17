"""Pinned exact-build producer/consumer checks in isolated Unicorn memory.
Original DF4020 runs through payload production. Original DF6510 runs fully;
only engine setter/dirty callbacks are replaced with observable call stubs.
No live process, DLL, or installation writes.
"""
from pathlib import Path
import hashlib, struct, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_XMM1
BASE=0x7FF618070000
EXPECTED="63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e"
root=Path(__file__).resolve().parents[2]
data=(root/"destiny2_unpacked.bin").read_bytes()
assert hashlib.sha256(data).hexdigest()==EXPECTED,"wrong build"
uc=Uc(UC_ARCH_X86,UC_MODE_64)
uc.mem_map(BASE,(len(data)+4095)&~4095);uc.mem_write(BASE,data)
HEAP=0x10000000;uc.mem_map(HEAP,0x10000)
DEVICE=HEAP+0x1000;LIST=HEAP+0x3000;STACK=HEAP+0x9000;STOP=HEAP+0xF000
calls=[]
def put(address,fmt,*values):uc.mem_write(address,struct.pack(fmt,*values))
def get(address,fmt):return struct.unpack(fmt,uc.mem_read(address,struct.calcsize(fmt)))
def run(rva,until=STOP):
    uc.reg_write(UC_X86_REG_RSP,STACK-8);put(STACK-8,"<Q",STOP)
    uc.reg_write(UC_X86_REG_RCX,DEVICE);uc.reg_write(UC_X86_REG_RDX,LIST);uc.reg_write(UC_X86_REG_R8,LIST+0x400)
    uc.emu_start(BASE+rva,until,count=10000)
def stub(uc,address,size,user):
    rva=address-BASE
    if rva in (0xDF6C70,0xDF6900,0xDF71C0):
        value=struct.unpack("<f",struct.pack("<I",uc.reg_read(UC_X86_REG_XMM1)&0xFFFFFFFF))[0]
        snap=uc.reg_read(UC_X86_REG_R8)&255
        calls.append((rva,value,snap))
        # Isolated setter stand-in, not a test of renderer/effect side effects.
        target,current={0xDF6C70:(0x37C,0x370),0xDF6900:(0x6AC,0x6A0),0xDF71C0:(0xAC,0xA0)}[rva]
        put(DEVICE+target,"<f",value)
        if snap:put(DEVICE+current,"<f",value)
    elif rva==0x3F9030:pass
    else:return
    rsp=uc.reg_read(UC_X86_REG_RSP)
    ret=get(rsp,"<Q")[0];uc.reg_write(UC_X86_REG_RSP,rsp+8);uc.reg_write(UC_X86_REG_RIP,ret)
uc.hook_add(UC_HOOK_CODE,stub)
put(DEVICE+0x24,"<I",0xABCD);put(DEVICE+0x2C,"<I",1);put(DEVICE+0x70,"<i",-1)
put(BASE+0x26BE0E0,"<I",2)
put(LIST,"<i",2);put(LIST+0x10,"<I",0x80804FCA);put(LIST+0x70,"<I",0x80805063)
payload=LIST+0x80
# Exact native producer fields and order. Deliberately distinct each channel.
for offset,fmt,value in [(0x950,"<i",101),(0x954,"<i",102),(0xAC,"<f",.75),(0x958,"<i",201),(0x95C,"<i",202),(0x6AC,"<f",.5),(0x960,"<i",301),(0x964,"<i",302),(0x37C,"<f",.125)]:put(DEVICE+offset,fmt,value)
run(0xDF4020,BASE+0xDF40E6)
expected=struct.pack("<iifiifiif",101,102,.75,201,202,.5,301,302,.125)
assert bytes(uc.mem_read(uc.reg_read(UC_X86_REG_RSP)+0x20,36))==expected,"original producer payload"
# Position-only dynamic authority preserves power/lock and their counters.
for offset in (0x950,0x954,0x958,0x95C,0x960,0x964):put(DEVICE+offset,"<i",-1)
def command(revision,snap,value):put(payload,"<iifiifiif",-1,-1,1.,-1,-1,0.,revision,snap,value)
command(0,0,.1);run(0xDF6510)
assert len(calls)==1 and calls[0][0]==0xDF6C70 and calls[0][2]==1
assert get(DEVICE+0x960,"<ii")== (0,0)
for offset in (0x950,0x954,0x958,0x95C):assert get(DEVICE+offset,"<i")[0]==-1
for _ in range(1000):run(0xDF6510)
assert len(calls)==1,"repeated authority cannot restart pose"
command(-1,-1,0.);run(0xDF6510);assert len(calls)==1,"stale authority ignored"
# High revision/snap acceptance and native gate remain unchanged.
command(701,901,.2);run(0xDF6510)
assert len(calls)==2 and get(DEVICE+0x960,"<ii")== (701,901) and calls[-1][2]==1
put(BASE+0x26BE0E0,"<I",0);command(702,902,.1);run(0xDF6510)
assert len(calls)==2 and get(DEVICE+0x960,"<ii")== (701,901),"authority gate preserved"
put(BASE+0x26BE0E0,"<I",2);run(0xDF6510)
assert len(calls)==3 and get(DEVICE+0x960,"<ii")== (702,902)
command(702,903,0.);run(0xDF6510)
assert len(calls)==3 and get(DEVICE+0x960,"<ii")== (702,903),"snap revision alone does not invoke position setter"
print("native pose: pinned SHA256; original producer field parity; original consumer first apply, 1000 replays, stale revision, high revision, authority gate, independent snap: PASS")
