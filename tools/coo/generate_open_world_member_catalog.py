"""Generate a digest-pinned lookup for native open-world source member offsets.

The catalog contains only positive-weight native choices belonging to sources
selected by the open-world profiles (ordinary patrols and registered NPCs),
Mercury's persistent patrol director, and Lost Sectors. It
does not infer species, rank, actor counts, or live member kind.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import generate_open_world_profiles as profiles  # noqa: E402

PIN_PATH = Path(__file__).with_name("open_world_member_catalog_pins.json")
HEADER_PATH = ROOT / "Dawn/src/state/activity/coo/open_world_member_catalog.h"
SOURCE_OUTER_CLASS = 0x80809C36
SOURCE_DEFINITION_CLASS = 0x8080948F
CATEGORY_CLASS = 0x80808356
CHOICE_CLASS = 0x80808358
MEMBER_RECORD_CLASS = 0x808099D8
MAX_CATEGORIES = 8
MAX_CHOICES_PER_VARIANT = 128


@dataclass(frozen=True)
class SelectedSource:
    activity: str
    origin: str
    resource: int
    registry: int
    source: int


def _hex(value: int) -> str:
    return f"{value:08X}"


def _cpp(value: int) -> str:
    return f"0x{value:08X}U"


def _block(text: str, begin: str, end: str) -> str:
    start = text.find(begin)
    if start < 0:
        raise ValueError(f"missing source block: {begin}")
    finish = text.find(end, start + len(begin))
    if finish < 0:
        raise ValueError(f"unterminated source block: {begin}")
    return text[start:finish]


def _groups(target: profiles.Target) -> dict[tuple[int, int], dict]:
    _, objects_by_bubble = profiles.scenario_objects(target.scenario)
    result: dict[tuple[int, int], dict] = {}
    for bubble, objects in objects_by_bubble.items():
        for object_tag in objects:
            group = profiles.resolve_group(object_tag)
            if group is None or group["mask"] != 1 << bubble:
                continue
            identity = (bubble, group["key"])
            if identity in result and result[identity] != group:
                raise ValueError(f"ambiguous registry {_hex(group['key'])} in bubble {bubble}")
            result[identity] = group
    return result


def generic_sources() -> list[SelectedSource]:
    selection = json.loads((Path(__file__).with_name("open_world_spawn_selections.json")).read_text())
    if selection.get("schema_version") != 1 or selection.get("package_build") != 86657 \
            or set(selection.get("destinations", {})) != {target.activity for target in profiles.TARGETS}:
        raise ValueError("unsupported or incomplete open-world source selections")
    result = []
    for target in profiles.TARGETS:
        groups = _groups(target)
        pins = selection["destinations"][target.activity]
        resolved = profiles.selected_patrols(target, groups, pins)
        for (group, source, _, _), pin in zip(resolved, pins, strict=True):
            slots = [slot for slot in group["slots"] if slot["type"] == 1 and slot["index"] == source]
            if len(slots) != 1 or slots[0]["descriptor"] != int(pin["source_descriptor"], 16):
                raise ValueError("selected generic source descriptor changed")
            result.append(SelectedSource(target.activity, "generic", slots[0]["descriptor"],
                                         group["key"], source))
    return result


def npc_sources() -> list[SelectedSource]:
    result = []
    for target in profiles.TARGETS:
        groups = _groups(target)
        for bubble, key in target.npcs:
            group = groups.get((bubble, key))
            if group is None:
                raise ValueError(f"selected NPC registry {_hex(key)} is not package-resolved")
            slots = [slot for slot in group["slots"] if slot["type"] == 1]
            if not slots or not any(slot["type"] == 42 for slot in group["slots"]):
                raise ValueError(f"selected NPC registry {_hex(key)} lacks source/controller")
            # Profile generation intentionally selects the first authored source.
            slot = slots[0]
            result.append(SelectedSource(target.activity, "npc", slot["descriptor"],
                                         group["key"], slot["index"]))
    return result


def edz_moon_bootstrap_sources() -> list[SelectedSource]:
    specs = (
        ("edz_freeroam", 0x80B2F00A, 0, 0x52695108, 1, 0x80BE2F2E),
        ("luna_freeroam", 0x81503E69, 0, 0x2F2CA9F5, 0, 0x81565CBB),
    )
    result = []
    for activity, scenario, bubble, key, source, descriptor in specs:
        target = profiles.Target(activity, activity, activity, scenario, bubble, (), (), ())
        group = _groups(target).get((bubble, key))
        if group is None:
            raise ValueError(f"bootstrap registry {_hex(key)} is not package-resolved")
        slots = [slot for slot in group["slots"]
                 if slot["type"] == 1 and slot["index"] == source]
        if len(slots) != 1 or slots[0]["descriptor"] != descriptor:
            raise ValueError(f"bootstrap source {_hex(key)}/{source} changed")
        result.append(SelectedSource(activity, "bootstrap", descriptor, key, source))
    return result


def _mercury_registry_keys() -> list[int | None]:
    text = (ROOT / "Dawn/src/state/activity/coo/mercury_registries.h").read_text()
    body = _block(text, "inline constexpr std::array<registry::Definition,20> kRegistries{{", "}};")
    token = re.compile(
        r'\{"mercury_freeroam",0x[0-9A-Fa-f]+,0x([0-9A-Fa-f]+)'
        r'|\*ambient::find\(0x([0-9A-Fa-f]+)\)->registry'
        r'|(adventure::mercury::kRegistry|public_event_rally::kRegistry)')
    values: list[int | None] = []
    for match in token.finditer(body):
        value = match.group(1) or match.group(2)
        values.append(int(value, 16) if value is not None else None)
    if len(values) != 20:
        raise ValueError(f"Mercury registry catalog width changed: {len(values)}")
    return values


def _mercury_capability_bindings() -> list[tuple[int, int] | None]:
    text = (ROOT / "Dawn/src/server/runtime/activity/mercury_populations.h").read_text()
    body = _block(text, "std::array<population::Capability,32> result{{", "}};")
    token = re.compile(
        r'\{&kRegistries\[(\d+)\],(\d+),[^}]*\}'
        r'|public_events::k[A-Za-z0-9_]+\[(\d+)\]')
    values: list[tuple[int, int] | None] = []
    for match in token.finditer(body):
        values.append((int(match.group(1)), int(match.group(2))) if match.group(1) else None)
    if len(values) != 32:
        raise ValueError(f"Mercury population capability width changed: {len(values)}")
    return values


def _mercury_patrol_capabilities() -> list[int]:
    text = (ROOT / "Dawn/src/server/runtime/activity/mercury_freeroam_runtime.h").read_text()
    body = _block(text, "inline constexpr std::array<Patrol,27> kPatrols{{", "}};")
    values = [int(value) for value in re.findall(r"^\s*\{(\d+),Faction::", body, re.MULTILINE)]
    if len(values) != 27 or len(set(values)) != len(values):
        raise ValueError(f"Mercury patrol capability set changed: {values}")
    return values


def mercury_sources() -> list[SelectedSource]:
    keys = _mercury_registry_keys()
    bindings = _mercury_capability_bindings()
    target = profiles.Target("mercury", "Mercury", "mercury_freeroam", 0x80F4696A,
                             15, ((15, 1),), (), ())
    groups = {key: group for (_, key), group in _groups(target).items()}
    result = []
    for capability in _mercury_patrol_capabilities():
        binding = bindings[capability]
        if binding is None:
            raise ValueError(f"selected Mercury capability {capability} is not a direct source")
        registry_index, source = binding
        key = keys[registry_index]
        if key is None or key not in groups:
            raise ValueError(f"selected Mercury registry {registry_index} is not package-resolved")
        group = groups[key]
        slots = [slot for slot in group["slots"] if slot["type"] == 1 and slot["index"] == source]
        if len(slots) != 1:
            raise ValueError(f"selected Mercury source {_hex(key)}/{source} is ambiguous")
        result.append(SelectedSource("mercury_freeroam", "mercury", slots[0]["descriptor"], key, source))
    # The retained Mercury activity also publishes Vance and four Crossroads
    # event sources outside kPatrols. They still receive native population
    # leases and therefore need exact member-category attribution for streamed
    # recreation, even though none authorizes ordinary patrol replenishment.
    extras = (
        (0x564C6ECE, 0, 0x80F5B9CC),
        (0xC8229B2B, 44, 0x80F5E3A4),
        (0xC8229B2B, 46, 0x80F5E3AA),
        (0xC8229B2B, 48, 0x80F5E3B0),
        (0xC8229B2B, 49, 0x80F5E3B3),
    )
    for key, source, descriptor in extras:
        group = groups.get(key)
        if group is None:
            raise ValueError(f"retained Mercury registry {_hex(key)} is not package-resolved")
        slots = [slot for slot in group["slots"]
                 if slot["type"] == 1 and slot["index"] == source]
        if len(slots) != 1 or slots[0]["descriptor"] != descriptor:
            raise ValueError(f"retained Mercury source {_hex(key)}/{source} changed")
        result.append(SelectedSource("mercury_freeroam", "retained", descriptor, key, source))
    return result


def lost_sector_sources() -> list[SelectedSource]:
    import generate_lost_sector_catalog as lost
    groups, _ = lost.resolve()
    result = []
    for sector in lost.SECTORS:
        for stage in sector["stages"]:
            for key, source_slots in stage:
                group = groups[(sector["scenario"], key)]
                for source in source_slots:
                    slots = [slot for slot in group["slots"]
                             if slot["type"] == 1 and slot["index"] == source]
                    if len(slots) != 1:
                        raise ValueError(f"Lost Sector source {_hex(key)}/{source} is ambiguous")
                    result.append(SelectedSource(sector["activity"], "lost_sector",
                                                 slots[0]["descriptor"], key, source))
    research = json.loads((Path(__file__).with_name("lost_sector_edz_moon_native_research.json")).read_text())
    if research.get("schema") != 1:
        raise ValueError("EDZ/Moon Lost Sector research schema changed")
    for sector in research["sectors"]:
        activity = "edz_freeroam" if sector["namespace"] == "edz" else "moon_freeroam"
        registry = int(sector["registry"], 16)
        for source in sector["sources"]:
            result.append(SelectedSource(activity, "lost_sector", int(source["descriptor"], 16),
                                         registry, source["slot"]))
    return result


def selected_sources() -> list[SelectedSource]:
    candidates = (generic_sources() + npc_sources() + edz_moon_bootstrap_sources() + mercury_sources()
                  + lost_sector_sources())
    result = {}
    for row in candidates:
        identity=(row.resource,row.registry,row.source)
        previous=result.get(identity)
        if previous and (previous.registry,previous.source)!=(row.registry,row.source):
            raise ValueError("conflicting selected source identity")
        result.setdefault(identity,row)
    return sorted(result.values(), key=lambda row: (row.activity, row.registry, row.source, row.resource))


def decode_source(selected: SelectedSource) -> dict:
    outer_class, raw = profiles.package_read.read(selected.resource)
    if outer_class != SOURCE_OUTER_CLASS:
        raise ValueError(f"source {_hex(selected.resource)} outer class")
    definition = profiles.relative(raw, 24)
    if definition < 4 or definition + 0xB8 > len(raw) \
            or profiles.u32(raw, definition - 4) != SOURCE_DEFINITION_CLASS:
        raise ValueError(f"source {_hex(selected.resource)} definition class/bounds")
    if struct.unpack_from("<IHH", raw, definition + 0x30) \
            != (selected.registry, 1, selected.source):
        raise ValueError(f"source {_hex(selected.resource)} native identity")
    categories = profiles.array_rows(raw, definition + 0xA8, 104, CATEGORY_CLASS)
    if not 1 <= len(categories) <= MAX_CATEGORIES:
        raise ValueError(f"source {_hex(selected.resource)} category width {len(categories)}")
    choices = []
    for category_index, category in enumerate(categories):
        category_key = profiles.u32(raw, category)
        if category_key in (0, 0xFFFFFFFF, 0x811C9DC5):
            raise ValueError(f"source {_hex(selected.resource)} invalid category key")
        for variant in range(6):
            rows = profiles.array_rows(raw, category + 8 + variant * 16, 24, CHOICE_CLASS)
            if len(rows) > MAX_CHOICES_PER_VARIANT:
                raise ValueError(f"source {_hex(selected.resource)} choice width")
            for choice_index, row in enumerate(rows):
                weight = profiles.u32(raw, row + 12)
                if not weight:
                    continue
                body = profiles.relative(raw, row)
                if body < definition or body < 4 \
                        or profiles.u32(raw, body - 4) != MEMBER_RECORD_CLASS:
                    raise ValueError(f"source {_hex(selected.resource)} member record")
                offset = body - definition
                if offset > 0x7FFFFFFFFFFFFFFF:
                    raise ValueError(f"source {_hex(selected.resource)} member offset")
                entity = profiles.u32(raw, body)
                if entity in (0, 0xFFFFFFFF, 0x811C9DC5):
                    raise ValueError(f"source {_hex(selected.resource)} entity")
                choices.append({
                    "memberOffset": offset,
                    "categoryKey": _hex(category_key),
                    "entity": _hex(entity),
                    "weight": weight,
                    "category": category_index,
                    "variant": variant,
                    "choice": choice_index,
                })
    offsets = [row["memberOffset"] for row in choices]
    if not choices or len(offsets) != len(set(offsets)):
        raise ValueError(f"source {_hex(selected.resource)} absent/ambiguous positive members")
    choice_digest = hashlib.sha256(
        json.dumps(choices, sort_keys=True, separators=(",", ":")).encode("ascii")).hexdigest()
    return {
        "activity": selected.activity,
        "origin": selected.origin,
        "resource": _hex(selected.resource),
        "registry": _hex(selected.registry),
        "source": selected.source,
        "resourceClass": _hex(outer_class),
        "definitionClass": _hex(SOURCE_DEFINITION_CLASS),
        "resourceSha256": hashlib.sha256(raw).hexdigest(),
        "categoryCount": len(categories),
        "positiveChoiceCount": len(choices),
        "choiceDigestSha256": choice_digest,
        "choices": choices,
    }


def current_document() -> dict:
    rows = [decode_source(source) for source in selected_sources()]
    return {"schemaVersion": 1, "packageBuild": 86657,
            "scope": "registered generic, Mercury patrol, and Lost Sector population sources",
            "sources": rows}


def canonical_json(document: dict) -> str:
    return json.dumps(document, indent=2) + "\n"


def validate_pins(actual: dict, pinned: dict) -> None:
    if pinned != actual:
        actual_ids = {(x["resource"], x["registry"], x["source"]) for x in actual["sources"]}
        pinned_ids = {(x["resource"], x["registry"], x["source"]) for x in pinned.get("sources", [])}
        if actual_ids != pinned_ids:
            raise ValueError(f"selected source/pin identity drift: added={sorted(actual_ids-pinned_ids)} removed={sorted(pinned_ids-actual_ids)}")
        raise ValueError("selected source bytes, native choices, or digest pins changed")


def render_header(document: dict) -> str:
    choices = []
    ranges = []
    first = 0
    for source in document["sources"]:
        source_choices = source["choices"]
        ranges.append((source, first, len(source_choices)))
        choices.extend(source_choices)
        first += len(source_choices)
    pin_digest = hashlib.sha256(canonical_json(document).encode("utf-8")).hexdigest().upper()
    lines = [
        "#pragma once",
        "#include <array>",
        "#include <cstdint>",
        "",
        "// Generated by tools/coo/generate_open_world_member_catalog.py.",
        "// Positive-weight package choices only; no species, rank, count, or member-kind inference.",
        f"// Canonical pin SHA-256: {pin_digest}",
        "namespace dawn::state::activity::open_world_members {",
        "struct MemberChoice final {",
        "    std::int64_t memberOffset{};",
        "    std::uint32_t categoryKey{},entity{},weight{};",
        "    std::uint8_t category{},variant{};",
        "    std::uint16_t choice{};",
        "};",
        "struct SourceRange final {",
        "    std::uint32_t resource{},registry{},first{},count{};",
        "    std::uint16_t source{};",
        "};",
        f"inline constexpr std::array<MemberChoice,{len(choices)}> kChoices{{{{",
    ]
    for row in choices:
        lines.append(f"    {{{row['memberOffset']},{_cpp(int(row['categoryKey'],16))},"
                     f"{_cpp(int(row['entity'],16))},{row['weight']}U,{row['category']},"
                     f"{row['variant']},{row['choice']}}},")
    lines.extend(["}};", f"inline constexpr std::array<SourceRange,{len(ranges)}> kSources{{{{"])
    for source, begin, count in ranges:
        lines.append(f"    {{{_cpp(int(source['resource'],16))},{_cpp(int(source['registry'],16))},"
                     f"{begin}U,{count}U,{source['source']}}}, // {source['activity']} {source['resourceSha256'].upper()}")
    lines.extend([
        "}};",
        "[[nodiscard]] constexpr bool registered(std::uint32_t resource,",
        "    std::uint32_t registry,std::uint16_t source) noexcept {",
        "    unsigned matches{};",
        "    for(const auto& range:kSources)",
        "        matches+=(range.resource==resource && range.registry==registry && range.source==source)?1U:0U;",
        "    return matches==1;",
        "}",
        "[[nodiscard]] constexpr const MemberChoice* lookup(std::uint32_t resource,",
        "    std::uint32_t registry,std::uint16_t source,std::int64_t memberOffset) noexcept {",
        "    const MemberChoice* result=nullptr;",
        "    for(const auto& range:kSources) {",
        "        if(range.resource!=resource || range.registry!=registry || range.source!=source)continue;",
        "        for(std::uint32_t index=0;index<range.count;++index) {",
        "            const auto& choice=kChoices[range.first+index];",
        "            if(choice.memberOffset!=memberOffset)continue;",
        "            if(result!=nullptr)return nullptr;",
        "            result=&choice;",
        "        }",
        "    }",
        "    return result;",
        "}",
        "} // namespace dawn::state::activity::open_world_members",
        "",
    ])
    return "\n".join(lines)


def check_file(path: Path, expected: str, label: str) -> None:
    if not path.exists() or path.read_text(encoding="utf-8") != expected:
        raise ValueError(f"{label} drift: run generator after explicit pin review")


def main() -> None:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--refresh-pins", action="store_true",
                      help="review operation: replace native source/digest pins and generated header")
    args = parser.parse_args()
    actual = current_document()
    if args.refresh_pins:
        PIN_PATH.write_text(canonical_json(actual), encoding="utf-8", newline="\n")
        HEADER_PATH.write_text(render_header(actual), encoding="utf-8", newline="\r\n")
    else:
        if not PIN_PATH.exists():
            raise ValueError("member catalog pins absent; explicit --refresh-pins review required")
        pin_text = PIN_PATH.read_text(encoding="utf-8")
        pinned = json.loads(pin_text)
        check_file(PIN_PATH, canonical_json(pinned), "pin formatting")
        validate_pins(actual, pinned)
        expected = render_header(pinned)
        if args.check:
            # Path.read_text() normalizes CRLF to LF, so compare the logical text.
            check_file(HEADER_PATH, expected, "generated member catalog")
        else:
            HEADER_PATH.write_text(expected, encoding="utf-8", newline="\r\n")
    print(f"{'Checked' if args.check else 'Generated'} {len(actual['sources'])} sources, "
          f"{sum(row['positiveChoiceCount'] for row in actual['sources'])} positive choices")


if __name__ == "__main__":
    main()
