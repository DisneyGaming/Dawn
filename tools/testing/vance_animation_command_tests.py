"""Offline command-format checks; never creates a game command file."""
import unittest
from vance_animation_command import command_text


class CommandTests(unittest.TestCase):
    def test_start_and_stop(self):
        self.assertEqual(command_text(99, 42, 7, 1, 1, 0x564C6ECE, 2, 1),
                         "npc1 0000000000000063 000000000000002A 7 1 1 564C6ECE 2 1\n")
        self.assertTrue(command_text(99, 42, 7, 2, 2, 0x564C6ECE, 2, 0).endswith(" 2 0\n"))

    def test_invalid_ranges(self):
        valid = [99, 42, 7, 1, 1, 0x564C6ECE, 2, 1]
        bad = [(0, 0), (0, 2**64), (1, 0), (2, 0), (3, 0), (4, 0),
               (5, 0), (5, 0xFFFFFFFF), (5, 0x811C9DC5), (6, -1), (6, 32768), (7, -1), (7, 2**32)]
        for index, value in bad:
            case = valid.copy()
            case[index] = value
            with self.subTest(index=index, value=value), self.assertRaises(ValueError):
                command_text(*case)


if __name__ == "__main__":
    unittest.main()
