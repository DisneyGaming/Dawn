import unittest
from decode_population_log import decode
from population_command import command_text


class PopulationToolsTest(unittest.TestCase):
    def test_command_has_process_and_activity_lifetimes(self):
        self.assertEqual(command_text(0xAA, 0x2A, 7, 2, 9, 0x74337EDD, 1, 3),
                         "v2 00000000000000AA 000000000000002A 7 2 9 74337EDD 1 3\n")
        for boot in (0, -1, 2**64):
            with self.assertRaises(ValueError):
                command_text(boot, 42, 7, 2, 9, 0x74337EDD, 1, 3)

    def test_captured_generation_delta(self):
        result = decode(111, [0xC000000084000000, 0x52400000003])
        self.assertEqual(result["fields"], {"00": 1, "14": 1})
        self.assertEqual(result["revision"], 3)
        self.assertNotIn("consumed", result)

    def test_captured_actor_and_failed_request_are_distinct(self):
        actor = decode(62, [0x220C612400000004])
        failed = decode(85, [0x8093180000001000, 4])
        self.assertEqual(actor["fields"]["0C"], 1)
        self.assertNotIn("0C", failed["fields"])
        self.assertEqual(failed["consumed"], [1])

    def test_truncated_and_overwide_captures_fail(self):
        with self.assertRaises(ValueError):
            decode(257, [0, 0, 0])
        with self.assertRaises(ValueError):
            decode(33, [2**34])


if __name__ == "__main__":
    unittest.main()
