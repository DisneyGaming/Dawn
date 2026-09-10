"""Original 4E5640 self-reference proof using an archived type68 component.
No process access. Synthetic private address tables retain the captured common
row and full source salt. Optional component+160 remains absent in every case.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--native',type=Path,required=True)
    args=parser.parse_args();sys.path.insert(0,str(args.native))
    from verify_member_lifecycle_offline import machine,execute,code,B,G,D,M,ACTORS,UC_X86_REG_RCX,UC_X86_REG_RDX
    report=json.loads((args.capture/'capture.json').read_text())
    match=[x for x in report['cues'] if x.get('pool')=='common'];assert len(match)==1
    item=match[0];component=(args.capture/f"common-component-{item['slot']}.bin").read_bytes()
    row=bytes.fromhex(item['raw_row']);member=int(item['handle'],16)
    assert component[:16]==bytes.fromhex('676df480544f8080880b000000000000')
    assert component[0x160:0x168]==b'\xff'*8
    assert struct.unpack_from('<I',row,0x20)[0]==member
    assert report['common']['source_class']=='0x80809a3b'
    args.output.mkdir(parents=True,exist_ok=True);cases=[]
    for label,salt,offset in [('captured',member,0),('new-salt',member+0x2000,0),('embedded',member,0x180)]:
        u=machine();u.mem_write(M+offset,component)
        poolrow=bytearray(row);struct.pack_into('<I',poolrow,0x20,salt)
        stride=report['common']['stride'];slot=struct.unpack_from('<H',component,0x20)[0]&0x1fff
        u.mem_write(B+0x1F92108,struct.pack('<Q',ACTORS));u.mem_write(B+0x1F92110,struct.pack('<I',stride))
        u.mem_write(ACTORS+slot*stride,bytes(poolrow))
        u.mem_write(B+0x1FA0D38,struct.pack('<Q',D+0x100));u.mem_write(D+0x100,struct.pack('<I',0x80809A3B))
        u.mem_write(B+0x2439C70,struct.pack('<Q',G));u.mem_write(G,struct.pack('<Q',G+0x100))
        shifted=((salt if salt<0x80000000 else salt-(1<<32))>>13)&0xffffffff
        bucket=((shifted|0xffc0000)>>18)&(shifted&0xffff)
        descriptor=G+0x100+bucket*0x40
        u.mem_write(descriptor+8,struct.pack('<Q',M-slot*0x40));u.mem_write(descriptor+0x30,struct.pack('<Ii',0x40,0))
        u.reg_write(UC_X86_REG_RCX,M+offset);u.reg_write(UC_X86_REG_RDX,D);execute(u,0x4E5640)
        actual=bytes(u.mem_read(D,16));assert actual==struct.pack('<IIq',salt,0x80809A3B,offset)
        (args.output/f'{label}.self.bin').write_bytes(actual)
        (args.output/f'{label}.row.bin').write_bytes(poolrow)
        (args.output/f'{label}.input.bin').write_bytes(struct.pack('<QQIQ',M+offset,ACTORS,stride,M))
        cases.append({'case':label,'self':actual.hex(),'passed':True})
    (args.output/'component.bin').write_bytes(component)
    (args.output/'applied-decoded.bin').write_bytes((args.capture/'applied-decoded.bin').read_bytes())
    (args.output/'manager-entry.bin').write_bytes((args.capture/'manager.bin').read_bytes()[:0x148])
    result={'passed':True,'original_function':'4E5640','intercepted_functions':[],
        'native_image_sha256':hashlib.sha256(code).hexdigest(),'captured_pid':report['pid'],
        'captured_creation_filetime':report['creation_filetime'],'component_sha256':hashlib.sha256(component).hexdigest(),
        'cases':cases,'limits':'Private address tables; does not recover the past1009C00 argument registers or manufacture a live before/after receipt.'}
    (args.output/'native-proof.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
