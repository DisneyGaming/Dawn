import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "coo"))
import generate_open_world_profiles as gen
import squad_cohort_policy as cohorts


TARGET = next(t for t in gen.TARGETS if t.activity == "tangled_shore_freeroam")
CASES = {
    "83BDA483": {
        "object": "80FD5D99", "lists": ["80FD5D78"], "fallback": 21,
        "digest": "92eceaeee2da53a06af0cf055bbfd75070954164f981f10e2e25dacc5d5839c8",
        "disclosure": [{"sources": [0, 1, 2], "rule": 12, "descriptor": "80FD598F",
                        "sha256": "783bcc6517d050f7fcf9151c7fd1f84df134d7f26aae5f4c3e165b90ebe61725",
                        "unresolved_guids": ["883ABD98CD72B843"]}],
    },
    "7CE2E09B": {
        "object": "80FD5DB9", "lists": ["80FD5D9B"], "fallback": 17,
        "digest": "db0f27dc02d9850ac9893115b5dc2c21628986fdc7508d85df3a1edf2ec4e434",
        "disclosure": [{"sources": [0, 1], "rule": 9, "descriptor": "80FD59B3",
                        "sha256": "8c97018cd8465b1347335fd2dfacd92e957deb1f20def3b2978e43bf12499170",
                        "unresolved_guids": ["6ADBE24B2975EB82"]}],
    },
    "9FFBD85F": {
        "object": "80FD9A1C", "lists": ["80FD99F8"], "fallback": 20,
        "digest": "9f3311b94c56a46eceeb4a5c4a6041d8afbc150db6fe9ff2386be4f301a679a0",
        "disclosure": [{"sources": [0, 1, 2, 3], "rule": 13, "descriptor": "80FD9728",
                        "sha256": "ccf00b00558d2b7517f9cb55d3bbfa19a463a1a6e57d4c68ef0dcea3b8d9bf2b",
                        "unresolved_guids": ["8BCCF71B23BEF3F7"]}],
    },
}


def inputs(case):
    group = gen.resolve_group(int(case["object"], 16))
    sources = [slot["index"] for slot in group["slots"] if slot["type"] == 1]
    selected = {source: case["fallback"] for source in sources}
    return group, sources, selected


def prove(case, selected=None, disclosure=None, lists=None):
    group, _, defaults = inputs(case)
    return cohorts.native_evidence(
        gen, TARGET, group, case["lists"] if lists is None else lists, [], [],
        "selected_fallback", defaults if selected is None else selected,
        case["disclosure"] if disclosure is None else disclosure)


class SelectedFallbackEvidenceTests(unittest.TestCase):
    def test_existing_full_evidence_digest_is_unchanged(self):
        mars = next(t for t in gen.TARGETS if t.activity == "polaris_freeroam")
        group = gen.resolve_group(0x80F7385F)
        self.assertEqual(
            cohorts.native_evidence(gen, mars, group, ["80EA89B0"], ["80EA89B1"]),
            "09a5aa69701c92e445048f1bc5b2bb6d67a4fd62d68247c902fdcf2b295f9e43")
        with self.assertRaises(ValueError):
            cohorts.native_evidence(gen, mars, group, ["80EA89B0"], ["80EA89B1"],
                                    unresolved_primary_rules=[])

    def test_three_tangled_fallback_proofs_are_stable(self):
        for name, case in CASES.items():
            with self.subTest(name=name):
                self.assertEqual(prove(case), case["digest"])

    def test_every_source_must_select_exact_authored_fallback(self):
        case = CASES["83BDA483"]
        group, sources, selected = inputs(case)
        for mutation in ("missing", "primary", "foreign"):
            changed = dict(selected)
            if mutation == "missing":
                changed.pop(sources[-1])
            elif mutation == "primary":
                changed[sources[-1]] = gen.source_spawn_rule(group, sources[-1], "primary")
            else:
                changed[sources[-1]] = 0x7FFF
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                prove(case, selected=changed)

    def test_primary_disclosure_is_exact_and_complete(self):
        case = CASES["83BDA483"]
        mutations = []
        mutations.append([])
        for field, value in (("sources", [0, 1]), ("rule", 13),
                             ("descriptor", "80FD5992"), ("sha256", "0" * 64),
                             ("unresolved_guids", []),
                             ("unresolved_guids", ["883ABD98CD72B842"])):
            changed = copy.deepcopy(case["disclosure"])
            changed[0][field] = value
            mutations.append(changed)
        for disclosure in mutations:
            with self.subTest(disclosure=disclosure), self.assertRaises(ValueError):
                prove(case, disclosure=disclosure)

    def test_selected_fallback_point_proof_is_not_relaxed(self):
        case = CASES["83BDA483"]
        for lists in ([], ["80FD5D9B"], ["80FD5D78", "80FD5D9B"]):
            with self.subTest(lists=lists), self.assertRaises(ValueError):
                prove(case, lists=lists)

    def test_resolve_rejects_primary_or_mixed_published_roles(self):
        case = CASES["7CE2E09B"]
        group, sources, selected = inputs(case)
        members = [{"source": source, "targets": [2] if source == 0 else [4]} for source in sources]
        record = {
            "registry": "7CE2E09B", "object": case["object"], "bubble": 9,
            "classification": "ordinary_ambient", "evidence_grade": "strong_B",
            "activation_policy": "inferred_registry_components", "evidence": ["fixture"],
            "unresolved": ["fixture"], "point_lists": case["lists"], "point_containers": [],
            "point_owner_objects": [], "native_evidence_mode": "selected_fallback",
            "unresolved_nonselected_primary_rules": case["disclosure"],
            "native_evidence_sha256": case["digest"], "members": members,
        }
        resolved = [(group, source, 2, selected[source]) for source in sources]
        self.assertEqual(cohorts.resolve(gen, TARGET, resolved, [record]),
                         {(0x7CE2E09B, 0): (2, 0), (0x7CE2E09B, 1): (4, 0)})
        primary = gen.source_spawn_rule(group, sources[-1], "primary")
        with self.assertRaises(ValueError):
            cohorts.resolve(gen, TARGET, resolved[:-1] + [(group, sources[-1], 2, primary)], [record])


if __name__ == "__main__":
    unittest.main()
