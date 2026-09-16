"""Reproduce and mutation-test the package-backed Lost Sector catalog."""
from __future__ import annotations

import copy
import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))

import generate_lost_sector_catalog as catalog


EXPECTED_QUOTA_TOTALS = {
    "Pariah's Refuge": 51,
    "Core Terminus": 56,
    "Ma'adim Subterrane": 34,
    "Methane Flush": 31,
    "DS Quarters-2": 52,
    "Cargo Bay 3": 19,
    "Sanctum of Bones": 46,
    "Aphix Conduit": 38,
    "Grove of Ulan-Tan": 45,
    "The Rift": 42,
    "The Conflux": 30,
    "The Orrery": 44,
    "The Carrion Pit": 39,
    "Ancient's Haunt": 35,
    "Trapper's Cave": 37,
    "Wolfship Turbine": 36,
    "Kingship Dock": 29,
    "The Empty Tank": 54,
    "Shipyard AWO-43": 43,
}


def expect_quota_rejected(document: dict, groups: dict, label: str) -> None:
    try:
        catalog.validate_encounter_quotas(document, groups)
    except ValueError:
        return
    raise SystemExit(f"invalid Lost Sector quota accepted: {label}")


def main() -> None:
    header, evidence = catalog.emit()
    actual_header = (ROOT / "Sunrise/src/server/runtime/activity/lost_sector_catalog.h").read_text()
    actual_evidence = json.loads((ROOT / "tools/coo/lost_sector_mercury_mars_evidence.json").read_text())
    if actual_header.replace("\r\n", "\n") != header.replace("\r\n", "\n"):
        raise SystemExit("generated Lost Sector header drift")
    if actual_evidence != evidence:
        raise SystemExit("generated Lost Sector point evidence drift")
    quota_document, quota_file_sha256 = catalog.load_encounter_quotas()
    groups, _ = catalog.resolve()
    quota_rows = catalog.validate_encounter_quotas(quota_document, groups)
    quota_manifest = evidence.get("quota_manifest", {})
    if quota_manifest.get("file_sha256") != quota_file_sha256 \
            or quota_manifest.get("content_sha256") != catalog.quota_content_sha256(quota_document):
        raise SystemExit("generated Lost Sector quota digest drift")
    footage_raw = (ROOT / "tools/coo/lost_sector_footage_review.json").read_bytes()
    if evidence.get("footage_review", {}).get("file_sha256") != hashlib.sha256(footage_raw).hexdigest():
        raise SystemExit("generated Lost Sector footage-review digest drift")
    quota_totals = {
        name: sum(sum(row["targets"]) for stage in sector["stages"] for row in stage["sources"])
        for name, sector in quota_document["sectors"].items()
    }
    if len(quota_rows) != 455 or quota_totals != EXPECTED_QUOTA_TOTALS:
        raise SystemExit(f"Lost Sector quota coverage/baseline changed: rows={len(quota_rows)} totals={quota_totals}")
    sectors = evidence["sectors"]
    proofs = evidence["registry_proofs"]
    enabled = sum(len(row["enabled_sources"]) for row in proofs)
    omitted = {(row["registry"], source) for row in proofs for source in row["omitted_sources"]}
    expected_omitted = {
        ("A27443E8", 5), ("2E3D2EB4", 2), ("5BA616DA", 20),
        ("DB5D8740", 4), ("33C30847", 5),
        ("6717656F", 2), ("BB69D2E9", 4), ("3F8AF55C", 5),
        ("2F8DB58A", 2), ("100F6578", 2),
        ("9A24C39A", 52), ("9A24C39A", 60), ("9A24C39A", 64),
        ("9A24C39A", 70), ("9A24C39A", 76),
    }
    # The entire 69A1B17C source8 group is extraction-only and therefore is
    # not one of the 38 runtime proofs.
    if len(sectors) != 19 or len(proofs) != 38 or enabled != 455 \
            or omitted != expected_omitted:
        raise SystemExit("Lost Sector coverage/omission invariant changed")

    # A valid target change must alter both generated policy and manifest digest.
    drifted = copy.deepcopy(quota_document)
    drift_row = drifted["sectors"]["The Rift"]["stages"][0]["sources"][0]
    drift_row["targets"][0] += 1
    catalog.validate_encounter_quotas(drifted, groups)
    drift_header, drift_evidence = catalog.emit(drifted)
    if drift_header == header or drift_evidence["quota_manifest"]["content_sha256"] \
            == evidence["quota_manifest"]["content_sha256"]:
        raise SystemExit("Lost Sector quota drift did not affect generated output")

    invalid_vector = copy.deepcopy(quota_document)
    invalid_vector["sectors"]["The Rift"]["stages"][0]["sources"][0]["targets"].append(0)
    expect_quota_rejected(invalid_vector, groups, "category arity")

    invalid_bound = copy.deepcopy(quota_document)
    invalid_bound["sectors"]["The Rift"]["stages"][0]["sources"][0]["targets"][0] = 64
    expect_quota_rejected(invalid_bound, groups, "target bound")

    invalid_sum = copy.deepcopy(quota_document)
    invalid_sum["sectors"]["Pariah's Refuge"]["stages"][0]["sources"][1]["targets"] = [40, 24]
    expect_quota_rejected(invalid_sum, groups, "combined runtime source limit")

    disabled_category = copy.deepcopy(quota_document)
    disabled_category["sectors"]["Pariah's Refuge"]["stages"][0]["sources"][1]["targets"][1] = 0
    expect_quota_rejected(disabled_category, groups, "disabled native category")

    invalid_reference = copy.deepcopy(quota_document)
    invalid_reference["sectors"]["The Rift"]["review"]["video_or_guide_reference"] = None
    expect_quota_rejected(invalid_reference, groups, "missing review reference")

    original_category_count = catalog.gen.source_category_count
    try:
        catalog.gen.source_category_count = lambda group, source: 3
        unsupported_categories = copy.deepcopy(quota_document)
        unsupported_categories["sectors"]["Pariah's Refuge"]["stages"][0]["sources"][0]["targets"] = [1, 1, 1]
        expect_quota_rejected(unsupported_categories, groups, "unsupported three-category source")
    finally:
        catalog.gen.source_category_count = original_category_count

    missing_row = copy.deepcopy(quota_document)
    missing_row["sectors"]["The Rift"]["stages"][0]["sources"].pop()
    expect_quota_rejected(missing_row, groups, "missing source")

    boss_mutation = copy.deepcopy(quota_document)
    boss_mutation["sectors"]["The Rift"]["stages"][-1]["sources"][-1]["targets"][0] = 2
    expect_quota_rejected(boss_mutation, groups, "boss singleton")

    species_singleton = copy.deepcopy(quota_document)
    singleton_row = next(row for row in species_singleton["sectors"]["The Rift"]["stages"][0]["sources"]
                         if row["registry"] == "D7BD9746" and row["source"] == 6)
    singleton_row["targets"][0] = 2
    expect_quota_rejected(species_singleton, groups, "species singleton")

    # A point-list pin mutation must fail rather than silently bless a fresh
    # digest. Restore the module constant even when the expected failure fires.
    key = 0x3A80D8D4
    original = catalog.POINT_DEPENDENCIES[key]
    catalog.POINT_DEPENDENCIES[key] = (["80F4E134"], original[1])
    try:
        try:
            catalog.emit()
        except ValueError:
            pass
        else:
            raise SystemExit("mutated Lost Sector point closure was accepted")
    finally:
        catalog.POINT_DEPENDENCIES[key] = original
    print(f"PASS Lost Sector catalog: sectors={len(sectors)} proofs={len(proofs)} "
          f"enabled={enabled} quota_rows={len(quota_rows)} omitted=16")


if __name__ == "__main__":
    main()
