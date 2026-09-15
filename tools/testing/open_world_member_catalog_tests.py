"""Focused tests for the digest-pinned open-world member-offset catalog."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import generate_open_world_member_catalog as catalog  # noqa: E402


class MemberCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.actual = catalog.current_document()
        cls.pin_text = catalog.PIN_PATH.read_text(encoding="utf-8")
        cls.pinned = json.loads(cls.pin_text)

    def test_current_native_sources_match_reviewed_pins(self) -> None:
        catalog.validate_pins(self.actual, self.pinned)
        self.assertEqual(self.pin_text, catalog.canonical_json(self.pinned))
        self.assertEqual(len(self.actual["sources"]), 746)
        self.assertEqual(sum(row["positiveChoiceCount"] for row in self.actual["sources"]), 6102)
        self.assertEqual(
            {origin: sum(row["origin"] == origin for row in self.actual["sources"])
             for origin in ("generic", "mercury")},
            {"generic": 719, "mercury": 27},
        )
        self.assertEqual(
            {width: sum(row["categoryCount"] == width for row in self.actual["sources"])
             for width in (1, 2)},
            {1: 670, 2: 76},
        )

    def test_selected_source_identity_coverage_is_exact(self) -> None:
        selected = {
            (f"{row.resource:08X}", f"{row.registry:08X}", row.source)
            for row in catalog.selected_sources()
        }
        emitted = {
            (row["resource"], row["registry"], row["source"])
            for row in self.actual["sources"]
        }
        self.assertEqual(emitted, selected)
        self.assertEqual(len(emitted), len(self.actual["sources"]))
        mercury = catalog.mercury_sources()
        self.assertEqual(len(mercury), 27)
        self.assertEqual(
            {(f"{row.resource:08X}", f"{row.registry:08X}", row.source) for row in mercury},
            {(row["resource"], row["registry"], row["source"])
             for row in self.actual["sources"] if row["origin"] == "mercury"},
        )

    def test_every_choice_is_positive_bounded_and_unique(self) -> None:
        for source in self.actual["sources"]:
            offsets = [choice["memberOffset"] for choice in source["choices"]]
            self.assertEqual(len(offsets), source["positiveChoiceCount"])
            self.assertEqual(len(offsets), len(set(offsets)))
            self.assertEqual({choice["variant"] for choice in source["choices"]}, set(range(6)))
            for choice in source["choices"]:
                self.assertGreater(choice["weight"], 0)
                self.assertGreaterEqual(choice["memberOffset"], 0)
                self.assertLessEqual(choice["memberOffset"], 0x7FFFFFFFFFFFFFFF)
                self.assertLess(choice["category"], source["categoryCount"])
                self.assertLessEqual(choice["choice"], catalog.MAX_CHOICES_PER_VARIANT - 1)
                self.assertEqual(
                    set(choice),
                    {"memberOffset", "categoryKey", "entity", "weight",
                     "category", "variant", "choice"},
                )

    def test_exact_identity_and_offset_model_lookup(self) -> None:
        by_key = {}
        for source in self.actual["sources"]:
            identity = (source["resource"], source["registry"], source["source"])
            by_key[identity] = {choice["memberOffset"]: choice for choice in source["choices"]}
        first = self.actual["sources"][0]
        identity = (first["resource"], first["registry"], first["source"])
        choice = first["choices"][0]
        self.assertIs(by_key[identity][choice["memberOffset"]], choice)
        self.assertNotIn(choice["memberOffset"] + (1 << 48), by_key[identity])
        self.assertNotIn(("00000000", first["registry"], first["source"]), by_key)

    def test_identity_and_native_byte_drift_fail_closed(self) -> None:
        changed_identity = copy.deepcopy(self.pinned)
        changed_identity["sources"].pop()
        with self.assertRaisesRegex(ValueError, "selected source/pin identity drift"):
            catalog.validate_pins(self.actual, changed_identity)

        changed_digest = copy.deepcopy(self.pinned)
        changed_digest["sources"][0]["choiceDigestSha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "source bytes, native choices, or digest pins"):
            catalog.validate_pins(self.actual, changed_digest)

    def test_generated_header_and_drift_check(self) -> None:
        expected = catalog.render_header(self.pinned)
        catalog.check_file(catalog.HEADER_PATH, expected, "generated member catalog")
        header = catalog.HEADER_PATH.read_text(encoding="utf-8")
        self.assertIn("const MemberChoice* lookup(std::uint32_t resource,", header)
        self.assertIn("if(result!=nullptr)return nullptr;", header)
        with tempfile.TemporaryDirectory() as directory:
            changed = Path(directory) / "changed.h"
            changed.write_text(expected + "// drift\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "generated member catalog drift"):
                catalog.check_file(changed, expected, "generated member catalog")

    def test_marked_member_before_definition_is_rejected(self) -> None:
        selected = catalog.selected_sources()[0]
        outer_class, original = catalog.profiles.package_read.read(selected.resource)
        raw = bytearray(original)
        definition = catalog.profiles.relative(raw, 24)
        category = catalog.profiles.array_rows(
            raw, definition + 0xA8, 104, catalog.CATEGORY_CLASS)[0]
        choice_row = next(
            row
            for variant in range(6)
            for row in catalog.profiles.array_rows(
                raw, category + 8 + variant * 16, 24, catalog.CHOICE_CLASS)
            if catalog.profiles.u32(raw, row + 12) > 0
        )
        body = definition - 16
        self.assertGreaterEqual(body, 4)
        struct.pack_into("<I", raw, body - 4, catalog.MEMBER_RECORD_CLASS)
        struct.pack_into("<I", raw, body, 0x12345678)
        struct.pack_into("<q", raw, choice_row, body - choice_row)
        self.assertEqual(catalog.profiles.relative(raw, choice_row), body)
        self.assertEqual(catalog.profiles.u32(raw, body - 4), catalog.MEMBER_RECORD_CLASS)
        with patch.object(catalog.profiles.package_read, "read",
                          return_value=(outer_class, bytes(raw))):
            with self.assertRaisesRegex(ValueError, "member record"):
                catalog.decode_source(selected)

    def test_cli_check(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools/coo/generate_open_world_member_catalog.py"), "--check"],
            cwd=ROOT, check=False, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Checked 746 sources, 6102 positive choices", result.stdout)


if __name__ == "__main__":
    unittest.main()
