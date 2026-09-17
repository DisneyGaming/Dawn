"""Original kind9 target-to-opcode45 proof; no game-process access."""
from pathlib import Path
import hashlib,json,struct as S,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/coo'))
import package_read
IMAGE=(ROOT/'destiny2_unpacked.bin').read_bytes()
SHA='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
assert hashlib.sha256(IMAGE).hexdigest()==SHA
BASE=0x7FF618070000;RAM=0x10000000
cls,CURVE=package_read.read(0x80F47562);assert cls==0x8080834C
assert S.unpack_from('<I',CURVE,0x40)[0]==5
cls,SELECTORS=package_read.read(0x80F4519A);assert cls==0x8080815F
EXPECTED=[(-1425.054931640625,169.7905731201172,-43.593299865722656),
(-1596.3707275390625,120.1434555053711,-62.594085693359375),
(-1486.6104736328125,11.791311264038086,-73.3829116821289),
(-1489.560791015625,-165.89990234375,-62.49308395385742),
(-1490.12109375,-491.90216064453125,-32.84910202026367)]

def producer(marker):
    u=Uc(UC_ARCH_X86,UC_MODE_64)
    u.mem_map(BASE,(len(IMAGE)+0xFFF)&~0xFFF);u.mem_write(BASE,IMAGE)
    u.mem_map(RAM,0x100000)
    def put(a,fmt,*v):u.mem_write(a,S.pack(fmt,*v))
    def get(a,fmt):return S.unpack(fmt,u.mem_read(a,S.calcsize(fmt)))
    # Captured actor table and native inline registry directory layouts.
    actorBase=RAM+0x20000;actor=actorBase+0xB000;arena=RAM+0x40000
    member=arena+0x2000;curve=arena+0x3000;directory=RAM+0x1000;row=RAM+0x2000
    put(BASE+0x1F9D7F8,'<Q',actorBase);put(BASE+0x1F9D800,'<I',0xB000)
    put(actor+0x2C,'<I',4);put(actor+0x48,'<I',1);put(actor+0x60,'<IIQ',2,0x8080834E,0)
    put(actor+0xA190,'<i',-1) # mode0 has no actor target in this case
    put(BASE+0x2439C70,'<Q',directory);put(directory,'<Q',row)
    put(row+8,'<Q',arena);put(row+0x30,'<Ii',0x1000,0)
    # Native action runner references the type2 member's first queue entry.
    put(member+0xA40,'<Q',BASE+0x1BFE3E8);put(member+0xA60,'<IIQ',2,0,0x238)
    put(member+0x228,'<II',0,0);put(member+0x230,'<I',1);put(member+0x238,'<B',9)
    put(member+0x248,'<4I',0x1F992208,0xCBFDCA32,0x811C9DC5,0x95FB2E01)
    put(member+0x258,'<BBhbb',48,0,55,0,marker)
    u.mem_write(curve,CURVE);u.mem_write(arena+0x4000,SELECTORS)
    context,command,stack,stop=RAM+0x100,RAM+0x200,RAM+0x1F008,RAM+0x1FFF0
    put(context,'<I',1);put(command+0xA,'<B',0x45);put(stack,'<Q',stop)
    # Standard TLS/static initialization services; execute the original spline matrix initialization.
    u.reg_write(UC_X86_REG_GS_BASE,RAM+0x80000);put(RAM+0x80058,'<Q',RAM+0x81000)
    put(BASE+0x330F0E0,'<I',0);put(RAM+0x81000,'<Q',RAM);put(RAM+0x9AA54,'<I',0)
    put(BASE+0x27C4150,'<i',1)
    observations=[]
    def ret(value=None):
        sp=u.reg_read(UC_X86_REG_RSP);dest=get(sp,'<Q')[0]
        u.reg_write(UC_X86_REG_RSP,sp+8);u.reg_write(UC_X86_REG_RIP,dest)
        if value is not None:u.reg_write(UC_X86_REG_RAX,value)
    def hook(uc,address,size,user):
        rva=address-BASE
        if address==stop:uc.emu_stop();return
        if rva==0x4FFEC0:
            out=uc.reg_read(UC_X86_REG_RCX);ref=uc.reg_read(UC_X86_REG_RDX)
            assert get(ref,'<IBBh')==(0x95FB2E01,48,0,55)
            put(out,'<IIQ',3,0x8080834D,0);ret(out)
        elif rva==0x187C480:ret()
        elif rva==0x187C934:put(uc.reg_read(UC_X86_REG_RCX),'<i',-1);ret()
        elif rva==0x187C8D4:put(uc.reg_read(UC_X86_REG_RCX),'<i',0);ret()
        elif rva==0xAB0C30:observations.append(('marker',uc.reg_read(UC_X86_REG_EDX)))
        elif rva==0xAB0750:observations.append(('curve',uc.reg_read(UC_X86_REG_RCX)==curve+0x10))
    u.hook_add(UC_HOOK_CODE,hook)
    for r,v in [(UC_X86_REG_RCX,context),(UC_X86_REG_RDX,command),(UC_X86_REG_R8,0),(UC_X86_REG_RSP,stack)]:u.reg_write(r,v)
    try:u.emu_start(BASE+0xA0F0E0,stop,count=20000)
    except Exception:
        print('failed native RVA',hex(u.reg_read(UC_X86_REG_RIP)-BASE),'rcx',hex(u.reg_read(UC_X86_REG_RCX)),'rdx',hex(u.reg_read(UC_X86_REG_RDX)));raise
    payload=bytes(u.mem_read(command+0x10,0x20))
    assert get(command+0x10,'<II')==(1,0)
    assert payload[8]==0 and S.unpack_from('<ii',payload,0xC)==(-1,-1)
    actual=S.unpack_from('<3f',payload,0x14)
    assert actual==EXPECTED[marker],(marker,actual,EXPECTED[marker])
    assert observations==[('marker',marker),('curve',True)]
    # The pinned teleport handler discards both incoming AI target arguments
    # before its first helper call. Compare its complete observable entry state
    # for unspecified versus populated target/mode values.
    selector=RAM+0x50000;put(selector,'<IIQ',4,0,0);put(selector+0x2C,'<I',1)
    put(BASE+0x26BE0E0,'<I',2)
    states=[]
    for mode,target in ((0,0xFFFFFFFF),(6,12345)):
        for r,v in [(UC_X86_REG_RCX,selector),(UC_X86_REG_RDX,mode),(UC_X86_REG_R8,target),
                    (UC_X86_REG_R9,command+0x24),(UC_X86_REG_RSP,stack)]:u.reg_write(r,v)
        u.emu_start(BASE+0x10C6AF0,BASE+0x10C6BC9,count=1000)
        states.append(tuple(u.reg_read(r) for r in (UC_X86_REG_RAX,UC_X86_REG_RBX,UC_X86_REG_RCX,
            UC_X86_REG_RDX,UC_X86_REG_RSI,UC_X86_REG_RDI,UC_X86_REG_R8,UC_X86_REG_R9,
            UC_X86_REG_R12,UC_X86_REG_R13,UC_X86_REG_R14,UC_X86_REG_R15)))
    assert states[0]==states[1], 'native mode/target arguments affect teleport handler entry'
    return {'marker':marker,'nativeDestination':actual,'requestGroup':1,'requestSequence':0,'mode':0,'targetReference':[-1,-1],'handlerDiscardsAiTargetArguments':True}

results=[producer(marker) for marker in range(5)]
result={'imageSha256':SHA,'curveTag':'80F47562','curveSha256':hashlib.sha256(CURVE).hexdigest(),'selectorSha256':hashlib.sha256(SELECTORS).hexdigest(),
'nativePath':'A0F0E0 -> AB4030 -> A97800 -> AB0C30 -> AAF990 -> AB0750 -> AAD5B0',
'cases':results,'gameProcessAccess':False,
'limits':'Registry target lookup and TLS services are fixture boundaries; original named selector lookup, curve evaluation and opcode45 payload construction are executed. No claim of network delivery or movement completion.'}
out=ROOT/'build/coo/validation-active-arm/native-movement-program-proof.json'
out.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
