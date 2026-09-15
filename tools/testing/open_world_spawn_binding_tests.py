"""Verify exact native source-to-rule joins; never attach to or modify the game."""
import copy
from dataclasses import replace
import hashlib
from pathlib import Path
import re
import struct
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import generate_open_world_profiles as gen
import spawn_count_policy as counts


class SourceRuleChecks(unittest.TestCase):
    def setUp(self):
        self.key = 0x12345678
        self.group = {"key": self.key, "slots": [
            {"index": 0, "type": 1, "descriptor": 101},
            {"index": 11, "type": 66, "descriptor": 102},
            {"index": 12, "type": 66, "descriptor": 103},
            {"index": 13, "type": 66, "descriptor": 104},
        ]}
        self.source = bytearray(0x180)
        struct.pack_into("<q", self.source, 24, 0x40 - 24)
        struct.pack_into("<I", self.source, 0x3C, 0x8080948F)
        struct.pack_into("<IHH", self.source, 0x40 + 48, self.key, 1, 0)
        struct.pack_into("<IHH", self.source, 0x40 + 0x98, self.key, 66, 11)
        struct.pack_into("<IHH", self.source, 0x40 + 0xA0, self.key, 66, 12)
        self.rule = bytearray(0x80)
        struct.pack_into("<q", self.rule, 24, 0x40 - 24)
        struct.pack_into("<I", self.rule, 0x3C, 0x808094D0)
        self.source_class = self.rule_class = 0x80809C36

    def read(self, tag):
        return (self.source_class, self.source) if tag == 101 else (self.rule_class, self.rule)

    def resolve(self, role="fallback"):
        with patch.object(gen.package_read, "read", side_effect=self.read):
            return gen.source_spawn_rule(self.group, 0, role)

    def test_source_reference_not_registry_order(self):
        self.assertEqual(self.resolve(), 12)
        self.assertEqual(self.resolve("primary"), 11)
        self.group["slots"].reverse()
        self.assertEqual(self.resolve(), 12)

    def test_invalid_role(self):
        with self.assertRaises(ValueError): self.resolve("last")

    def test_missing_and_duplicate_source(self):
        original = copy.deepcopy(self.group)
        self.group["slots"].pop(0)
        with self.assertRaises(ValueError): self.resolve()
        self.group = original
        self.group["slots"].append(copy.deepcopy(self.group["slots"][0]))
        with self.assertRaises(ValueError): self.resolve()

    def test_wrong_source_class(self):
        self.source_class = 0
        with self.assertRaises(ValueError): self.resolve()

    def test_wrong_definition_class(self):
        struct.pack_into("<I", self.source, 0x3C, 0)
        with self.assertRaises(ValueError): self.resolve()

    def test_truncated_definition(self):
        self.source = self.source[:0xA0]
        with self.assertRaises(ValueError): self.resolve()

    def test_wrong_source_identity(self):
        for offset, value, fmt in ((0x70, 2, "I"), (0x74, 3, "H"), (0x76, 1, "H")):
            original = self.source[:]
            struct.pack_into("<" + fmt, self.source, offset, value)
            with self.assertRaises(ValueError): self.resolve()
            self.source = original

    def test_absent_rule_requires_explicit_policy(self):
        struct.pack_into("<IHH", self.source, 0xE0, 0x811C9DC5, 255, 65535)
        with self.assertRaisesRegex(ValueError, "no authored fallback"): self.resolve()

    def test_foreign_wrong_kind_missing_rule(self):
        for reference in ((2, 66, 12), (self.key, 1, 12), (self.key, 66, 99)):
            struct.pack_into("<IHH", self.source, 0xE0, *reference)
            with self.assertRaises(ValueError): self.resolve()

    def test_duplicate_rule(self):
        self.group["slots"].append(copy.deepcopy(self.group["slots"][2]))
        with self.assertRaises(ValueError): self.resolve()

    def test_wrong_rule_descriptor_or_body_class(self):
        self.rule_class = 0
        with self.assertRaises(ValueError): self.resolve()
        self.rule_class = 0x80809C36
        struct.pack_into("<I", self.rule, 0x3C, 0)
        with self.assertRaises(ValueError): self.resolve()


