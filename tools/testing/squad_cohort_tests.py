"""Whole-squad budgets and exact native dependency regression tests."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import generate_open_world_profiles as gen
import squad_cohort_policy as cohorts


def choice(species, rank="minor"):
    return {"species": species, "rank": rank}


class BudgetChecks(unittest.TestCase):
    def setUp(self):
        self.choices = {0: [[choice("psion")]],
                        1: [[choice("legionary")], [choice("phalanx")]],
                        2: [[choice("legionary")], [choice("phalanx")]]}
        self.widths = {0: 1, 1: 2, 2: 2}
        self.members = [{"source": 0, "targets": [2]},
                        {"source": 1, "targets": [2, 1]},
                        {"source": 2, "targets": [2, 1]}]

    def resolve(self):
        return cohorts.validate_budget(self.members, self.choices, self.widths)

    def test_distributed_budget_not_baseline_per_source(self):
        self.assertEqual(self.resolve(), {0: (2, 0), 1: (2, 1), 2: (2, 1)})
        self.members[1]["targets"][0] = self.members[2]["targets"][0] = 4
        with self.assertRaisesRegex(ValueError, "shared legionary"): self.resolve()

    def test_duplicate_missing_and_foreign_members(self):
        original = copy.deepcopy(self.members)
        for members in (original[:2], original + [original[0]], [{"source": 9, "targets": [1]}]):
            self.members = members
            with self.assertRaises(ValueError): self.resolve()

    def test_bad_widths_and_values(self):
        for vector in ([], [0], [True], [22], [1, 2], [-1], [1.1]):
            self.members[0]["targets"] = vector
            with self.assertRaises(ValueError): self.resolve()

    def test_unknown_primary_one_and_secondary_zero(self):
        self.choices[0] = [[choice(None)]]
        with self.assertRaisesRegex(ValueError, "unknown primary"): self.resolve()
        self.members[0]["targets"] = [1]
        self.resolve()
        self.choices[1][1] = [choice(None)]
        with self.assertRaisesRegex(ValueError, "unknown secondary"): self.resolve()
        self.members[1]["targets"][1] = 0
        self.resolve()

    def test_unknown_rank_does_not_gain_bulk_quota(self):
        self.choices[0] = [[choice("psion", None)]]
        with self.assertRaises(ValueError): self.resolve()

    def test_fixed_species_cannot_multiply_across_sources_or_categories(self):
        for species in gen.squad_counts.FIXED_SPECIES:
            with self.subTest(species=species):
                self.choices = {0: [[choice(species)]], 1: [[choice(species)]]}
                self.widths = {0: 1, 1: 1}
                self.members = [{"source": 0, "targets": [1]}, {"source": 1, "targets": [1]}]
                with self.assertRaisesRegex(ValueError, "shared"): self.resolve()

    def test_leaders_across_different_species_cannot_overlap(self):
        self.choices[0] = [[choice("psion", "major")]]
        self.choices[1][0] = [choice("legionary", "ultra")]
        self.members[0]["targets"] = [1]
        self.members[1]["targets"][0] = 1
        with self.assertRaisesRegex(ValueError, "segmented-rank"): self.resolve()

    def test_weighted_choices_are_not_added(self):
        self.choices = {0: [[choice("knight"), choice("wizard")]]}
        self.widths = {0: 1}
        self.members = [{"source": 0, "targets": [1]}]
        self.assertEqual(self.resolve(), {0: (1, 0)})
        self.members[0]["targets"] = [2]
        with self.assertRaises(ValueError): self.resolve()

    def test_knight_and_eye_require_specific_two_count_evidence(self):
        for species in gen.squad_counts.UNVERIFIED_ONE_DEFAULTS:
            self.choices = {0: [[choice(species)]]}
            self.widths = {0: 1}
            self.members = [{"source": 0, "targets": [2]}]
            with self.assertRaises(ValueError): self.resolve()


class InstalledCohorts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.target = next(t for t in gen.TARGETS if t.activity == "polaris_freeroam")
        cls.rows = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"][cls.target.activity]
        cls.pins = json.loads((ROOT / "tools/coo/open_world_spawn_selections.json").read_text())["destinations"][cls.target.activity]
        groups = {}
        _, objects = gen.scenario_objects(cls.target.scenario)
        for pin in cls.pins:
            cls.assertIn(cls, int(pin["object"], 16), objects[pin["bubble"]])
            groups[(pin["bubble"], int(pin["registry"], 16))] = gen.resolve_group(int(pin["object"], 16))
        cls.resolved = gen.selected_patrols(cls.target, groups, cls.pins)

    def resolve(self, rows=None, resolved=None):
        return cohorts.resolve(gen, self.target, self.resolved if resolved is None else resolved,
                               self.rows if rows is None else rows)

    def test_expanded_mars_preserves_accepted_original_squad_counts(self):
        targets = self.resolve()
        self.assertEqual(len(self.rows), 35)
        self.assertEqual(len(targets), 104)
        self.assertEqual(sum(sum(v) for v in targets.values()), 159)
        self.assertEqual(targets[(0xD503E412, 2)], (6, 0))
        self.assertEqual(targets[(0xD503E412, 3)], (1, 0))
        for source in (1, 2): self.assertEqual(targets[(0x8717837E, source)], (2, 1))
        self.assertEqual(targets[(0x14810D98, 2)], (1, 1))
        self.assertEqual(targets[(0x79632BFE, 0)], (2, 0))
        self.assertEqual(targets[(0x79632BFE, 1)], (1, 0))
        self.assertEqual(targets[(0x3D44B1DA, 1)], (1, 0))

    def test_native_evidence_drift_is_rejected(self):
        rows = copy.deepcopy(self.rows)
        rows[0]["native_evidence_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "dependency changed"): self.resolve(rows)

    def test_missing_duplicate_unselected_and_wrong_ownership_policy(self):
        with self.assertRaisesRegex(ValueError, "explicit reviewed"): self.resolve([])
        for field, value in (("classification", "public_event"), ("evidence_grade", "D"),
                             ("activation_policy", "weighted_alternates"), ("evidence", []), ("unresolved", [])):
            rows = copy.deepcopy(self.rows); rows[0][field] = value
            with self.assertRaises(ValueError): self.resolve(rows)
        with self.assertRaisesRegex(ValueError, "duplicate"): self.resolve(self.rows + [self.rows[0]])

    def test_omitting_native_sibling_cannot_claim_complete_cohort(self):
        reduced = [row for row in self.resolved if not (row[0]["key"] == 0xD503E412 and row[1] == 2)]
        with self.assertRaisesRegex(ValueError, "all its native source"): self.resolve(resolved=reduced)

    def test_hilltop_cross_object_primary_dependency_is_explicit(self):
        row = next(r for r in self.rows if r["registry"] == "8717837E")
        self.assertEqual(row["point_owner_objects"], ["80F73C8B"])
        changed = copy.deepcopy(self.rows)
        next(r for r in changed if r["registry"] == "8717837E")["point_owner_objects"] = []
        with self.assertRaisesRegex(ValueError, "not joined"): self.resolve(changed)

    def test_changed_point_record_requires_review_even_if_guid_stays_same(self):
        original = gen.package_read.read
        def read(tag):
            cls, raw = original(tag)
            if tag == 0x80EA89B0:
                raw = bytearray(raw)
                at = gen.array_rows(raw, 8, 144, 0x808099D8)[31]
                raw[at + 32] ^= 1
            return cls, raw
        with patch.object(gen.package_read, "read", side_effect=read):
                with self.assertRaisesRegex(ValueError, "dependency changed"): self.resolve()


class TitanSolariumCohort(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.target = next(t for t in gen.TARGETS if t.activity == "fleet_freeroam")
        cls.rows = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"][cls.target.activity]
        cls.pins = json.loads((ROOT / "tools/coo/open_world_spawn_selections.json").read_text())["destinations"][cls.target.activity]
        groups = {}
        _, objects = gen.scenario_objects(cls.target.scenario)
        for pin in cls.pins:
            cls.assertIn(cls, int(pin["object"], 16), objects[pin["bubble"]])
            groups[(pin["bubble"], int(pin["registry"], 16))] = gen.resolve_group(int(pin["object"], 16))
        cls.resolved = gen.selected_patrols(cls.target, groups, cls.pins)

    def resolve(self, rows=None, resolved=None):
        return cohorts.resolve(gen, self.target, self.resolved if resolved is None else resolved,
                               self.rows if rows is None else rows)

    def test_exact_three_source_conservative_cohort(self):
        targets = self.resolve()
        self.assertEqual(len(self.rows), 29)
        self.assertEqual(len(targets), 92)
        self.assertEqual(sum(sum(v) for v in targets.values()), 124)
        solarium = {identity: vector for identity, vector in targets.items() if identity[0] == 0x1EE02F73}
        self.assertEqual(solarium, {(0x1EE02F73, 0): (1, 0),
                                   (0x1EE02F73, 1): (1, 0),
                                   (0x1EE02F73, 2): (1, 0)})
        pins = [pin for pin in self.pins if pin["registry"] == "1EE02F73"]
        self.assertEqual([(p["source"], p["rule"], p["source_descriptor"], p["rule_descriptor"])
                          for p in pins],
                         [(0, 25, "80B986B7", "80B986B1"),
                          (1, 24, "80B986BA", "80B986AA"),
                          (2, 6, "80B986BD", "80B986A1")])

    def test_phase_duplicate_must_remain_byte_equivalent(self):
        original = gen.package_read.read
        def read(tag):
            cls, raw = original(tag)
            if tag == 0x80F33662:
                raw = bytearray(raw)
                at = gen.array_rows(raw, 8, 144, 0x808099D8)[169]
                raw[at + 32] ^= 1
            return cls, raw
        with patch.object(gen.package_read, "read", side_effect=read):
            with self.assertRaisesRegex(ValueError, "non-equivalent duplicates"):
                self.resolve()

    def test_unknown_primary_and_secondary_safeguards_are_enforced(self):
        row = copy.deepcopy(self.rows[0])
        row["members"][1]["targets"] = [2]
        with self.assertRaisesRegex(ValueError, "unknown primary"): self.resolve([row])
        row = copy.deepcopy(self.rows[0])
        row["members"][0]["targets"] = [1, 1]
        with self.assertRaisesRegex(ValueError, "unknown secondary"): self.resolve([row])

    def test_exact_owner_rule_point_or_source_set_drift_fails_closed(self):
        row = copy.deepcopy(self.rows[0])
        row["native_evidence_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "dependency changed"): self.resolve([row])
        reduced = [item for item in self.resolved
                   if not (item[0]["key"] == 0x1EE02F73 and item[1] == 2)]
        with self.assertRaisesRegex(ValueError, "all its native source"): self.resolve(resolved=reduced)


class AllDestinationExpansion(unittest.TestCase):
    def test_io_geographic_coverage_and_capacity_contract(self):
        target = next(t for t in gen.TARGETS if t.activity == "eden_freeroam")
        self.assertEqual(target.ambient_bubbles, ((0, 15), (1, 17), (2, 9), (4, 29), (15, 5), (17, 29), (20, 21), (21, 11)))
        rows = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"][target.activity]
        keys = {row["registry"] for row in rows}
        # Roads, pools, glade, drill legs and interior must not regress to only
        # the original cave groups. Alternatives remain deliberate exclusions.
        self.assertTrue({"18927EBC", "0F537AA4", "906062F5", "0B56D8B2",
                         "DAEAE948", "DAEAE94B", "DAEAE94A", "8C1D18F0"} <= keys)
        self.assertFalse(keys & {"70EA8165", "9D72C99E", "9D72C99D", "A38FB7F7", "07AB791C",
                                 "F8AF6C07", "244EFEA6", "F3860D4A"})
        self.assertEqual((gen.SOURCE_CAPACITY, gen.REGISTRY_CAPACITY,
                          gen.RETAINED_REQUEST_CAPACITY, gen.ADMISSION_BURST_CAPACITY,
                          gen.EVENT_BURST_CAPACITY), (256, 96, 384, 1152, 3456))

    def test_io_sidezones_are_complete_and_lost_sectors_stay_excluded(self):
        pins = json.loads((ROOT / "tools/coo/open_world_spawn_selections.json").read_text())["destinations"]["eden_freeroam"]
        rows = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"]["eden_freeroam"]
        self.assertEqual(pins[0]["bubble"], 4)  # Preserve arrival/bootstrap.
        self.assertEqual({pin["bubble"] for pin in pins}, {0, 1, 2, 4, 15, 17, 20, 21})
        self.assertFalse({pin["bubble"] for pin in pins} & {5, 6, 18})
        self.assertFalse({pin["bubble"] for pin in pins} & {3, 8, 16, 19})
        expected = {0: (4, 15, 25), 1: (5, 17, 41), 2: (6, 9, 20)}
        for bubble, (encounters, sources, requests) in expected.items():
            subset = [row for row in rows if row["bubble"] == bubble]
            self.assertEqual(len(subset), encounters)
            self.assertEqual(sum(len(row["members"]) for row in subset), sources)
            self.assertEqual(sum(sum(member["targets"]) for row in subset for member in row["members"]), requests)
            self.assertTrue(all(not row["point_owner_objects"] for row in subset))

    def test_exact_source_sets_whole_squad_budgets_and_retained_capacity(self):
        pins_by_world = json.loads((ROOT / "tools/coo/open_world_spawn_selections.json").read_text())["destinations"]
        rows_by_world = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"]
        expected = {"eden_freeroam": (136, 63, 250), "fleet_freeroam": (92, 29, 125),
                    "polaris_freeroam": (104, 35, 160), "planet_x_freeroam": (233, 74, 322),
                    "tangled_shore_freeroam": (139, 50, 201)}
        for target in gen.TARGETS:
            if target.activity not in expected:
                continue
            with self.subTest(activity=target.activity):
                pins, rows = pins_by_world[target.activity], rows_by_world[target.activity]
                source_count, cohort_count, request_count = expected[target.activity]
                self.assertEqual(len(pins), source_count)
                self.assertEqual(len(rows), cohort_count)
                groups = {(pin["bubble"], int(pin["registry"], 16)):
                          gen.resolve_group(int(pin["object"], 16)) for pin in pins}
                resolved = gen.selected_patrols(target, groups, pins)
                targets = cohorts.resolve(gen, target, resolved, rows)
                requests = 1  # Destination vendor; never part of a patrol cohort.
                for group, source, _, rule in resolved:
                    self.assertEqual(rule, gen.source_spawn_rule(group, source, "fallback"))
                    pin = next(pin for pin in pins if int(pin["registry"], 16) == group["key"] and pin["source"] == source)
                    requests += sum(targets[(group["key"], source)]) if (group["key"], source) in targets else sum(
                        gen.species_request_overrides(target, group, source,
                            gen.published_category_count(target, group, source), pin))
                self.assertEqual(requests, request_count)
                self.assertLessEqual(source_count + 1, gen.SOURCE_CAPACITY)
                self.assertLessEqual(requests, gen.RETAINED_REQUEST_CAPACITY)
                self.assertLessEqual(requests * 3, gen.ADMISSION_BURST_CAPACITY)
                self.assertLessEqual(requests * 9, gen.EVENT_BURST_CAPACITY)
                self.assertTrue(all(row["classification"] == "ordinary_ambient" for row in rows))

    def test_ordinary_coverage_excludes_dungeons_and_gated_owners(self):
        selections = json.loads((ROOT / "tools/coo/open_world_spawn_selections.json").read_text())["destinations"]
        expected = {
            "polaris_freeroam": {0, 1, 5, 7, 9, 10},
            "fleet_freeroam": {0, 1, 2, 5, 7, 11},
            "planet_x_freeroam": {1, 2, 3, 4, 8, 10, 11, 13, 30, 32, 33, 37, 38},
            "tangled_shore_freeroam": {5, 7, 9, 13, 14, 18, 20},
        }
        excluded = {
            # Titan's explicit alternate families, gated hangar/service/bridge
            # paths and unresolved selected-point owners remain disabled.
            "fleet_freeroam": {"FE8C4213", "3ED62BD8", "29B3C161", "0D1608A8", "5FEBBB4A",
                                "BF42D6D7", "060C1E06", "7E4FD2C5"},
            # Include every candidate dungeon/scripted owner even where its
            # English lost-sector name has not been decoded conclusively.
            "planet_x_freeroam": {"46CEE7B0", "5C01717F", "A9907097", "0ACD7A28", "F8CF1F86",
                                  "8A984FD5", "9F6C42EE", "75D82B96", "4D2D1130", "4D2D1137",
                                  "33F12BB1", "B08A35E7", "DAEAD3C4", "7C34262A", "DCC536FD",
                                  "AB38F151", "00477BAF", "A8700978"},
            "tangled_shore_freeroam": set(),
        }
        for target in gen.TARGETS:
            if target.activity not in expected: continue
            pins = selections[target.activity]
            self.assertEqual({p["bubble"] for p in pins}, expected[target.activity])
            self.assertEqual(pins[0]["bubble"], target.primary_bubble)
            self.assertFalse({p["registry"] for p in pins} & excluded.get(target.activity, set()))
            self.assertEqual(len(pins), len({(p["registry"], p["source"]) for p in pins}))

    def test_every_expanded_native_cohort_has_exact_physical_points(self):
        # resolve() re-reads owner state/lane, complete sibling membership,
        # task/provider identity and point-list/container bytes for every row.
        documents = json.loads((ROOT / "tools/coo/open_world_squad_cohorts.json").read_text())["destinations"]
        for activity, rows in documents.items():
            self.assertEqual(len(rows), len({row["registry"] for row in rows}))
            for row in rows:
                self.assertEqual(len(row["native_evidence_sha256"]), 64)
                self.assertTrue(row["point_lists"])
                self.assertTrue(row["unresolved"])  # Inference remains explicit.


if __name__ == "__main__":
    unittest.main()
