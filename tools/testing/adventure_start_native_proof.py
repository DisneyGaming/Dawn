"""Offline original-code decoder proof for an emitted synthetic type-11 fixture.

Usage: python adventure_start_native_proof.py FIXTURE OUTPUT --native ROOT
ROOT is the local vmprotect-kit evidence directory. No process is opened.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--native',type=Path,required=True)
    args=parser.parse_args()
    sys.path.insert(0,str(args.native))
    from verify_member_lifecycle_offline import (machine,execute,code,B,G,D,UC_X86_REG_RCX,UC_X86_REG_RDX)
    from verify_tactical_source_native_exports import records_for
    records=records_for(0x80808698)
    u=machine();table=0x600000;stride=0x800;u.mem_map(table,0x1000000)
    u.mem_write(B+0x2439C70,struct.pack('<Q',G));u.mem_write(G,struct.pack('<Q',G+0x100))
    for tag,(rva,info,_) in records.items():
        assert info['record_length']<=stride
        shifted=((tag-(1<<32))>>13)&0xffffffff
        index=((shifted|0xffc0000)>>18)&(shifted&0xffff)
        row=G+0x100+index*0x40
        u.mem_write(row+8,struct.pack('<Q',table));u.mem_write(row+0x30,struct.pack('<I',stride))
        u.mem_write(table+(tag&0x1fff)*stride,code[rva:rva+info['record_length']])
    target=D;context=D+0x200;reader=D+0x300;mask=D+0x400;flags=D+0x500;wire=D+0x600
    fixture=args.fixture.read_bytes();assert len(fixture)==73
    u.mem_write(target,bytes(0x118));u.mem_write(wire,fixture)
    u.mem_write(reader,bytes(0x40));u.mem_write(reader,struct.pack('<QQ',wire,wire+len(fixture)))
    u.mem_write(reader+0x30,struct.pack('<I',64));u.mem_write(reader+0x38,struct.pack('<Q',wire))
    u.mem_write(mask,struct.pack('<QI',flags,0x3ffe));u.mem_write(flags,bytes(64))
    u.mem_write(context,struct.pack('<QQQIIQ',reader,target,target,0,0,mask))
    rva,info,_=records[0x80808698]
    u.reg_write(UC_X86_REG_RCX,B+rva+info['block_offset']+8);u.reg_write(UC_X86_REG_RDX,context)
    execute(u,0x4BEE90)
    decoded=bytes(u.mem_read(target,0x118));consumed=struct.unpack('<I',u.mem_read(reader+0x24,4))[0]
    values={'reason':decoded[0],'source':struct.unpack_from('<h',decoded,2)[0],
            'target':struct.unpack_from('<h',decoded,4)[0],'element':struct.unpack_from('<i',decoded,8)[0],
            'account':f'{struct.unpack_from("<Q",decoded,0x10)[0]:016X}',
            'nonce':f'{struct.unpack_from("<Q",decoded,0x18)[0]:016X}','revision':decoded[0x44],
            'package':decoded[0x50:0x78].split(b'\0')[0].decode('ascii')}
    assert consumed==581
    assert values=={'reason':1,'source':1076,'target':1076,'element':-1,
                   'account':'9EAA300100100100','nonce':'A0A016D88F937C17',
                   'revision':3,'package':'mercury_freeroam'},values
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'native-decoded.bin').write_bytes(decoded)
    report={'passed':True,'scope':__doc__,'schema':'80808698 -> 80808716',
            'native_functions':['4BEE90 and original nested/primitive decoders'],
            'intercepted_functions':[],'source_fixture':str(args.fixture),'fixture_sha256':hashlib.sha256(fixture).hexdigest(),
            'native_image_sha256':hashlib.sha256(code).hexdigest(),'consumed_bits':consumed,'decoded':values,
            'limitations':['Synthetic complete request with the exact live 64-byte prefix. The live 90-byte tail is unavailable.',
                           'No live account policy, transactional State commit or client transition is exercised.']}
    (args.output/'native-proof.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