class SourceCategoryChecks(unittest.TestCase):
    def setUp(self):
        self.key = 0x12345678
        self.group = {"key": self.key, "slots": [
            {"index": 0, "type": 1, "descriptor": 101},
        ]}
        self.source_class = 0x80809C36
        self.source = bytearray(0x300)
        struct.pack_into("<q", self.source, 24, 0x40 - 24)
        struct.pack_into("<I", self.source, 0x3C, 0x8080948F)
        struct.pack_into("<IHH", self.source, 0x40 + 48, self.key, 1, 0)
        self.set_count(2)

    def set_count(self, count):
        offset = 0x40 + 0xA8
        struct.pack_into("<Qq", self.source, offset, count, 0x100 - (offset + 8) if count else 0)
        if count:
            struct.pack_into("<IQI", self.source, 0xFC, 0x80809FBD, count, 0x80808356)

    def count(self):
        with patch.object(gen.package_read, "read", return_value=(self.source_class, self.source)):
            return gen.source_category_count(self.group, 0)

    def test_exact_native_width(self):
        self.assertEqual(self.count(), 2)

    def test_zero_and_more_than_two_categories_rejected(self):
        for count in (0, 3):
            self.set_count(count)
            with self.assertRaises(ValueError): self.count()

    def test_wrong_descriptor_class_and_source_identity_rejected(self):
        self.source_class = 0
        with self.assertRaises(ValueError): self.count()
        self.source_class = 0x80809C36
        struct.pack_into("<IHH", self.source, 0x40 + 48, self.key, 1, 1)
        with self.assertRaises(ValueError): self.count()

    def test_reviewed_source_must_remain_native_width_two(self):
        self.set_count(1)
        target = gen.Target("test", "Test", "test", 1, 2, ((2, 1),), (), (),
                            two_category_sources=((self.key, 0),))
        with patch.object(gen.package_read, "read", return_value=(self.source_class, self.source)):
            with self.assertRaisesRegex(ValueError, "no longer has two"):
                gen.published_category_count(target, self.group, 0)


class RequestPolicyChecks(unittest.TestCase):
    def setUp(self):
        self.policy = {"kind": "bounded_inference", "targets": [1, 0],
                       "evidence": ["https://example.org/observed-cohort"],
                       "reason": "Repeated one-leader pattern; native alternatives retained.",
                       "unresolved": ["Escort quota is not yet established."]}

    def resolve(self, categories=2):
        return gen.request_overrides({"request_policy": self.policy}, categories)

    def test_no_policy_preserves_destination_default(self):
        self.assertEqual(gen.request_overrides({}, 1), (0, 0))
        self.assertEqual(gen.request_overrides({}, 2), (0, 0))

    def test_explicit_one_leader_keeps_escort_dormant(self):
        self.assertEqual(self.resolve(), (1, 0))
        self.policy["targets"] = [2]
        self.assertEqual(self.resolve(1), (2, 0))

    def test_wrong_width_and_invalid_counts_rejected(self):
        for targets in ([], [1], [1, 0, 0], [0, 1], [22, 0], [1, 22], [True, 0], [1, -1], [1.5, 0]):
            self.policy["targets"] = targets
            with self.assertRaises(ValueError): self.resolve()
        for categories in (0, 3):
            with self.assertRaises(ValueError): self.resolve(categories)

    def test_unlabeled_or_unsourced_counts_rejected(self):
        original = copy.deepcopy(self.policy)
        for key, value in (("kind", "native_weight"), ("evidence", []),
                           ("evidence", ["file:///scratch"]), ("reason", ""),
                           ("unresolved", None), ("unresolved", [""])):
            self.policy = {**original, key: value}
            with self.assertRaises(ValueError): self.resolve()


