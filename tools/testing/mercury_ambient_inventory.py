"""Recover ordinary Lighthouse ambient identities from installed package bytes.

This is read-only package research. It does not activate every reachable group.
Usage: python mercury_ambient_inventory.py --reader-dir D:/Dawn-work/scripts
       --inventory D:/Dawn-work/mercury_inventory/components.json --output DIR
The inventory selects named candidates; every emitted descriptor, scoped rule,
monitor target and tactical row count is checked against package bytes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def scoped(data, off):
    key, kind, index = struct.unpack_from("<IBxH", data, off)
    return {"registry": f"{key:08X}", "type": kind, "slot": index}


def typed(data, marker):
    matches = [i + 4 for i in range(0, len(data) - 4, 4) if u32(data, i) == marker]
    if len(matches) != 1:
        raise ValueError(f"expected one {marker:08X} body, found {matches}")
    return matches[0]


def extract(reader, inventory):
    result = []
    for tag, obj in inventory["objects"].items():
        sources = [c for c in obj["components"] if c.get("slot_type") == 1]
        if obj["bubbles"] != [15] or not sources:
            continue
        names = [c.get("resource_name") or "" for c in sources]
        if not all(n.startswith(("pf_lighthouse_ca_", "pf_lighthouse_vx_")) for n in names):
            continue
        group_name = names[0].split("._squad[")[0]
        key = int(obj["regkey"], 16)
        raw, cls = reader.read_tag(int(tag, 16))
        assert cls == 0x80809462 and u32(raw, 12) == key
        group = {"name": group_name, "object": tag, "registry": obj["regkey"],
                 "scenario": "80F4696A", "bubble": 15, "bubble_hash": "A83A9175",
                 "hotspot": "_hotspot" in group_name, "descriptors": [], "sources": [],
                 "retail_initial_requests": None, "retail_replacement_delay": None,
                 "retail_activation_and_exclusion_policy": None}
        for component in obj["components"]:
            resource = int(component["resource"], 16)
            data, cls = reader.read_tag(resource)
            assert cls == 0x80809C36
            # Descriptor qualification mirrors the production catalog traversal:
            # exact definition tag, component class, descriptor width and key.
            descriptors = []
            for off in range(0, len(data) - 76, 4):
                if (u32(data, off) == resource and u32(data, off + 4) == int(component["kind"], 16)
                        and u32(data, off + 8) == 0x70 and u32(data, off + 48) == key):
                    descriptors.append(off)
            if not descriptors:
                continue  # Non-networked trigger/firing-area definitions.
            assert len(descriptors) == 1
            off = descriptors[0]
            kind, index = struct.unpack_from("<HH", data, off + 52)
            descriptor = {"slot": index, "type": kind, "component": component["kind"],
                          "sense": f"{u32(data, off + 68):08X}",
                          "authority": f"{u32(data, off + 72):08X}",
                          "definition": component["resource"], "name": component.get("resource_name"),
                          "sha256": hashlib.sha256(data).hexdigest().upper()}
            group["descriptors"].append(descriptor)
            if kind == 1:
                body = off
                assert u32(data, body - 4) == 0x8080948F and scoped(data, body + 48) == {
                    "registry": obj["regkey"], "type": 1, "slot": index}
                primary, fallback = scoped(data, body + 0x98), scoped(data, body + 0xA0)
                assert primary["registry"] == fallback["registry"] == obj["regkey"]
                assert primary["type"] == fallback["type"] == 66
                assert u32(data, body + 0x84) == 0xA83A9175
                group["sources"].append({"slot": index, "definition": component["resource"],
                    "name": descriptor["name"], "typed_offset": body,
                    "primary_rule": primary["slot"], "fallback_rule": fallback["slot"],
                    "retail_tactical_row": None})
            if kind == 3:
                marker = typed(data, 0x80807F00) - 4
                # Native array prefix: count64 at marker-8, element class32,
                # padding32; these are native tactical rows, not actor counts.
                rows = u32(data, marker - 8)
                assert 0 < rows <= 24 and u32(data, marker - 4) == 0
                group["tactical"] = {"slot": index, "definition": component["resource"],
                                     "rows": rows, "source_row_assignment": None}
            if kind == 30:
                body = off
                assert u32(data, body - 4) == 0x80809530
                target = scoped(data, body + 0x58)
                assert target["registry"] == obj["regkey"] and target["type"] == 60
                authored = obj["slots"][target["slot"]]
                assert authored["type"] == 60
                group["monitor"] = {"slot": index, "definition": component["resource"],
                                    "target": target, "target_name": authored["name"]}
        group["descriptors"].sort(key=lambda d: d["slot"])
        group["sources"].sort(key=lambda s: s["slot"])
        assert "monitor" in group and "tactical" in group
        assert len(group["sources"]) == len(sources)
        for source in group["sources"]:
            for field in ("primary_rule", "fallback_rule"):
                rule = next(d for d in group["descriptors"] if d["slot"] == source[field])
                assert rule["type"] == 66
        result.append(group)
    return result


def header(groups):
    lines = ['#pragma once', '#include "authored_registry.h"', '#include <array>', '',
        '// Generated by tools/testing/mercury_ambient_inventory.py from package bytes.',
        '// Reachable candidates only: no admission, initial counts or retail scheduling implied.',
        'namespace dawn::state::activity::coo::mercury::ambient {',
        'struct Source final { std::uint16_t slot, primaryRule, fallbackRule; std::uint32_t definition; };',
        'struct Group final { std::string_view name; const registry::Definition* registry;',
        '    std::span<const Source> sources; std::uint16_t monitorSlot, triggerSlot, tacticalSlot;',
        '    std::uint8_t tacticalRows; bool hotspot; };', '']
    for group in groups:
        key = group['registry']
        lines.append(f'inline constexpr std::array<registry::Slot,{len(group["descriptors"])}> kSlots_{key}{{{{')
        for d in group['descriptors']:
            lines.append(f'    {{{d["slot"]},{d["type"]},0x{d["component"]},0x{d["sense"]},0x{d["authority"]},0x{d["definition"]}}},')
        lines.extend(['}};', f'inline constexpr std::array<Source,{len(group["sources"])}> kSources_{key}{{{{'])
        for s in group['sources']:
            lines.append(f'    {{{s["slot"]},{s["primary_rule"]},{s["fallback_rule"]},0x{s["definition"]}}},')
        lines.extend(['}};', ''])
    lines.append(f'inline constexpr std::array<registry::Definition,{len(groups)}> kRegistries{{{{')
    for g in groups:
        lines.append(f'    {{"mercury_freeroam",0x80F4696A,0x{g["registry"]},0x{g["object"]},0xA83A9175,15,kSlots_{g["registry"]}}},')
    lines.extend(['}};', f'inline constexpr std::array<Group,{len(groups)}> kGroups{{{{'])
    for i, g in enumerate(groups):
        lines.append(f'    {{"{g["name"]}",&kRegistries[{i}],kSources_{g["registry"]},'
                     f'{g["monitor"]["slot"]},{g["monitor"]["target"]["slot"]},{g["tactical"]["slot"]},'
                     f'{g["tactical"]["rows"]},{str(g["hotspot"]).lower()}}},')
    lines.extend(['}};', '[[nodiscard]] constexpr const Group* find(std::uint32_t key) noexcept {',
                  '    for(const auto& group:kGroups) if(group.registry->key==key) return &group;',
                  '    return nullptr;', '}', '} // namespace dawn::state::activity::coo::mercury::ambient', ''])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', required=True, type=Path)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir))
    from pkg import Reader
    groups = extract(Reader(), json.loads(args.inventory.read_text()))
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {'scope': 'ordinary Lighthouse ambient candidates; policy not inferred', 'groups': groups}
    (args.output / 'mercury_ambient_inventory.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (args.output / 'mercury_ambient_catalog.h').write_text(header(groups))
    print(f'{len(groups)} registries, {sum(len(g["sources"]) for g in groups)} sources, '
          f'{sum(len(g["descriptors"]) for g in groups)} descriptors; '
          f'{sum(g["hotspot"] for g in groups)} hotspot variants')


if __name__ == '__main__':
    main()
