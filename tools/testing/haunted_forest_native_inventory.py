"""Extract installed Haunted Forest native registry and slot evidence.

This is a read-only structural extractor.  It uses the checked-in package reader and mirrors the
validated C++ table readers; it does not inspect gameplay behavior or a running game.  Raw tag
fixtures, when enabled by this extraction, are written only below the output file's ``fixtures``
directory.  Package-reader key material is never copied into this report.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[2]
SCENARIO_TAG = 0x81550015
TARGET_BUBBLE = 13
SCENARIO_CLASS = 0x80809994
BUBBLE_CLASS = 0x8080924D
STATE_CLASS = 0x8080924F
ENTRY_CLASS = 0x8080925B
REGISTRY_CLASS = 0x8080925E
OBJECT_CLASS = 0x80809462
INDIRECT_CLASS = 0x80809468
REDIRECT_CLASS = 0x80809B14
DESCRIPTOR_CLASS = 0x80809C36
DESCRIPTOR_MARK = 0x70
ABSENT_SCHEMA = 0xFFFFFFFF
TAG_LOW = 0x80800000
TAG_HIGH = 0x82000000
MAX_ARRAY_COUNT = 300000
DIRECTIVE_BANK_TAG = 0x81550359
DIRECTIVE_BANK_CLASS = 0x80804F72
DIRECTIVE_RECORD_CLASS = 0x80804F74
DIRECTIVE_VALUE_CLASS = 0x80804F76
LOCALIZED_CONTAINER_CLASS = 0x80809A88
LOCALIZED_HASH_CLASS = 0x80800070
LOCALIZED_TEXT_CLASS = 0x80809A8A
LOCALIZED_PART_CLASS = 0x80809A90
LOCALIZED_COMBO_CLASS = 0x80809A8E
LOCALIZED_SENTINEL = 0x811C9DC5


class ExtractionError(RuntimeError):
    """A required installed structure could not be read or validated."""


def u8(data: bytes, offset: int) -> int:
    return struct.unpack_from("<B", data, offset)[0]


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def i64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<q", data, offset)[0]


def tag_text(value: int) -> str:
    return f"{value:08X}"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def array_json(array: dict) -> dict:
    return {"offset": array["offset"], "count": array["count"],
            "data": array["data"], "class": tag_text(array["class"])}


def checked_read(data: bytes, offset: int, size: int, label: str):
    if offset < 0 or offset + size > len(data):
        raise ExtractionError(f"{label} at 0x{offset:X} exceeds {len(data)} bytes")


def array_at(data: bytes, descriptor_offset: int, label: str):
    """Resolve the exact count/self-relative/header form used by find_array_at."""
    checked_read(data, descriptor_offset, 16, label)
    count = u64(data, descriptor_offset)
    relative = i64(data, descriptor_offset + 8)
    if count == 0 or count > MAX_ARRAY_COUNT:
        raise ExtractionError(f"{label} has invalid count {count}")
    # The C++ resolver applies the signed displacement from the end of the descriptor's
    # eight-byte count/relative pair (descriptor_offset + 8).
    header = descriptor_offset + 8 + relative
    checked_read(data, header - 4, 4, f"{label} marker")
    checked_read(data, header, 16, f"{label} header")
    if u32(data, header - 4) >> 16 != 0x8080:
        raise ExtractionError(f"{label} marker is not a native tag")
    if u64(data, header) != count:
        raise ExtractionError(f"{label} header count mismatch")
    element_class = u32(data, header + 8)
    if element_class >> 16 != 0x8080:
        raise ExtractionError(f"{label} element class is not a native tag")
    return {"offset": descriptor_offset, "count": count,
            "data": header + 16, "class": element_class}


def array_values(data: bytes, array: dict, stride: int, label: str):
    total = array["count"] * stride
    checked_read(data, array["data"], total, label)
    return [array["data"] + index * stride for index in range(array["count"])]


def hex_schema(value: int) -> str:
    return tag_text(value)


def ascii_names(data: bytes) -> list[str]:
    names = []
    seen = set()
    for match in re.finditer(rb"[ -~]{7,}", data):
        name = match.group().decode("ascii")
        if name not in seen:
            names.append(name)
            seen.add(name)
    return names


def descriptor_arrays(data: bytes) -> list[dict]:
    """Find structurally valid array descriptors using the C++ generic scan shape."""
    result = []
    seen = set()
    for offset in range(0, max(0, len(data) - 15), 8):
        try:
            found = array_at(data, offset, f"array descriptor 0x{offset:X}")
        except (ExtractionError, struct.error):
            continue
        key = (found["offset"], found["count"], found["data"], found["class"])
        if key in seen:
            continue
        seen.add(key)
        result.append(array_json(found))
    return result


def possible_tag_refs(data: bytes, reader, source: str) -> list[dict]:
    refs = []
    seen = set()
    for offset in range(0, len(data) - 3, 4):
        value = u32(data, offset)
        if not TAG_LOW <= value < TAG_HIGH or value in seen:
            continue
        try:
            cls, _ = reader.read(value)
        except (AssertionError, FileNotFoundError):
            continue
        refs.append({"offset": offset, "tag": tag_text(value), "class": tag_text(cls)})
        seen.add(value)
    return refs


def fixture(reader, fixture_dir: Path, tag: int, expected: int | None = None):
    cls, data = reader.read(tag)
    if expected is not None and cls != expected:
        raise ExtractionError(
            f"{tag_text(tag)} has class {tag_text(cls)}, expected {tag_text(expected)}")
    fixture_dir.mkdir(parents=True, exist_ok=True)
    (fixture_dir / f"{tag_text(tag)}.{tag_text(cls)}.bin").write_bytes(data)
    return cls, data


def descriptor_record(reader, fixture_dir: Path, source_tag: int, blob: bytes,
                      offset: int, registry_key: int, chain: list[dict]) -> dict:
    own = u32(blob, offset)
    component = u32(blob, offset + 4)
    mark = u32(blob, offset + 8)
    key = u32(blob, offset + 48)
    slot_type = u16(blob, offset + 52)
    slot_index = u16(blob, offset + 54)
    sense = u32(blob, offset + 68)
    auth = u32(blob, offset + 72)
    record = {
        "definition": tag_text(source_tag),
        "descriptor_offset": offset,
        "sourceOffset": offset,
        "own_tag": tag_text(own),
        "component": tag_text(component),
        "type": slot_type,
        "slot": slot_index,
        "sense": hex_schema(sense),
        "auth": hex_schema(auth),
        "registry_key": tag_text(key),
        "sha256": sha256(blob),
        "names": ascii_names(blob),
        "arrays": descriptor_arrays(blob),
        "possible_tag_refs": possible_tag_refs(blob, reader, tag_text(source_tag)),
        "chain": chain,
    }
    return record


def localized_text(reader, fixture_dir: Path, container: int, wanted: int) -> dict:
    """Resolve one localized hash with its authored container, not a global catalog."""
    if wanted == LOCALIZED_SENTINEL:
        return {"text": None, "text_status": "sentinel"}
    try:
        container_class, data = fixture(reader, fixture_dir, container)
    except (FileNotFoundError, KeyError, OSError, ValueError) as exc:
        return {"text": None, "text_status": "missing",
                "text_reason": f"container read failed: {type(exc).__name__}"}
    if not data or container_class != LOCALIZED_CONTAINER_CLASS:
        return {"text": None, "text_status": "missing",
                "text_reason": "container class mismatch"}

    hashes = array_at(data, 8, f"localized {tag_text(container)} hashes")
    if hashes["class"] != LOCALIZED_HASH_CLASS:
        raise ExtractionError(
            f"localized {tag_text(container)} hash class is {tag_text(hashes['class'])}")
    index = next((i for i, position in enumerate(
        array_values(data, hashes, 4, "localized hashes"))
        if u32(data, position) == wanted), None)
    if index is None:
        return {"text": None, "text_status": "missing", "text_reason": "hash absent"}

    checked_read(data, 24, 4, f"localized {tag_text(container)} english handle")
    english = u32(data, 24)
    try:
        text_class, text = fixture(reader, fixture_dir, english)
    except (FileNotFoundError, KeyError, OSError, ValueError) as exc:
        return {"text": None, "text_status": "missing",
                "text_reason": f"text read failed: {type(exc).__name__}"}
    if text_class != LOCALIZED_TEXT_CLASS:
        return {"text": None, "text_status": "missing",
                "text_reason": "text class mismatch"}

    parts = array_at(text, 8, f"localized {tag_text(english)} parts")
    combos = array_at(text, 72, f"localized {tag_text(english)} combos")
    if parts["class"] != LOCALIZED_PART_CLASS:
        raise ExtractionError(
            f"localized {tag_text(english)} part class is {tag_text(parts['class'])}")
    if combos["class"] != LOCALIZED_COMBO_CLASS:
        raise ExtractionError(
            f"localized {tag_text(english)} combo class is {tag_text(combos['class'])}")
    if index >= combos["count"]:
        return {"text": None, "text_status": "missing", "text_reason": "combo absent"}

    combo = combos["data"] + index * 16
    checked_read(text, combo, 16, "localized combo")
    first = combo + i64(text, combo)
    count = i64(text, combo + 8)
    if not 0 < count <= 64 or first < parts["data"]:
        raise ExtractionError("localized combo has invalid part span")
    if (first - parts["data"]) % 32 != 0:
        raise ExtractionError("localized combo is not part-aligned")
    if (first - parts["data"]) // 32 + count > parts["count"]:
        raise ExtractionError("localized combo part span exceeds parts")

    result = []
    for part_index in range(count):
        part = first + part_index * 32
        checked_read(text, part, 32, "localized part")
        characters = part + 8 + i64(text, part + 8)
        size = u16(text, part + 20)
        shift = u16(text, part + 24)
        if not 0 <= characters <= len(text) or size > len(text) - characters:
            raise ExtractionError("localized characters exceed text blob")
        decoded = text[characters:characters + size].decode("utf-8")
        result.append("".join(chr(ord(character) + shift) for character in decoded))
    return {"text": "".join(result).split("\0")[0], "text_status": "resolved"}


def directive_banks(reader, fixture_dir: Path) -> list[dict]:
    bank_class, bank = fixture(reader, fixture_dir, DIRECTIVE_BANK_TAG, DIRECTIVE_BANK_CLASS)
    records_array = array_at(bank, 8, "Haunted Forest directive records")
    if records_array["class"] != DIRECTIVE_RECORD_CLASS:
        raise ExtractionError(
            f"directive record class is {tag_text(records_array['class'])}")
    if records_array["count"] > 64:
        raise ExtractionError(f"directive record count is {records_array['count']}")

    records = []
    for index, offset in enumerate(array_values(bank, records_array, 40,
                                                "directive records")):
        checked_read(bank, offset, 40, "directive record")
        event = u32(bank, offset)
        domain = u32(bank, offset + 4)
        enabled = u64(bank, offset + 8)
        values_array = array_at(bank, offset + 16, "directive record values")
        if values_array["class"] != DIRECTIVE_VALUE_CLASS:
            raise ExtractionError(
                f"directive value class is {tag_text(values_array['class'])}")
        if values_array["count"] > 128:
            raise ExtractionError(f"directive value count is {values_array['count']}")
        values = []
        for value_offset in array_values(bank, values_array, 36, "directive values"):
            checked_read(bank, value_offset, 36, "directive value")
            typed = []
            for pair_offset in range(0, 8, 2):
                container = u32(bank, value_offset + pair_offset * 4)
                wanted = u32(bank, value_offset + (pair_offset + 1) * 4)
                localized = localized_text(reader, fixture_dir, container, wanted)
                typed.append({"container": tag_text(container), "hash": tag_text(wanted),
                              **localized})
            values.append({"typed_hashes": typed, "flags_raw": u32(bank, value_offset + 32)})
        records.append({"index": index, "event": tag_text(event), "domain": tag_text(domain),
                        "enabled_raw": enabled, "values": values})
    return [{"bank": tag_text(DIRECTIVE_BANK_TAG), "class": tag_text(bank_class),
             "records": records}]


def local_tactical_and_rule_references(blob: bytes, registry_key: int,
                                       declared_slots: list[dict]) -> list[dict]:
    """Copy only aligned local registry references whose type/slot is declared."""
    declared = {row["slot"]: row["type"] for row in declared_slots}
    refs = []
    for offset in range(0, max(0, len(blob) - 7), 4):
        ref, type_slot = struct.unpack_from("<2I", blob, offset)
        slot_type = type_slot & 0xFFFF
        slot = type_slot >> 16
        if ref != registry_key or slot_type not in (3, 66):
            continue
        if declared.get(slot) != slot_type:
            continue
        refs.append({"offset": offset, "type": slot_type, "slot": slot})
    return refs


def scan_descriptors(reader, fixture_dir: Path, source_tag: int, blob: bytes,
                     registry_key: int) -> list[dict]:
    if len(blob) < 128:
        return []
    records = []
    for offset in range(0, len(blob) - 128 + 1, 4):
        # These are the four required predicates from visit_slot_descriptors, including the
        # schema/class shape checks.  No candidate is admitted on a partial match.
        if (u32(blob, offset) != source_tag or u32(blob, offset + 8) != DESCRIPTOR_MARK
                or u32(blob, offset + 48) != registry_key):
            continue
        component = u32(blob, offset + 4)
        sense = u32(blob, offset + 68)
        auth = u32(blob, offset + 72)
        if component >> 16 != 0x8080:
            continue
        if sense != ABSENT_SCHEMA and sense >> 16 != 0x8080:
            continue
        if auth != ABSENT_SCHEMA and auth >> 16 != 0x8080:
            continue
        records.append(descriptor_record(reader, fixture_dir, source_tag, blob, offset,
                                         registry_key, []))
    return records


def resolve_handle(reader, fixture_dir: Path, handle: int, registry_key: int):
    chain = []
    seen = set()
    current = handle
    while True:
        if not TAG_LOW <= current < TAG_HIGH:
            return {"handle": tag_text(handle), "status": "unresolved",
                    "chain": chain, "unknown": f"next tag {current:08X} is outside native range"}
        if current in seen:
            return {"handle": tag_text(handle), "status": "cycle",
                    "chain": chain, "unknown": "cycle detected"}
        seen.add(current)
        try:
            cls, data = fixture(reader, fixture_dir, current)
        except (AssertionError, FileNotFoundError) as exc:
            return {"handle": tag_text(handle), "status": "unresolved",
                    "chain": chain, "unknown": f"read failed: {type(exc).__name__}: {exc}"}
        chain.append({"tag": tag_text(current), "class": tag_text(cls), "sha256": sha256(data)})
        if cls == DESCRIPTOR_CLASS:
            records = scan_descriptors(reader, fixture_dir, current, data, registry_key)
            if not records:
                return {"handle": tag_text(handle), "status": "unresolved",
                        "chain": chain, "unknown": "no qualifying descriptor"}
            for record in records:
                record["chain"] = chain
            return {"handle": tag_text(handle), "status": "resolved", "chain": chain,
                    "descriptors": records}
        if cls == INDIRECT_CLASS:
            try:
                handles = array_at(data, 16, f"indirect {tag_text(current)} handles")
                positions = array_values(data, handles, 4, "indirect handles")
                if not positions:
                    raise ExtractionError("indirect handle array is empty")
                current = u32(data, positions[0])
            except (ExtractionError, struct.error) as exc:
                return {"handle": tag_text(handle), "status": "unresolved",
                        "chain": chain, "unknown": str(exc)}
            continue
        if cls == REDIRECT_CLASS:
            try:
                current = u32(data, 12)
            except struct.error as exc:
                return {"handle": tag_text(handle), "status": "unresolved",
                        "chain": chain, "unknown": str(exc)}
            continue
        return {"handle": tag_text(handle), "status": "unresolved", "chain": chain,
                "unknown": f"unsupported chain class {tag_text(cls)}"}


def declared_slots(data: bytes, slots: dict, label: str) -> list[dict]:
    positions = array_values(data, slots, 8, label)
    rows = []
    for ordinal, offset in enumerate(positions):
        rows.append({"slot": ordinal, "type": u32(data, offset),
                     "name_hash": tag_text(u32(data, offset + 4))})
    return rows


def object_inventory(reader, fixture_dir: Path, object_tag: int) -> dict:
    cls, data = fixture(reader, fixture_dir, object_tag, OBJECT_CLASS)
    key = u32(data, 12)
    result = {"tag": tag_text(object_tag), "class": tag_text(cls), "registry_key": tag_text(key),
              "sha256": sha256(data), "declared_slots": [], "resolved_slots": [],
              "handles": [], "unresolved": [], "discrepancies": []}
    try:
        slots = array_at(data, 32, f"object {tag_text(object_tag)} slots")
        result["declared_slots"] = declared_slots(data, slots, "object slots")
    except (ExtractionError, struct.error) as exc:
        result["unresolved"].append({"status": "unknown", "kind": "declared_slots", "reason": str(exc)})

    handle_rows = []
    try:
        bubbles = array_at(data, 56, f"object {tag_text(object_tag)} bubbles")
        for position in array_values(data, bubbles, 24, "object bubbles"):
            bubble_index = i32(data, position)
            if bubble_index not in (TARGET_BUBBLE, -1):
                continue
            try:
                handles = array_at(data, position + 8,
                                   f"object {tag_text(object_tag)} bubble {bubble_index} handles")
                for handle_position in array_values(data, handles, 4, "object placed handles"):
                    handle_rows.append({"bubble": bubble_index, "handle": u32(data, handle_position)})
            except (ExtractionError, struct.error) as exc:
                result["unresolved"].append({"status": "unknown", "kind": "handles",
                                              "bubble": bubble_index, "reason": str(exc)})
    except (ExtractionError, struct.error) as exc:
        result["unresolved"].append({"status": "unknown", "kind": "bubbles", "reason": str(exc)})

    unique = set()
    for row in handle_rows:
        if row["handle"] in unique:
            continue
        unique.add(row["handle"])
        resolved = resolve_handle(reader, fixture_dir, row["handle"], key)
        resolved["bubble"] = row["bubble"]
        result["handles"].append(resolved)
        if resolved["status"] != "resolved":
            result["unresolved"].append(resolved)
        for descriptor in resolved.get("descriptors", []):
            descriptor["handle"] = tag_text(row["handle"])
            index = descriptor["slot"]
            declared = result["declared_slots"]
            if index >= len(declared):
                descriptor["declared_slot"] = None
                descriptor["status"] = "unmatched_declared_slot"
                result["discrepancies"].append(
                    f"descriptor slot {index} is outside declared slot count {len(declared)}")
            elif declared[index]["type"] != descriptor["type"]:
                descriptor["declared_slot"] = declared[index]
                descriptor["status"] = "declared_type_mismatch"
                result["discrepancies"].append(
                    f"slot {index} type {descriptor['type']} != declared {declared[index]['type']}")
            else:
                descriptor["declared_slot"] = declared[index]
                descriptor["status"] = "resolved"
            if descriptor["type"] in (1, 2):
                _, descriptor_blob = fixture(
                    reader, fixture_dir, int(descriptor["definition"], 16))
                descriptor["local_tactical_and_rule_references"] = (
                    local_tactical_and_rule_references(descriptor_blob, key, declared))
            result["resolved_slots"].append(descriptor)

    resolved_indices = {row["slot"] for row in result["resolved_slots"]
                        if row.get("status") == "resolved"}
    for row in result["declared_slots"]:
        if row["slot"] not in resolved_indices:
            result["unresolved"].append({"status": "unresolved_declared_slot", **row})
    return result


def parse_header(path: Path) -> list[tuple[int, int, int, int, int, int]]:
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    section = text.split("kStartSlots", 1)[-1].split("}};", 1)[0]
    pattern = re.compile(
        r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*0x([0-9A-Fa-f]+)\s*,\s*"
        r"0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*\}")
    return [tuple(int(value, 16) if ordinal >= 2 else int(value)
                  for ordinal, value in enumerate(match.groups()))
            for match in pattern.finditer(section)]


def validate_start_group(report: dict, header_path: Path) -> dict:
    expected = parse_header(header_path)
    validation = {"header": str(header_path.resolve()), "expected_rows": len(expected),
                  "actual_rows": 0, "discrepancies": []}
    start_objects = [obj for registry in report["registries"] for obj in registry["objects"]
                     if obj["registry_key"] == "34D23982"]
    if len(start_objects) != 1:
        validation["discrepancies"].append(
            f"expected exactly one start group object, found {len(start_objects)}")
        report["validation"] = validation
        return validation
    actual = start_objects[0]["resolved_slots"]
    validation["actual_rows"] = len(actual)
    actual_map = {}
    for row in actual:
        if row.get("status") != "resolved":
            continue
        key = row["slot"]
        value = (row["slot"], row["type"], int(row["component"], 16),
                 int(row["sense"], 16), int(row["auth"], 16), int(row["definition"], 16))
        if key in actual_map:
            validation["discrepancies"].append(f"duplicate resolved start slot {key}")
        actual_map[key] = value
    expected_map = {row[0]: row for row in expected}
    for slot, row in expected_map.items():
        if actual_map.get(slot) != row:
            validation["discrepancies"].append(
                f"start slot {slot}: expected {row}, actual {actual_map.get(slot)}")
    for slot in sorted(set(actual_map) - set(expected_map)):
        validation["discrepancies"].append(f"unexpected resolved start slot {slot}: {actual_map[slot]}")
    if len(expected) != 126:
        validation["discrepancies"].append(f"header defines {len(expected)} rows, expected 126")
    validation["checks"] = {
        "row_30_definition": actual_map.get(30, (None,) * 6)[5] == 0x815500A3,
        "row_109_definition": actual_map.get(109, (None,) * 6)[5] == 0x81550170,
        "row_98_definition": actual_map.get(98, (None,) * 6)[5] == 0x8155015B,
        "exact_126_rows": len(expected) == 126 and len(actual) == 126,
    }
    report["validation"] = validation
    return validation


def extract(reader, output: Path, header_path: Path) -> dict:
    fixture_dir = output.parent / "fixtures"
    scenario_cls, scenario = fixture(reader, fixture_dir, SCENARIO_TAG, SCENARIO_CLASS)
    bubbles = array_at(scenario, 80, "scenario bubbles")
    if bubbles["class"] != BUBBLE_CLASS:
        raise ExtractionError(f"scenario bubble class is {tag_text(bubbles['class'])}")
    if TARGET_BUBBLE >= bubbles["count"]:
        raise ExtractionError(f"scenario has {bubbles['count']} bubbles, missing {TARGET_BUBBLE}")
    bubble_offset = bubbles["data"] + TARGET_BUBBLE * 24
    checked_read(scenario, bubble_offset, 24, "target bubble")
    states = array_at(scenario, bubble_offset + 8, "target bubble states")
    if states["class"] != STATE_CLASS:
        raise ExtractionError(f"target state class is {tag_text(states['class'])}")

    registry_tags = []
    states_out = []
    for state_index, state_offset in enumerate(array_values(scenario, states, 76, "states")):
        entry_tag = u32(scenario, state_offset + 68)
        entry_cls, entry = fixture(reader, fixture_dir, entry_tag, ENTRY_CLASS)
        registry_tag = u32(entry, 20)
        registry_cls, registry = fixture(reader, fixture_dir, registry_tag, REGISTRY_CLASS)
        state_row = {"ordinal": state_index, "offset": state_offset,
                     "state_hash": tag_text(u32(scenario, state_offset + 4)),
                     "enabled": bool(u8(scenario, state_offset)),
                     "public": bool(u8(scenario, state_offset + 12)),
                     "map_bubble": u32(scenario, state_offset + 28),
                     "entry": tag_text(entry_tag), "entry_index": u32(entry, 16),
                     "registry": tag_text(registry_tag), "registry_sha256": sha256(registry)}
        states_out.append(state_row)
        if registry_tag not in registry_tags:
            registry_tags.append(registry_tag)

    report = {"schema": 1, "scenario": tag_text(SCENARIO_TAG), "scenario_class": tag_text(scenario_cls),
              "bubble": TARGET_BUBBLE, "bubble_hash": tag_text(u32(scenario, bubble_offset)),
              "states": states_out, "registries": [],
              "expected": {"full7_plus_group": True, "start_group_rows": 126},
              "validation": {}, "directive_banks": directive_banks(reader, fixture_dir)}
    for registry_tag in registry_tags:
        _, registry = fixture(reader, fixture_dir, registry_tag, REGISTRY_CLASS)
        registry_row = {"tag": tag_text(registry_tag), "class": tag_text(REGISTRY_CLASS),
                        "sha256": sha256(registry), "arrays": [], "objects": [], "unresolved": []}
        object_tags = []
        for descriptor_offset in (8, 24, 40):
            try:
                objects = array_at(registry, descriptor_offset,
                                   f"registry {tag_text(registry_tag)} array 0x{descriptor_offset:X}")
                registry_row["arrays"].append(array_json(objects))
                if objects["class"] != 0x80809260:
                    registry_row["unresolved"].append({"status": "unknown", "array": objects,
                                                        "reason": "unexpected element class"})
                    continue
                for position in array_values(registry, objects, 4, "registry objects"):
                    object_tag = u32(registry, position)
                    if object_tag not in object_tags:
                        object_tags.append(object_tag)
            except (ExtractionError, struct.error) as exc:
                # Registry arrays may be absent; preserve that fact in the report.
                registry_row["unresolved"].append({"status": "unknown",
                                                    "descriptor_offset": descriptor_offset,
                                                    "reason": str(exc)})
        for object_tag in object_tags:
            registry_row["objects"].append(object_inventory(reader, fixture_dir, object_tag))
        report["registries"].append(registry_row)

    validation = validate_start_group(report, header_path)
    validation["registry_tag_count"] = len(report["registries"])
    validation["object_registry_count"] = sum(
        len(registry["objects"]) for registry in report["registries"])
    validation["nonempty_object_registry_count"] = sum(
        bool(obj["resolved_slots"])
        for registry in report["registries"] for obj in registry["objects"])
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="JSON output path; use build/coo/haunted-forest-20260913")
    parser.add_argument("--header", type=Path,
                        default=ROOT / "Sunrise/src/server/runtime/activity/haunted_forest_registries.h")
    args = parser.parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        coo = ROOT / "tools/coo"
        sys.path.insert(0, str(coo))
        import package_read
        report = extract(package_read, output, args.header.resolve())
    except Exception as exc:
        # In particular, do not catch and continue after package authentication/decryption errors.
        print(f"Extraction stopped: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 2
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    validation = report["validation"]
    objects = sum(len(registry["objects"]) for registry in report["registries"])
    descriptors = sum(len(obj["resolved_slots"]) for registry in report["registries"]
                      for obj in registry["objects"])
    unresolved = sum(len(obj["unresolved"]) for registry in report["registries"]
                     for obj in registry["objects"])
    print(f"{len(report['registries'])} registries; {objects} objects; {descriptors} descriptors; "
          f"{unresolved} unresolved; validation discrepancies={len(validation['discrepancies'])}.")
    return 1 if validation["discrepancies"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
