"""Pinned-image, offline proof for the Panoptes .5 scalar authority boundary.

No process access. Execute the original real32 decoder and control scalar loop
in Unicorn; intercept only the final engine setter to record its arguments.
"""
from pathlib import Path
import hashlib
import json
import struct as S
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

ROOT = Path(__file__).resolve().parents[3]
IMAGE = (ROOT / 'destiny2_unpacked.bin').read_bytes()
assert hashlib.sha256(IMAGE).hexdigest() == '63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
OUT = ROOT / 'build/coo/omega-server-native-boundary-20260912'
OUT.mkdir(parents=True, exist_ok=True)

def fields(at, tag, count):
    assert S.unpack_from('<I', IMAGE, at)[0] == tag
    assert S.unpack_from('<I', IMAGE, at-8)[0] == count
    return [S.unpack_from('<10I', IMAGE, at+0x18+i*0x28) for i in range(count)]

control = fields(0x38F45B8, 0x80807DAC, 7)
assert [x[0] for x in control] == [0, 4, 5, 8, 0x24, 0x2c, 0x30]
assert control[-1][5] == 0x80806F11
array = fields(0x38E3E08, 0x80806F11, 2)
assert array[0][7] == 5 and array[1][5] == 0x80806F09
rows = fields(0x39089C8, 0x80806F09, 1)
assert rows[0][5] == 0x80807DAF and S.unpack_from('<I', IMAGE, 0x39089D8)[0] == 16
row = fields(0x3778C98, 0x80807DAF, 2)
assert row[0][0] == 0 and row[0][4] == 9 and row[0][7] == 32
assert row[1][0] == 4 and row[1][4] == 11 and row[1][8] == 32

def mapped(rva, length):
    u = Uc(UC_ARCH_X86, UC_MODE_64)
    start = rva & ~4095
    size = ((rva+length+4095)&~4095)-start
    u.mem_map(start, size)
    u.mem_write(start, IMAGE[start:start+size])
    u.mem_map(0x10000000, 0x10000)
    return u

