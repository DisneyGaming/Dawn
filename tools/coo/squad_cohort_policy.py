"""Explicit whole-encounter budgets and native evidence for reviewed siblings.

These are host composition policies, not recovered retail state machines. No
coordinates are emitted: the runtime still publishes each source's authored rule.
"""
from collections import Counter
import hashlib
import json
import struct

import spawn_count_policy as counts


def validate_budget(members, choices_by_source, widths):
    """Conservatively bound every possible weighted result across the cohort."""
    targets = {}
    species_totals = Counter()
    leaders = 0
    for member in members:
        source, vector = member["source"], member["targets"]
        if type(source) is not int or source in targets or source not in choices_by_source:
            raise ValueError("duplicate or unknown cohort source")
        if not isinstance(vector, list) or len(vector) != widths[source] \
                or any(type(n) is not int or not 0 <= n <= 21 for n in vector) or not vector[0]:
            raise ValueError("cohort target width or request capacity")
        choices = choices_by_source[source]
        for category, amount in enumerate(vector):
            row = choices[category]
            unresolved = not row or any(c.get("species") not in counts.RULES
                                       or c.get("rank") not in counts.RANK_BY_HASH.values() for c in row)
            if unresolved and amount > (1 if category == 0 else 0):
                raise ValueError("unknown primary must be one; unknown secondary must be dormant")
            if any(c.get("rank") in counts.SINGLETON_RANKS for c in row):
                leaders += amount
            for species in {c.get("species") for c in row} & counts.RULES.keys():
                # Choices in one category are alternatives, never extra quotas.
                # Across independent categories this is a worst-case total.
                species_totals[species] += amount
        targets[source] = tuple(vector) if len(vector) == 2 else (vector[0], 0)
    if set(targets) != set(choices_by_source):
        raise ValueError("cohort must describe every enabled sibling exactly once")
    if leaders > 1:
        raise ValueError("cohort can select multiple segmented-rank leaders")
    for species, total in species_totals.items():
        maximum = 1 if species in counts.UNVERIFIED_ONE_DEFAULTS else counts.RULES[species][2]
        if total > maximum:
            raise ValueError(f"cohort exceeds shared {species} budget: {total}>{maximum}")
    return targets


