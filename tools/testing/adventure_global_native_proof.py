"""Decode a production global-state fixture and apply it with original native code.

Private synthetic memory only. No game process is opened and no function is
intercepted. This proves the descriptor response storage contract, not Adventure
encounter scripts, native account eligibility, or a completed live transition.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--native',type=Path,required=True)
    args=parser.parse_args();sys.path.insert(0,str(args.native))
    from verify_member_lifecycle_offline import (machine,execute,code,B,G,D,S,
        UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_RSP,END)
    from verify_tactical_source_native_exports import records_for
    # Qualify the recovered message-1 dispatch and provider vtable in this image.
    assert struct.unpack_from('<QQ',code,0x1F92370)==(1,B+0x4F2AD0)
    assert struct.unpack_from('<Q',code,0x1FB5DC8)[0]==B+0x1C139B8
    assert struct.unpack_from('<Q',code,0x1C139B8+0x310)[0]==B+0xB42C40
    assert code[0xB42C46]==0xE9
    assert 0xB42C4B+struct.unpack_from('<i',code,0xB42C47)[0]==0x16F0930
    records=records_for(0x8080867F)
    u=machine();table=0x600000;stride=0x800;u.mem_map(table,0x1000000)
    u.mem_write(B+0x2439C70,struct.pack('<Q',G));u.mem_write(G,struct.pack('<Q',G+0x100))
    for tag,(rva,info,_) in records.items():
        assert info['record_length']<=stride
        shifted=((tag-(1<<32))>>13)&0xffffffff
        index=((shifted|0xffc0000)>>18)&(shifted&0xffff)
        row=G+0x100+index*0x40
        u.mem_write(row+8,struct.pack('<Q',table));u.mem_write(row+0x30,struct.pack('<I',stride))
        u.mem_write(table+(tag&0x1fff)*stride,code[rva:rva+info['record_length']])
    target=D;context=D+0x800;reader=D+0x900;mask=D+0xA00;flags=D+0xB00;wire=D+0xC00
    fixture=args.fixture.read_bytes();assert len(fixture)<=400
    u.mem_write(target,bytes(0x648));u.mem_write(wire,fixture)
    u.mem_write(reader,bytes(0x40));u.mem_write(reader,struct.pack('<QQ',wire,wire+len(fixture)))
    u.mem_write(reader+0x30,struct.pack('<I',64));u.mem_write(reader+0x38,struct.pack('<Q',wire))
    u.mem_write(mask,struct.pack('<QI',flags,0x3ffe));u.mem_write(flags,bytes(64))
    u.mem_write(context,struct.pack('<QQQIIQ',reader,target,target,0,0,mask))
    rva,info,_=records[0x8080867F]
    u.reg_write(UC_X86_REG_RCX,B+rva+info['block_offset']+8);u.reg_write(UC_X86_REG_RDX,context)
    execute(u,0x4BEE90)
    decoded=bytes(u.mem_read(target,0x648));consumed=struct.unpack('<I',u.mem_read(reader+0x24,4))[0]
    assert 0<=len(fixture)*8-consumed<8
    assert struct.unpack_from('<i',decoded,0x10)[0]==24
    assert struct.unpack_from('<h',decoded,0x54)[0]==120
    assert struct.unpack_from('<I',decoded,0x58)[0]==0xD49C610E
    descriptor=decoded[0x68:0x180]
    assert descriptor[0]==1 and struct.unpack_from('<hh',descriptor,2)==(1076,1076)
    assert descriptor[0x44]==3 and descriptor[0x50:0x78].split(b'\0')[0]==b'mercury_freeroam'
    # Original application copies into an existing simulation context. The
    # optional telemetry branch (+20) is disabled in this synthetic context.
    owner=0x1800000;client=owner+0x100000
    u.mem_map(owner,0x120000);before=bytes([0xA5])*0x60000;u.mem_write(owner,before)
    u.mem_write(client,bytes(0x12000));u.mem_write(client+0x10,struct.pack('<Q',owner))
    u.reg_write(UC_X86_REG_RSP,S);u.mem_write(S,struct.pack('<Q',END))
    u.reg_write(UC_X86_REG_RCX,client);u.reg_write(UC_X86_REG_RDX,target)
    execute(u,0x3CB4A0)
    after=bytes(u.mem_read(owner,len(before)));expected=bytearray(before)
    expected[0x592E8:0x59930]=decoded;expected[0x592E0]=1
    assert after==expected, 'Native apply changed bytes beyond global state and dirty flag'
    u.reg_write(UC_X86_REG_RSP,S);u.mem_write(S,struct.pack('<Q',END))
    u.reg_write(UC_X86_REG_RCX,owner);u.reg_write(UC_X86_REG_RDX,D+0xF00)
    execute(u,0x3CAEB0)
    assert struct.unpack('<h',u.mem_read(D+0xF00,2))[0]==1076
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'native-global-decoded.bin').write_bytes(decoded)
    report={'passed':True,'schema':'8080867F -> 808086E6 -> 808086E7 -> 80808716',
            'source_fixture':str(args.fixture),'fixture_sha256':hashlib.sha256(fixture).hexdigest(),
            'native_image_sha256':hashlib.sha256(code).hexdigest(),'consumed_bits':consumed,
            'native_functions':['4BEE90 and nested decoders','3CB4A0 original global-state apply','3CAEB0 target getter'],
            'intercepted_functions':[], 'response_descriptor_offset':0x68,'target':1076,
            'qualified_dispatch':['1F92370 type1 -> 4F2AD0','1FB5DC8 -> vtable1C139B8 slot310 -> B42C40 -> 16F0930'],
            'same_context_preserved':True,'only_writes':'context+592E8 (648 bytes), context+592E0 dirty byte',
            'limitations':['Synthetic complete request; actual 90-byte request tail remains uncaptured.',
                           'Original simulation-event dispatcher and optional telemetry branch are not executed.',
                           'No game verification or Adventure encounter execution is claimed.']}
    (args.output/'native-global-proof.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