class SpeciesCountChecks(unittest.TestCase):
    def test_all_fixed_one_species_stay_singleton_for_every_identity(self):
        for species, (_, low, high) in counts.RULES.items():
            if (low, high) != (1, 1):
                continue
            for seed in range(100):
                self.assertEqual(counts.category_target(str(seed), [{"species": species, "rank": "minor"}])[0], 1)

    def test_boss_alternative_locks_whole_category(self):
        for rank in counts.SINGLETON_RANKS:
            self.assertEqual(counts.category_target("boss", [
                {"species": "goblin", "rank": "minor"},
                {"species": "goblin", "rank": rank}]), (1, "segmented-rank-singleton"))
        self.assertEqual(counts.category_target("captain", [
            {"species": "dreg", "rank": "minor"},
            {"species": "captain", "rank": "minor"}])[0], 1)

    def test_variation_is_stable_bounded_and_not_constant(self):
        for species, (_, low, high) in counts.RULES.items():
            if species in counts.UNVERIFIED_ONE_DEFAULTS:
                low = high = 1
            choice = [{"species": species, "rank": "minor"}]
            seen = set()
            for seed in range(100):
                value = counts.category_target(str(seed), choice)
                self.assertEqual(value, counts.category_target(str(seed), choice))
                self.assertLessEqual(low, value[0]); self.assertLessEqual(value[0], high)
                seen.add(value[0])
            self.assertEqual(seen, set(range(low, high + 1)))

    def test_mixed_native_choices_are_not_added_as_separate_squads(self):
        choice = [{"species": "shank", "rank": "minor"},
                  {"species": "shank_exploder", "rank": "minor"}]
        for seed in range(20):
            self.assertIn(counts.category_target(str(seed), choice)[0], (2, 3))
        choice[0]["species"] = "thrall"
        self.assertEqual(counts.category_target("mixed", choice), (2, "mixed-choice-conservative"))

    def test_unknown_types_and_ranks_are_not_given_bulk_counts(self):
        for choice in ([], [{"species": "trooper", "rank": "minor"}],
                       [{"species": "goblin", "rank": None}]):
            self.assertEqual(counts.category_target("unknown", choice)[0], 1)


class InstalledBindingChecks(unittest.TestCase):
    def test_existing_primary_policy_is_explicit(self):
        target = next(t for t in gen.TARGETS if t.namespace == "dreaming_city")
        self.assertEqual(target.primary_sources, ((0x1170A615, 0), (0x4DDE0388, 0),
            (0x843307B2, 0), (0x4E54F61A, 0), (0xCA0046CA, 0), (0xB8DAD7D0, 0),
            (0x33E12E3B, 0), (0x99D49DC8, 0)))
        self.assertTrue(all(not t.primary_sources for t in gen.TARGETS if t != target))

    def test_generated_catalog_and_scripts_match_package_evidence(self):
        header, scripts = gen.emit()
        self.assertEqual(header, (ROOT / "Sunrise/src/state/activity/coo/open_world_catalog.h").read_text())
        for name, contents in scripts.items():
            self.assertEqual(contents, (ROOT / "Sunrise/scripts" / name).read_text())
        # Counts follow the user policy; native selection and width stay pinned.
        self.assertEqual(len(re.findall(r"PopulationKind::patrol,true,", header)), 722)
        reviewed = [identity for target in gen.TARGETS for identity in target.two_category_sources]
        self.assertEqual(len(reviewed), 73)
        self.assertEqual(len(set(reviewed)), 73)
        self.assertEqual(len(re.findall(r"PopulationKind::patrol,true,\d+,[1-9]\d*,\d+,[12]\}", header)), 704)
        self.assertEqual(len(re.findall(r"PopulationKind::patrol,true,8,1,0,2\}", header)), 4)
        dreaming_city = header.split("namespace dreaming_city {", 1)[1].split("} // namespace dreaming_city", 1)[0]
        self.assertNotRegex(dreaming_city, r"PopulationKind::(?:patrol|npc),(?:true|false),\d+,0,0,2\}")
        self.assertEqual(len(re.findall(r"PopulationKind::patrol,true,\d+,0,0,1\}", dreaming_city)), 18)


