"""Read-only verification of Garden's delayed native speech input and its successors."""
import json
import struct
from extract_gateway_bindings import array, u32, i64
from package_read import read

def verify():
    tag = 0x80F44F11
    _, graph = read(tag)
    node = 0x1590 + 0x90
    signal = node + 0xD0
    assert struct.unpack_from('<IIq', graph, node) == (tag, 0x808062F6, 0x4D38)
    assert struct.unpack_from('<IIq', graph, array(graph, 0xF8, 16)[16]) == (tag, 0x80806388, signal)
    assert struct.unpack_from('<IIIIq', graph, signal + 0x20) == (0x80806342, 0, tag, 0x808062F5, node)
    assert u32(graph, signal + 0x50) == 0xFFFFFFFF
    assert u32(graph, signal + 0x60) == 0
    actions = {}
    for entry in array(graph, 0xC8, 0x30):
        p = entry + 0x20 + i64(graph, entry + 0x20)
        kind, definition = u32(graph, p + 4), i64(graph, p + 8)
        if kind in (0x8080631F, 0x80806323):
            continue
        start = u32(graph, definition + 0x48)
        actions[start] = (kind, [u32(graph, x) for x in array(graph, definition + 0x20, 4)])
    assert actions[16] == (0x808062F6, [17])
    assert actions[17] == (0x80806285, [20])
    assert actions[20] == (0x808062F6, [18])
    assert actions[18] == (0x80806285, [19])
    assert actions[19] == (0x808062E4, [])
    print(json.dumps({'result':'PASS', 'graph':hex(tag), 'input':16,
        'preservedChain':['reaction speech','4s pause','capture scream','3s pause','closing cue']}))

if __name__ == '__main__':
    verify()
