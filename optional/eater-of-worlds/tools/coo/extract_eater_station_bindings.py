"""Recover exact Eater fire-station interaction bindings from package metadata."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/raids/eater-of-worlds/tools"))
import extract_eater_of_worlds as eater

INPUT = ROOT / "docs/raids/eater-of-worlds/evidence/runtime-bindings.json"
CHANNELS = ROOT / "docs/raids/eater-of-worlds/evidence/channel-target-bindings.json"
EVIDENCE = ROOT / "docs/raids/eater-of-worlds/evidence/station-bindings.json"
HEADER = ROOT / "Dawn/src/state/activity/eater_of_worlds/station_bindings.h"
ENTITY_CLASS = 0x80809C0F
CONFIG_CLASS = 0x80809C36
INTERACTION_DECLARATION = 0x80804FB0
INTERACTION_KIND = 0x80804FB2
INTERACTION_OFFSET = 0x388
ENTITY_INTERFACE_ROW = 0x80809C50
ENTITY_INTERFACE_TABLE = 0x80809C22
DYNAMIC_AUTHORITY_INTERFACE = 0x80809AE3
DYNAMIC_AUTHORITY_LINK = 0x80C23296
DYNAMIC_AUTHORITY_OFFSET = 0x648
DYNAMIC_AUTHORITY_LINK_CLASS = 0x80809C54
DYNAMIC_AUTHORITY_METHOD_ROW = 0x80809C56
DYNAMIC_AUTHORITY_METHOD_SLOTS = (4, 5)
INHERITED_INTERACTION_LINK = 0x80C707B5
INHERITED_INTERACTION_SUBTYPE = 0x80804FBB


def verify_dynamic_authority_join(entity_tag: int, config: int, config_blob: bytes) -> None:
    cls, entity = eater.packages.read(entity_tag)
    assert cls == ENTITY_CLASS
    rows = eater.array(entity, 0x58, 40, ENTITY_INTERFACE_TABLE)
    matches = [row for row in rows
               if struct.unpack_from("<IIIqI", entity, row + 12)
               == (INTERACTION_DECLARATION, config, ENTITY_INTERFACE_ROW,
                   DYNAMIC_AUTHORITY_OFFSET, DYNAMIC_AUTHORITY_LINK)]
    assert len(matches) == 1, (hex(entity_tag), hex(config), matches)
    inherited = [row for row in rows
                 if struct.unpack_from("<IIIqI", entity, row + 12)
                 == (INTERACTION_DECLARATION, config, ENTITY_INTERFACE_ROW,
                     0x5F8, INHERITED_INTERACTION_LINK)]
    assert len(inherited) == 1, (hex(entity_tag), hex(config), inherited)

    cls, link = eater.packages.read(DYNAMIC_AUTHORITY_LINK)
    assert cls == DYNAMIC_AUTHORITY_LINK_CLASS
    assert struct.unpack_from("<II", link, 8) == (0x80809AE4, DYNAMIC_AUTHORITY_INTERFACE)
    methods = eater.array(link, 0x10, 24, DYNAMIC_AUTHORITY_METHOD_ROW)
    assert tuple(struct.unpack_from("<II", link, row) for row in methods) == tuple(
        (INTERACTION_DECLARATION, slot) for slot in DYNAMIC_AUTHORITY_METHOD_SLOTS)

    cls, inherited_link = eater.packages.read(INHERITED_INTERACTION_LINK)
    assert cls == DYNAMIC_AUTHORITY_LINK_CLASS
    inherited_methods = eater.array(inherited_link, 0x10, 24, DYNAMIC_AUTHORITY_METHOD_ROW)
    assert tuple(struct.unpack_from("<II", inherited_link, row) for row in inherited_methods) == (
        (INHERITED_INTERACTION_SUBTYPE, 16), (INHERITED_INTERACTION_SUBTYPE, 17))
    assert struct.unpack_from("<q", config_blob, DYNAMIC_AUTHORITY_OFFSET)[0] == -0x2C0
    assert DYNAMIC_AUTHORITY_OFFSET + struct.unpack_from("<q", config_blob,
                                                         DYNAMIC_AUTHORITY_OFFSET)[0] == INTERACTION_OFFSET


def recover() -> dict:
    raw = INPUT.read_bytes()
    channel_raw = CHANNELS.read_bytes()
    inventory = json.loads(raw)
    channels = json.loads(channel_raw)
    descriptors = {}
    for group in inventory["groups"]:
        registry = int(group["registryKey"], 16)
        for descriptor in group["descriptors"]:
            descriptors[(registry, descriptor["type"], descriptor["index"])] = descriptor

    fire_channels = {}
    for edge in channels["bindings"]:
        name = edge["name"]
        if not (name.startswith("fires[") and name.endswith("].ch_fire")):
            continue
        registry = int(edge["registry"], 16)
        lane = int(name[len("fires["):name.index("]")])
        target = edge["target"]
        fire_channels[(registry, lane)] = {
            "channelSlot": edge["slot"],
            "channelSource": int(edge["sourceTag"], 16),
            "fireSlot": target["slot"],
            "fireSource": int(target["sourceTag"], 16),
        }

    bindings = []
    for entity in inventory["entities"]:
        descriptor = descriptors.get((entity["registry"], 4, entity["slot"]))
        name = descriptor.get("name", "") if descriptor else ""
        if not (name.startswith("fires[") and name.endswith("].o_interactable")):
            continue
        lane = int(name[len("fires["):name.index("]")])
        fire = fire_channels[(entity["registry"], lane)]
        assert fire["fireSlot"] + 1 == entity["slot"]
        matches = []
        for resource in entity["resources"]:
            cls, blob = eater.packages.read(resource)
            if cls != CONFIG_CLASS or len(blob) < 0xA0:
                continue
            declaration = struct.unpack_from("<I", blob, 0x8C)[0]
            config, kind, offset = struct.unpack_from("<IIQ", blob, 0x90)
            if config == resource and declaration == INTERACTION_DECLARATION and kind == INTERACTION_KIND:
                assert offset == INTERACTION_OFFSET and len(blob) >= offset + 16
                verify_dynamic_authority_join(entity["entity"], config, blob)
                matches.append((config, offset))
        assert len(matches) == 1, (entity["registry"], entity["slot"], name, matches)
        config, offset = matches[0]
        bindings.append({
            "registry": entity["registry"], "slot": entity["slot"],
            "source": entity["source"], "entity": entity["entity"],
            "config": config, "offset": offset, "lane": lane,
            **fire,
        })

    bindings.sort(key=lambda binding: (binding["registry"], binding["slot"], binding["source"]))
    identities = [(b["registry"], b["slot"], b["source"]) for b in bindings]
    assert len(bindings) == 18 and len(set(identities)) == len(bindings)
    assert Counter(b["registry"] for b in bindings) == {0x91264981: 9, 0xE8D290A0: 9}
    assert Counter((b["entity"], b["config"]) for b in bindings) == {
        (0x80F42F4F, 0x80F42F4E): 2, (0x80F42F52, 0x80F42F51): 2,
        (0x80F42F55, 0x80F42F54): 2, (0x80F42F58, 0x80F42F57): 2,
        (0x80F42F5B, 0x80F42F5A): 2, (0x80F42F5E, 0x80F42F5D): 2,
        (0x80F42F61, 0x80F42F60): 2, (0x80F42F64, 0x80F42F63): 2,
        (0x80F42F67, 0x80F42F66): 2,
    }
    return {
        "schema": "eater-fire-station-bindings-v1",
        "runtimeBindingsSha256": hashlib.sha256(raw).hexdigest(),
        "channelTargetBindingsSha256": hashlib.sha256(channel_raw).hexdigest(),
        "runtimeDeclaration": f"0x{INTERACTION_DECLARATION:08X}",
        "runtimeKind": f"0x{INTERACTION_KIND:08X}",
        "dynamicAuthorityInterface": f"0x{DYNAMIC_AUTHORITY_INTERFACE:08X}",
        "dynamicAuthorityLink": f"0x{DYNAMIC_AUTHORITY_LINK:08X}",
        "dynamicAuthorityOffset": f"0x{DYNAMIC_AUTHORITY_OFFSET:X}",
        "dynamicAuthorityMethodSlots": list(DYNAMIC_AUTHORITY_METHOD_SLOTS),
        "inheritedInteractionLink": f"0x{INHERITED_INTERACTION_LINK:08X}",
        "inheritedInteractionSubtype": f"0x{INHERITED_INTERACTION_SUBTYPE:08X}",
        "bindings": [{
            key: (f"0x{value:08X}" if key not in ("slot", "offset", "lane", "channelSlot", "fireSlot") else
                  (f"0x{value:X}" if key == "offset" else value))
            for key, value in binding.items()
        } for binding in bindings],
    }


def render_header(data: dict) -> str:
    lines = [
        "// Generated by tools/coo/extract_eater_station_bindings.py. Do not edit by hand.",
        "#pragma once", "#include <cstddef>", "#include <cstdint>",
        '#include "carry_bindings.h"',
        "namespace dawn::state::activity::eater_of_worlds {",
        "inline constexpr std::uint32_t kStationInteractionKind=0x80804FB2U;",
        "inline constexpr std::uint32_t kStationDynamicAuthorityInterface=0x80809AE3U;",
        "inline constexpr std::uint32_t kStationDynamicAuthorityLink=0x80C23296U;",
        "inline constexpr std::uint64_t kStationDynamicAuthorityOffset=0x648ULL;",
        "struct StationBinding { coo::Asset source; std::uint32_t entity,config; std::uint64_t offset; std::uint8_t lane; coo::Asset fire,channel; };",
        "struct StationReceipt { coo::Generation generation{}; coo::Asset source{}; std::uintptr_t sourcePointer{}; std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},component{UINT32_MAX}; std::int32_t requested{},consumedBefore{},consumedAfter{}; CraniumReceipt cranium{}; };",
        "inline constexpr StationBinding kStationBindings[]{",
    ]
    for value in data["bindings"]:
        lines.append(
            f"    {{{{{value['registry']}U,{value['source']}U,4,{value['slot']}}},"
            f"{value['entity']}U,{value['config']}U,{value['offset']}ULL,{value['lane']},"
            f"{{{value['registry']}U,{value['fireSource']}U,4,{value['fireSlot']}}},"
            f"{{{value['registry']}U,{value['channelSource']}U,24,{value['channelSlot']}}}}},")
    lines += [
        "};",
        "constexpr std::size_t station_index(coo::Asset source) noexcept {",
        "    for(std::size_t i=0;i<std::size(kStationBindings);++i) if(kStationBindings[i].source==source) return i;",
        "    return std::size(kStationBindings);",
        "}",
        "} // namespace dawn::state::activity::eater_of_worlds", "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = recover()
    evidence = json.dumps(data, indent=2) + "\n"
    header = render_header(data)
    if args.check:
        if not EVIDENCE.is_file() or EVIDENCE.read_text() != evidence:
            raise SystemExit(f"stale generated evidence: {EVIDENCE}")
        if not HEADER.is_file() or HEADER.read_text() != header:
            raise SystemExit(f"stale generated header: {HEADER}")
        print(f"PASS: {len(data['bindings'])} exact Eater fire-station bindings")
        return
    EVIDENCE.write_text(evidence)
    HEADER.write_text(header)
    print(f"wrote {EVIDENCE} and {HEADER}")


if __name__ == "__main__":
    main()