class PinnedSelectionChecks(unittest.TestCase):
    def setUp(self):
        self.fixture = SourceRuleChecks()
        self.fixture.setUp()
        self.group = self.fixture.group
        self.group.update(object=0xABC, mask=4)
        self.group["slots"].extend([
            {"index": 2, "type": 3, "descriptor": 105},
            {"index": 3, "type": 30, "descriptor": 106},
        ])
        self.target = gen.Target("test", "Test", "test", 1, 2, ((2, 1),), (), ())
        self.groups = {(2, self.fixture.key): self.group}
        self.pin = {"bubble": 2, "registry": f"{self.fixture.key:08X}", "object": "00000ABC",
                    "source": 0, "tactical": 2, "rule": 12, "role": "fallback"}
        for name, descriptor in (("source", 101), ("tactical", 105), ("rule", 103)):
            self.pin[name + "_descriptor"] = f"{descriptor:08X}"
            self.pin[name + "_sha256"] = hashlib.sha256(self.fixture.read(descriptor)[1]).hexdigest()

    def select(self, pins=None):
        with patch.object(gen.package_read, "read", side_effect=self.fixture.read):
            return gen.selected_patrols(self.target, self.groups, pins if pins is not None else [self.pin])

    def test_unrelated_groups_and_sibling_order_do_not_select_other_sources(self):
        intruder = copy.deepcopy(self.group)
        intruder["key"] = 99
        self.groups = {(2, 99): intruder, **self.groups}
        self.group["slots"].insert(0, {"index": 5, "type": 1, "descriptor": 999})
        self.group["slots"].reverse()
        selected = self.select()
        self.assertEqual(len(selected), 1)
        self.assertIs(selected[0][0], self.group)
        self.assertEqual(selected[0][1:], (0, 2, 12))

    def test_missing_encounter_and_wrong_object_or_bubble(self):
        original = copy.deepcopy(self.group)
        for field, value in (("object", 7), ("mask", 8)):
            self.group[field] = value
            with self.assertRaises(ValueError): self.select()
            self.group[field] = original[field]
        self.groups = {}
        with self.assertRaises(ValueError): self.select()

    def test_missing_duplicate_and_changed_descriptor(self):
        original = copy.deepcopy(self.group["slots"])
        self.group["slots"].pop(0)
        with self.assertRaises(ValueError): self.select()
        self.group["slots"] = copy.deepcopy(original) + [copy.deepcopy(original[0])]
        with self.assertRaises(ValueError): self.select()
        self.group["slots"] = copy.deepcopy(original)
        self.group["slots"][0]["descriptor"] = 999
        with self.assertRaises(ValueError): self.select()

    def test_source_tactical_and_rule_digest_changes_require_review(self):
        for label in ("source", "tactical", "rule"):
            pin = {**self.pin, label + "_sha256": "0" * 64}
            with self.assertRaisesRegex(ValueError, "bytes changed"): self.select([pin])

    def test_wrong_class_and_event_shape_are_rejected(self):
        self.fixture.source_class = 0
        with self.assertRaises(ValueError): self.select()
        self.fixture.source_class = 0x80809C36
        self.group["slots"].append({"index": 99, "type": 99, "descriptor": 999})
        with self.assertRaisesRegex(ValueError, "ambient shape"): self.select()

    def test_same_source_rule_and_explicit_role_are_required(self):
        with self.assertRaisesRegex(ValueError, "host policy"):
            self.select([{**self.pin, "role": "primary"}])
        pin = {**self.pin, "rule": 13, "rule_descriptor": f"{104:08X}"}
        with self.assertRaisesRegex(ValueError, "authored reference"): self.select([pin])

    def test_duplicate_source_and_coverage_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "coverage"): self.select([])
        self.target = replace(self.target, ambient_bubbles=((2, 2),))
        with self.assertRaisesRegex(ValueError, "duplicate pinned"):
            self.select([self.pin, self.pin])


if __name__ == "__main__":
    unittest.main()
