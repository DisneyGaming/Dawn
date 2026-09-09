"""Offline evidence checks for Future cue timing and native tactical assignments."""
import struct
from generate_beyond_infinity_ai import generate
from package_read import read
from extract_gateway_bindings import array,i64,u32
header,report=generate()
assert [j['row'] for j in report['joins']]==[0,1,2,3,4,5,6,6,6]
_,root=read(0x80EC0901);_,child=read(0x80EC0872)
assert struct.unpack_from('<IIQ',child,0x90)==(0x80EC0872,0x808084E9,0x2648)
assert struct.unpack_from('<IIQ',child,0x10D0)==(0x80EC0872,0x8080658E,0x3490)
assert u32(child,0x34D4)==0xD44E3621 and u32(child,0x34DC)==0x80F1FDF7
start=struct.unpack_from('<f',child,0x34AC)[0]
actions=array(root,0xC8,0x30)
p=actions[23]+0x20+i64(root,actions[23]+0x20);d=i64(root,p+8)
assert u32(root,p+4)==0x80806285
# Delay action's duration is a package float in the definition.
assert struct.unpack_from('<ff',root,d+0x58)==(33.5,33.5)
assert 33.3<start<33.5
print('PASS: native Go starts',start,'seconds, before the witnessed33.5-second cue; all9 tactical joins verified')
