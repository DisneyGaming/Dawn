"""Offline pinned-image proof of .6 delivery; does not simulate animation playback."""
from pathlib import Path
import hashlib,json,struct as S
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[3]
IMAGE=(ROOT/'destiny2_unpacked.bin').read_bytes()
SHA='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
assert hashlib.sha256(IMAGE).hexdigest()==SHA

def fields(address,tag,count):
    assert S.unpack_from('<I',IMAGE,address)[0]==tag
    assert S.unpack_from('<I',IMAGE,address-8)[0]==count
    return [S.unpack_from('<10I',IMAGE,address+0x18+0x28*i) for i in range(count)]
program=fields(0x38E6EB0,0x80807F6F,3)
assert [x[0] for x in program]==[0,4,8] and [program[0][7],program[1][7]]==[31,6]
queue=fields(0x3903638,0x80807F72,2)
assert queue[0][7]==6 and queue[1][0]==8 and queue[1][5]==0x80807F71
payload=fields(0x3850C78,0x80807F76,6)
assert [x[0] for x in payload]==[0,4,8,12,20,21]
assert [x[7] for x in payload[:3]]==[32,32,32]
assert payload[3][5]==0x80809C42 and payload[4][6:8]==(1,3) and payload[5][6:8]==(128,8)

def native_queue(sequence,marker=-1):
    q=bytearray(0x808);S.pack_into('<I',q,0,1);q[8]=9
    S.pack_into('<4I',q,0x18,0xAFB11A12,sequence,0x811C9DC5,0x811C9DC5)
    q[0x28]=255;S.pack_into('<h',q,0x2A,-1)
    if marker>=0:
        S.pack_into('<I',q,0x18,0x1F992208);S.pack_into('<I',q,0x24,0x95FB2E01)
        q[0x28]=48;S.pack_into('<h',q,0x2A,55);q[0x2D]=marker
    return q

def delivery(sequence,revision,previous,marker=-1):
    u=Uc(UC_ARCH_X86,UC_MODE_64)
    for address,length in [(0xAB6000,0x1000),(0xA98000,0x6000),(0xA92000,0x1000)]:
        u.mem_map(address,length);u.mem_write(address,IMAGE[address:address+length])
    u.mem_map(0x10000000,0x20000)
    member,authority,stack=0x10000000,0x10002000,0x10018008
    q=native_queue(sequence,marker) if sequence else bytearray(0x808)
    baseline=bytearray(0x1000);S.pack_into('<I',baseline,0x190,previous);S.pack_into('<I',baseline,0x21C,123)
    auth=bytearray(0x1000);auth[6]=1;S.pack_into('<I',auth,0x100,revision);auth[0x108:0x910]=q
    u.mem_write(member,bytes(baseline));u.mem_write(authority,bytes(auth))
    for r,v in [(UC_X86_REG_RBX,member),(UC_X86_REG_RDI,authority),(UC_X86_REG_RSP,stack)]:u.reg_write(r,v)
    dispatches=[]
    def ret(uc,value=0):
        sp=uc.reg_read(UC_X86_REG_RSP);address=S.unpack('<Q',uc.mem_read(sp,8))[0]
        uc.reg_write(UC_X86_REG_RSP,sp+8);uc.reg_write(UC_X86_REG_RAX,value);uc.reg_write(UC_X86_REG_RIP,address)
    def hook(uc,address,size,user):
        if address==0xAB6C60:dispatches.append(uc.reg_read(UC_X86_REG_R8))
        # Stop at native action-runner construction/continuation. Original
        # AB6C60/A9C960/A9C4C0 still perform their real control and queue copy.
        if address==0xA98E40:ret(uc)
        elif address==0xA92E90:ret(uc,0)
    u.hook_add(UC_HOOK_CODE,hook)
    try:u.emu_start(0xAB6669,0xAB66BD,count=10000)
    except Exception:
        print("failure RIP",hex(u.reg_read(UC_X86_REG_RIP)));raise
    actual=bytes(u.mem_read(member,0x1000))
    if previous==revision:
        assert not dispatches and actual==baseline
    else:
        assert dispatches==[0]
        assert S.unpack_from('<I',actual,0x190)[0]==revision
        assert actual[0x230:0xA38]==q
        assert S.unpack_from('<I',actual,0x228)[0]==0
        allowed=set(range(0x18C,0x194))|set(range(0x228,0x22C))|set(range(0x230,0xA38))|{0xA80}
        assert all(i in allowed or x==baseline[i] for i,x in enumerate(actual)), 'unrelated member state changed'
    return {'sequence':f'{sequence:08X}','revision':revision,'previous':previous,'dispatches':len(dispatches),'marker':marker}
results=[]
for sequence in (0x65D2379F,0x65D2379C,0x65D2379E,0x65D2379D,0x65D2379B):
    results += [delivery(sequence,4,3),delivery(sequence,4,4)]
for marker in range(5):
    results += [delivery(0xCBFDCA32,10,9,marker),delivery(0xCBFDCA32,10,10,marker)]
results += [delivery(0,0,4),delivery(0,5,4)]
out=ROOT/'build/coo/validation-active-arm';out.mkdir(parents=True,exist_ok=True)
result={'imageSha256':SHA,'programSchema':'80807F6F/80807F72/80807F76','nativePath':'AB6669 -> AB6C60 -> A9C960 -> A9C4C0','cases':results,'gameProcessAccess':False,'limits':'Action-runner construction and completion are intercepted; no claim of live animation or network acceptance.'}
(out/'native-program-proof.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
