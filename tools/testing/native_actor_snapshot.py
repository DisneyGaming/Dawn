"""Read-only exact-build actor/source inspection. Does not submit receipts or change the game."""
import ctypes as c
from ctypes import wintypes as w
import struct, json, sys, hashlib
from pathlib import Path
pid=int(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
expected=('5EDF057BF16B73D1A013F8883E522BC6DE864E267DCEF1EFB1C34E8C7D201C29','A0E886CC0806FEA08CDC207CE87D49CB3A5D6791E5C2524569AABDC2F3A86815','B56CF7AA9263075C2C52EA8D5E9563B3996F7F32EE6C0249BDAC87A4E451F9C9','DEADEEC96491CEC0BC3B3B52FEB99E524F4B6F0C694BB1EECFCF672A4FE09AF9')
expected += ('62C4EAE8B12402236E20AF68A725CA7BB435B39B72C2D4CFB2F80B97B84DB28E',)
expected += ('8518034A052E8D7345A749977293C701D9B903DF29B0F8D0FADAA8A94CA887E1',)
expected += ('7F6D2E6CD4688BE86304729DFB8DA884F07A78C370318799317A1DFD9A86FC67',)
assert hashlib.sha256(Path(r'D:/Destiny3/bin/x64/steam_api64.dll').read_bytes()).hexdigest().upper() in expected
k=c.WinDLL('kernel32',use_last_error=True);p=c.WinDLL('psapi',use_last_error=True)
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)];k.ReadProcessMemory.restype=w.BOOL
k.CloseHandle.argtypes=[w.HANDLE]
k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
p.EnumProcessModulesEx.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),w.DWORD]
p.GetModuleFileNameExW.argtypes=[w.HANDLE,c.c_void_p,w.LPWSTR,w.DWORD]
h=k.OpenProcess(0x410,False,pid)
if not h:raise c.WinError(c.get_last_error())
def read(a,n):
 if a<65536 or n>1048576: raise ValueError('bounds')
 b=c.create_string_buffer(n);got=c.c_size_t()
 if not k.ReadProcessMemory(h,a,b,n,c.byref(got)) or got.value!=n:raise OSError('unreadable')
 return b.raw
