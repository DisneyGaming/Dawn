"""Generate pinned native free-roam catalogs from the installed destination packages.

The generated header contains only ordinary ambient populations, destination NPCs,
and adventure/story flag placements. Public-event registries are deliberately not
selected. Run from the repository root with the installed package set available.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from collections import Counter
from functools import lru_cache
import hashlib
import json
import struct
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import package_read
import spawn_count_policy as squad_counts
import squad_cohort_policy as squad_cohorts

# Mirrors the bounded runtime/wire capacities. Package regression tests enforce
# these limits before generated catalogs can enter the authored roster.
SOURCE_CAPACITY = 256
REGISTRY_CAPACITY = 96
RETAINED_REQUEST_CAPACITY = 384
ADMISSION_BURST_CAPACITY = 1152
EVENT_BURST_CAPACITY = 3456


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
    # Explicit exceptions preserve previously selected, source-authored primary
    # rules. This is current host policy, not inferred retail scheduling.
    primary_sources: tuple[tuple[int, int], ...] = ()
    # Reviewed two-category identities. Other native two-category arrays retain
    # legacy one-category publication until separately enabled.
    two_category_sources: tuple[tuple[int, int], ...] = ()


TARGETS = (
    Target("io", "Io", "eden_freeroam", 0x80B56B1B, 4,
           ((0, 15), (1, 17), (2, 9), (4, 29), (15, 5), (17, 29), (20, 21), (21, 11)),
           ((4, 0x36E4495D),),
           ((4, 0xBCD2B135, 9, ((1146, "adventure_outlaws_cabal_dbda"),)),
            (4, 0xBCD2B135, 10, ((1147, "adventure_outlaws_cabal_dbdb"),)),
            (17, 0x5D376320, 0, ((1145, "adventure_outlaws_cabal_oada"),))),
           two_category_sources=((0xDF6FFA82,0),(0x00654B89,0),(0xC301BE41,0),(0xB9B49307,0),(0xD81F9ACD,0),(0x7EF2EBBA,0),(0xBD9BF7A9,0),(0xF12478FC,0),(0x38532046,1),(0x38532046,2),(0xBD9BF7AA,0),(0x07216F67,1),(0x544B4757,3),(0x5238F6F3,0),(0x7F702948,3),(0x2A79AC34,0))),
    Target("titan", "Titan", "fleet_freeroam", 0x80B3E142, 2,
           ((0, 5), (1, 5), (2, 27), (5, 12), (7, 31), (11, 9)),
           ((2, 0x05324D75),),
           ((2, 0xF2C8FA93, 0, ((1142, "adventure_outlaws_hive_bada"),)),
            (7, 0x9B958891, 0, ((1143, "adventure_outlaws_hive_plda"),)),
            (7, 0x9B958891, 1, ((1144, "adventure_outlaws_hive_pldb"),))),
           two_category_sources=((0x1EE02F73,0),(0x75AF16D5,1),(0x75AF16D5,2),(0x75AF16D5,3),(0x1FD9CD4B,1),(0xD2C94BE3,1),(0xFFEBF746,0),(0xC691626B,0),(0x56B27F78,1),(0x56B27F78,2),(0x2577170D,2),(0x74AE0CA8,0),(0xC5A11199,1),(0xC5A11199,2),(0x67EACF2B,2),(0xDACDA04B,1))),
    Target("mars", "Mars", "polaris_freeroam", 0x80F6AB20, 1,
           ((0, 21), (1, 24), (5, 24), (7, 22), (9, 5), (10, 8)),
           ((1, 0x0D1B60CF),),
           ((1, 0x7EAECBEB, 1, ((1151, "adventure_outlaws_polaris_brda"),)),
            (5, 0x63CAF4E4, 3, ((1152, "adventure_outlaws_polaris_glda"),))),
           two_category_sources=((0x8717837E,1),(0x8717837E,2),(0x14810D98,2),(0x1B2FFE05,2),(0xAACB9196,1),(0xAACB9196,2))),
    Target("nessus", "Nessus", "planet_x_freeroam", 0x80B43A1C, 3,
           ((1, 27), (2, 7), (3, 17), (4, 12), (8, 6), (10, 11), (11, 38), (13, 11), (30, 36), (32, 13), (33, 16), (37, 31), (38, 8)),
           ((3, 0x7B3D65F8),),
           ((30, 0xDA814632, 0, ((1140, "adventure_outlaws_vex_scda"),)),
            (37, 0xCA092634, 4, ((1141, "adventure_outlaws_vex_wwda"),))),
           two_category_sources=((0x223BD54D,1),(0x9B4BD35A,1),(0xB7E779AC,1),(0x8300E551,2),(0x61BE51A1,1),(0x61BE51A1,2),(0xF10934F1,2),(0x747A05D8,2),(0x747A05D8,3),(0x388426A2,1),(0x6F8DF85C,0),(0x171B29F1,2),(0x171B29F1,3),(0x87218FB0,3),(0xA9602D0A,1),(0xFB2195D2,2),(0x5CCEA00E,1),(0x9CD851C0,2),(0xA1CE40EF,2),(0x9519F1AF,2),(0xAE47D797,0),(0x5DDFB36D,1),(0x6528172C,2),(0xDBE1BAA9,0),(0xDCE0C4A2,3),(0x688A9D3F,0),(0xF5B72314,0),(0x87AE0BCF,1),(0xAD8A7075,1),(0xD29B10F1,1))),
    Target("tangled_shore", "The Tangled Shore", "tangled_shore_freeroam", 0x80FC9645, 7,
           ((5, 3), (7, 6), (9, 38), (13, 14), (14, 39), (18, 32), (20, 7)),
           ((21, 0x6D47E9B3),),
           ((21, 0x8ADAE5CC, 0,
             ((404, "adventure_brainwash_heroic"), (405, "adventure_money_trail_heroic"),
              (406, "adventure_piker_gang_heroic"), (407, "adventure_showdown_heroic"),
              (408, "adventure_soul_stealer_heroic"), (409, "adventure_stash_heroic"))),),
           two_category_sources=((0xBD441DD1,1),(0xE48E0E82,0),(0xE48E0E82,1),(0x96F4E362,0))),
    Target("dreaming_city", "The Dreaming City", "dreaming_city_freeroam", 0x80F1404D, 1,
           ((1, 3), (2, 3), (11, 3), (18, 3), (19, 3), (20, 3)),
           ((1, 0x87660C45), (18, 0xDBB95609), (20, 0x4ECC8169)),
           ((1, 0x1046B790, 0, ((372, "mission_demontower"),)),
            (18, 0x21EDF044, 0, ((373, "mission_bridge"),)),
            (20, 0x03B24A6D, 0, ((371, "mission_tunnel"),))),
           primary_sources=((0x1170A615, 0), (0x4DDE0388, 0), (0x843307B2, 0),
                            (0x4E54F61A, 0), (0xCA0046CA, 0), (0xB8DAD7D0, 0),
                            (0x33E12E3B, 0), (0x99D49DC8, 0))),
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


def source_category_count(group: dict, source_index: int) -> int:
    sources = [slot for slot in group["slots"]
               if slot["index"] == source_index and slot["type"] == 1]
    if len(sources) != 1:
        raise ValueError("source slot is missing or ambiguous")
    actual_class, raw = package_read.read(sources[0]["descriptor"])
    if actual_class != 0x80809C36:
        raise ValueError("source descriptor class")
    definition = relative(raw, 24)
    if definition < 4 or definition + 0xB8 > len(raw) \
            or u32(raw, definition - 4) != 0x8080948F:
        raise ValueError("source definition class or bounds")
    if struct.unpack_from("<IHH", raw, definition + 48) != (group["key"], 1, source_index):
        raise ValueError("source native identity does not match its registry slot")
    categories = array_rows(raw, definition + 0xA8, 104, 0x80808356)
    if not 1 <= len(categories) <= 2:
        raise ValueError(f"source has unsupported native category count {len(categories)}")
    return len(categories)


def published_category_count(target: Target, group: dict, source_index: int) -> int:
    native = source_category_count(group, source_index)
    reviewed = (group["key"], source_index) in target.two_category_sources
    if reviewed and native != 2:
        raise ValueError("reviewed two-category source no longer has two native categories")
    return 2 if reviewed else 1


def request_overrides(pin: dict, categories: int) -> tuple[int, int]:
    """Keep inferred counts explicit and separate from unchanged starter policy."""
    if categories not in (1, 2):
        raise ValueError("unsupported reviewed category width")
    policy = pin.get("request_policy")
    if policy is None:
        return 0, 0
    if not isinstance(policy, dict) or policy.get("kind") not in ("video_observed", "bounded_inference"):
        raise ValueError("request policy requires an explicit evidence kind")
    targets = policy.get("targets")
    if not isinstance(targets, list) or len(targets) != categories \
            or any(type(value) is not int or not 0 <= value <= 21 for value in targets) \
            or not targets[0]:
        raise ValueError("request targets must match the reviewed category width and capacity")
    evidence = policy.get("evidence")
    if not isinstance(evidence, list) or not evidence \
            or any(not isinstance(url, str) or not url.startswith("https://") for url in evidence) \
            or not isinstance(policy.get("reason"), str) or not policy["reason"].strip():
        raise ValueError("request policy requires evidence links and a rationale")
    unresolved = policy.get("unresolved")
    if not isinstance(unresolved, list) or any(not isinstance(item, str) or not item.strip() for item in unresolved):
        raise ValueError("request policy must explicitly record unresolved coverage")
    return targets[0], targets[1] if categories == 2 else 0


@lru_cache(maxsize=None)
def template_type_evidence(entity: int) -> tuple[frozenset[int], str]:
    """Read type constraints, not filenames or a guessed display-name mapping."""
    cls, raw = package_read.read(entity)
    if cls != 0x80809C0F:
        raise ValueError("combatant template class")
    options: set[int] = set()
    evidence = [(entity, hashlib.sha256(raw).hexdigest())]
    for row in array_rows(raw, 16, 12, 0x80809C04):
        resource = u32(raw, row)
        resource_cls, blob = package_read.read(resource)
        if resource_cls != 0x80809C36 or len(blob) < 32:
            continue
        body = relative(blob, 24)
        if body < 4 or u32(blob, body - 4) != 0x808038A1:
            continue
        evidence.append((resource, hashlib.sha256(blob).hexdigest()))
        for constraint in array_rows(blob, body + 0xA8, 24, 0x808038AA):
            if u32(blob, constraint + 4) == 0x26170C92:
                options.update(u32(blob, at) for at in array_rows(blob, constraint + 8, 4, 0x80800070))
    digest = hashlib.sha256(json.dumps(evidence, separators=(",", ":")).encode("ascii")).hexdigest()
    return frozenset(options), digest


def source_count_choices(group: dict, source_index: int) -> list[list[dict]]:
    """Inspect all six native variants, including each positive-weight choice."""
    source_category_count(group, source_index)  # Validate class, identity and bounds first.
    source = next(slot for slot in group["slots"] if slot["type"] == 1 and slot["index"] == source_index)
    _, raw = package_read.read(source["descriptor"])
    body = relative(raw, 24)
    result = []
    for category in array_rows(raw, body + 0xA8, 104, 0x80808356):
        choices = []
        for variant in range(6):
            for at in array_rows(raw, category + 8 + variant * 16, 24, 0x80808358):
                if not u32(raw, at + 12):
                    continue
                entity_body = relative(raw, at)
                if u32(raw, entity_body - 4) != 0x808099D8:
                    raise ValueError("source choice entity class")
                entity = u32(raw, entity_body)
                selector = relative(raw, entity_body + 0x78)
                if u32(raw, selector - 4) in (0x80807EB6, 0x80804B8B):
                    selector = relative(raw, selector)
                attributes = {}
                if u32(raw, selector - 4) == 0x808038A0:
                    for attr in array_rows(raw, selector + 16, 8, 0x8080389F):
                        key, value = u32(raw, attr), u32(raw, attr + 4)
                        if key in attributes and attributes[key] != value:
                            raise ValueError("conflicting native choice selector attributes")
                        attributes[key] = value
                types, template_digest = template_type_evidence(entity)
                selected_type = attributes.get(0x26170C92)
                if selected_type is not None:
                    # A concrete native selector may narrow a polymorphic template.
                    types = frozenset((selected_type,)) if selected_type in types else frozenset()
                species = squad_counts.SPECIES_BY_HASH.get(next(iter(types))) if len(types) == 1 else None
                choices.append({"entity": f"{entity:08X}", "template_evidence_sha256": template_digest, "species": species,
                                "rank": squad_counts.RANK_BY_HASH.get(attributes.get(0xB10F785D))})
        result.append(choices)
    return result


def count_choices_digest(choices: list[list[dict]]) -> str:
    return hashlib.sha256(json.dumps(choices, sort_keys=True, separators=(",", ":")).encode("ascii")).hexdigest()


def species_request_overrides(target: Target, group: dict, source: int,
                              categories: int, pin: dict) -> tuple[int, int]:
    if target.namespace == "dreaming_city":  # Explicitly deferred by the user.
        return request_overrides(pin, categories)
    choices = source_count_choices(group, source)
    if pin.get("count_choices_sha256") != count_choices_digest(choices):
        raise ValueError("count-policy native templates or selectors changed; review required")
    counts = [squad_counts.category_target(f"{target.activity}:{group['key']:08X}:{source}:{index}", row)
              for index, row in enumerate(choices[:categories])]
    first = counts[0][0]
    second = 0
    if categories == 2:
        # Only reviewed category widths can acquire escorts. An unresolved
        # secondary category remains dormant, rather than guessing its type.
        if not counts[1][1].startswith("unresolved"):
            second = counts[1][0]
        # At most one boss-capable category in this source is active at once.
        if any(c.get("rank") in squad_counts.SINGLETON_RANKS for c in choices[0]) \
                and any(c.get("rank") in squad_counts.SINGLETON_RANKS for c in choices[1]):
            second = 0
        first_fixed = {c.get("species") for c in choices[0]} & squad_counts.FIXED_SPECIES
        second_fixed = {c.get("species") for c in choices[1]} & squad_counts.FIXED_SPECIES
        if first_fixed & second_fixed:
            # Two categories must not independently request the same fixed-one
            # species. Preserve the primary category and leave the other idle.
            second = 0
    return first, second


def source_spawn_rule(group: dict, source_index: int, role: str) -> int:
    """Resolve an explicitly chosen rule from this source's native definition.

    Registry slot order does not associate a rule with an enemy source. In
    particular, the last type-66 slot may belong to a sibling source or to an
    alternate placement. Preserve the referenced native rule without copying
    point transforms or manufacturing a replacement volume.
    """
    if role not in ("primary", "fallback"):
        raise ValueError("spawn rule role must be primary or fallback")
    sources = [slot for slot in group["slots"]
               if slot["index"] == source_index and slot["type"] == 1]
    if len(sources) != 1:
        raise ValueError("source slot is missing or ambiguous")
    actual_class, raw = package_read.read(sources[0]["descriptor"])
    if actual_class != 0x80809C36:
        raise ValueError("source descriptor class")
    definition = relative(raw, 24)
    if definition < 4 or definition + 0xA8 > len(raw) \
            or u32(raw, definition - 4) != 0x8080948F:
        raise ValueError("source definition class or bounds")
    if struct.unpack_from("<IHH", raw, definition + 48) != (group["key"], 1, source_index):
        raise ValueError("source native identity does not match its registry slot")
    key, kind, index = struct.unpack_from("<IHH", raw, definition + (0x98 if role == "primary" else 0xA0))
    if (key, kind, index) == (0x811C9DC5, 255, 65535):
        raise ValueError(f"source has no authored {role} rule; explicit policy required")
    rules = [slot for slot in group["slots"] if slot["index"] == index and slot["type"] == 66]
    if key != group["key"] or kind != 66 or len(rules) != 1:
        raise ValueError("source rule does not resolve uniquely inside its owning encounter")
    rule_class, rule_raw = package_read.read(rules[0]["descriptor"])
    if rule_class != 0x80809C36:
        raise ValueError("spawn rule descriptor class")
    rule_body = relative(rule_raw, 24)
    if rule_body < 4 or u32(rule_raw, rule_body - 4) != 0x808094D0:
        raise ValueError("spawn rule definition class")
    return index


def selected_patrols(target: Target, groups: dict, pins: list[dict]) -> list[tuple[dict, int, int, int]]:
    """Resolve reviewed identities, never the first N encounters or source slots.

    Pins preserve the existing starter policy, not retail concurrency or counts.
    Descriptor digests also pin the authored placement-rule bytes: a different
    rule volume under the same tag requires another review before regeneration.
    """
    if Counter(pin["bubble"] for pin in pins) != Counter(dict(target.ambient_bubbles)):
        raise ValueError(f"{target.activity} pinned coverage differs from its explicit starter policy")
    result = []
    identities = set()
    for pin in pins:
        key = int(pin["registry"], 16)
        group = groups.get((pin["bubble"], key))
        if group is None or group["object"] != int(pin["object"], 16) \
                or group["mask"] != 1 << pin["bubble"]:
            raise ValueError("pinned encounter is missing, ambiguous or in another bubble")
        source, tactical = pin["source"], pin["tactical"]
        if (key, source) in identities:
            raise ValueError("duplicate pinned enemy source")
        identities.add((key, source))
        types = {slot["type"] for slot in group["slots"]}
        if not types.issubset({1, 3, 30, 66, 70}) or not {1, 3, 30, 66}.issubset(types):
            raise ValueError("pinned encounter no longer has the reviewed ambient shape")
        role = "primary" if (key, source) in target.primary_sources else "fallback"
        if pin["role"] != role:
            raise ValueError("pinned spawn-rule role differs from explicit host policy")
        for label, kind, index in (("source", 1, source), ("tactical", 3, tactical), ("rule", 66, pin["rule"])):
            matches = [s for s in group["slots"] if s["type"] == kind and s["index"] == index]
            if len(matches) != 1 or matches[0]["descriptor"] != int(pin[label + "_descriptor"], 16):
                raise ValueError(f"pinned {label} descriptor is missing or changed")
            actual_class, blob = package_read.read(matches[0]["descriptor"])
            if actual_class != 0x80809C36 or hashlib.sha256(blob).hexdigest() != pin[label + "_sha256"]:
                raise ValueError(f"pinned {label} bytes changed; native bindings require review")
        rule = source_spawn_rule(group, source, role)
        if rule != pin["rule"]:
            raise ValueError("pinned rule is not this source's authored reference")
        result.append((group, source, tactical, rule))
    return result


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
    selection = json.loads((Path(__file__).with_name("open_world_spawn_selections.json")).read_text())
    cohorts = json.loads((Path(__file__).with_name("open_world_squad_cohorts.json")).read_text())
    if selection["schema_version"] != 1 or selection["package_build"] != 86657 \
            or set(selection["destinations"]) != {t.activity for t in TARGETS}:
        raise ValueError("unsupported or incomplete open-world spawn selections")
    if cohorts["schema_version"] != 1 or cohorts["package_build"] != 86657 \
            or not set(cohorts["destinations"]).issubset({t.activity for t in TARGETS if t.namespace != "dreaming_city"}):
        raise ValueError("unsupported or out-of-scope squad cohort policy")
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
        "    std::uint8_t requestOverride{}; // Zero preserves the destination-wide request policy.",
        "    std::uint8_t secondRequestOverride{}; // Zero keeps the second native category dormant.",
        "    std::uint8_t categories{1}; // Reviewed native category-array width to publish.",
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
        for bubble, objects in objects_by_bubble.items():
            for object_tag in objects:
                group = resolve_group(object_tag)
                if group is None:
                    continue
                expected_mask = 1 << bubble
                if group["mask"] == expected_mask:
                    existing = by_bubble_key.get((bubble, group["key"]))
                    if existing is not None and existing != group:
                        raise ValueError(f"ambiguous registry {group['key']:08X} in bubble {bubble}")
                    by_bubble_key[(bubble, group["key"])] = group

        selected: list[dict] = []
        population_rows: list[tuple[int, int, int, int, int, str, bool, int, int, int, int]] = []

        def add_group(group: dict) -> int:
            for index, existing in enumerate(selected):
                if existing["key"] == group["key"]:
                    if existing["object"] != group["object"] or existing["mask"] != group["mask"]:
                        raise ValueError(f"ambiguous registry {group['key']:08X}")
                    return index
            selected.append(group)
            return len(selected) - 1

        pins = selection["destinations"][target.activity]
        resolved = selected_patrols(target, by_bubble_key, pins)
        cohort_targets = squad_cohorts.resolve(sys.modules[__name__], target, resolved,
                                               cohorts["destinations"].get(target.activity, []))
        for (group, source, tactical, rule), pin in zip(resolved, pins, strict=True):
            index = add_group(group)
            categories = published_category_count(target,group,source)
            first, second = species_request_overrides(target, group, source, categories, pin)
            # Count-template pins above still validate every source. A reviewed
            # complete squad distributes its shared budget across the siblings.
            first, second = cohort_targets.get((group["key"], source), (first, second))
            population_rows.append((index, source, rule, tactical, 0, "patrol", True,
                                    tactical_row_count(group, tactical),categories,first,second))

        for bubble, key in target.npcs:
            group = by_bubble_key.get((bubble, key))
            if group is None:
                raise ValueError(f"missing {target.activity} NPC {key:08X} in bubble {bubble}")
            source_slots = [slot["index"] for slot in group["slots"] if slot["type"] == 1]
            if not source_slots or not any(slot["type"] == 42 for slot in group["slots"]):
                raise ValueError(f"NPC registry {key:08X} lacks source/controller")
            index = add_group(group)
            population_rows.append((index, source_slots[0], 0, 0, -1, "npc", False, 0,
                                    published_category_count(target,group,source_slots[0]),0,0))

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

        if len(selected) + 5 > REGISTRY_CAPACITY:
            raise ValueError(f"{target.activity} leaves no authored-roster headroom")
        if len(population_rows) > SOURCE_CAPACITY or len(placements) > 32:
            raise ValueError(f"{target.activity} exceeds native batch capacity")
        retained_requests = sum((first or 1) + second for _, _, _, _, _, _, _, _, _, first, second in population_rows)
        if retained_requests > RETAINED_REQUEST_CAPACITY or retained_requests * 3 > ADMISSION_BURST_CAPACITY or retained_requests * 9 > EVENT_BURST_CAPACITY:
            raise ValueError(f"{target.activity} exceeds conservative retained actor/mailbox capacity")
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
        for registry_index, source, rule, tactical, row, kind, has_rule, row_count, categories, first, second in population_rows:
            lines.append(
                f"    {{{registry_index},{source},{rule},{tactical},{row},PopulationKind::{kind},"
                f"{'true' if has_rule else 'false'},{row_count},{first},{second},{categories}}},")
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
    output.write_text(header, encoding="utf-8", newline="\r\n")
    for name, contents in scripts.items():
        policy_output = ROOT / "Sunrise/scripts" / name
        # Avoid normalizing intentionally retained mixed line endings when the
        # generated policy text is otherwise byte-for-byte equivalent by line.
        if policy_output.read_text(encoding="utf-8") != contents:
            policy_output.write_text(contents, encoding="utf-8", newline="\r\n")
    print(f"Generated {output.relative_to(ROOT)} and {len(scripts)} policies "
          f"from {len(TARGETS)} destination packages")


if __name__ == "__main__":
    main()