def float_decode(value, offset):
    u = mapped(0x9F8D50, 0x60)
    u.mem_map(0x350000, 0x2000)
    u.mem_write(0x350000, IMAGE[0x350000:0x352000])
    reader, buf, output, descriptor, sp, stop = [0x10000000+x for x in (0, 0x1000, 0x2000, 0x3000, 0x8008, 0xF000)]
    stream = '1'*offset + f'{value:032b}' + '0'*128
    stream += '0'*(-len(stream)%8)
    packet = int(stream, 2).to_bytes(len(stream)//8, 'big')
    state = bytearray(64)
    S.pack_into('<Q', state, 8, buf+len(packet))
    S.pack_into('<IIQI', state, 0x20, 64, offset, (int.from_bytes(packet[:8], 'big')<<offset)&0xffffffffffffffff, offset)
    S.pack_into('<Q', state, 0x38, buf+8)
    u.mem_write(reader, bytes(state)); u.mem_write(buf, packet)
    u.mem_write(descriptor, S.pack('<III', 0, 0, row[1][8]))
    u.mem_write(sp, S.pack('<Q', stop))
    for reg, value in ((UC_X86_REG_RSP,sp),(UC_X86_REG_RCX,output),(UC_X86_REG_R8,descriptor),(UC_X86_REG_R9,reader)):
        u.reg_write(reg,value)
    u.emu_start(0x9F8D50, stop, count=10000)
    return S.unpack('<I',u.mem_read(output,4))[0]

def defaults():
    result=bytearray(0xB4)
    def program(start,base=0):
        end=start+S.unpack_from('<Q',IMAGE,start)[0]; at=start+8
        while at<end:
            while S.unpack_from('<I',IMAGE,at)[0] not in (0x80800072,0x80800073,0x80800074):
                assert S.unpack_from('<I',IMAGE,at)[0]==0;at+=4
            kind=S.unpack_from('<I',IMAGE,at)[0];payload=at+4
            if kind==0x80800073:
                count,dest=S.unpack_from('<QQ',IMAGE,payload)
                result[base+dest:base+dest+count]=IMAGE[payload+16:payload+16+count]
                at=payload+16+count
            else:
                assert kind==0x80800074
                dest,stride,count=S.unpack_from('<QQQ',IMAGE,payload);at=payload+24
                while S.unpack_from('<I',IMAGE,at)[0]!=0x80800072:
                    assert S.unpack_from('<I',IMAGE,at)[0]==0;at+=4
                nested=at+4
                for i in range(count):program(nested,base+dest+i*stride)
                at=nested+S.unpack_from('<Q',IMAGE,nested)[0]
        assert at==end
    at=0x38F4550+0x40
    program(at+S.unpack_from('<Q',IMAGE,at)[0])
    return result

DEFAULT=defaults()
assert DEFAULT[4:0xC]==bytes(8)
assert S.unpack_from('<6I',DEFAULT,0xC)==(0x811C9DC5,)*6
assert DEFAULT[0x24:0x30]==bytes.fromhex('c59d1c81ff00ffffffffffff')

def apply_rows(values,revision=29):
    # Execute all of AB2D00, including flag gates and target-list caching.
    u = mapped(0xAB2D00, 0x400)
    member,authority,actor,sp,stop = 0x10000000,0x10001000,0x10002000,0x10008008,0x1000F000
    u.mem_map(0x1F9D000,0x1000);u.mem_map(0x20A9000,0x1000)
    u.mem_write(0x1F9D800,S.pack('<I',0));u.mem_write(0x1F9D7F8,S.pack('<Q',actor))
    u.mem_write(actor+0x4C,S.pack('<I',0x45670004))
    data=bytearray(0x1000);data[6]=1;data[0x4C:0x100]=DEFAULT
    S.pack_into('<I',data,0x4C,revision);S.pack_into('<I',data,0x7C,len(values))
    for i,(key,value) in enumerate(zip((0xA2AE120F,0x8496ABD2),values)):
        S.pack_into('<II',data,0x80+8*i,key,value)
    baseline=bytearray(0x1000);S.pack_into('<I',baseline,0x21C,1)
    baseline[0xA90:0xAAC]=DEFAULT[8:0x24]
    u.mem_write(member,bytes(baseline));u.mem_write(authority,bytes(data))
    actor_before=bytes(u.mem_read(actor,0x1000))
    u.mem_write(sp,S.pack('<Q',stop))
    for reg,value in ((UC_X86_REG_RCX,member),(UC_X86_REG_RDX,authority),(UC_X86_REG_RSP,sp)):
        u.reg_write(reg,value)
    calls=[]
    def call(uc,address,size,user):
        if address==0xAB301E: # security-cookie check; no semantic engine mutation
            uc.reg_write(UC_X86_REG_RIP,0xAB3023);return
        if address!=0xAB2FFA:return
        entity=uc.reg_read(UC_X86_REG_RCX)
        key=S.unpack('<I',uc.mem_read(uc.reg_read(UC_X86_REG_RDX),4))[0]
        lanes=S.unpack('<4I',uc.mem_read(uc.reg_read(UC_X86_REG_R8),16))
        calls.append((entity,key,list(lanes)))
        uc.reg_write(UC_X86_REG_RAX,1);uc.reg_write(UC_X86_REG_RIP,0xAB2FFF)
    u.hook_add(UC_HOOK_CODE,call)
    u.emu_start(0xAB2D00,stop,count=1000)
    assert calls==[(0x45670004,key,[value]*4) for key,value in zip((0xA2AE120F,0x8496ABD2),values)]
    S.pack_into('<I',baseline,0xAB0,revision)
    assert bytes(u.mem_read(member,0x1000))==bytes(baseline), 'non-scalar member state changed'
    assert bytes(u.mem_read(actor,0x1000))==actor_before, 'actor flags/targeting changed'
    return calls

for offset in range(8):
    for value in (0,0x3F800000):
        assert float_decode(value,offset) == value
calls=[apply_rows(values) for values in ((0,0),(0x3F800000,0),(0,0x3F800000))]
calls.append(apply_rows((),0))
result={'imageSha256':hashlib.sha256(IMAGE).hexdigest(),'nativeFloatDecoder':'9F8D50 / 3513B0',
        'nativeScalarLoop':'entire AB2D00 -> 576420; no non-scalar changes except committed revision','floatDecodeCases':16,'scalarApplicationCases':4,
        'controlSchema':'80807DAC.6 -> 80806F11 -> 80806F09 -> 80807DAF',
        'calls':calls,'gameProcessAccess':False,
        'limits':'Does not emulate actor allocation, networking, or native animation playback.'}
(OUT/'native-arm-control-proof.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
