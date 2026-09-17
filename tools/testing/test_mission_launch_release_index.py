"""Offline checks for the explicit release map generator; no game or network required."""
import copy
import json
from pathlib import Path
import unittest

from mission_launch_release_index import generate


class LocalReleaseMapTests(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[2] / "Dawn"
        self.ledger = json.loads((root / "analysis/mission_launch_release_index.json").read_text(encoding="utf8"))

    def test_known_coverage_and_no_website_metadata(self):
        rows = self.ledger["rows"]
        self.assertEqual(len(rows), 1170)
        self.assertEqual(len({row["experience"] for row in rows}), 629)
        self.assertEqual(sum(row["release"] != 22 for row in rows), 1141)
        result = generate(self.ledger)
        self.assertNotIn("http", result)
        self.assertNotIn("sources", self.ledger)
        self.assertNotIn("string_view", result)
        self.assertEqual(rows[229]["experience"], rows[648]["experience"])
        self.assertNotEqual(rows[298]["experience"], rows[229]["experience"])

    def test_invalid_identity_and_group_data_is_rejected(self):
        mutations = [
            ("hash", self.ledger["rows"][1]["hash"]),
            ("hash", "00000000"),
            ("index", 100),
            ("release", 100),
            ("experience", 1170),
            ("kind", 256),
            ("source", "external-release-service"),
        ]
        for field, value in mutations:
            with self.subTest(field=field, value=value):
                invalid = copy.deepcopy(self.ledger)
                invalid["rows"][0][field] = value
                with self.assertRaises(ValueError):
                    generate(invalid)


if __name__ == "__main__":
    unittest.main()
