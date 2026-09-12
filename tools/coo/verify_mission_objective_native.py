"""Replay actual mission objective bodies through original native ring/apply/installer.
The reflection decoder, content provider, UI record builder/manager and HUD builder
are modeled boundaries. This proves dispatch/retirement, not live rendered HUD.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import *
from verify_mission_dialogue_native import Bits,PIN,BASE,HEAP,ROOT
COMPONENT,DECODED,TAG=HEAP+0x1000,HEAP+0x5000,HEAP+0x7000
ROOTPTR,POOLS,DATA=HEAP+0x7800,HEAP+0x8000,HEAP+0x10000
CONTENT,MANAGER,VIEWER=HEAP+0x30000,HEAP+0x40000,HEAP+0x50000
STACK,STOP=HEAP+0x70000,HEAP+0x7F000

def decode(raw,bits):
    r=Bits(raw);out=bytearray(0x300)
    def put(at,fmt,*args):struct.pack_into('<'+fmt,out,at,*args)
    def ref(at):
        put(at,'I',r.get(32));put(at+4,'b',r.get(7)-1);put(at+6,'h',r.get(16)-32768)
    ref(0);ref(8)
    for i in range(3):
        at=0x10+i*0xF8
        put(at,'I',r.get(32));put(at+4,'i',r.get(32)-0x80000000);put(at+8,'b',r.get(2)-1)
        put(at+0x10,'B',r.get(1))
        for j in range(5):put(at+0x18+j*8,'Q',r.get(64))
        put(at+0x40,'I',r.get(32))
        for j in range(4):put(at+0x48+j*4,'i',r.get(32)-0x80000000)
        put(at+0x58,'b',r.get(2)-1);ref(at+0x5C);put(at+0x64,'b',r.get(3)-1)
        for j in range(4):
            target=at+0x68+j*0x24;ref(target);ref(target+8)
            for k in range(4):put(target+0x10+k*4,'I',r.get(32))
            put(target+0x20,'B',r.get(1))
    put(0x2F8,'i',r.get(3)-1)
    assert r.at==bits and len(raw)==(bits+7)//8
    return bytes(out)

def replay(image,directory,name,cold=False,manager_mode=0):
    u=Uc(UC_ARCH_X86,UC_MODE_64);u.mem_map(BASE,(len(image)+4095)&~4095);u.mem_write(BASE,image);u.mem_map(HEAP,0x80000)
    def put(at,fmt,*args):u.mem_write(at,struct.pack('<'+fmt,*args))
    def get(at,fmt):return struct.unpack('<'+fmt,u.mem_read(at,struct.calcsize('<'+fmt)))
    put(BASE+0x2077200,'Q',TAG);put(TAG,'I',0x80804F67)
    put(BASE+0x2439C70,'Q',ROOTPTR);put(ROOTPTR,'Q',POOLS);put(POOLS+8,'Q',DATA)
    put(POOLS+0x30,'II',0x1000,0);put(COMPONENT,'I',1);put(DATA+0x1000+0x58,'B',manager_mode)
    put(CONTENT+8,'QQ',1,0x100);put(COMPONENT+0x478,'i',-1)
    for i in range(3):put(COMPONENT+0x190+i*0xF8,'IIb',0x811C9DC5,0,-1)
    state={'ready':not cold,'event':0};installs=[];retires=[];builds=[]
    def ret(value=0):
        sp=u.reg_read(UC_X86_REG_RSP);u.reg_write(UC_X86_REG_RAX,value);u.reg_write(UC_X86_REG_RIP,get(sp,'Q')[0]);u.reg_write(UC_X86_REG_RSP,sp+8)
    def hook(_u,address,_size,_user):
        at=address-BASE
        if at==0x4A6340:
            assert u.reg_read(UC_X86_REG_RDX)==0x80804F67;ret(DECODED)
        elif at==0x1009430:ret(CONTENT if state['ready'] else 0)
        elif at==0x1008B40:
            record=u.reg_read(UC_X86_REG_RDX);event,variant,lifecycle=get(record,'Iib')
            assert variant==0 and lifecycle==0,'authored variant must never be a transport counter'
            state['event']=event;ret()
        elif at==0x137E1D0:ret(int(state['ready']))
        elif at==0x137D6F0:ret(MANAGER)
        elif at in (0x137BD50,0x137BF40):
            assert u.reg_read(UC_X86_REG_RCX)==MANAGER
            assert at==(0x137BD50 if manager_mode==0 else 0x137BF40)
            installs.append(state['event']);ret()
        elif at==0x137E2C0:
            retires.append((u.reg_read(UC_X86_REG_RDX),u.reg_read(UC_X86_REG_R8),u.reg_read(UC_X86_REG_R9)&0xFFFFFFFF));ret()
        elif at==0x4294C0:ret(VIEWER)
        elif at==0x429BA0:put(u.reg_read(UC_X86_REG_RDX),'I',7);ret(u.reg_read(UC_X86_REG_RDX))
        elif at==0x100A6E0:builds.append(get(COMPONENT+0x478,'i')[0]);ret()
        elif at==0x187C480:ret()
    def invalid(_u,_kind,address,_size,_value,_user):raise RuntimeError(f'native memory {address:x} at {u.reg_read(UC_X86_REG_RIP)-BASE:x}')
    u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def apply(body):
        u.mem_write(DECODED,body);put(STACK-8,'Q',STOP);u.reg_write(UC_X86_REG_RSP,STACK-8)
        u.reg_write(UC_X86_REG_RCX,COMPONENT);u.reg_write(UC_X86_REG_RDX,TAG+0x10)
        u.emu_start(BASE+0x1009C00,STOP,count=100000)
        assert u.reg_read(UC_X86_REG_RIP)==STOP
        assert bytes(u.mem_read(COMPONENT+0x180,0x300))==body
    replay_count=0
    for stage in range(6):
        path=directory/f'{name}-objective-{stage}'
        event,active,selector,bits,key,kind,slot,*locator=map(int,path.with_suffix('.txt').read_text().split())
        body=decode(path.with_suffix('.bin').read_bytes(),bits)
        assert struct.unpack_from('<i',body,0x2F8)[0]==selector
        for i in range(3):
            at=0x10+i*0xF8;selected=bool(active and i==selector)
            assert struct.unpack_from('<Iib',body,at)==(event if selected else 0x811C9DC5,0,0 if selected else -1)
            assert body[at+0x58]==0 and body[at+0x64]==(2 if selected else 0)
            if selected:
                assert struct.unpack_from('<I',body,at+0x68)[0]==key and body[at+0x6C]==kind
                assert struct.unpack_from('<h',body,at+0x6E)[0]==slot
                assert list(struct.unpack_from('<IIII',body,at+0x78))==locator
        put(CONTENT+0x120,'I',event);before=len(installs);apply(body)
        if cold and stage==0:
            assert len(installs)==0;state['ready']=True;apply(body)
            assert len(installs)==0,'fixture must demonstrate missed first install without ring rotation'
        else:
            expected=0 if stage in (1,5) else 1
            assert len(installs)==before+expected,(name,cold,stage,installs)
        for _ in range(20):apply(body);replay_count+=1
        assert len(installs)==before+(0 if (cold and stage==0) or stage in (1,5) else 1)
    assert all(kind==4 and state==0xFFFFFFFF for _,kind,state in retires)
    return {'mission':name,'cold_content_recovery':cold,'manager_mode':manager_mode,'installs':len(installs),
        'retirements':len(retires),'reapplies_without_duplicate':replay_count,'authored_variant_always_zero':True,
        'all_three_ring_slots_delivered':True,'marker_reference_locator_display_mode_delivered':True}

def main():
    p=argparse.ArgumentParser();p.add_argument('fixtures',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    image=(ROOT/'destiny2_unpacked.bin').read_bytes();assert hashlib.sha256(image).hexdigest()==PIN
    result={'image_sha256':PIN,'original':['1009C00','10098C0','1009ED0','100A110'],
        'modeled':['reflection decoder using captured 80804F67/68/6B layouts','content lookup','UI record builder','UI manager','HUD builder','viewer','security cookie'],
        'cases':[replay(image,a.fixtures,name,cold,mode) for name in ('trial','hijacked') for cold in (False,True) for mode in (0,1)]}
    if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
