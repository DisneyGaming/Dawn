"""Pinned native source apply and worker replay; no running game access or writes.
The reflected type37 wire decoder is modeled; 103E8E0 and 10059A0 instructions
are original. Only identity lookup, CRT calls and the solver body are stubbed.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[2]
PIN='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
IMAGE=0x140000000;DIRECTORY=0x400000000;ROWS=DIRECTORY+0x1000
DATA=0x410000000;SENSOR=DATA+0x2000;WORKER=0x430000000;AUTH=0x440000000
STACK=0x500000000;STOP=0x600000000
p32=lambda n:struct.pack('<I',n&0xffffffff)
p64=lambda n:struct.pack('<Q',n)
f32=lambda n:struct.pack('<f',n)
class Bits:
    def __init__(self,b):self.b=b;self.at=0
    def get(self,n):
        v=0
        for _ in range(n):v=(v<<1)|((self.b[self.at//8]>>(7-self.at%8))&1);self.at+=1
        return v

def decode(raw):
    r=Bits(raw);out=bytearray(0x5b4)
    def put(at,fmt,value):struct.pack_into('<'+fmt,out,at,value)
    for base in (0,0x2a8):
        put(base,'I',r.get(32));put(base+4,'b',r.get(8)-128)
        # Four reflected groups align each float independently; no common stride.
        for a,b,weight,active in ((8,9,12,16),(17,18,20,24),(25,26,28,32),(33,34,36,40)):
            put(base+a,'b',r.get(8)-128);put(base+b,'b',r.get(8)-128)
            put(base+weight,'I',r.get(32));put(base+active,'B',r.get(1))
        put(base+0x2c,'B',r.get(7));put(base+0x2d,'B',r.get(1))
        for at in (0x30,0x34):put(base+at,'I',r.get(32))
        for i in range(5):put(base+0x38+i*4,'I',(r.get(32)+0x80000000)&0xffffffff)
        count=r.get(7);put(base+0x4c,'I',count)
        for i in range(count):
            for j in range(3):put(base+0x50+i*6+j*2,'h',r.get(16)-32768)
    put(0x550,'I',r.get(32))
    for i in range(96):put(0x554+i,'B',r.get(8))
    assert r.at==1750 and len(raw)==219
    return bytes(out)

def replay(image,decoded,name):
    u=Uc(UC_ARCH_X86,UC_MODE_64)
    for address,size in ((IMAGE,(len(image)+4095)&~4095),(DIRECTORY,0x2000),(DATA,0x10000),(WORKER,0x10000),(AUTH,0x2000),(STACK,0x20000),(STOP,0x1000)):u.mem_map(address,size)
    u.mem_write(IMAGE,image);u.mem_write(AUTH,decoded)
    def w32(a,n):u.mem_write(a,p32(n))
    def w64(a,n):u.mem_write(a,p64(n))
    def r32(a):return struct.unpack('<I',u.mem_read(a,4))[0]
    def r64(a):return struct.unpack('<Q',u.mem_read(a,8))[0]
    def rf(a):return struct.unpack('<f',u.mem_read(a,4))[0]
    w64(IMAGE+0x2439c70,DIRECTORY);w64(DIRECTORY,ROWS);w64(ROWS+8,DATA);w32(ROWS+0x30,0x1000)
    w64(IMAGE+0x2076c88,DIRECTORY+0x200);w32(DIRECTORY+0x200,0x80805008)
    w32(WORKER,1);w64(WORKER+8,0x100);w32(WORKER+0x2c,0x1234)
    w32(IMAGE+0x26be0e0+((0x1234>>5)*4),0)
    w32(WORKER+0x948,0xffffffff);w32(WORKER+0x96c,0x80f4e01e)
    u.mem_write(WORKER+0x9bb,b'\x01');u.mem_write(WORKER+0x9b8,b'\x00')
    # Definition defaults retained when source density uses its -1 sentinel.
    u.mem_write(DATA+0x1000+0x100+0xe1,b'\x04\x05')
    solver=[];calls=[]
    def returned(value=0):
        sp=u.reg_read(UC_X86_REG_RSP);u.reg_write(UC_X86_REG_RAX,value);u.reg_write(UC_X86_REG_RIP,r64(sp));u.reg_write(UC_X86_REG_RSP,sp+8)
    def hook(uc,address,size,_):
        if address==STOP:u.emu_stop();return
        rva=address-IMAGE;c=u.reg_read(UC_X86_REG_RCX);d=u.reg_read(UC_X86_REG_RDX);e=u.reg_read(UC_X86_REG_R8)
        if rva==0x4a6340:returned(AUTH)
        elif rva==0x103e700:w32(c,2);w32(c+4,0x80804ef6);w64(c+8,0);returned(c)
        elif rva==0x187e85c:u.mem_write(c,bytes(u.mem_read(d,e)));returned(c)
        elif rva==0x187e83e:returned(0 if u.mem_read(c,e)==u.mem_read(d,e) else 1)
        elif rva==0xff2f80:
            a=struct.unpack('<f',p32(u.reg_read(UC_X86_REG_XMM1)))[0]
            b=struct.unpack('<f',p32(u.reg_read(UC_X86_REG_XMM2)))[0]
            solver.append([a,b]);returned(0)
        elif rva==0x3f9030:raise AssertionError('native owner bitmap unexpectedly granted')
        elif rva==0x187c480:returned() # security cookie helper
        elif rva==0x44e340:returned() # engine job submission is outside the isolated fixture
        calls.append(rva)
    def invalid(uc,kind,address,size,value,_):raise RuntimeError(f'{name}: memory {address:x} at {u.reg_read(UC_X86_REG_RIP)-IMAGE:x}')
    u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def invoke(rva,c,d=0):
        sp=STACK+0x18000-8;w64(sp,STOP);u.reg_write(UC_X86_REG_RSP,sp);u.reg_write(UC_X86_REG_RCX,c);u.reg_write(UC_X86_REG_RDX,d)
        try:u.emu_start(IMAGE+rva,STOP,count=100000)
        except Exception as error:raise RuntimeError((name,hex(u.reg_read(UC_X86_REG_RIP)-IMAGE),[hex(a) for a in calls[-12:]])) from error
        assert u.reg_read(UC_X86_REG_RIP)==STOP,(name,hex(u.reg_read(UC_X86_REG_RIP)-IMAGE),calls[-20:])
    before=bytes(u.mem_read(SENSOR,0x1000));invoke(0x103e8e0,SENSOR,AUTH+0x1000)
    assert bytes(u.mem_read(SENSOR+0x180,0x5b4))==decoded
    assert bytes(u.mem_read(SENSOR,0x180))==before[:0x180] and bytes(u.mem_read(SENSOR+0x734,0x8cc))==before[0x734:]
    invoke(0x10059a0,WORKER)
    # Model completion of the external44E340 generation job. State1 is the
    # original jump-table entry that performs the enabled check and calls solver.
    assert u.mem_read(WORKER+0x9bc,1)[0]==6
    u.mem_write(WORKER+0x9bc,b'\x01')
    invoke(0x103e8e0,SENSOR,AUTH+0x1000) # retransmitted identical authority
    invoke(0x10059a0,WORKER)
    return dict(name=name,seed=r32(WORKER+0x940),anchors=[[struct.unpack('<b',u.mem_read(WORKER+offset,1))[0],struct.unpack('<b',u.mem_read(WORKER+offset+1,1))[0],rf(WORKER+weight),u.mem_read(WORKER+active,1)[0]] for offset,weight,active in ((0x994,0x998,0x99c),(0x99d,0x9a0,0x9a4),(0x9a5,0x9a8,0x9ac),(0x9ad,0x9b0,0x9b4))],solver=solver,stored=[rf(WORKER+0x950),rf(WORKER+0x954)],ownerBitmap=r32(IMAGE+0x26be0e0+((0x1234>>5)*4)),state=u.mem_read(WORKER+0x9bc,1)[0])

def main():
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);args=parser.parse_args()
    # Verify Garden publication independently of worker behavior.
    garden_source=decode((args.directory/'garden.bin').read_bytes())
    assert [[struct.unpack_from('<b',garden_source,offset)[0],struct.unpack_from('<b',garden_source,height)[0],struct.unpack_from('<f',garden_source,weight)[0],struct.unpack_from('<B',garden_source,active)[0]] for offset,height,weight,active in ((8,9,12,16),(17,18,20,24),(25,26,28,32),(33,34,36,40))]==[[-1,1,0.,0],[-1,0,0.,0],[1,2,1.,1],[1,0,0.,1]]
    image=(ROOT/'destiny2_unpacked.bin').read_bytes();assert hashlib.sha256(image).hexdigest()==PIN
    results=[]
    for name in ('garden','disabled','authored','tree','omega','beyond1','beyond2'):
        decoded=decode((args.directory/(name+'.bin')).read_bytes());result=replay(image,decoded,name);results.append(result)
    print(json.dumps(results,indent=2))
    assert results[0]['solver']==results[2]['solver']==results[3]['solver'] and results[0]['solver']!=[[0.0,0.0]]
    assert results[0]['stored']==results[2]['stored']==results[3]['stored']
    assert not results[1]['solver']
    assert results[2]['solver']==results[3]['solver'] and results[2]['solver']!=[[0.0,0.0]]
    assert all(x['ownerBitmap']==0 for x in results)
    assert results[4]['anchors']==[[-1,2,0.,0],[-1,0,0.,0],[2,0,0.,1],[1,2,1.,0]]
    assert results[5]['anchors']==[[-1,0,0.,0],[0,0,1.,0],[1,1,0.,1],[-1,0,0.,0]]
    assert results[6]['anchors']==[[0,0,1.,0],[0,0,0.,1],[-1,1,0.,0],[-1,0,0.,0]]
    assert [x['seed'] for x in results[4:]]==[12345,12345,67890]
    assert all(x['solver']==[[0.,0.]] for x in results[4:])
    (args.directory/'native-proof.json').write_text(json.dumps(dict(imageSha256=PIN,results=results,modeled='reflection decoder; source identity lookup; CRT; external44E340 job completion to state1; solver body',original='103E8E0 source apply; complete10059A0 worker; F9CA accessors',checks='Garden desired inputs and native disable; authored defaults unchanged; native owner gates unchanged'),indent=2))
if __name__=='__main__':main()
