"""Decode actual Adventure opening lifetime bodies using original offline client code, without game access."""
from pathlib import Path
import struct,sys,hashlib,json
sys.path.insert(0,r'D:/Dawn-work/vmprotect-kit-20260905')
from verify_member_lifecycle_offline import machine,execute,code,B,G,D,UC_X86_REG_RCX,UC_X86_REG_RDX
from verify_tactical_source_native_exports import records_for
base=Path(sys.argv[1]);records=records_for(0x8080991A);records.update(records_for(0x80800007));records.update(records_for(0x80800046));reports=[];decoded=[]
for filename,expected in [('lifetime-before.body',0),('lifetime-adventure15.body',15)]:
 raw=(base/filename).read_bytes();u=machine();table=0x600000;stride=0x800;u.mem_map(table,0x1000000)
 u.mem_write(B+0x2439C70,struct.pack('<Q',G));u.mem_write(G,struct.pack('<Q',G+0x100))
 for tag,(rva,info,_) in records.items():
  assert info['record_length']<=stride
  shifted=((tag-(1<<32))>>13)&0xffffffff;index=((shifted|0xffc0000)>>18)&(shifted&0xffff);row=G+0x100+index*0x40
  u.mem_write(row+8,struct.pack('<Q',table));u.mem_write(row+0x30,struct.pack('<I',stride))
  u.mem_write(table+(tag&0x1fff)*stride,code[rva:rva+info['record_length']])
 target=D;context=D+0x1000;reader=D+0x1100;mask=D+0x1200;flags=D+0x1300;wire=D+0x2000
 u.mem_write(target,bytes(0x514));u.mem_write(wire,raw);u.mem_write(reader,bytes(0x40));u.mem_write(reader,struct.pack('<QQ',wire,wire+len(raw)))
 u.mem_write(reader+0x30,struct.pack('<I',64));u.mem_write(reader+0x38,struct.pack('<Q',wire))
 u.mem_write(mask,struct.pack('<QI',flags,0x3ffe));u.mem_write(flags,bytes(64));u.mem_write(context,struct.pack('<QQQIIQ',reader,target,target,0,0,mask))
 rva,info,_=records[0x8080991A];u.reg_write(UC_X86_REG_RCX,B+rva+info['block_offset']+8);u.reg_write(UC_X86_REG_RDX,context);execute(u,0x4BEE90)
 data=bytes(u.mem_read(target,0x514));consumed=struct.unpack('<I',u.mem_read(reader+0x24,4))[0];actual=struct.unpack_from('<i',data,0xC)[0]
 assert consumed==520,(filename,consumed);assert actual==expected,(filename,actual);decoded.append(data)
 (base/(filename+'.decoded.bin')).write_bytes(data)
 reports.append(dict(file=filename,bits=consumed,decoded_authority_C=actual,wire_sha256=hashlib.sha256(raw).hexdigest(),decoded_sha256=hashlib.sha256(data).hexdigest()))
difference=[i for i,(a,b) in enumerate(zip(*decoded)) if a!=b];assert difference==[12],difference
report=dict(passed=True,schema='8080991A',native_image_sha256=hashlib.sha256(code).hexdigest(),original_functions=['4BEE90 and nested original decoders'],intercepted_decoder_functions=[],bodies=reports,decoded_differing_byte_offsets=difference,limitations=['Does not execute authority apply, native region lookup, arrival or gameplay. Live retest still required.'])
(base/'adventure_opening_lifetime_native_decode.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

