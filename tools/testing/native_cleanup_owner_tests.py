"""Execute the production MASM guard and original native cleanup in Unicorn.

Only surrounding resource/lookup services are fixtures. The failed native owner
resolver, sibling walk, cleanup decisions, zeroing calls and epilogue execute.
No live process access. A passing test is not a live fast-travel certification.
"""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import unittest
from unicorn import UcError
from unicorn.x86_const import *
sys.path.insert(0,str(Path(__file__).parent))
from mercury_streaming_native import Native, BASE, DATA, STACK, END, IMAGE_SHA256

ROOT=Path(__file__).resolve().parents[2]
ASM=ROOT/'Sunrise/src/client/hooks/bootflow/native_cleanup_owner_guard.asm'
IMAGE=ROOT/'build/baseline-02fc2c30/destiny2_unpacked.bin'
ML=Path(r'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/ml64.exe')
GUARD=BASE+0x10000000
GLOBALS=GUARD+0x800

def assemble():
    out=ROOT/'build/coo/cleanup-owner-native-20260915'
    out.mkdir(parents=True,exist_ok=True)
    obj=out/'native_cleanup_owner_guard.obj'
    subprocess.run([str(ML),'/nologo','/c',f'/Fo{obj}',str(ASM)],check=True,capture_output=True)
    blob=obj.read_bytes()
    _,count,_,symoff,symcount,opt,_=struct.unpack_from('<HHIIIHH',blob)
    strings=symoff+symcount*18
    def name(raw):
        if raw[:4]==bytes(4):
            start=strings+struct.unpack_from('<I',raw,4)[0]
            return blob[start:blob.index(0,start)].decode()
        return raw.rstrip(b'\0').decode()
    symbols={}
    index=0
    while index<symcount:
        raw,value,section,kind,storage,aux=struct.unpack_from('<8sIhHBB',blob,symoff+index*18)
        symbols[index]=(name(raw),value,section)
        index+=aux+1
    sections=[]
    for index in range(count):
        fields=struct.unpack_from('<8sIIIIIIHHI',blob,20+opt+index*40)
        sections.append(fields)
    text_index=next(i for i,s in enumerate(sections,1) if s[0].startswith(b'.text'))
    sec=sections[text_index-1]
    code=bytearray(blob[sec[4]:sec[4]+sec[3]])
    externs={n:GLOBALS+i*8 for i,n in enumerate(('cleanup_owner_cookie','cleanup_owner_resolver',
        'cleanup_owner_resume','cleanup_owner_tail','cleanup_owner_misses'))}
    for index in range(sec[7]):
        offset,symbol,kind=struct.unpack_from('<IIH',blob,sec[5]+index*10)
        n,value,section=symbols[symbol]
        address=externs[n] if section==0 else GUARD+value
        assert section in (0,text_index) and kind==4,(n,section,kind)
        addend=struct.unpack_from('<i',code,offset)[0]
        struct.pack_into('<i',code,offset,address+addend-(GUARD+offset+4))
    return bytes(code),externs

class CleanupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image=IMAGE.read_bytes()
        assert hashlib.sha256(cls.image).hexdigest()==IMAGE_SHA256
        cls.code,cls.externs=assemble()

    def fixture(self,missing,guarded,siblings=0,resource=True):
        n=Native(self.image)
        n.uc.mem_map(GUARD,0x1000)
        n.uc.mem_write(GUARD,self.code)
        for name,value in zip(self.externs,(BASE+0x20A9A88,BASE+0x4ECBF0,BASE+0xF9C190,BASE+0xF9C27F,0)):
            n.put(self.externs[name],value,'Q')
        cookie=0x127934ADB6
        n.put(BASE+0x20A9A88,cookie,'Q')
        component=DATA+0x20000
        context=DATA+0x100000
        top=DATA+0x1000
        descriptors=DATA+0x60000
        pool=DATA+0x70000
        n.put(BASE+0x2439C70,DATA+0x50000,'Q')
        n.put(DATA+0x50000,descriptors,'Q')
        n.put(descriptors+8,pool,'Q')
        n.put(descriptors+0x30,0x1000)
        n.put(descriptors+0x34,0)
        n.put(component,0)
        n.put(component+8,0,'Q')
        n.put(pool+0x30,0xACBD1234)
        n.put(BASE+0x1F91FE8,1,'H')
        n.put(top,context,'Q')
        n.put(context+8,1)
        n.put(context+0x14,0xACBD1234)
        n.put(context+0x1C,0xFFFFFFFF if missing else 1)
        n.put(pool+0x1000+0x10EB0,0x1337,'Q')
        n.put(context+0x20452,siblings,'h')
        n.put(component+0x1E0,2)
        n.put(component+0x1E8,0,'Q')
        n.put(pool+0x2000,0x123456 if resource else 0,'Q')
        # Each descriptor exercises the native resource-release decision branch.
        for i,rva in enumerate((0x20773B0,0x1FA2B38,0x2077388,0x1FA2B70)):
            pointer=DATA+0x30000+i*0x100
            n.put(BASE+rva,pointer,'Q')
            n.put(pointer+8,pointer+0x40,'Q')
            n.put(pointer+0x40+0x1C,8,'H')
        releases=[]
        sibling_calls=[]
        unlink=[]
        zeroes=[]
        n.stubs[0x323D40]=lambda:top
        n.stubs[0x4E5C40]=lambda:n.put(n.arg(0),1,'H')
        n.stubs[0x4ED320]=lambda:DATA+0x25000
        n.stubs[0x13BF280]=lambda:n.put(n.arg(1),1) or n.arg(1)
        n.stubs[0xAB9D30]=lambda:1
        n.stubs[0xF9EBA0]=lambda:sibling_calls.append((n.arg(0),n.arg(1)))
        n.stubs[0x3055A0]=lambda:n.put(n.arg(0),16,'Q')
        def zero():
            zeroes.append((n.arg(0),n.arg(2)))
            n.uc.mem_write(n.arg(0),bytes(n.arg(2)))
        n.stubs[0x187E862]=zero
        n.stubs[0x372A60]=lambda:releases.append((n.arg(0),n.arg(1)))
        n.stubs[0xBF3180]=lambda:unlink.append(('container',n.arg(0)))
        n.stubs[0xA56EE0]=lambda:unlink.append(('resource',n.arg(0)))
        n.stubs[0xAB92C0]=lambda:unlink.append(('component',n.arg(0)))
        def cookie_check():
            self.assertEqual(n.arg(0),cookie)
        n.stubs[0x187C480]=cookie_check
        preserved=(UC_X86_REG_RBX,UC_X86_REG_RSI,UC_X86_REG_RDI,UC_X86_REG_RBP,
            UC_X86_REG_R12,UC_X86_REG_R13,UC_X86_REG_R14,UC_X86_REG_R15)
        seeds={register:0x123000+i*0x100 for i,register in enumerate(preserved)}
        for register,value in seeds.items():n.uc.reg_write(register,value)
        try:
            n.call((GUARD-BASE) if guarded else 0xF9C150,component)
        except UcError as error:
            error.rva=n.uc.reg_read(UC_X86_REG_RIP)-BASE
            error.add_note(f'RIP={error.rva:X}')
            raise
        self.assertEqual(n.uc.reg_read(UC_X86_REG_RSP),STACK+0xF0000)
        for register,value in seeds.items():self.assertEqual(n.uc.reg_read(register),value)
        self.assertEqual(len(zeroes),4)
        self.assertEqual(len(releases),4)
        self.assertEqual(len(unlink),3 if resource else 1)
        self.assertEqual(len(sibling_calls),0 if missing else siblings)
        self.assertEqual(n.get(self.externs['cleanup_owner_misses'],'Q'),int(missing))
        return releases,unlink,zeroes,sibling_calls

    def test_original_reproduces_dump_failure(self):
        with self.assertRaises(UcError) as caught:self.fixture(True,False)
        self.assertEqual(caught.exception.rva,0xF9C189)

    def test_missing_owner_continues_entire_native_tail(self):
        for resource in (False,True):
            with self.subTest(resource=resource):self.fixture(True,True,2,resource)

    def test_valid_owner_is_identical_to_original(self):
        for siblings in (0,1,3):
            for resource in (False,True):
                with self.subTest(siblings=siblings,resource=resource):
                    self.assertEqual(self.fixture(False,True,siblings,resource),self.fixture(False,False,siblings,resource))

    def test_native_entry_and_continuations_pinned(self):
        source=(ROOT/'Sunrise/src/client/hooks/bootflow/native_cleanup_owner_guard.cpp').read_text()
        import re
        for name,rva,size in (('prefix',0xF9C150,64),('tail',0xF9C27F,13)):
            body=source.split(f'> {name}'+'{',1)[1].split('};',1)[0]
            values=bytes(int(s,16) for s in re.findall(r'0x([0-9a-f]+)',body))
            self.assertEqual(values,self.image[rva:rva+size])

if __name__=='__main__':unittest.main()
