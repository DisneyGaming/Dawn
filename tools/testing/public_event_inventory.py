"""Re-read Mercury public-event metadata from installed packages, without a live process.

The private package reader/inventory remain in the RE workspace. Output contains
tag identities, descriptor provenance and raw reference facts, not package bytes.
No discovered source/rule or reward variant is made into activation policy.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--re-root", type=Path, default=Path("D:/Sunrise-work"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--header", type=Path, help="emit the pinned C++ registry metadata")
    parser.add_argument("--fixtures", type=Path, help="capture private blobs for the independent production descriptor-reader test")
    args = parser.parse_args()
    sys.path[:0] = [str(args.re_root / "scripts"), str(args.re_root / "mercury_inventory")]
    from pkg import Reader
    from descriptors import descriptors
    from scenario_walk import scenario_objects
    from arrays import find_arrays, resolve_descriptor

    reader = Reader()
    if args.fixtures:
        args.fixtures.mkdir(parents=True, exist_ok=True)

    def capture(tag: int, blob: bytes, cls: int) -> None:
        if args.fixtures:
            (args.fixtures / f"{tag:08X}.bin").write_bytes(struct.pack("<I", cls) + blob)

    def localized(container: int, wanted: int) -> str | None:
        if wanted == 0x811C9DC5:
            return None
        data, cls = reader.read_tag(container)
        if not data or cls != 0x80809A88:
            return None
        count, hashes, hash_class = resolve_descriptor(data, 8)
        assert hash_class == 0x80800070
        index = next((i for i in range(count) if struct.unpack_from("<I", data, hashes + i * 4)[0] == wanted), None)
        if index is None:
            return None
        english = struct.unpack_from("<I", data, 24)[0]
        text, text_class = reader.read_tag(english)
        assert text_class == 0x80809A8A
        part_count, parts, part_class = resolve_descriptor(text, 8)
        combo_count, combos, combo_class = resolve_descriptor(text, 72)
        assert part_class == 0x80809A90 and combo_class == 0x80809A8E and index < combo_count
        combo = combos + index * 16
        first = combo + struct.unpack_from("<q", text, combo)[0]
        n = struct.unpack_from("<q", text, combo + 8)[0]
        assert 0 < n <= 64 and first >= parts and (first - parts) % 32 == 0 and (first - parts) // 32 + n <= part_count
        result = []
        for i in range(n):
            part = first + i * 32
            characters = part + 8 + struct.unpack_from("<q", text, part + 8)[0]
            size = struct.unpack_from("<H", text, part + 20)[0]
            shift = struct.unpack_from("<H", text, part + 24)[0]
            assert 0 <= characters <= len(text) and size <= len(text) - characters
            result.append("".join(chr(ord(c) + shift) for c in text[characters:characters + size].decode("utf-8")))
        return "".join(result).split("\0")[0]
    inventory = json.loads((args.re_root / "mercury_inventory/components.json").read_text())["objects"]
    scenario = 0x80F4696A
    scopes = scenario_objects(scenario)
    labels = {0x80F5BF4E: "rally", 0x80F5E517: "crossroads", 0x80F5E643: "crossroads_geometry"}
    result = {"scenario": f"{scenario:08X}", "registries": [], "unresolved": [
        "event orchestration and exact start inputs", "native rally device channel semantics",
        "directive bank selectors", "source tactical row assignments and request counts",
        "heroic condition aggregation", "reward selection/transactions", "source quiescence and renewal"]}
    for tag, label in labels.items():
        raw, cls = reader.read_tag(tag)
        capture(tag, raw, cls)
        key, rows = descriptors(tag)
        bubbles = [bubble for bubble, tags in scopes.items() if tag in tags]
        assert cls == 0x80809462 and bubbles == [15]
        known = inventory[f"{tag:08X}"]
        assert int(known["regkey"], 16) == key
        names = {s.get("component"): s.get("name") for s in known["slots"] if s.get("component")}
        item = {"name": label, "object": f"{tag:08X}", "registry": f"{key:08X}",
                "bubbles": bubbles, "object_sha256": hashlib.sha256(raw).hexdigest().upper(),
                "declared_slot_count": len(known["slots"]), "descriptor_count": len(rows), "slots": []}
        for index, kind, component, sense, auth, descriptor, definition in rows:
            blob, blob_class = reader.read_tag(definition)
            assert blob_class == 0x80809C36
            capture(definition, blob, blob_class)
            row = {"index": index, "type": kind, "name": names.get(f"{descriptor:08X}"),
                   "component_class": f"{component:08X}", "sense_schema": f"{sense:08X}",
                   "auth_schema": f"{auth:08X}", "redirect": f"{descriptor:08X}", "descriptor": f"{definition:08X}",
                   "definition": f"{definition:08X}", "definition_size": len(blob),
                   "definition_sha256": hashlib.sha256(blob).hexdigest().upper()}
            # Exact typed source offsets, not the old fixed 1984/1992 assumption.
            # Descriptor typed reference is {definition tag,8080948F,body offset,0}.
            if kind == 1:
                typed = []
                for offset in range(0, len(blob) - 15, 4):
                    ref, ty, body, zero = struct.unpack_from("<4I", blob, offset)
                    if ref == definition and ty == 0x8080948F and zero == 0 and 4 <= body < len(blob):
                        if struct.unpack_from("<I", blob, body - 4)[0] == 0x8080948F:
                            typed.append(body)
                row["typed_source_bodies"] = sorted(set(typed))
                refs = []
                for offset in range(0, len(blob) - 7, 4):
                    ref, type_slot = struct.unpack_from("<2I", blob, offset)
                    if ref == key and type_slot & 0xFFFF in (3, 66):
                        refs.append({"offset": offset, "type": type_slot & 0xFFFF, "slot": type_slot >> 16})
                row["local_tactical_and_rule_references"] = refs
            row["array_classes"] = sorted({f"{array[2]:08X}" for array in find_arrays(blob).values() if array[2]})
            item["slots"].append(row)
        result["registries"].append(item)
    # Bounded reference census reports presence only. It never interprets a
    # name, class, count, or plausible float as a server transition contract.
    seen, frontier, classes, patterns = set(), [(tag, 0) for tag in labels], Counter(), []
    leaves = []
    for tag, level in frontier:
        if tag in seen or level > 4:
            continue
        seen.add(tag)
        try:
            blob, cls = reader.read_tag(tag)
        except (OSError, ValueError, KeyError, IndexError):
            continue
        if blob is None or cls is None:
            continue
        classes[f"{cls:08X}"] += 1
        if cls in (0x80804F72, 0x808099D6, 0x80809BBB, 0x80809C54):
            leaves.append({"tag": f"{tag:08X}", "class": f"{cls:08X}", "size": len(blob), "depth": level})
        if cls == 0x8080941E:
            patterns.append(f"{tag:08X}")
        if cls not in (0x80809462, 0x80809468, 0x80809B14, 0x80809C36, 0x8080941E):
            continue
        for offset in range(0, len(blob) - 3, 4):
            value = struct.unpack_from("<I", blob, offset)[0]
            if 0x80810000 < value < 0x82000000:
                frontier.append((value, level + 1))
    result["bounded_reference_census"] = {"depth": 4, "classes": dict(classes), "behavior_patterns": patterns, "leads": leaves}
    result["directive_banks"] = []
    for bank in (0x80FD3320, 0x80F5E35B):
        blob, cls = reader.read_tag(bank)
        capture(bank, blob, cls)
        assert cls == 0x80804F72
        count, start, element = resolve_descriptor(blob, 8)
        assert element == 0x80804F74 and count <= 64
        records = []
        for index in range(count):
            offset = start + index * 40
            key, domain, enabled = struct.unpack_from("<2IQ", blob, offset)
            value_count, values, value_class = resolve_descriptor(blob, offset + 16)
            assert value_class == 0x80804F76 and value_count <= 128
            typed = [struct.unpack_from("<9I", blob, values + v * 36) for v in range(value_count)]
            records.append({"index": index, "event": f"{key:08X}", "domain": f"{domain:08X}", "enabled_raw": enabled,
                            "values": [{"typed_hashes": [{"container": f"{x[h]:08X}", "hash": f"{x[h+1]:08X}",
                                                          "text": localized(x[h], x[h+1])}
                                        for h in range(0, 8, 2)], "flags_raw": x[8]} for x in typed]})
        result["directive_banks"].append({"bank": f"{bank:08X}", "records": records})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.header:
        lines = ["#pragma once", '#include "authored_registry.h"', "#include <array>", "",
                 "namespace sunrise::state::activity::coo::mercury::public_events {",
                 "// Package-derived metadata only. Generated by tools/testing/public_event_inventory.py.",
                 "// Recovered descriptors are not activation policy; no raw spawner requests are exposed.",
                 "namespace authored = state::activity::coo::registry;"]
        for record in result["registries"]:
            key = record["registry"]
            lines.append(f'inline constexpr std::array<authored::Slot,{record["descriptor_count"]}> kSlots_{key}{{{{')
            for row in record["slots"]:
                fields = f'{row["index"]},{row["type"]},0x{row["component_class"]},0x{row["sense_schema"]},0x{row["auth_schema"]},0x{row["descriptor"]}'
                lines.append(f'    {{{fields}}}, // {row["name"] or "unresolved name"}')
            lines.append("}};")
        lines.append("inline constexpr std::array<authored::Definition,3> kRegistries{{")
        for record in result["registries"]:
            lines.append('    {"mercury_freeroam",0x80F4696A,0x%s,0x%s,0xA83A9175,15,kSlots_%s},' %
                         (record["registry"], record["object"], record["registry"]))
        lines.extend(["}};", "} // namespace sunrise::state::activity::coo::mercury::public_events", ""])
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text("\n".join(lines), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "registries": [(r["registry"], r["descriptor_count"]) for r in result["registries"]],
                      "behavior_patterns": patterns}))


if __name__ == "__main__":
    main()