def val(a,fmt):return struct.unpack('<'+fmt,read(a,struct.calcsize('<'+fmt)))[0]
def at(b,o,fmt):return struct.unpack_from('<'+fmt,b,o)[0]
try:
 name=c.create_unicode_buffer(1024);size=w.DWORD(1024)
 assert k.QueryFullProcessImageNameW(h,0,name,c.byref(size)) and name.value.lower()==r'd:\destiny3\destiny2.exe'
 mods=(c.c_void_p*1024)();needed=w.DWORD();assert p.EnumProcessModulesEx(h,mods,c.sizeof(mods),c.byref(needed),3)
 base=None
 for mod in mods[:needed.value//c.sizeof(c.c_void_p)]:
  name=c.create_unicode_buffer(1024);p.GetModuleFileNameExW(h,mod,name,1024)
  if name.value.lower().endswith('\\destiny2.exe'):base=mod
 assert base
 directory=val(base+0x2439C70,'Q');registry=val(directory,'Q')
 def resolve(handle,kind=0,offset=0):
  if handle==0xffffffff:raise ValueError('absent')
  shifted=(c.c_int32(handle).value>>13)&0xffffffff
  ix=((shifted|0xffc0000)>>18)&(shifted&0xffff)
  t=read(registry+ix*0x40,0x38);stride=at(t,0x30,'i');mask=at(t,0x34,'i')
  assert 0<stride<=1048576
  element=at(t,8,'Q')+(handle&0x1fff)*stride
  correction=val(element+8,'Q') & (mask&0xffffffffffffffff)
  return ((element-correction)&0xffffffffffffffff)+offset
 actorbase=val(base+0x1F9D7F8,'Q');stride=val(base+0x1F9D800,'i');print('base',hex(base),'actorbase',hex(actorbase),'stride',stride);assert 0x70<=stride<=1048576
 result={'pid':pid,'base':hex(base),'actorbase':hex(actorbase),'stride':stride,'actors':[],'pool':read(base+0x1F9D7F0,0x50).hex(),'retire_prologue':read(base+0xA7E400,16).hex(),'destroy_prologue':read(base+0xA85540,16).hex(),'slot1_raw':read(actorbase+stride,0x70).hex(),'slot1_generation':val(actorbase+stride+val(base+0x1F9D7F0+0x1C,'I'),'I'),'pool_header':read(val(base+0x1F9D7F0,'Q'),0x28).hex()}
 for i in range(8192):
  try:
   b=read(actorbase+i*stride,0x80);handle=at(b,0x48,'I')
   if handle==0xffffffff or handle&0x1fff!=i:continue
   source=struct.unpack_from('<IIq',b,0x38)
   if source[0]==0xffffffff:continue
   address=resolve(*source);d=read(address,0x250);definition=struct.unpack_from('<IIq',d)
   if definition[0] not in (0x80F5B68B,0x80F5B68E,0x80F5B9CC):continue
   da=resolve(*definition);identity=read(da+0x30,8)
   parent=at(b,0x50,'I');pa=resolve(parent);ph=read(pa,0x30)
   item={'actor':hex(handle),'entity':hex(at(b,0x4c,'I')),'animation_ref':[hex(v) for v in struct.unpack_from('<IIq',b,0x70)],'source_ref':[hex(v) for v in source], 'source_address':hex(address),'definition':[hex(v) for v in definition], 'scoped':identity.hex(),'generation':at(d,0x1fc,'I'),'sense_generation':at(d,0x244,'I'),'definition_marker':hex(val(da-4,'I')),'parent_entity':hex(at(ph,0x2c,'I')),'parent':hex(parent),'parent_kind':hex(at(ph,4,'I')),'parent_self':hex(at(ph,0x24,'I')),'parent_actor':hex(val(pa+0x1470,'I'))}
   animation=struct.unpack_from('<IIq',b,0x70)
   if animation[0]!=0xffffffff:
    try:
     animation_bytes=read(resolve(*animation),0x1b0)
     item['animation_control']=animation_bytes[0x180:0x1b0].hex()
     (out/f'animation-{handle:08X}.bin').write_bytes(animation_bytes)
    except (OSError,ValueError,AssertionError):pass
   result['actors'].append(item)
   (out/f'actor-{handle:08X}.bin').write_bytes(b);(out/f'source-{handle:08X}.bin').write_bytes(d)
   if definition[0]==0x80F5B9CC:
    (out/f'vance-parent-{parent:08X}.bin').write_bytes(read(pa,0x1480))
    (out/f'vance-actor-full-{handle:08X}.bin').write_bytes(read(actorbase+i*stride,stride))
  except (OSError,ValueError,AssertionError):continue
 # Native facet table has 1024 slots (1719130 iterates to 0x400). +50 is the
 # lifecycle flag word; +68 is a different scheduling word, not the delete bit.
 # Samples are diagnostic evidence only, never population retirement receipts.
 facets=[]
 # Native 1711F70 clears this allocation bit and the owning manager's C920
 # bit before zeroing the global row. Capture them independently of pointers.
 allocation=read(base+0x30B0340,128)
 result['facet_allocated_slots']=[i for i in range(1024) if allocation[i//8]&(1<<(i%8))]
 netbase=val(base+0x2037D48,'Q');netstride=val(base+0x2037D50,'I')
 result['net_entity_pool']={'base':hex(netbase),'stride':netstride}
 for i in range(1024):
  try:
   address=base+0x30B0440+i*0x70;entry=read(address,0x70)
   pointer=at(entry,0x60,'Q')
   if not 0x10000<=pointer<0x7fffffffffff:continue
   sync=read(pointer,8)
   if read(address,0x70)!=entry or read(pointer,8)!=sync:continue
   netentity=at(entry,4,'I');netbytes=''
   if netentity!=0xffffffff and 0<netstride<=0x10000:
    netbytes=read(netbase+(netentity&0x1fff)*netstride,min(netstride,0x100)).hex()
   facets.append({'slot':i,'allocated':bool(allocation[i//8]&(1<<(i%8))),'kind':at(entry,0,'b'),'owner_peer':at(entry,1,'b'),'net_entity_bytes':netbytes,
      'entity':hex(at(entry,4,'I')),'handle':hex(at(entry,8,'I')),
      'lifecycle_flags':hex(at(entry,0x50,'H')),'delete_pending':bool(at(entry,0x50,'H')&4),
      'sync_pointer':hex(pointer),'held_by':hex(at(sync,4,'I'))})
  except (OSError,ValueError):continue
 result['facets']=facets
 result['facet_allocation_stable']=read(base+0x30B0340,128)==allocation
 result['groups']=[]
 for argument in sys.argv[3:]:
  group=int(argument,16);manager=group+0x270
  mask=val(manager+0x110,'I');peers=[]
  for index in range(31):
   view=val(manager+0x18+index*8,'Q')
   if not view:continue
   peer=view-0xa8;data=read(peer,0x100)
   assert at(data,0x48,'Q')==group and at(data,0x50,'I')==index
   assert val(view+0x10,'Q')==manager and val(view+0xc,'I')==index
   transport=at(data,0x68,'Q')
   peers.append({'index':index,'peer':hex(peer),'machine':hex(at(data,0x54,'Q')),
      'member':at(data,0x5c,'I'),'active':at(data,0x44,'B'),'role_flags':[at(data,0x45,'B'),at(data,0x46,'B')],
      'transport':hex(transport),'transport_peer_record':hex(val(transport+8,'Q')) if transport else None})
  owned=read(manager+0xc920,128)
  mapping=read(manager+0x114,8192*6)
  mappings=[]
  for slot in range(8192):
   global_slot=struct.unpack_from('<h',mapping,slot*6)[0]
   if global_slot!=-1:
    mappings.append({'handle_slot':slot,'global_slot':global_slot,'generation_bytes':mapping[slot*6+2:slot*6+6].hex()})
  result['groups'].append({'group':hex(group),'facet_manager':hex(manager),'mask':hex(mask),'peers':peers,
   'owned_global_slots':[i for i in range(1024) if owned[i//8]&(1<<(i%8))],
   'handle_mappings':mappings,'ownership_stable':read(manager+0xc920,128)==owned and read(manager+0x114,8192*6)==mapping})
 result['facet_limitations']='Stable readable rows only; handles must be correlated with a native manager before treating them as owned. No endpoint or release proof from held bits alone.'
 (out/'snapshot.json').write_text(json.dumps(result,indent=2))
 print(json.dumps({k:v for k,v in result.items() if k not in ('facets','groups')},indent=2))
 print(json.dumps([{'group':g['group'],'mask':g['mask'],'peers':g['peers'],'owned_count':len(g['owned_global_slots']),'mapping_count':len(g['handle_mappings']),'stable':g['ownership_stable']} for g in result['groups']],indent=2))
 print('facet samples',len(facets),'pending deletion',sum(x['delete_pending'] for x in facets))
finally:k.CloseHandle(h)
