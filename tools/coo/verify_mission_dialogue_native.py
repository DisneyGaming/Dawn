"""Original dialogue apply/scan; modeled wire decoder, identity lookup and audio callback.
No live process access. Execute only the pinned build in isolated Unicorn memory.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RAX,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_RSP,UC_X86_REG_RIP
ROOT=Path(__file__).resolve().parents[2]
PIN='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
BASE=0x7FF618070000;HEAP=0x10000000
COMPONENT,DECODED,TAG=HEAP+0x1000,HEAP+0x5000,HEAP+0x7000
ROOTPTR,POOLS,DATA=HEAP+0x7800,HEAP+0x8000,HEAP+0x10000
STACK,STOP=HEAP+0x70000,HEAP+0x7F000
class Bits:
    def __init__(self,data):self.data,self.at=data,0
    def get(self,size):
        result=0
        for _ in range(size):
            result=result*2+((self.data[self.at//8]>>(7-self.at%8))&1);self.at+=1
        return result

def decode(raw,prior,bit_count):
    reader=Bits(raw);result=bytearray(prior)
    def reference(at):
        key,kind,index=reader.get(32),reader.get(7)-1,reader.get(16)-32768
        struct.pack_into('<Ib',result,at,key,kind);struct.pack_into('<h',result,at+6,index)
        assert (key,kind,index)==(0x811C9DC5,-1,-1)
    reference(0)
    for i in range(128):
        at=8+i*32;struct.pack_into('<Q',result,at,reader.get(64))
        if reader.get(1):struct.pack_into('<Q',result,at+8,reader.get(64))
        reference(at+16)
        struct.pack_into('<I',result,at+24,(reader.get(32)-0x80000000)&0xFFFFFFFF)
        result[at+28]=reader.get(2)-1
    assert reader.at==bit_count and len(raw)==(bit_count+7)//8
    return bytes(result)

def replay(image,directory,mission):
    uc=Uc(UC_ARCH_X86,UC_MODE_64)
    uc.mem_map(BASE,(len(image)+4095)&~4095);uc.mem_write(BASE,image);uc.mem_map(HEAP,0x80000)
    def put(address,fmt,*values):uc.mem_write(address,struct.pack('<'+fmt,*values))
    def get(address,fmt):return struct.unpack('<'+fmt,uc.mem_read(address,struct.calcsize('<'+fmt)))
    put(BASE+0x2077180,'Q',TAG);put(TAG,'I',0x80804F77)
    put(BASE+0x2439C70,'Q',ROOTPTR);put(ROOTPTR,'Q',POOLS);put(POOLS+8,'Q',DATA)
    put(POOLS+0x30,'II',0x1000,0);put(COMPONENT,'I',1);put(DATA+0x1000+0x58,'I',2)
    put(DATA+0x2000+0x10,'Q',0x20)
    dispatches=[];lookups=[]
    def returned(value=0):
        sp=uc.reg_read(UC_X86_REG_RSP);uc.reg_write(UC_X86_REG_RAX,value)
        uc.reg_write(UC_X86_REG_RIP,get(sp,'Q')[0]);uc.reg_write(UC_X86_REG_RSP,sp+8)
    def hook(_uc,address,_size,_user):
        if address==BASE+0x4A6340:
            assert uc.reg_read(UC_X86_REG_RDX)==0x80804F77
            lookups.append(0x80804F77);returned(DECODED)
        elif address==BASE+0x10097D0:
            assert uc.reg_read(UC_X86_REG_RCX)==COMPONENT
            dispatches.append(uc.reg_read(UC_X86_REG_RDX));returned()
    def invalid(_uc,_kind,address,_size,_value,_user):
        raise RuntimeError(f'native memory {address:x} at {uc.reg_read(UC_X86_REG_RIP)-BASE:x}')
    uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def invoke(rva):
        put(STACK-8,'Q',STOP);uc.reg_write(UC_X86_REG_RSP,STACK-8)
        uc.reg_write(UC_X86_REG_RCX,COMPONENT);uc.reg_write(UC_X86_REG_RDX,TAG+0x10)
        uc.emu_start(BASE+rva,STOP,count=100000)
        assert uc.reg_read(UC_X86_REG_RIP)==STOP,hex(uc.reg_read(UC_X86_REG_RIP)-BASE)
    def apply(decoded):
        prior=bytes(uc.mem_read(COMPONENT,0x1500));uc.mem_write(DECODED,decoded);invoke(0x1009B60)
        applied=bytes(uc.mem_read(COMPONENT,0x1500))
        assert applied[0x180:0x1188]==decoded
        assert applied[:0x180]==prior[:0x180] and applied[0x1188:]==prior[0x1188:],'apply modified processed generations'
    previous=bytes(0x1008);replays=0
    fixtures=sorted(directory.glob(mission+'-*-active.txt'),key=lambda p:int(p.stem.split('-')[1]));assert fixtures
    for active in fixtures:
        for retired in (False,True):
            meta=active if not retired else active.with_name(active.name.replace('-active','-retired'))
            rows,row,is_retired,bits=map(int,meta.read_text().split());assert bool(is_retired)==retired
            put(DATA+0x2000+8,'Q',rows);decoded=decode(meta.with_suffix('.bin').read_bytes(),previous,bits);previous=decoded
            for i in range(128):
                time,optional,reference,generation,mode=struct.unpack_from('<QQQIB',decoded,8+i*32)
                assert time==0xFFFFFFFFFFFFFFFF and reference==0xFFFF00FF811C9DC5
                assert generation==(1 if i<=row else 0)
                assert (optional,mode)==((1,2) if i==row and not retired else (0,0))
            before=len(dispatches);apply(decoded);invoke(0x100A180)
            assert dispatches[before:]==([] if retired else [row])
            assert get(COMPONENT+0x1188+row*4,'I')[0]==1
            for _ in range(20):apply(decoded);invoke(0x100A180);replays+=1
            assert len(dispatches)==before+(0 if retired else 1),'replayed dispatch'
    return {'mission':mission,'rows_dispatched_once':len(dispatches),'reapplied_without_duplicate':replays,
            'native_typed_apply_calls':len(lookups),'apply_preserved_processed_generations':True,'retired_optional_times_cleared':True}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('fixtures',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
    image=(ROOT/'destiny2_unpacked.bin').read_bytes();assert hashlib.sha256(image).hexdigest()==PIN,'wrong native image'
    result={'image_sha256':PIN,'native_functions':['1009B60','100A180','A70F50','4E2990'],
            'modeled':['reflected wire decoder','typed identity lookup','final audio dispatch'],
            'missions':[replay(image,args.fixtures,mission) for mission in ('trial','hijacked')]}
    if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
