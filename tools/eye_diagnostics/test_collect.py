"""Integrity tests: do not allow stale logs or partial calls to imply a diagnosis."""
import json
from pathlib import Path
import unittest
from collect import analyze

CONTRACT = json.loads((Path(__file__).parent / "contract.json").read_text())
MANIFEST = {"build_id": "ABC", "dll_sha256": "DEF", "source_sha256": "123"}
HEADER = ("core t=1 ev=build_identity build_id=ABC module_sha256=DEF source_sha256=123\n"
          "core t=2 ev=omega_reveal stage=install result=ok revision=26 hooks=21 eye_diagnostics=discovery_v1\n")


class CaptureTests(unittest.TestCase):
    def parse(self, text):
        return analyze(text.encode(), MANIFEST, CONTRACT)

    def test_valid_log_is_not_complete_diagnosis(self):
        result = self.parse(HEADER)
        self.assertTrue(result["evidence_valid"])
        self.assertFalse(result["diagnosis_complete"])
        self.assertFalse(result["replay_ready"])
        self.assertFalse(result["observed"]["incoming_damage"])

    def test_wrong_or_multiple_builds_rejected(self):
        for text in (HEADER.replace("build_id=ABC", "build_id=OLD"), HEADER + HEADER,
                     HEADER.replace("module_sha256=DEF", "module_sha256=BAD")):
            self.assertFalse(self.parse(text)["evidence_valid"])

    def test_stale_revision_rejected(self):
        self.assertFalse(self.parse(HEADER.replace("revision=26", "revision=25"))["evidence_valid"])

    def test_missing_exit_and_cross_epoch_pair_rejected(self):
        entry = "core t=5 ev=omega_mission stage=eye_damage_enter run=1 epoch=14 trace=7\n"
        bad_exit = "core t=6 ev=omega_mission stage=eye_damage_exit run=1 epoch=15 trace=7 current=1\n"
        self.assertFalse(self.parse(HEADER + entry)["evidence_valid"])
        self.assertFalse(self.parse(HEADER + entry + bad_exit)["evidence_valid"])
        self.assertTrue(self.parse(HEADER + entry + bad_exit.replace("epoch=15", "epoch=14"))["evidence_valid"])

    def test_partial_line_omitted(self):
        result = self.parse(HEADER + "core t=5 ev=omega_mission stage=eye_health_sample crossed=1")
        self.assertFalse(result["evidence_valid"])
        self.assertFalse(result["observed"]["qualified_health_crossing_reported"])

    def test_bad_timestamp_and_duplicate_exit_rejected(self):
        self.assertFalse(self.parse(HEADER + "core ev=omega_mission stage=eye_damage_enter\n")["evidence_valid"])
        entry = "core t=5 ev=omega_mission stage=eye_damage_enter run=1 epoch=1 trace=2\n"
        end = "core t=6 ev=omega_mission stage=eye_damage_exit run=1 epoch=1 trace=2\n"
        self.assertFalse(self.parse(HEADER + entry + end + end)["evidence_valid"])


if __name__ == "__main__":
    unittest.main()
