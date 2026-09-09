"""Verify that detaching selected Well speech cannot disconnect native visuals.

Reads the installed package graph, including the transitive completion edges.
No game process or package is modified.
"""
import json
import struct
from pathlib import Path
from extract_gateway_bindings import array, u32, i64
from generate_beyond_infinity_runtime import playback
from package_read import read

ROOT = Path(__file__).resolve().parents[2]
GRAPHS = (0x80EC097D, 0x80EC096A, 0x80EC0975, 0x80EC095C, 0x80EC0966)


def verify():
    data = json.loads((ROOT / 'build/coo/beyond-infinity-research/native-bindings.json').read_text())
    total = 0
    for tag in GRAPHS:
        _, graph = read(tag)
        actions = {}
        for entry in array(graph, 0xC8, 0x30):
            p = entry + 0x20 + i64(graph, entry + 0x20)
            kind, definition = u32(graph, p + 4), i64(graph, p + 8)
            if kind in (0x8080631F, 0x80806323):
                continue
            start = u32(graph, definition + 0x48)
            assert start not in actions
            actions[start] = (kind, [u32(graph, x) for x in array(graph, definition + 0x20, 4)])
        inputs = array(graph, 0xF8, 16)
        for row, offset, definition, start in playback(tag, data)[1]:
            assert struct.unpack_from('<IIq', graph, inputs[start]) == (tag, 0x80806388, offset + 0x90 + 0xD0)
            assert u32(graph, offset + 0x90 + 0x98) == 0
            signal = offset + 0x90 + 0xD0
            # The runtime linker replaces the package tag with selector self.
            assert struct.unpack_from('<IIIIq', graph, signal + 0x20) == (0x80806342, 0, tag, 0x808062F5, offset + 0x90)
            assert u32(graph, signal + 0x50) == 0xFFFFFFFF
            assert u32(graph, signal + 0x60) == 0
            pending, seen = list(actions[start][1]), set()
            while pending:
                successor = pending.pop()
                if successor in seen:
                    continue
                seen.add(successor)
                kind, outputs = actions[successor]
                assert kind in (0x808062F6, 0x80806285), (hex(tag), row, successor, hex(kind))
                pending.extend(outputs)
            total += 1
    print(f'PASS: {total} Well speech callbacks; downstream edges contain only speech/delays, no animation or actor actions')


if __name__ == '__main__':
    verify()
