"""Read Eater's two Argos Scene descriptors and their native graph contracts."""
import json
import struct
from pathlib import Path

from extract_gateway_bindings import array, i64, u32
from package_read import read

ROOT = Path(__file__).resolve().parents[2]
SOURCE_TAGS = (0x80C421A1, 0x80C424FF)


def hx(value):
    return f"0x{value:08X}"


def main():
    inventory = json.loads((ROOT / "docs/raids/eater-of-worlds/evidence/runtime-bindings.json").read_text())
    names = {}
    for group in inventory["groups"]:
        for descriptor in group["descriptors"]:
            names[(int(group["registryKey"], 16), descriptor["type"], descriptor["index"])] = descriptor["name"]
    result = []
    for group in inventory["groups"]:
        for descriptor in group["descriptors"]:
            tag = int(descriptor["sourceTag"], 16)
            if tag not in SOURCE_TAGS:
                continue
            registry = int(group["registryKey"], 16)
            slot_type, slot = descriptor["type"], descriptor["index"]
            cls, blob = read(tag)
            offset = descriptor["sourceOffset"]
            assert descriptor["componentClass"] == "0x80806382"
            assert struct.unpack_from("<IHH", blob, offset + 0x30) == (registry, slot_type, slot)
            cast = []
            for ref in array(blob, offset + 0x68, 8, 0x80806268):
                target = ref + i64(blob, ref)
                kind = u32(blob, target - 4)
                identity = struct.unpack_from("<IHH", blob, target + 8)
                assert kind in (0x80806262, 0x80806264)
                cast.append({"kind": "actor" if kind == 0x80806262 else "object",
                             "registry": hx(identity[0]), "type": identity[1], "slot": identity[2],
                             "name": names.get(identity)})
            selector = u32(blob, offset + 0x60)
            selector_class, entity = read(selector)
            assert selector_class == 0x80809C0F
            graphs = []
            for resource in array(entity, 0x10, 12):
                graph_tag = u32(entity, resource)
                graph_class, graph = read(graph_tag)
                if graph_class == 0x80809C36 and len(graph) >= 0xA0 and u32(graph, 0x94) == 0x80806384:
                    assert u32(graph, 0x90) == graph_tag
                    graphs.append((graph_tag, i64(graph, 0x98), graph))
            assert len(graphs) == 1
            graph_tag, graph_offset, graph = graphs[0]
            events = []
            for entry in array(graph, 0xE8, 0xC0):
                assert u32(graph, entry) == graph_tag and u32(graph, entry + 4) == 0x8080638A
                definition = i64(graph, entry + 8)
                assert u32(graph, definition + 4) == 0x8080637D
                event = u32(graph, definition + 0x10)
                if event != 0x385838EC:
                    events.append(hx(event))
            result.append({"source": hx(tag), "registry": hx(registry), "type": slot_type,
                           "slot": slot, "name": descriptor["name"], "selector": hx(selector),
                           "selectorClass": hx(selector_class), "graph": hx(graph_tag),
                           "graphOffset": graph_offset, "cast": cast, "nativeEvents": events})
    assert {int(row["source"], 16) for row in result} == set(SOURCE_TAGS)
    print(json.dumps({"schema": "eater-scene-bindings-v1", "scenes": result}, indent=2))


if __name__ == "__main__":
    main()
