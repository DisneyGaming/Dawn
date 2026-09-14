"""Verify Eater's type-24 target edges without assigning unproved channel values.

Reads the installed packages only. This is a structural evidence extractor, not an
authority publisher. A target reference does not establish its channel row count,
the meaning of a value, a successful interaction, or target destruction.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import extract_eater_of_worlds as eater

ROOT = Path(__file__).resolve().parents[4]
INVENTORY = ROOT / "docs/raids/eater-of-worlds/evidence/native-inventory.json"
OUTPUT = INVENTORY.with_name("channel-target-bindings.json")


def number(value):
    return int(value, 16) if isinstance(value, str) else value


def recover():
    raw = INVENTORY.read_bytes()
    inventory = json.loads(raw)
    groups = {
        number(g["registryKey"]): g for g in inventory["groups"]
        if number(g["registryKey"]) in (0x91264981, 0xE8D290A0)
    }
    descriptors = {
        (key, d["type"], d["index"]): d
        for key, group in groups.items() for d in group["descriptors"]
    }
    blobs = {}

    def read(tag):
        if tag not in blobs:
            cls, blob = eater.packages.read(tag)
            blobs[tag] = (cls, blob)
        return blobs[tag]

    rows = []
    for identity, descriptor in sorted(descriptors.items()):
        if identity[1] != 24:
            continue
        assert tuple(number(descriptor[name]) for name in
                     ("componentClass", "senseSchema", "authSchema")) == (
                         0x80804F3B, 0x80804F3D, 0x80804F40)
        tag, offset = number(descriptor["sourceTag"]), number(descriptor["sourceOffset"])
        cls, blob = read(tag)
        assert cls == 0x80809C36
        assert struct.unpack_from("<III", blob, offset) == (tag, 0x80804F3B, 0x70)
        assert struct.unpack_from("<IHH", blob, offset + 0x30) == identity
        target = struct.unpack_from("<IHH", blob, offset + 0x58)
        assert target[1] == 4 and target in descriptors
        target_descriptor = descriptors[target]
        object_tag = number(target_descriptor["sourceTag"])
        object_offset = number(target_descriptor["sourceOffset"])
        object_class, object_blob = read(object_tag)
        assert object_class == 0x80809C36
        assert struct.unpack_from("<IHH", object_blob, object_offset + 0x30) == target
        entity_tag = eater.u32(object_blob, object_offset + 0xB8)
        entity_class, entity = read(entity_tag)
        assert entity_class == 0x80809C0F
        builds = [eater.u32(entity, p) for p in eater.array(entity, 0x10, 12)]
        generic_builds = set()
        for p in eater.array(entity, 0x68, 24):
            config, subtype, runtime_bytes, reserved, ordinal, reserved2 = struct.unpack_from("<6I", entity, p)
            assert ordinal < len(builds) and builds[ordinal] == config
            if subtype == 0x8080390E:
                assert runtime_bytes == 0x90
                generic_builds.add((config, ordinal, subtype, runtime_bytes))
        rows.append({
            "registry": f"0x{identity[0]:08X}",
            "slot": identity[2],
            "name": descriptor["name"],
            "sourceTag": f"0x{tag:08X}",
            "sourceOffset": offset,
            "target": {
                "registry": f"0x{target[0]:08X}", "type": target[1], "slot": target[2],
                "name": target_descriptor["name"], "sourceTag": f"0x{object_tag:08X}",
                "sourceOffset": object_offset, "entity": f"0x{entity_tag:08X}",
                "builds": [f"0x{t:08X}" for t in builds],
                "genericDeviceBuilds": [
                    {"config": f"0x{c:08X}", "ordinal": o,
                     "subtype": f"0x{s:08X}", "runtimeBytes": n}
                    for c, o, s, n in sorted(generic_builds)
                ],
            },
            "descriptorTail60To74": blob[offset + 0x60:offset + 0x74].hex(),
            "nativeChannelCount": None,
            "valueSemantics": None,
        })
    counts = {f"0x{k:08X}": sum(r["registry"] == f"0x{k:08X}" for r in rows) for k in groups}
    assert counts == {"0xE8D290A0": 72, "0x91264981": 70}
    return {
        "schema": "eater-channel-target-bindings-v1",
        "inventorySha256": hashlib.sha256(raw).hexdigest(),
        "boundary": "PACKAGE: exact target edges and component build identities only; channel counts, controls and gameplay effects remain unresolved.",
        "counts": counts,
        "bindings": rows,
        "inputs": [
            {"tag": f"0x{t:08X}", "class": f"0x{c:08X}", "bytes": len(b),
             "sha256": hashlib.sha256(b).hexdigest()}
            for t, (c, b) in sorted(blobs.items())
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(recover(), indent=2) + "\n"
    if args.check:
        if OUTPUT.read_text(encoding="utf-8") != rendered:
            raise SystemExit("Eater channel evidence differs from installed packages")
    else:
        OUTPUT.write_text(rendered, encoding="utf-8")
    print("Verified 142 exact Eater channel target edges; gameplay semantics are unresolved.")


if __name__ == "__main__":
    main()