def native_evidence(gen, target, group, point_lists, point_containers, point_owner_objects=(),
                    evidence_mode=None, selected_rules=None, unresolved_primary_rules=None):
    """Re-read the owner chain, encounter closure and exact rule/point joins.

    This validates the reviewed Mars layout. Unsupported ownership forms fail
    closed; adding another layout requires its own review, not a guessed join.
    """
    dependencies = {}

    def read(tag, expected=None):
        cls, raw = gen.package_read.read(tag)
        if expected is not None and cls != expected:
            raise ValueError("cohort dependency class changed")
        dependencies[f"{tag:08X}"] = [f"{cls:08X}", hashlib.sha256(raw).hexdigest()]
        return cls, raw

    extra_owners = {int(tag, 16) for tag in point_owner_objects}
    if len(extra_owners) != len(point_owner_objects) or group["object"] in extra_owners:
        raise ValueError("duplicate point owner object")
    owner_records = {tag: [] for tag in extra_owners | {group["object"]}}
    _, scenario = read(target.scenario, 0x80809994)
    for bubble_index, bubble in enumerate(gen.array_rows(scenario, 80, 24, 0x8080924D)):
        for state_index, state in enumerate(gen.array_rows(scenario, bubble + 8, 76, 0x8080924F)):
            entry_tag = gen.u32(scenario, state + 68)
            if entry_tag in (0, 0xFFFFFFFF):
                continue
            entry_cls, entry = gen.package_read.read(entry_tag)
            if entry_cls != 0x8080925B:
                raise ValueError("cohort owner entry class")
            container_tag = gen.u32(entry, 20)
            if container_tag in (0, 0xFFFFFFFF):
                continue
            container_cls, container = gen.package_read.read(container_tag)
            if container_cls != 0x8080925E:
                raise ValueError("cohort owner container class")
            for lane, offset in enumerate((8, 24, 40)):
                for member_index, at in enumerate(gen.array_rows(container, offset, 4, 0x80809260)):
                    object_tag = gen.u32(container, at)
                    if object_tag not in owner_records:
                        continue
                    read(entry_tag, 0x8080925B)
                    read(container_tag, 0x8080925E)
                    owner_records[object_tag].append({"bubble": bubble_index, "bubble_hash": gen.u32(scenario, bubble),
                                   "state": state_index, "state_sha256": hashlib.sha256(scenario[state:state+76]).hexdigest(),
                                   "map_index": gen.u32(scenario, state + 28), "entry": entry_tag,
                                   "container": container_tag, "lane": lane, "member": member_index})
    owners = owner_records[group["object"]]
    if len(owners) != 1 or group["mask"] != 1 << owners[0]["bubble"] or owners[0]["lane"] != 1:
        raise ValueError("cohort requires one reviewed whole-encounter owner")
    for tag in extra_owners:
        rows = owner_records[tag]
        if len(rows) != 1 or any(rows[0][key] != owners[0][key] for key in owners[0] if key != "member"):
            raise ValueError("referenced point owner is not in the exact same scenario state and lane")

    _, obj = read(group["object"], 0x80809462)
    pending = []
    for bubble in gen.array_rows(obj, 56, 24):
        pending.extend(gen.u32(obj, row) for row in gen.array_rows(obj, bubble + 8, 4))
    # An authored GUID may intentionally name another same-bubble object's
    # point list. Record that dependency; never activate the other encounter.
    for tag in sorted(extra_owners):
        _, other = read(tag, 0x80809462)
        for bubble in gen.array_rows(other, 56, 24):
            if struct.unpack_from("<i", other, bubble)[0] in (-1, owners[0]["bubble"]):
                pending.extend(gen.u32(other, row) for row in gen.array_rows(other, bubble + 8, 4))
    visited, owned_lists = set(), set()
    while pending:
        tag = pending.pop()
        if tag in visited or tag in (0, 0xFFFFFFFF):
            continue
        visited.add(tag)
        cls, raw = read(tag)
        if cls == 0x80809B14:
            pending.append(gen.u32(raw, 12))
        elif cls == 0x80809468:
            named = gen.u32(raw, 8)
            if named not in (0, 0xFFFFFFFF):
                owned_lists.add(named)
                pending.append(named)
            pending.extend(gen.u32(raw, row) for row in gen.array_rows(raw, 16, 4))
        elif cls not in (0x80809C36, 0x808099D6):
            raise ValueError("unreviewed cohort component wrapper")

    lists = {int(tag, 16) for tag in point_lists}
    containers = {int(tag, 16) for tag in point_containers}
    if not lists or len(lists) != len(point_lists) or len(containers) != len(point_containers):
        raise ValueError("cohort requires unique point-list and map-container pins")
    mapped = lists & owned_lists
    map_index = owners[0]["map_index"]
    if map_index >= 256:
        raise ValueError("cohort map index outside authored mask")
    for tag in containers:
        _, raw = read(tag, 0x80808A54)
        if not raw[8 + map_index // 8] & (1 << (map_index % 8)):
            raise ValueError("spawn point container does not belong to the encounter map")
        matches = {gen.u32(raw, row) for row in gen.array_rows(raw, 40, 4, 0x80808BB0)} & lists
        if not matches:
            raise ValueError("unrelated spawn point container")
        mapped.update(matches)
    if mapped != lists:
        raise ValueError("spawn point list is not joined to the encounter map")
    points = {}
    for tag in sorted(lists):
        _, raw = read(tag, 0x808099D6)
        for index, at in enumerate(gen.array_rows(raw, 8, 144, 0x808099D8)):
            guid = struct.unpack_from("<Q", raw, at + 112)[0]
            points.setdefault(guid, []).append([tag, index, hashlib.sha256(raw[at:at+144]).hexdigest()])
    source_slots = [slot for slot in group["slots"] if slot["type"] == 1]
    if evidence_mode is None:
        if selected_rules is not None or unresolved_primary_rules is not None:
            raise ValueError("full native evidence does not accept selected-role fields")
        roles = ("primary", "fallback")
    elif evidence_mode == "selected_fallback":
        if not isinstance(selected_rules, dict) or set(selected_rules) != {slot["index"] for slot in source_slots} \
                or unresolved_primary_rules is None:
            raise ValueError("selected-fallback proof requires every selected source rule")
        for slot in source_slots:
            fallback = gen.source_spawn_rule(group, slot["index"], "fallback")
            primary = gen.source_spawn_rule(group, slot["index"], "primary")
            if selected_rules[slot["index"]] != fallback or fallback == primary:
                raise ValueError("selected-fallback proof requires a distinct authored fallback for every source")
        roles = ("fallback",)
    else:
        raise ValueError("unsupported cohort native evidence mode")

    joins = []
    for slot in source_slots:
        for role in roles:
            rule_index = gen.source_spawn_rule(group, slot["index"], role)
            rule = next(s for s in group["slots"] if s["type"] == 66 and s["index"] == rule_index)
            _, raw = read(rule["descriptor"], 0x80809C36)
            rule_points = gen.array_rows(raw, 0x210, 72, 0x80809840)
            if not rule_points:
                raise ValueError("cohort source has no decoded authored rule points")
            for at in rule_points:
                guid = struct.unpack_from("<Q", raw, at)[0]
                matches = points.get(guid, [])
                if len(matches) != 1:
                    raise ValueError("cohort source point GUID is missing or ambiguous")
                joins.append([slot["index"], role, rule_index, f"{guid:016X}", matches[0]])
    if evidence_mode is None:
        record = {"dependencies": dependencies, "owners": owner_records, "joins": joins}
    else:
        actual = {}
        for slot in source_slots:
            rule_index = gen.source_spawn_rule(group, slot["index"], "primary")
            rule = next(s for s in group["slots"] if s["type"] == 66 and s["index"] == rule_index)
            _, raw = read(rule["descriptor"], 0x80809C36)
            authored = []
            for at in gen.array_rows(raw, 0x210, 72, 0x80809840):
                guid = struct.unpack_from("<Q", raw, at)[0]
                matches = points.get(guid, [])
                if len(matches) > 1:
                    raise ValueError("nonselected primary point GUID is ambiguous")
                if not matches:
                    authored.append(f"{guid:016X}")
            key = (rule_index, rule["descriptor"])
            row = actual.setdefault(key, {"sources": [], "rule": rule_index,
                                         "descriptor": f"{rule['descriptor']:08X}",
                                         "sha256": dependencies[f"{rule['descriptor']:08X}"][1],
                                         "unresolved_guids": sorted(set(authored))})
            if row["unresolved_guids"] != sorted(set(authored)):
                raise ValueError("shared primary rule disclosure changed across sources")
            row["sources"].append(slot["index"])
        actual_rows = sorted(({**row, "sources": sorted(row["sources"])} for row in actual.values()),
                             key=lambda row: (row["rule"], row["descriptor"]))
        if not isinstance(unresolved_primary_rules, list) or unresolved_primary_rules != actual_rows \
                or any(not row["unresolved_guids"] for row in actual_rows):
            raise ValueError("nonselected primary rule disclosure is incomplete or changed")
        record = {"dependencies": dependencies, "owners": owner_records, "joins": joins,
                  "evidence_mode": evidence_mode,
                  "unresolved_nonselected_primary_rules": actual_rows}
    return hashlib.sha256(json.dumps(record, sort_keys=True, separators=(",", ":")).encode("ascii")).hexdigest()


def resolve(gen, target, resolved, cohorts):
    selected = {}
    selected_rules = {}
    for group, source, _, rule in resolved:
        selected.setdefault(group["key"], []).append((group, source))
        selected_rules[(group["key"], source)] = rule
    output, covered = {}, set()
    for cohort in cohorts:
        key = int(cohort["registry"], 16)
        if key in covered or key not in selected:
            raise ValueError("duplicate or unselected cohort registry")
        covered.add(key)
        if cohort.get("classification") != "ordinary_ambient" or cohort.get("evidence_grade") != "strong_B" \
                or cohort.get("activation_policy") != "inferred_registry_components" \
                or not cohort.get("evidence") or not cohort.get("unresolved"):
            raise ValueError("cohort lacks reviewed ordinary ownership and inference disclosure")
        group = selected[key][0][0]
        if cohort["object"] != f"{group['object']:08X}" or group["mask"] != 1 << cohort["bubble"]:
            raise ValueError("cohort object or bubble changed")
        if {source for _, source in selected[key]} != {s["index"] for s in group["slots"] if s["type"] == 1}:
            raise ValueError("reviewed complete cohort must include all its native source slots")
        mode = cohort.get("native_evidence_mode")
        if mode is None and "unresolved_nonselected_primary_rules" in cohort:
            raise ValueError("primary-rule disclosure requires an explicit native evidence mode")
        evidence = native_evidence(
            gen, target, group, cohort["point_lists"], cohort["point_containers"],
            cohort["point_owner_objects"], mode,
            {source: selected_rules[(key, source)] for _, source in selected[key]} if mode is not None else None,
            cohort.get("unresolved_nonselected_primary_rules") if mode is not None else None)
        if evidence != cohort["native_evidence_sha256"]:
            raise ValueError("cohort native owner, rule, point or tactical dependency changed")
        choices = {source: gen.source_count_choices(group, source) for _, source in selected[key]}
        widths = {source: gen.published_category_count(target, group, source) for _, source in selected[key]}
        for source, vector in validate_budget(cohort["members"], choices, widths).items():
            output[(key, source)] = vector
    if any(len(rows) > 1 and key not in covered for key, rows in selected.items()):
        raise ValueError("multiple selected siblings require an explicit reviewed cohort budget")
    return output
