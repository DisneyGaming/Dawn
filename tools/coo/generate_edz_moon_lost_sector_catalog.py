"""Generate package-checked EDZ and Moon open-world/Lost Sector catalogs."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_open_world_profiles as gen  # noqa: E402

RESEARCH = Path(__file__).with_name("lost_sector_edz_moon_native_research.json")
OUTPUT = ROOT / "Sunrise/src/server/runtime/activity/edz_moon_lost_sector_catalog.h"
GROUP_OUTPUT = ROOT / "Sunrise/src/state/activity/coo/edz_moon_lost_sector_group_catalog.h"

ROOTS = {
    "edz": ("edz_freeroam", 0x80B2F00A, 0x80BE1D6F, 0x52695108, 1, 19, 4, 6, 1, 3),
    "moon": ("luna_freeroam", 0x81503E69, 0x81567734, 0x2F2CA9F5, 0, 9, 2, 3, 8, 1),
}


def cpp(value: int) -> str:
    return f"0x{value:08X}U"


def value(text: str | int) -> int:
    return int(text, 16) if isinstance(text, str) else text


def resolve_group(scenario: int, bubble: int, key: int, object_tag: int) -> tuple[dict, list[int]]:
    hashes, objects = gen.scenario_objects(scenario)
    matches = []
    for tag in objects[bubble]:
        group = gen.resolve_group(tag)
        if group and group["key"] == key:
            matches.append(group)
    if len(matches) != 1 or matches[0]["object"] != object_tag \
            or matches[0]["mask"] != 1 << bubble:
        raise ValueError(f"registry {key:08X} is not an exact bubble-{bubble} member")
    return matches[0], hashes


def check_slot(group: dict, slot: int, slot_type: int, descriptor: int) -> dict:
    rows = [row for row in group["slots"] if row["index"] == slot and row["type"] == slot_type]
    if len(rows) != 1 or rows[0]["descriptor"] != descriptor:
        raise ValueError(f"slot drift {group['key']:08X}/{slot}/{slot_type}")
    return rows[0]


def emit_slots(lines: list[str], name: str, group: dict) -> None:
    lines.append(f"inline constexpr std::array<registry::Slot,{len(group['slots'])}> {name}{{{{")
    for slot in group["slots"]:
        lines.append(f"    {{{slot['index']},{slot['type']},{cpp(slot['component'])},{cpp(slot['sense'])},"
                     f"{cpp(slot['authority'])},{cpp(slot['descriptor'])}}},")
    lines.append("}};")


def generate() -> str:
    document = json.loads(RESEARCH.read_text(encoding="utf-8"))
    if document.get("schema") != 1:
        raise ValueError("research schema changed")
    digest = hashlib.sha256(RESEARCH.read_bytes()).hexdigest().upper()
    resolved: dict[str, dict] = {}
    for namespace, root in ROOTS.items():
        activity, scenario, bootstrap_object, bootstrap_key, *_ = root
        bootstrap, hashes = resolve_group(scenario, 0, bootstrap_key, bootstrap_object)
        sectors = [row for row in document["sectors"] if row["namespace"] == namespace]
        encounter_groups, reward_groups = [], []
        for sector in sectors:
            if value(sector["scenario"]) != scenario or not sector.get("one_stage_all_sources"):
                raise ValueError(f"sector root/stage drift: {sector['name']}")
            bubble = sector["bubble"]
            if hashes[bubble] != value(sector["bubble_hash"]):
                raise ValueError(f"bubble hash drift: {sector['name']}")
            encounter, _ = resolve_group(scenario, bubble, value(sector["registry"]),
                                         value(sector["encounter_object"]))
            chest = sector["chest"]
            reward, _ = resolve_group(scenario, bubble, value(chest["registry"]), value(chest["object"]))
            check_slot(reward, chest["slot"], 4, value(chest["descriptor"]))
            tactical = sector["tactical_candidates"][0]
            check_slot(encounter, tactical["slot"], 3, value(tactical["descriptor"]))
            seen = set()
            boss_hits = 0
            for source in sector["sources"]:
                slot = source["slot"]
                if slot in seen:
                    raise ValueError(f"duplicate source {sector['name']}/{slot}")
                seen.add(slot)
                check_slot(encounter, slot, 1, value(source["descriptor"]))
                if source["rule_role"] != "implicit_no_rule":
                    check_slot(encounter, source["rule_slot"], 66, value(source["rule_descriptor"]))
                if source["named_member_slot"] is not None:
                    check_slot(encounter, source["named_member_slot"], 2,
                               value(source["named_member_descriptor"]))
                categories = source["categories"]
                if not 1 <= len(categories) <= 8 or [row["index"] for row in categories] != list(range(len(categories))):
                    raise ValueError(f"category width/order {sector['name']}/{slot}")
                targets = [row["estimated_target"] for row in categories]
                if any(type(target) is not int or target < 0 or target > 63 for target in targets) \
                        or sum(targets) > 63 or targets[0] == 0:
                    raise ValueError(f"target bounds {sector['name']}/{slot}")
                boss_hits += bool(source["boss"])
            if boss_hits != 1 or sector["boss"]["source_slot"] not in seen:
                raise ValueError(f"boss identity {sector['name']}")
            encounter_groups.append(encounter)
            reward_groups.append(reward)
        resolved[namespace] = {"activity": activity, "scenario": scenario, "bootstrap": bootstrap,
                               "hashes": hashes, "sectors": sectors, "encounters": encounter_groups,
                               "rewards": reward_groups}

    lines = [
        "#pragma once", "", "#include \"lost_sector_runtime.h\"",
        "#include \"../../../state/activity/coo/open_world_catalog.h\"", "#include <array>", "",
        f"// Generated from installed build 86657; research SHA-256 {digest}.",
        "// All authored sector combat sources activate together; boss death alone enables the chest.", "",
    ]
    for namespace, data in resolved.items():
        root = ROOTS[namespace]
        _, scenario, _, _, source_slot, rule_slot, tactical_slot, tactical_rows, categories, request = root
        lines.append(f"namespace sunrise::state::activity::coo::open_world::{namespace} {{")
        emit_slots(lines, "kSlots0", data["bootstrap"])
        lines.extend([
            "inline constexpr std::array<registry::Definition,1> kRegistries{{",
            f"    {{\"{data['activity']}\",{cpp(scenario)},{cpp(data['bootstrap']['key'])},"
            f"{cpp(data['bootstrap']['object'])},{cpp(data['hashes'][0])},0,kSlots0}},",
            "}};",
            "inline constexpr std::array<PopulationBinding,1> kPopulations{{",
            f"    {{0,{source_slot},{rule_slot},{tactical_slot},0,PopulationKind::patrol,true,{tactical_rows},{request},0,{categories}}},",
            "}};",
            "inline constexpr std::array<PlacementBinding,0> kPlacements{};",
            "inline constexpr std::array<AdventureBinding,0> kAdventures{};",
            f"inline constexpr Destination kDestination{{\"{namespace.upper() if namespace == 'edz' else 'Moon'}\","
            f"\"{data['activity']}\",{cpp(scenario)},0,kRegistries,kPopulations,kPlacements,kAdventures}};",
            f"}} // namespace sunrise::state::activity::coo::open_world::{namespace}", "",
        ])

        lines.append(f"namespace sunrise::server::runtime::activity::lost_sector::catalog::{namespace} {{")
        for i, group in enumerate(data["encounters"]):
            emit_slots(lines, f"kEncounterSlots{i}", group)
        for i, group in enumerate(data["rewards"]):
            emit_slots(lines, f"kRewardSlots{i}", group)
        lines.append(f"inline constexpr std::array<registry::Definition,{len(data['sectors'])}> kRegistries{{{{")
        for i, (sector, group) in enumerate(zip(data["sectors"], data["encounters"])):
            lines.append(f"    {{\"{data['activity']}\",{cpp(data['scenario'])},{cpp(group['key'])},"
                         f"{cpp(group['object'])},{cpp(data['hashes'][sector['bubble']])},{sector['bubble']},"
                         f"kEncounterSlots{i}}},")
        lines.append("}};")
        lines.append(f"inline constexpr std::array<registry::Definition,{len(data['sectors'])}> kRewardRegistries{{{{")
        for i, (sector, group) in enumerate(zip(data["sectors"], data["rewards"])):
            lines.append(f"    {{\"{data['activity']}\",{cpp(data['scenario'])},{cpp(group['key'])},"
                         f"{cpp(group['object'])},{cpp(data['hashes'][sector['bubble']])},{sector['bubble']},"
                         f"kRewardSlots{i}}},")
        lines.append("}};")
        capabilities, policies, stages, sectors = [], [], [], []
        for sector_index, sector in enumerate(data["sectors"]):
            first = len(capabilities)
            tactical = sector["tactical_candidates"][0]["slot"]
            for source in sector["sources"]:
                targets = [row["estimated_target"] for row in source["categories"]]
                has_rule = source["rule_role"] != "implicit_no_rule"
                rule = source["rule_slot"] if has_rule else 0
                member = source["named_member_slot"]
                member_text = "population::kNoNamedMember" if member is None else str(member)
                capabilities.append(f"    {{&kRegistries[{sector_index}],{source['slot']},{rule},"
                                    f"{{{cpp(value(sector['registry']))},{tactical},0}},{str(has_rule).lower()},"
                                    f"0x00000001U,false,{member_text},{len(targets)}}},")
                extras = targets[2:] + [0] * (6 - max(0, len(targets) - 2))
                policies.append(f"    {{{targets[0]},{targets[1] if len(targets)>1 else 0},"
                                f"{str(source['boss']).lower()},{{{','.join(map(str, extras[:6]))}}},"
                                f"{len(targets)}}},")
            stages.append(f"    {{{first},{len(capabilities)-first}}},")
            sectors.append(f"    {{\"{sector['name']}\",{sector['bubble']},{sector_index},1,"
                           f"&kRewardRegistries[{sector_index}],{sector['chest']['slot']}}},")
        lines.extend([
            f"inline constexpr std::array<population::Capability,{len(capabilities)}> kCapabilities{{{{",
            *capabilities, "}};",
            f"inline constexpr std::array<SourcePolicy,{len(policies)}> kPolicies{{{{", *policies, "}};",
            f"inline constexpr std::array<Stage,{len(stages)}> kStages{{{{", *stages, "}};",
            f"inline constexpr std::array<Sector,{len(sectors)}> kSectors{{{{", *sectors, "}};",
            "[[nodiscard]] constexpr Definition definition(std::uint16_t capabilityBase) noexcept {",
            "    return {kSectors,kStages,kPolicies,capabilityBase};", "}",
            f"}} // namespace sunrise::server::runtime::activity::lost_sector::catalog::{namespace}", "",
        ])
    return "\n".join(lines)


def generate_group_catalog() -> str:
    """Emit the minimal client-side extraction allowlist without server runtime dependencies."""
    document = json.loads(RESEARCH.read_text(encoding="utf-8"))
    digest = hashlib.sha256(RESEARCH.read_bytes()).hexdigest().upper()
    groups: list[tuple[int, dict]] = []
    for namespace, root in ROOTS.items():
        _, scenario, bootstrap_object, bootstrap_key, *_ = root
        bootstrap, _ = resolve_group(scenario, 0, bootstrap_key, bootstrap_object)
        groups.append((scenario, bootstrap))
        for sector in [row for row in document["sectors"] if row["namespace"] == namespace]:
            bubble = sector["bubble"]
            encounter, _ = resolve_group(scenario, bubble, value(sector["registry"]),
                                         value(sector["encounter_object"]))
            chest = sector["chest"]
            reward, _ = resolve_group(scenario, bubble, value(chest["registry"]), value(chest["object"]))
            groups.extend(((scenario, encounter), (scenario, reward)))
    rows = []
    for scenario, group in groups:
        sources = sum(slot["type"] == 1 for slot in group["slots"])
        rows.append(f"    {{{cpp(scenario)},{cpp(group['object'])},{cpp(group['key'])},"
                    f"UINT64_C(0x{group['mask']:016X}),{sources}}},")
    return "\n".join([
        "#pragma once", "", "#include <array>", "#include <cstdint>", "",
        "namespace sunrise::state::activity::coo::edz_moon_lost_sector_groups {",
        f"// Generated from installed build 86657; research SHA-256 {digest}.",
        "struct RegistryGroup final { std::uint32_t scenario{},object{},key{}; std::uint64_t sliceMask{}; std::uint16_t sourceCount{}; };",
        f"inline constexpr std::array<RegistryGroup,{len(rows)}> kRegistryGroups{{{{", *rows, "}};",
        "[[nodiscard]] constexpr bool required(std::uint32_t scenario,std::uint32_t object,std::uint32_t key,std::uint64_t mask) noexcept {",
        "    for(const auto& group:kRegistryGroups)if(group.scenario==scenario && group.object==object",
        "        && group.key==key && group.sliceMask==mask)return true;",
        "    return false;",
        "}",
        "} // namespace sunrise::state::activity::coo::edz_moon_lost_sector_groups", "",
    ])


if __name__ == "__main__":
    text = generate()
    OUTPUT.write_text(text, encoding="utf-8", newline="\r\n")
    group_text = generate_group_catalog()
    GROUP_OUTPUT.write_text(group_text, encoding="utf-8", newline="\r\n")
    print(f"Generated {OUTPUT} ({len(text)} characters)")
    print(f"Generated {GROUP_OUTPUT} ({len(group_text)} characters)")
