"""Rebuild the Eater of Worlds research inventory from build-86657 inputs.

This is a read-only extractor.  It reads the generated Sunrise build-data cache,
the installed package set through tools/coo/package_read.py, and (when present)
the user-supplied DECOMP_SHARE behavior corpus.  It writes no package bytes and
does not print or persist package key material.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools" / "coo"))
import package_read as packages  # noqa: E402


SCENARIO = 0x80B49E7A
PUBLIC_ACTIVITY_TABLE = 0x81327CF0
DISPLAY_ACTIVITY_TABLE = 0x81327D35
METADATA_GLOBALS = ROOT / "build" / "coo" / "native-r13-installed-fixtures" / "metadata" / "globals.bin"
CACHE = ROOT / "Sunrise" / "cache" / "build_data.bin"
DECOMP = (
    ROOT
    / "build"
    / "decomp-share-20260913"
    / "DECOMP_SHARE"
    / "SCRIPT"
    / "extracted_package_programs"
)

# The cache reader asserts this exact version and byte layout.  If the producer
# changes either, regenerate these constants from tools/coo/cache_layout.vcxproj.
CACHE_VERSION = 54
HEADER_SIZE = 201
COUNT_OFFSET = 92
DOMAIN_SIZES = [
    140, 12, 76, 68, 294, 8, 8, 2, 8, 16, 220, 928, 4,
    1366, 30730, 44, 72, 20, 56, 12, 64, 48, 28,
]
SCENARIO_DOMAIN = 13
ROSTER_DOMAIN = 14
HASH_NAME_DOMAIN = 18


def u16(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def u64(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", blob, offset)[0]


def i64(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<q", blob, offset)[0]


def hx(value: int) -> str:
    return f"0x{value:08X}"


def array(blob: bytes, descriptor: int, stride: int, element_class: int | None = None) -> list[int]:
    count = u64(blob, descriptor)
    if count == 0:
        return []
    header = descriptor + 8 + i64(blob, descriptor + 8)
    if not (0 <= header <= len(blob) - 16) or u64(blob, header) != count:
        raise ValueError(f"invalid array descriptor at {descriptor:#x}")
    if element_class is not None and u32(blob, header + 8) != element_class:
        raise ValueError(f"wrong element class at {descriptor:#x}")
    start = header + 16
    if count > 100_000 or start + count * stride > len(blob):
        raise ValueError(f"array outside blob at {descriptor:#x}")
    return list(range(start, start + count * stride, stride))


def cache_domains(cache: bytes) -> tuple[list[int], list[int]]:
    if cache[:8] != b"SUNRISEB" or u32(cache, 8) != CACHE_VERSION:
        raise ValueError("expected the version-54 Sunrise build-data cache")
    counts = [u32(cache, COUNT_OFFSET + index * 4) for index in range(len(DOMAIN_SIZES))]
    offsets: list[int] = []
    cursor = HEADER_SIZE
    for count, size in zip(counts, DOMAIN_SIZES):
        offsets.append(cursor)
        cursor += count * size
    if cursor != len(cache):
        raise ValueError("cache domain sizes do not consume the complete payload")
    return counts, offsets


def hash_names(cache: bytes, counts: list[int], offsets: list[int]) -> dict[int, str]:
    result: dict[int, str] = {}
    start = offsets[HASH_NAME_DOMAIN]
    for index in range(counts[HASH_NAME_DOMAIN]):
        row = start + index * DOMAIN_SIZES[HASH_NAME_DOMAIN]
        name = cache[row : row + 48].split(b"\0", 1)[0].decode("utf-8", errors="replace")
        result[u32(cache, row + 48)] = name
    return result


def scenario_cache_record(cache: bytes, counts: list[int], offsets: list[int]) -> dict:
    start = offsets[SCENARIO_DOMAIN]
    size = DOMAIN_SIZES[SCENARIO_DOMAIN]
    for index in range(counts[SCENARIO_DOMAIN]):
        row = start + index * size
        if u32(cache, row + 40) != SCENARIO:
            continue
        bubble_count = cache[row + 45]
        package_count = cache[row + 1348]
        return {
            "cacheIndex": index,
            "name": cache[row : row + 40].split(b"\0", 1)[0].decode(),
            "tag": hx(SCENARIO),
            "bubbleCount": bubble_count,
            "rosterGroupCount": cache[row + 47],
            "bubbleGroupCount": cache[row + 49],
            "spawnStem": cache[row + 52 : row + 84].split(b"\0", 1)[0].decode(),
            "bubbleHashes": [hx(u32(cache, row + 148 + 4 * i)) for i in range(bubble_count)],
            "bubbleStateCounts": [cache[row + 404 + i] for i in range(bubble_count)],
            "bubbleMapIndices": [u16(cache, row + 1220 + 2 * i) for i in range(bubble_count)],
            "authoredGroupCounts": [cache[row + 516 + i] for i in range(bubble_count)],
            "packages": [f"0x{u16(cache, row + 1350 + 2 * i):04X}" for i in range(package_count)],
        }
    raise ValueError(f"scenario {SCENARIO:#x} is absent from the cache")


def walk_scenario() -> tuple[dict, list[dict]]:
    scenario_class, blob = packages.read(SCENARIO)
    if scenario_class != 0x80809994:
        raise ValueError("Eater scenario tag resolved to the wrong class")
    bubbles: list[dict] = []
    for bubble_ordinal, bubble in enumerate(array(blob, 80, 24, 0x8080924D)):
        states: list[dict] = []
        for state_ordinal, state in enumerate(array(blob, bubble + 8, 76, 0x8080924F)):
            entry_tag = u32(blob, state + 68)
            entry_class, entry = packages.read(entry_tag)
            if entry_class != 0x8080925B:
                raise ValueError("slice-set entry resolved to the wrong class")
            registry_tag = u32(entry, 20)
            registry_class, registry = packages.read(registry_tag)
            if registry_class != 0x8080925E:
                raise ValueError("object registry resolved to the wrong class")
            objects: list[dict] = []
            for array_ordinal, descriptor in enumerate((8, 24, 40)):
                for element in array(registry, descriptor, 4, 0x80809260):
                    tag = u32(registry, element)
                    cls, obj = packages.read(tag)
                    objects.append(
                        {
                            "tag": hx(tag),
                            "class": hx(cls),
                            "registryKey": hx(u32(obj, 12)),
                            "registryArray": array_ordinal,
                        }
                    )
            states.append(
                {
                    "state": state_ordinal,
                    "enabled": bool(blob[state]),
                    "public": bool(blob[state + 33]),
                    "stateHash": hx(u32(blob, state + 24)),
                    "mapBubbleIndex": u32(blob, state + 28),
                    "entryTag": hx(entry_tag),
                    "sliceSetIndex": u32(entry, 16) * 8,
                    "registryTag": hx(registry_tag),
                    "objects": objects,
                }
            )
        bubbles.append(
            {
                "ordinal": bubble_ordinal,
                "bubbleHash": hx(u32(blob, bubble)),
                "states": states,
            }
        )
    return {
        "tag": hx(SCENARIO),
        "class": hx(scenario_class),
        "packageDefinitionHash": hx(u32(blob, 8)),
        "sha256": hashlib.sha256(blob).hexdigest(),
        "byteLength": len(blob),
    }, bubbles


def public_activities() -> dict:
    table_class, blob = packages.read(PUBLIC_ACTIVITY_TABLE)
    count = u64(blob, 8)
    header = 16 + i64(blob, 16)
    if count != 1170 or u32(blob, header + 8) != 0x808076FC:
        raise ValueError("unexpected public activity table layout")
    start = header + 16
    rows: list[dict] = []
    for ordinal in range(count):
        entry = start + ordinal * 16
        record = entry + 8 + i64(blob, entry + 8)
        if not 0 <= record <= len(blob) - 0xE0:
            raise ValueError(f"public activity record {ordinal} is outside the table")
        name_at = record + 0x68 + i64(blob, record + 0x68)
        if not 0 <= name_at < len(blob):
            continue
        name = blob[name_at:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        if name != "raid_envy_v310":
            continue
        identity = u32(blob, entry)
        if u32(blob, record) != identity:
            raise ValueError(f"activity identity mismatch at ordinal {ordinal}")
        difficulty = u32(blob, record + 0x98)
        difficulty_class, difficulty_blob = packages.read(difficulty)
        rows.append(
            {
                "ordinal": ordinal,
                "identityHash": hx(identity),
                "recordOffset": record,
                "name": name,
                "power": u32(blob, record + 0xB4),
                "gameplaySettingsHash": hx(u32(blob, record + 0xDC)),
                "difficultySettingsTag": hx(difficulty),
                "difficultySettingsClass": hx(difficulty_class),
                "difficultySettingsSha256": hashlib.sha256(difficulty_blob).hexdigest(),
            }
        )
    if [row["ordinal"] for row in rows] != [536, 537, 538]:
        raise ValueError(f"unexpected Eater public activity rows: {rows}")
    return {
        "tableTag": hx(PUBLIC_ACTIVITY_TABLE),
        "tableClass": hx(table_class),
        "tableSha256": hashlib.sha256(blob).hexdigest(),
        "rows": rows,
    }


def activity_display(activities: dict) -> dict:
    """Join public activity ordinals to the installed localized display table."""
    if not METADATA_GLOBALS.exists():
        raise ValueError(f"installed metadata globals fixture is missing: {METADATA_GLOBALS}")
    display_class, display = packages.read(DISPLAY_ACTIVITY_TABLE)
    globals_blob = METADATA_GLOBALS.read_bytes()
    banks_tag = u32(globals_blob, 16 + 72 * 16)
    _, banks = packages.read(banks_tag)
    bank_rows = array(banks, 8, 8)

    def composite_text(container_tag: int, key: int) -> str:
        _, container = packages.read(container_tag)
        _, language = packages.read(u32(container, 24))
        keys = array(container, 8, 4)
        composites = array(language, 72, 16)
        for index, key_row in enumerate(keys):
            if u32(container, key_row) != key:
                continue
            composite = composites[index]
            first_part = composite + i64(language, composite)
            part_count = u64(language, composite + 8)
            result = bytearray()
            for part_index in range(part_count):
                part = first_part + part_index * 32
                start = part + 8 + i64(language, part + 8)
                length, shift = u16(language, part + 20), u16(language, part + 24)
                encoded = bytearray(language[start : start + length])
                cursor = 0
                while cursor < len(encoded):
                    width = 1 if encoded[cursor] < 0xC0 else 2 if encoded[cursor] < 0xE0 else 3 if encoded[cursor] < 0xF0 else 4
                    encoded[cursor + width - 1] = (encoded[cursor + width - 1] + shift) & 0xFF
                    cursor += width
                result.extend(encoded)
            return result.decode("utf-8")
        return ""

    def text_reference(offset: int) -> str:
        bank_index, key = struct.unpack_from("<II", display, offset)
        if bank_index == 0xFFFF or key == 0x811C9DC5:
            return ""
        if bank_index >= len(bank_rows):
            raise ValueError("activity display bank index is outside the installed table")
        container_tag = u32(banks, bank_rows[bank_index] + 4)
        return composite_text(container_tag, key)

    display_rows = array(display, 8, 16)
    rows: list[dict] = []
    for activity in activities["rows"]:
        ordinal = activity["ordinal"]
        outer = display_rows[ordinal] + 8 + i64(display, display_rows[ordinal] + 8)
        record = outer + i64(display, outer)
        rows.append(
            {
                "ordinal": ordinal,
                "title": text_reference(record + 4),
                "description": text_reference(record + 12),
            }
        )
    if any(row["title"] != "Leviathan, Eater of Worlds" or row["description"] != '"In the belly of the beast."' for row in rows):
        raise ValueError(f"unexpected Eater activity display rows: {rows}")
    return {
        "tableTag": hx(DISPLAY_ACTIVITY_TABLE),
        "tableClass": hx(display_class),
        "tableSha256": hashlib.sha256(display).hexdigest(),
        "rows": rows,
    }


def follow_descriptor_handle(tag: int, registry_key: int) -> list[dict]:
    for _ in range(8):
        cls, blob = packages.read(tag)
        if cls == 0x80809C36:
            found: list[dict] = []
            for base in range(0, max(0, len(blob) - 127), 4):
                if (
                    u32(blob, base) != tag
                    or u32(blob, base + 8) != 0x70
                    or u32(blob, base + 48) != registry_key
                ):
                    continue
                component, sense, auth = struct.unpack_from("<III", blob, base + 4)[0], u32(blob, base + 68), u32(blob, base + 72)
                if component >> 16 != 0x8080:
                    continue
                if sense != 0xFFFFFFFF and sense >> 16 != 0x8080:
                    continue
                if auth != 0xFFFFFFFF and auth >> 16 != 0x8080:
                    continue
                name_at = base + 0x50 + i64(blob, base + 0x50)
                name = ""
                if 0 <= name_at < len(blob):
                    name = blob[name_at:].split(b"\0", 1)[0].decode("utf-8", errors="replace")
                found.append(
                    {
                        "type": u16(blob, base + 52),
                        "index": u16(blob, base + 54),
                        "sourceTag": hx(tag),
                        "sourceOffset": base,
                        "componentClass": hx(component),
                        "senseSchema": hx(sense),
                        "authSchema": hx(auth),
                        "name": name,
                    }
                )
            return found
        if cls == 0x80809B14:
            tag = u32(blob, 12)
            continue
        if cls == 0x80809468:
            handles = array(blob, 16, 4)
            if not handles:
                return []
            tag = u32(blob, handles[0])
            continue
        return []
    raise ValueError("descriptor handle chain exceeded eight hops")


def recover_group(tag: int) -> dict:
    cls, blob = packages.read(tag)
    if cls != 0x80809462:
        raise ValueError(f"{tag:#x} is not a placed object")
    registry_key = u32(blob, 12)
    declared = [
        {"index": index, "type": u32(blob, offset), "nameHash": hx(u32(blob, offset + 4))}
        for index, offset in enumerate(array(blob, 32, 8))
    ]
    descriptors: dict[int, dict] = {}
    bubble_bindings: list[dict] = []
    for bubble in array(blob, 56, 24):
        bubble_index = struct.unpack_from("<i", blob, bubble)[0]
        handles = [u32(blob, offset) for offset in array(blob, bubble + 8, 4)]
        bubble_bindings.append({"bubbleIndex": bubble_index, "handles": [hx(value) for value in handles]})
        for handle in handles:
            for descriptor in follow_descriptor_handle(handle, registry_key):
                existing = descriptors.get(descriptor["index"])
                if existing is not None and existing != descriptor:
                    raise ValueError(f"conflicting descriptor for {tag:#x} slot {descriptor['index']}")
                descriptors[descriptor["index"]] = descriptor
    resolved = [descriptors[index] for index in sorted(descriptors)]
    return {
        "objectTag": hx(tag),
        "objectClass": hx(cls),
        "registryKey": hx(registry_key),
        "declaredSlotCount": len(declared),
        "resolvedClientDescriptorCount": len(resolved),
        "unresolvedHostOnlyCount": len(declared) - len(resolved),
        "typeCounts": {str(key): value for key, value in sorted(Counter(x["type"] for x in resolved).items())},
        "declaredSlots": declared,
        "descriptors": resolved,
        "bubbleBindings": bubble_bindings,
    }


def cache_published_objects(cache: bytes, counts: list[int], offsets: list[int]) -> set[int]:
    result: set[int] = set()
    start = offsets[ROSTER_DOMAIN]
    size = DOMAIN_SIZES[ROSTER_DOMAIN]
    for index in range(counts[ROSTER_DOMAIN]):
        result.add(u32(cache, start + index * size + 4))
    return result


def string_table(tag: int) -> dict[int, str]:
    if not 0x80800000 <= tag < 0x82000000:
        return {}
    try:
        cls, root = packages.read(tag)
    except (AssertionError, FileNotFoundError, ValueError):
        return {}
    if cls != 0x80809A88:
        return {}
    keys = [u32(root, offset) for offset in array(root, 8, 4, 0x80800070)]
    cls, language = packages.read(u32(root, 24))
    if cls != 0x80809A8A:
        return {}
    rows = array(language, 8, 32, 0x80809A90)
    if len(rows) != len(keys):
        raise ValueError("localized-string key and value counts differ")
    result: dict[int, str] = {}
    for key, row in zip(keys, rows):
        at = row + 8 + i64(language, row + 8)
        length, bias = u16(language, row + 20), u16(language, row + 24)
        encoded = language[at : at + length].decode("utf-8")
        result[key] = "".join(chr(ord(char) + bias) for char in encoded)
    return result


def class_references(blob: bytes, wanted_class: int) -> list[int]:
    result: set[int] = set()
    for offset in range(0, len(blob) - 3, 4):
        tag = u32(blob, offset)
        if not 0x80800000 <= tag < 0x82000000:
            continue
        try:
            cls, _ = packages.read(tag)
        except (AssertionError, FileNotFoundError, ValueError):
            continue
        if cls == wanted_class:
            result.add(tag)
    return sorted(result)


def discover_descriptor_refs(groups: list[dict], slot_types: set[int], wanted_class: int) -> list[int]:
    found: set[int] = set()
    sources: set[int] = set()
    for group in groups:
        for descriptor in group["descriptors"]:
            if descriptor["type"] in slot_types:
                sources.add(int(descriptor["sourceTag"], 16))
    for tag in sources:
        _, blob = packages.read(tag)
        found.update(class_references(blob, wanted_class))
    return sorted(found)


def objectives(tags: list[int]) -> list[dict]:
    output: list[dict] = []
    for tag in tags:
        cls, blob = packages.read(tag)
        if cls != 0x80804F72:
            continue
        for row_index, row in enumerate(array(blob, 8, 40, 0x80804F74)):
            variants: list[list[str]] = []
            for variant in array(blob, row + 16, 32, 0x80804F76):
                fields: list[str] = []
                for delta in (0, 8, 16, 24):
                    container, key = u32(blob, variant + delta), u32(blob, variant + delta + 4)
                    fields.append(string_table(container).get(key, ""))
                variants.append(fields)
            output.append({"tag": hx(tag), "row": row_index, "eventHash": hx(u32(blob, row)), "variants": variants})
    return output


def dialogue(tags: list[int]) -> list[dict]:
    output: list[dict] = []
    for tag in tags:
        cls, blob = packages.read(tag)
        if cls != 0x80808D54:
            continue
        roots = {u32(blob, offset): offset + 8 + i64(blob, offset + 8) for offset in array(blob, 24, 16)}
        starts = sorted(roots.values()) + [len(blob)]
        for row_index, row in enumerate(array(blob, 8, 8)):
            selector = u32(blob, row)
            start = roots[selector]
            end = starts[starts.index(start) + 1]
            texts: list[str] = []
            for offset in range(start, end - 7, 4):
                container, key = struct.unpack_from("<II", blob, offset)
                text = string_table(container).get(key)
                if text and text not in texts:
                    texts.append(text)
            output.append(
                {
                    "tag": hx(tag),
                    "row": row_index,
                    "selector": hx(selector),
                    "durationMs": round(struct.unpack_from("<f", blob, row + 4)[0] * 1000),
                    "texts": texts,
                }
            )
    return output


def squad_sources(groups: list[dict]) -> list[dict]:
    result: list[dict] = []
    for group in groups:
        registry_key = int(group["registryKey"], 16)
        for descriptor in group["descriptors"]:
            if descriptor["type"] != 1:
                continue
            tag = int(descriptor["sourceTag"], 16)
            cls, blob = packages.read(tag)
            base = descriptor["sourceOffset"] + 0x68
            if cls != 0x80809C36 or u32(blob, base) != tag or u32(blob, base + 4) != 0x80807EB9:
                continue
            categories: list[dict] = []
            for category in array(blob, base + 64, 104, 0x80808356):
                lanes: list[list[str]] = []
                for lane in range(6):
                    entities: list[str] = []
                    for entry in array(blob, category + 8 + 16 * lane, 24, 0x80808358):
                        target = entry + i64(blob, entry)
                        if not (4 <= target <= len(blob) - 4) or u32(blob, target - 4) != 0x808099D8:
                            raise ValueError("squad source entity reference did not validate")
                        entities.append(hx(u32(blob, target)))
                    lanes.append(entities)
                categories.append({"categoryHash": hx(u32(blob, category)), "laneEntities": lanes})
            rule_registry, packed_rule = struct.unpack_from("<II", blob, base + 56)
            result.append(
                {
                    "registryKey": hx(registry_key),
                    "slot": descriptor["index"],
                    "name": descriptor["name"],
                    "sourceTag": descriptor["sourceTag"],
                    "sourceOffset": descriptor["sourceOffset"],
                    "ruleRegistry": hx(rule_registry),
                    "ruleType": packed_rule & 0xFF,
                    "ruleSlot": packed_rule >> 16,
                    "categories": categories,
                }
            )
    return result


def package_inventory(tags: set[int]) -> list[dict]:
    package_ids = sorted({(tag - 0x80800000) >> 13 for tag in tags if 0x80800000 <= tag})
    output: list[dict] = []
    for package_id in package_ids:
        files = sorted((ROOT / "packages").glob(f"*_{package_id:04x}_*.pkg"))
        output.append(
            {
                "packageId": f"0x{package_id:04X}",
                "files": [{"name": path.name, "byteLength": path.stat().st_size} for path in files],
            }
        )
    return output


def behavior_evidence() -> list[dict]:
    roots_path = DECOMP / "behavior_trigger_map" / "roots.jsonl"
    if not roots_path.exists():
        return []
    output: list[dict] = []
    for line in roots_path.read_text(encoding="utf-8").splitlines():
        row = json.loads(line)
        paths = [link.get("path") or "" for link in row["semantics"].get("links", [])]
        if not any("\\envy\\" in path or "envy_d2" in path for path in paths):
            continue
        tag = int(row["tag"], 16)
        installed_class, installed = packages.read(tag)
        archive_raw = DECOMP / row["raw_path"]
        installed_sha = hashlib.sha256(installed).hexdigest()
        archive_sha = hashlib.sha256(archive_raw.read_bytes()).hexdigest()
        if installed_class != 0x8080941E or installed_sha != row["raw_sha256"] or archive_sha != installed_sha:
            raise ValueError(f"behavior archive does not match installed {tag:#x}")
        output.append(
            {
                "tag": row["tag"],
                "installedClass": hx(installed_class),
                "installedAndArchiveSha256": installed_sha,
                "byteLength": len(installed),
                "decompileStatus": row["decompile_status"],
                "semanticCoverage": row["semantic_coverage"],
                "synopsis": row["synopsis"],
                "ownerPaths": row["owner_paths"],
                "semantics": row["semantics"],
                "triggerStatus": row["trigger_status"],
                "triggerMechanism": row["trigger_mechanism"],
                "readableJsonPath": row["readable_json_path"],
                "readableTextPath": row["readable_text_path"],
            }
        )
    if len(output) != 6:
        raise ValueError(f"expected six envy-linked behavior roots, found {len(output)}")
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "docs" / "raids" / "eater-of-worlds" / "evidence" / "native-inventory.json",
    )
    args = parser.parse_args()

    cache = CACHE.read_bytes()
    counts, offsets = cache_domains(cache)
    scenario, bubbles = walk_scenario()
    activities = public_activities()
    display = activity_display(activities)
    names = hash_names(cache, counts, offsets)
    for bubble in bubbles:
        bubble["name"] = names.get(int(bubble["bubbleHash"], 16))
    object_tags = {
        int(obj["tag"], 16)
        for bubble in bubbles
        for state in bubble["states"]
        for obj in state["objects"]
    }
    groups = [recover_group(tag) for tag in sorted(object_tags)]
    cached = cache_published_objects(cache, counts, offsets)
    for group in groups:
        group["publishedInBuildDataRosterDomain"] = int(group["objectTag"], 16) in cached

    objective_tags = discover_descriptor_refs(groups, {68}, 0x80804F72)
    dialogue_tags = discover_descriptor_refs(groups, {53}, 0x80808D54)
    scene_tags = discover_descriptor_refs(groups, {6, 43}, 0x80809C0F)
    localization_tags: set[int] = set()
    for tag in [*objective_tags, *dialogue_tags]:
        _, blob = packages.read(tag)
        localization_tags.update(class_references(blob, 0x80809A88))
    behavior = behavior_evidence()
    squads = squad_sources(groups)
    package_tags = object_tags | {
        SCENARIO, PUBLIC_ACTIVITY_TABLE, DISPLAY_ACTIVITY_TABLE, *objective_tags,
        *dialogue_tags, *scene_tags,
        *localization_tags
    }
    package_tags.update(int(row["difficultySettingsTag"], 16) for row in activities["rows"])
    for bubble in bubbles:
        for state in bubble["states"]:
            package_tags.add(int(state["entryTag"], 16))
            package_tags.add(int(state["registryTag"], 16))
    for group in groups:
        package_tags.update(int(row["sourceTag"], 16) for row in group["descriptors"])
        for binding in group["bubbleBindings"]:
            package_tags.update(int(tag, 16) for tag in binding["handles"])
    for squad in squads:
        for category in squad["categories"]:
            for lane in category["laneEntities"]:
                package_tags.update(int(tag, 16) for tag in lane)
    package_tags.update(int(row["tag"], 16) for row in behavior)
    for row in behavior:
        package_tags.update(int(tag, 16) for tag in row["semantics"].get("dependency_tags", []) if tag != "0xFFFFFFFF")

    result = {
        "schema": "eater-of-worlds-native-research-v1",
        "provenance": {
            "build": 86657,
            "cacheVersion": CACHE_VERSION,
            "cacheSha256": hashlib.sha256(cache).hexdigest(),
            "cacheByteLength": len(cache),
            "method": "read-only installed package traversal plus byte-verified DECOMP_SHARE join",
        },
        "scenario": scenario,
        "publicActivities": activities,
        "activityDisplay": display,
        "cacheScenario": scenario_cache_record(cache, counts, offsets),
        "bubbles": bubbles,
        "groups": groups,
        "objectives": objectives(objective_tags),
        "dialogue": dialogue(dialogue_tags),
        "localizationTags": [hx(tag) for tag in sorted(localization_tags)],
        "sceneTags": [hx(tag) for tag in scene_tags],
        "squadSources": squads,
        "behaviors": behavior,
        "packages": package_inventory(package_tags),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(
        f"wrote {args.output}: {len(bubbles)} bubbles, {len(groups)} groups, "
        f"{sum(len(group['descriptors']) for group in groups)} client descriptors, "
        f"{len(result['squadSources'])} squads, {len(behavior)} behavior roots"
    )


if __name__ == "__main__":
    main()
