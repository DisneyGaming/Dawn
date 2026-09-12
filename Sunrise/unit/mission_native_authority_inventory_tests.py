from pathlib import Path
import hashlib, struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

ROOT=Path(__file__).resolve().parents[2]
BASE=0x7FF618070000
IMAGE=(ROOT/'destiny2_unpacked.bin').read_bytes()
assert hashlib.sha256(IMAGE).hexdigest()=='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e', 'wrong client build'

# The health interface has50 native methods. Its resource only has2 more.
# This rejects the specific generic dynamic-record route; it is not proof that
# no separate simulation replication protocol or authored graph can own health.
assert struct.unpack_from('<I',IMAGE,0x3265980)[0]==0x80804BEE
assert struct.unpack_from('<QQ',IMAGE,0x3265988)==(BASE+0x1CE7650,50)
health=[struct.unpack_from('<QQ',IMAGE,0x1CE7650+i*16) for i in range(50)]
assert all(sig&0xFFFFFFFF!=0x5B937938 for _,sig in health)
assert (BASE+0xCDE390,0x116D45DE4) in health
assert (BASE+0xCDE1F0,0x116D45DE4) in health

# Enumerate every aligned direct generic-record registration in the pinned
# callback region. An extra receiver requires review instead of a silent skip.
receivers=[]
for p in range(0x1CB0000,0x1D30000,8):
    if struct.unpack_from('<Q',IMAGE,p)[0]!=0x25B937938:continue
    callback=struct.unpack_from('<Q',IMAGE,p-8)[0]-BASE
    if 0<callback<len(IMAGE):receivers.append(callback)
assert receivers==[0x4F20D0,0x4F1C70,0xED75A0,0xED6800,0xED7820,0xED6960,
    0x1003900,0xB94700,0xF33930,0xB94700,0xFD72F0,0xFD3770,
    0xF37800,0xF32220,0x10036E0,0xB94700,0xFD7460,0xFD38B0,
    0xF37920,0xB94700,0xE96DB0,0xE91A50,0xDF6510,0xDF4020]

u=Uc(UC_ARCH_X86,UC_MODE_64)
u.mem_map(BASE,(len(IMAGE)+4095)&~4095);u.mem_write(BASE,IMAGE)
HEAP=0x10000000;u.mem_map(HEAP,0x20000)
health_ptr=HEAP+0x1000;context=HEAP+0x2000;stack=HEAP+0x10000;stop=HEAP+0x1F000
changed=[]
def put(a,f,*v):u.mem_write(a,struct.pack(f,*v))
def get(a,f):return struct.unpack(f,u.mem_read(a,struct.calcsize(f)))
def callback(u,a,size,user):
    if a!=BASE+0x3F9030:return
    changed.append(u.reg_read(UC_X86_REG_RCX)&0xFFFFFFFF)
    rsp=u.reg_read(UC_X86_REG_RSP);ret=get(rsp,'<Q')[0]
    u.reg_write(UC_X86_REG_RSP,rsp+8);u.reg_write(UC_X86_REG_RIP,ret)
u.hook_add(UC_HOOK_CODE,callback)
def call(rva,rcx,rdx=0):
    u.reg_write(UC_X86_REG_RSP,stack-8);put(stack-8,'<Q',stop)
    u.reg_write(UC_X86_REG_RCX,rcx);u.reg_write(UC_X86_REG_RDX,rdx)
    u.emu_start(BASE+rva,stop,count=10000)
    return u.reg_read(UC_X86_REG_RAX)&255
put(context+8,'<Q',health_ptr)
put(health_ptr+0x24,'<I',0x102003)
put(health_ptr+0x2C,'<I',0x4001)
for original in range(256):
    for enabled in [False,True]:
        put(health_ptr+0x338,'<B',original)
        call(0xCDE390,health_ptr,int(enabled))
        expected=(original|2) if enabled else (original&~2)
        assert get(health_ptr+0x338,'<B')[0]==expected
        assert call(0xCDCB60,context)==(not enabled)
assert changed==[0x102003]*512
put(health_ptr+0x2C,'<I',0xFFFFFFFF)
call(0xCDE390,health_ptr,1)
assert len(changed)==512

# Execute the original source placement override branch. No scoped reference
# selects only XYZ from source+1A0; source scale is deliberately different.
# The branch preserves the authored class, quaternion, and scale byte-for-byte.
source=HEAP+0x3000;authority=source+0x180;placement=HEAP+0x5000
u.mem_write(placement,bytes(range(48)))
put(authority+9,'<B',1)
put(source+0x1A0,'<4f',1.25,2.5,3.75,99.)
old=bytes(u.mem_read(placement,48))
ref_calls=[]
def placement_callback(u,a,size,user):
    if a==BASE+0x4E2990:
        ref_calls.append(u.reg_read(UC_X86_REG_RCX))
        u.reg_write(UC_X86_REG_RAX,0)
    else:return
    rsp=u.reg_read(UC_X86_REG_RSP);ret=get(rsp,'<Q')[0]
    u.reg_write(UC_X86_REG_RSP,rsp+8);u.reg_write(UC_X86_REG_RIP,ret)
u.hook_add(UC_HOOK_CODE,placement_callback)
def apply():
    u.reg_write(UC_X86_REG_RSP,stack)
    u.reg_write(UC_X86_REG_R12,authority);u.reg_write(UC_X86_REG_R13,0)
    u.reg_write(UC_X86_REG_R14,source);u.reg_write(UC_X86_REG_RSI,placement)
    u.emu_start(BASE+0x9EFCD4,BASE+0x9EFD26,count=10000)
apply()
assert bytes(u.mem_read(placement,32))==old[:32]
assert bytes(u.mem_read(placement+32,12))==struct.pack('<3f',1.25,2.5,3.75)
assert bytes(u.mem_read(placement+44,4))==old[44:48]
assert ref_calls==[authority+0x10]
u.mem_write(placement,old);put(authority+9,'<B',0);apply()
assert bytes(u.mem_read(placement,48))==old and len(ref_calls)==1
print('PASS:24 generic callback registrations,50 health methods,512 original native lethal-bit updates, source XYZ-only placement override')
