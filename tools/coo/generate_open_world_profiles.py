"""Generate pinned native free-roam catalogs from the installed destination packages.

The generated header contains only ordinary ambient populations, destination NPCs,
and adventure/story flag placements. Public-event registries are deliberately not
selected. Run from the repository root with the installed package set available.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import json
import struct
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import package_read


@dataclass(frozen=True)
class Target:
    namespace: str
    display: str
    activity: str
    scenario: int
    primary_bubble: int
    ambient_bubbles: tuple[tuple[int, int], ...]
    npcs: tuple[tuple[int, int], ...]
    adventures: tuple[tuple[int, int, int, tuple[tuple[int, str], ...]], ...]


TARGETS = (
    Target("io", "Io", "eden_freeroam", 0x80B56B1B, 4,
           ((4, 4), (15, 4), (17, 4), (21, 4)),
           ((4, 0x36E4495D),),
           ((4, 0xBCD2B135, 9, ((1146, "adventure_outlaws_cabal_dbda"),)),
            (4, 0xBCD2B135, 10, ((1147, "adventure_outlaws_cabal_dbdb"),)),
            (17, 0x5D376320, 0, ((1145, "adventure_outlaws_cabal_oada"),)))),
    Target("titan", "Titan", "fleet_freeroam", 0x80B3E142, 2,
           ((2, 4), (7, 4)),
           ((2, 0x05324D75),),
           ((2, 0xF2C8FA93, 0, ((1142, "adventure_outlaws_hive_bada"),)),
            (7, 0x9B958891, 0, ((1143, "adventure_outlaws_hive_plda"),)),
            (7, 0x9B958891, 1, ((1144, "adventure_outlaws_hive_pldb"),)))),
    Target("mars", "Mars", "polaris_freeroam", 0x80F6AB20, 1,
           ((1, 4), (5, 4)),
           ((1, 0x0D1B60CF),),
           ((1, 0x7EAECBEB, 1, ((1151, "adventure_outlaws_polaris_brda"),)),
            (5, 0x63CAF4E4, 3, ((1152, "adventure_outlaws_polaris_glda"),)))),
    Target("nessus", "Nessus", "planet_x_freeroam", 0x80B43A1C, 3,
           ((3, 4), (1, 4), (11, 4), (30, 4), (37, 4)),
           ((3, 0x7B3D65F8),),
           ((30, 0xDA814632, 0, ((1140, "adventure_outlaws_vex_scda"),)),
            (37, 0xCA092634, 4, ((1141, "adventure_outlaws_vex_wwda"),)))),
    Target("tangled_shore", "The Tangled Shore", "tangled_shore_freeroam", 0x80FC9645, 7,
           ((7, 4), (9, 4), (14, 4), (18, 4)),
           ((21, 0x6D47E9B3),),
           ((21, 0x8ADAE5CC, 0,
             ((404, "adventure_brainwash_heroic"), (405, "adventure_money_trail_heroic"),
              (406, "adventure_piker_gang_heroic"), (407, "adventure_showdown_heroic"),
              (408, "adventure_soul_stealer_heroic"), (409, "adventure_stash_heroic"))),)),
    Target("dreaming_city", "The Dreaming City", "dreaming_city_freeroam", 0x80F1404D, 1,
           ((1, 3), (2, 3), (11, 3), (18, 3), (19, 3), (20, 3)),
           ((1, 0x87660C45), (18, 0xDBB95609), (20, 0x4ECC8169)),
           ((1, 0x1046B790, 0, ((372, "mission_demontower"),)),
            (18, 0x21EDF044, 0, ((373, "mission_bridge"),)),
            (20, 0x03B24A6D, 0, ((371, "mission_tunnel"),)))),
)


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def relative(data: bytes, offset: int) -> int:
    result = offset + struct.unpack_from("<q", data, offset)[0]
    if not 0 <= result < len(data):
        raise ValueError("relative pointer outside tag")
    return result


def array_rows(data: bytes, offset: int, stride: int, expected: int | None = None) -> list[int]:
    count = struct.unpack_from("<Q", data, offset)[0]
    delta = struct.unpack_from("<q", data, offset + 8)[0]
    if count == 0:
        if delta != 0:
            raise ValueError("noncanonical empty array")
        return []
    if not 0 < count <= 300_000:
        raise ValueError("array count")
    header = offset + 8 + delta
    if not 4 <= header <= len(data) - 16:
        raise ValueError("array header outside tag")
    if u32(data, header - 4) != 0x80809FBD or struct.unpack_from("<Q", data, header)[0] != count:
        raise ValueError("array header mismatch")
    element_class = u32(data, header + 8)
    if expected is not None and element_class != expected:
        raise ValueError(f"array class {element_class:08X}, expected {expected:08X}")
    first = header + 16
    if first + count * stride > len(data):
        raise ValueError("array elements outside tag")
    return [first + index * stride for index in range(count)]


def scenario_objects(scenario_tag: int) -> tuple[list[int], dict[int, list[int]]]:
    actual_class, scenario = package_read.read(scenario_tag)
    if actual_class != 0x80809994:
        raise ValueError(f"scenario {scenario_tag:08X} class {actual_class:08X}")
    hashes: list[int] = []
    result: dict[int, list[int]] = {}
    for bubble_index, bubble in enumerate(array_rows(scenario, 80, 24, 0x8080924D)):
        hashes.append(u32(scenario, bubble))
        objects: list[int] = []
        for state in array_rows(scenario, bubble + 8, 76, 0x8080924F):
            entry_tag = u32(scenario, state + 68)
            if entry_tag in (0, 0xFFFFFFFF):
                continue
            entry_class, entry = package_read.read(entry_tag)
            if entry_class != 0x8080925B:
                raise ValueError("unexpected scenario entry class")
            registry_tag = u32(entry, 20)
            if registry_tag in (0, 0xFFFFFFFF):
                continue
            registry_class, registry = package_read.read(registry_tag)
            if registry_class != 0x8080925E:
                raise ValueError("unexpected scenario registry class")
            for descriptor in (8, 24, 40):
                for row in array_rows(registry, descriptor, 4, 0x80809260):
                    object_tag = u32(registry, row)
                    if object_tag not in (0, 0xFFFFFFFF) and object_tag not in objects:
                        objects.append(object_tag)
        result[bubble_index] = objects
    return hashes, result


def resolve_group(object_tag: int) -> dict | None:
    object_class, obj = package_read.read(object_tag)
    if object_class != 0x80809462 or len(obj) < 72:
        return None
    key = u32(obj, 12)
    if key in (0, 0xFFFFFFFF):
        return None
    declared = [(u32(obj, row), u32(obj, row + 4)) for row in array_rows(obj, 32, 8)]
    handles: list[int] = []
    bubble_mask = 0
    for bubble in array_rows(obj, 56, 24):
        bubble_index = struct.unpack_from("<i", obj, bubble)[0]
        if 0 <= bubble_index < 64:
            bubble_mask |= 1 << bubble_index
        for handle_row in array_rows(obj, bubble + 8, 4):
            handle = u32(obj, handle_row)
            if handle not in (0, 0xFFFFFFFF) and handle not in handles:
                handles.append(handle)
    slots: dict[int, dict] = {}
    for handle in handles:
        tag = handle
        for _ in range(8):
            try:
                actual_class, raw = package_read.read(tag)
            except (AssertionError, OSError, ValueError, IndexError):
                break
            if actual_class == 0x80809C36:
                for base in range(0, max(0, len(raw) - 127), 4):
                    if (u32(raw, base) != tag or u32(raw, base + 8) != 0x70
                            or u32(raw, base + 48) != key):
                        continue
                    component = u32(raw, base + 4)
                    sense, authority = u32(raw, base + 68), u32(raw, base + 72)
                    if component >> 16 != 0x8080:
                        continue
                    if sense != 0xFFFFFFFF and sense >> 16 != 0x8080:
                        continue
                    if authority != 0xFFFFFFFF and authority >> 16 != 0x8080:
                        continue
                    slot_type, slot_index = struct.unpack_from("<HH", raw, base + 52)
                    if slot_index >= len(declared) or declared[slot_index][0] != slot_type:
                        continue
                    slots[slot_index] = {
                        "index": slot_index, "type": slot_type, "component": component,
                        "sense": sense, "authority": authority, "descriptor": tag,
                    }
                break
            if actual_class == 0x80809B14:
                tag = u32(raw, 12)
                continue
            if actual_class == 0x80809468:
                rows = array_rows(raw, 16, 4)
                if not rows:
                    break
                tag = u32(raw, rows[0])
                continue
            break
    if not slots:
        return None
    return {"key": key, "object": object_tag, "mask": bubble_mask,
            "slots": [slots[index] for index in sorted(slots)]}


def activity_catalog() -> list[tuple[int, str]]:
    actual_class, public = package_read.read(0x81327CF0)
    if actual_class != 0x808076F0:
        raise ValueError(f"public activity table class {actual_class:08X}")
    result = []
    for entry in array_rows(public, 8, 16, 0x808076FC):
        identity = u32(public, entry)
        record = relative(public, entry + 8)
        if u32(public, record) != identity:
            raise ValueError("public activity identity mismatch")
        delta = struct.unpack_from("<q", public, record + 0x68)[0]
        name = ""
        if delta:
            start = relative(public, record + 0x68)
            name = public[start:public.index(0, start)].decode("ascii")
        result.append((identity, name))
    return result


def tactical_row_count(group: dict, slot_index: int) -> int:
    slot = next(slot for slot in group["slots"] if slot["index"] == slot_index and slot["type"] == 3)
    _, data = package_read.read(slot["descriptor"])
    definition = relative(data, 24)
    if u32(data, definition - 4) != 0x8080835A:
        raise ValueError("tactical definition class")
    rows = array_rows(data, definition + 0x88, 40, 0x80807D8F)
    if not 1 <= len(rows) <= 24:
        raise ValueError("tactical row count")
    for row in rows:
        tasks = array_rows(data, row + 16, 40, 0x80807D95)
        if not tasks:
            raise ValueError("empty tactical row")
        for task in tasks:
            key, kind, _ = struct.unpack_from("<IHH", data, task + 32)
            if key != group["key"] or kind != 45:
                raise ValueError("tactical provider outside the owning encounter")
    return len(rows)


def adventure_routes(activities: list[tuple[int, str]]) -> dict[int, list[int]]:
    actual_class, routes = package_read.read(0x81327D63)
    if actual_class != 0x80805B8F:
        raise ValueError(f"adventure route table class {actual_class:08X}")
    result: dict[int, list[int]] = {}
    for group in array_rows(routes, 8, 24, 0x80805B95):
        selector = u32(routes, group)
        choices = [i16(routes, row + 16) for row in array_rows(routes, group + 8, 24, 0x80805B97)]
        if selector in result:
            raise ValueError("duplicate adventure selector")
        if any(choice < -1 or choice >= len(activities) for choice in choices):
            raise ValueError("adventure activity outside public table")
        result[selector] = choices
    return result


def selector(descriptor: int) -> int:
    actual_class, raw = package_read.read(descriptor)
    if actual_class != 0x80809C36:
        raise ValueError("adventure descriptor is not a placed-object blob")
    matches = [offset for offset in range(0, len(raw) - 24, 4) if u32(raw, offset) == 0x80804CFC]
    if len(matches) != 1 or u32(raw, matches[0] + 12) != 2:
        raise ValueError("adventure descriptor has no unique typed selector")
    return u32(raw, matches[0] + 20)


def cpp_hex(value: int) -> str:
    return f"0x{value:08X}U"


def policy(target: Target, registry: dict, source: int) -> str:
    descriptor = next(slot["descriptor"] for slot in registry["slots"]
                      if slot["index"] == source and slot["type"] == 1)
    asset = lambda value: f"0x{value:08X}"
    result = {
        "format_version": 2,
        "mission": target.activity,
        "profile": f"{target.namespace}.freeroam.native.v1",
        "authority_schema": "nativeOtherActivities",
        "assets": {
            "persistent_module": {
                "registry": asset(target.scenario), "definition": asset(target.scenario),
                "type": 0, "slot": 0,
            },
            "bootstrap_population": {
                "registry": asset(registry["key"]), "definition": asset(descriptor),
                "type": 1, "slot": source,
            },
        },
        "bindings": {
            "start_services": {
                "capability": "persistent.start", "operation": "mechanic",
                "asset": "persistent_module", "argument": 1, "wait": "requested",
            },
            "bootstrap_population": {
                "capability": "bootstrap.population", "operation": "population",
                "asset": "bootstrap_population", "argument": 1, "wait": "requested",
            },
        },
        "graphs": {
            "entry": {
                "name": f"Start {target.display} free-roam services", "domain": "composition",
                "steps": [{"id": "activate", "after": [], "commands": [
                    {"id": "services", "binding": "start_services"}]}], "receipts": {},
            },
            "world": {
                "name": f"{target.display} native free-roam startup", "domain": "nativeActivity",
                "steps": [{"id": "population", "after": [], "commands": [
                    {"id": "bootstrap", "binding": "bootstrap_population"}]}], "receipts": {},
            },
        },
        "roles": {"persistent": "world"},
        "entry": "entry",
        "modules": ["persistent"],
        "observations": [],
        "parameters": {
            "freeroam_population_enabled": 1,
            "freeroam_respawn_ms": 30000,
            # One native request per selected source, as in the installed
            # Mercury policy. Repeating one source does not select its sibling
            # squads: it creates copies with the same template and tactical row.
            "freeroam_patrol_count": 1,
            "freeroam_npc_count": 1,
            "host.tick_hz": 30,
        },
        "presentation": {
            "dialogue": {"bank": "0x00000000", "rows": [], "objective_cues": [],
                         "dispatch_timeout_ms": 100, "spacing_ms": 0},
            "cue_sets": {}, "action_sets": {}, "binding_tables": {},
        },
    }
    return json.dumps(result, indent=2) + "\n"


def emit() -> tuple[str, dict[str, str]]:
    activities = activity_catalog()
    routes = adventure_routes(activities)
    lines = [
        "#pragma once",
        "#include \"authored_registry.h\"",
        "#include <array>",
        "",
        "namespace sunrise::state::activity::coo::open_world {",
        "enum class PopulationKind : std::uint8_t { patrol, npc };",
        "struct PopulationBinding final {",
        "    std::uint16_t registry{},source{},rule{},tactical{};",
        "    std::int8_t tacticalRow{-1};PopulationKind kind{PopulationKind::patrol};bool hasRule{true};",
        "    std::uint8_t tacticalRows{}; // Native cost selection stays within this objective.",
        "};",
        "struct PlacementBinding final {std::uint16_t registry{},slot{};};",
        "struct AdventureBinding final {",
        "    std::uint16_t registry{},slot{},activity{};std::uint32_t selector{},activityHash{};",
        "    std::string_view package;",
        "};",
        "struct Destination final {",
        "    std::string_view display,activity;std::uint32_t scenario{};std::uint8_t primaryBubble{};",
        "    std::span<const registry::Definition> registries;",
        "    std::span<const PopulationBinding> populations;",
        "    std::span<const PlacementBinding> placements;",
        "    std::span<const AdventureBinding> adventures;",
        "};",
        "",
        "// Generated from installed build 86657 package descriptors. Public-event",
        "// objects are intentionally absent from every destination catalog below.",
    ]
    destinations = []
    scripts: dict[str, str] = {}
    for target in TARGETS:
        hashes, objects_by_bubble = scenario_objects(target.scenario)
        by_bubble_key: dict[tuple[int, int], dict] = {}
        all_by_bubble: dict[int, list[dict]] = {}
        for bubble, objects in objects_by_bubble.items():
            groups = []
            for object_tag in objects:
                group = resolve_group(object_tag)
                if group is None:
                    continue
                expected_mask = 1 << bubble
                if group["mask"] == expected_mask:
                    by_bubble_key[(bubble, group["key"])] = group
                groups.append(group)
            all_by_bubble[bubble] = groups

        selected: list[dict] = []
        population_rows: list[tuple[int, int, int, int, int, str, bool, int]] = []

        def add_group(group: dict) -> int:
            for index, existing in enumerate(selected):
                if existing["key"] == group["key"]:
                    if existing["object"] != group["object"] or existing["mask"] != group["mask"]:
                        raise ValueError(f"ambiguous registry {group['key']:08X}")
                    return index
            selected.append(group)
            return len(selected) - 1

        for bubble, wanted in target.ambient_bubbles:
            candidates = []
            for group in all_by_bubble[bubble]:
                types = {slot["type"] for slot in group["slots"]}
                if group["mask"] != 1 << bubble:
                    continue
                if types.issubset({1, 3, 30, 66, 70}) and {1, 3, 30, 66}.issubset(types):
                    candidates.append(group)
            if len(candidates) < wanted:
                raise ValueError(f"{target.activity} bubble {bubble} has only {len(candidates)} ordinary populations")
            for group in candidates[:wanted]:
                index = add_group(group)
                source = next(slot["index"] for slot in group["slots"] if slot["type"] == 1)
                tactical = next(slot["index"] for slot in group["slots"] if slot["type"] == 3)
                rule = [slot["index"] for slot in group["slots"] if slot["type"] == 66][-1]
                population_rows.append((index, source, rule, tactical, 0, "patrol", True,
                                        tactical_row_count(group, tactical)))

        for bubble, key in target.npcs:
            group = by_bubble_key.get((bubble, key))
            if group is None:
                raise ValueError(f"missing {target.activity} NPC {key:08X} in bubble {bubble}")
            source_slots = [slot["index"] for slot in group["slots"] if slot["type"] == 1]
            if not source_slots or not any(slot["type"] == 42 for slot in group["slots"]):
                raise ValueError(f"NPC registry {key:08X} lacks source/controller")
            index = add_group(group)
            population_rows.append((index, source_slots[0], 0, 0, -1, "npc", False, 0))

        placements: list[tuple[int, int]] = []
        adventures = []
        for bubble, key, slot_index, expected_choices in target.adventures:
            group = by_bubble_key.get((bubble, key))
            if group is None:
                raise ValueError(f"missing {target.activity} adventure registry {key:08X}")
            slot = next((value for value in group["slots"] if value["index"] == slot_index), None)
            if slot is None or slot["type"] != 4 or slot["component"] != 0x80809927:
                raise ValueError(f"bad adventure placement {key:08X}/{slot_index}")
            selected_selector = selector(slot["descriptor"])
            actual_choices = routes.get(selected_selector)
            expected_ordinals = [choice[0] for choice in expected_choices]
            if actual_choices != expected_ordinals:
                raise ValueError(f"route choices changed for selector {selected_selector:08X}: {actual_choices}")
            index = add_group(group)
            if (index, slot_index) not in placements:
                placements.append((index, slot_index))
            for ordinal, expected_name in expected_choices:
                identity, name = activities[ordinal]
                if name != expected_name:
                    raise ValueError(f"activity {ordinal} is {name}, expected {expected_name}")
                adventures.append((index, slot_index, ordinal, selected_selector, identity, name))

        if len(selected) + 5 > 32:
            raise ValueError(f"{target.activity} leaves no authored-roster headroom")
        if len(population_rows) > 32 or len(placements) > 32:
            raise ValueError(f"{target.activity} exceeds native batch capacity")
        if not population_rows or population_rows[0][0] != 0 \
                or selected[0]["mask"] != 1 << target.primary_bubble:
            raise ValueError(f"{target.activity} bootstrap population is not in its primary bubble")
        scripts[f"{target.activity}.json"] = policy(target, selected[0], population_rows[0][1])

        ns = target.namespace
        lines.extend(["", f"namespace {ns} {{"])
        for index, group in enumerate(selected):
            lines.append(f"inline constexpr std::array<registry::Slot,{len(group['slots'])}> kSlots{index}{{{{")
            for slot in group["slots"]:
                lines.append(
                    f"    {{{slot['index']},{slot['type']},{cpp_hex(slot['component'])},"
                    f"{cpp_hex(slot['sense'])},{cpp_hex(slot['authority'])},{cpp_hex(slot['descriptor'])}}},")
            lines.append("}};")
        lines.append(f"inline constexpr std::array<registry::Definition,{len(selected)}> kRegistries{{{{")
        for index, group in enumerate(selected):
            bubble = group["mask"].bit_length() - 1
            lines.append(
                f"    {{\"{target.activity}\",{cpp_hex(target.scenario)},{cpp_hex(group['key'])},"
                f"{cpp_hex(group['object'])},{cpp_hex(hashes[bubble])},{bubble},kSlots{index}}},")
        lines.extend(["}};", f"inline constexpr std::array<PopulationBinding,{len(population_rows)}> kPopulations{{{{"])
        for registry_index, source, rule, tactical, row, kind, has_rule, row_count in population_rows:
            lines.append(
                f"    {{{registry_index},{source},{rule},{tactical},{row},PopulationKind::{kind},"
                f"{'true' if has_rule else 'false'},{row_count}}},")
        lines.extend(["}};", f"inline constexpr std::array<PlacementBinding,{len(placements)}> kPlacements{{{{"])
        for registry_index, slot_index in placements:
            lines.append(f"    {{{registry_index},{slot_index}}},")
        lines.extend(["}};", f"inline constexpr std::array<AdventureBinding,{len(adventures)}> kAdventures{{{{"])
        for registry_index, slot_index, ordinal, selected_selector, identity, name in adventures:
            lines.append(
                f"    {{{registry_index},{slot_index},{ordinal},{cpp_hex(selected_selector)},"
                f"{cpp_hex(identity)},\"{name}\"}},")
        lines.extend([
            "}};",
            f"inline constexpr Destination kDestination{{\"{target.display}\",\"{target.activity}\","
            f"{cpp_hex(target.scenario)},{target.primary_bubble},kRegistries,kPopulations,kPlacements,kAdventures}};",
            f"}} // namespace {ns}",
        ])
        destinations.append(f"&{ns}::kDestination")

    lines.extend([
        "",
        f"inline constexpr std::array<const Destination*,{len(destinations)}> kDestinations{{{{",
        "    " + ",".join(destinations),
        "}};",
        "[[nodiscard]] constexpr bool required(std::uint32_t scenario,std::uint32_t objectTag,",
        "    std::uint32_t key,std::uint64_t explicitBubbleMask) noexcept {",
        "    for(const auto* destination:kDestinations)for(const auto& definition:destination->registries)",
        "        if(registry::required(definition,scenario,objectTag,key,explicitBubbleMask))return true;",
        "    return false;",
        "}",
        "} // namespace sunrise::state::activity::coo::open_world",
        "",
    ])
    return "\n".join(lines), scripts


def main() -> None:
    output = ROOT / "Sunrise/src/state/activity/coo/open_world_catalog.h"
    header, scripts = emit()
    output.write_text(header, encoding="utf-8")
    for name, contents in scripts.items():
        (ROOT / "Sunrise/scripts" / name).write_text(contents, encoding="utf-8")
    print(f"Generated {output.relative_to(ROOT)} and {len(scripts)} policies "
          f"from {len(TARGETS)} destination packages")


if __name__ == "__main__":
    main()
