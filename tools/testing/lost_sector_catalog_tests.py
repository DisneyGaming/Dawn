"""Reproduce and mutation-test the package-backed Lost Sector catalog."""
from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))

import generate_lost_sector_catalog as catalog


def main() -> None:
    header, evidence = catalog.emit()
    actual_header = (ROOT / "Sunrise/src/server/runtime/activity/lost_sector_catalog.h").read_text()
    actual_evidence = json.loads((ROOT / "tools/coo/lost_sector_mercury_mars_evidence.json").read_text())
    if actual_header.replace("\r\n", "\n") != header.replace("\r\n", "\n"):
        raise SystemExit("generated Lost Sector header drift")
    if actual_evidence != evidence:
        raise SystemExit("generated Lost Sector point evidence drift")
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
    print(f"PASS Lost Sector catalog: sectors={len(sectors)} proofs={len(proofs)} enabled={enabled} omitted=16")


if __name__ == "__main__":
    main()
