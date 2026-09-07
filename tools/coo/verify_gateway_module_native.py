"""Replay native module generation and damage gates offline; never touch a process."""
import hashlib,json,struct
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RBX,UC_X86_REG_RSP,UC_X86_REG_RCX,UC_X86_REG_RAX
ROOT=Path(__file__).resolve().parents[2]
def verify(out):
 image=(ROOT/'destiny2_unpacked.bin').read_bytes()
 assert hashlib.sha256(image).hexdigest()=='63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
 def machine(page):
  u=Uc(UC_ARCH_X86,UC_MODE_64);u.mem_map(page,0x1000);u.mem_write(page,image[page:page+0x1000])
  for a in (0x100000,0x200000,0x300000):u.mem_map(a,0x1000)
  u.reg_write(UC_X86_REG_RSP,0x200800);return u
 rows=[]
 for requested,committed,active in [(1,1,1),(2,1,0),(2,1,1),(2,2,1),(2,2,0),(3,2,0),(4,2,1),(4,4,1)]:
  u=machine(0x9F2000);u.reg_write(UC_X86_REG_RBX,0x100000)
  u.mem_write(0x100180,struct.pack('<I',requested));u.mem_write(0x1002F0,struct.pack('<I',committed));u.mem_write(0x100188,bytes([active]))
  u.mem_write(0x200828,struct.pack('<Q',0x300000));outcome=[]
  def stop(u,addr,size,data):
   if addr==0x9F2FBA:outcome.append(True);u.emu_stop()
   elif addr==0x300000:outcome.append(False);u.emu_stop()
  u.hook_add(UC_HOOK_CODE,stop);u.emu_start(0x9F2F9B,0x300001,count=30)
  assert outcome==[requested>committed and bool(active)]
  rows.append(dict(requested=requested,committed=committed,active=active,create=outcome[0]))
 # CDCB60 is a native lethal-health gate. The pre-boss guard also bypasses
 # B804E0 so immunity does not merely clamp lethal damage after reducing HP.
 for flags in range(256):
  u=machine(0xCDC000);u.mem_write(0x100008,struct.pack('<Q',0x100100));u.mem_write(0x100438,bytes([flags]))
  u.reg_write(UC_X86_REG_RCX,0x100000);u.mem_write(0x200800,struct.pack('<Q',0x300000))
  u.emu_start(0xCDCB60,0x300000,count=15);assert u.reg_read(UC_X86_REG_RAX)&255 == int(not(flags&2))
 assert image[0xB806BB]==0xE8 and 0xB806C0+struct.unpack_from('<i',image,0xB806BC)[0]==0xCDCB60
 assert image[0xB815FA:0xB81605]==bytes.fromhex('807c2464000f841f010000')
 out.mkdir(parents=True,exist_ok=True)
 (out/'native-module-gates.json').write_text(json.dumps({'spawnCases':rows,'nativeHealthFlagCases':256,'nativeValidation':'pending fresh Gateway playthrough'},indent=2)+'\n')
 print('Verified 8 native creation cases and 256 native health gate cases.',flush=True)
if __name__=='__main__':verify(ROOT/'build/coo/validation-gateway-module')
