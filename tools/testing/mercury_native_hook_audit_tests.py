from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
BOOT = ROOT / "Dawn/src/client/hooks/bootflow"


class MercuryNativeHookAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lifecycle = (BOOT / "bootflow_hook_lifecycle.cpp").read_text(encoding="utf-8")

    def test_vance_diagnostics_are_disabled_in_normal_boot(self):
        self.assertRegex(
            self.lifecycle,
            r"constexpr bool kEnableVanceContactDiagnostics\s*=\s*false;",
        )
        self.assertRegex(
            self.lifecycle,
            r"kEnableVanceContactDiagnostics\s*&&\s*install_vance_contact_observer\(\)",
        )
        self.assertNotRegex(
            self.lifecycle,
            r"const bool vanceContact\s*=\s*install_vance_contact_observer\(\)",
        )

    def test_required_receipt_owners_remain_installed(self):
        for call in (
            "install_omega_enemy_lair_receipts()",
            "install_ambient_population_named_observer()",
            "install_omega_first_cannon_receipt()",
            "install_omega_arc_charge_receipts()",
            "public_event_participant_observer::install()",
        ):
            with self.subTest(call=call):
                self.assertIn(call, self.lifecycle)

    def test_broad_schema_probe_remains_quarantined(self):
        self.assertRegex(
            self.lifecycle,
            r"constexpr bool kEnableLegacyUnsafeObserverBundle\s*=\s*false;",
        )
        schema = (BOOT / "activity_schema_decode_probe.cpp").read_text(encoding="utf-8")
        self.assertIn("public_event_placement_observer::begin(component)", schema)
        self.assertIn("public_event_engagement_observer::begin(component,stateKey)", schema)
        self.assertIn("install_omega_first_cannon_receipt()", schema)

    def test_receipts_are_native_observations_not_executor_completion(self):
        enemy = (BOOT / "omega_enemy_lair_receipts.cpp").read_text(encoding="utf-8")
        named = (BOOT / "ambient_population_named_observer.cpp").read_text(encoding="utf-8")
        interaction = (BOOT / "omega_arc_charge_receipts.cpp").read_text(encoding="utf-8")
        participant = (BOOT / "public_event_participant_observer.cpp").read_text(encoding="utf-8")
        for marker in ("A0D510", "C72390", "A85540"):
            self.assertIn(marker, enemy)
        for marker in ("569D10", "4E25D0", "34F790"):
            self.assertIn(marker, named)
        for marker in ("D99620", "F36640", "9EFFC0", "F32CD0"):
            self.assertIn(marker, interaction)
        self.assertIn("BF5AA0", participant)
        self.assertIn("mutation=observe_only", named)
        self.assertIn("mutation=observe_only", participant)


if __name__ == "__main__":
    unittest.main()
