"""Replay the pinned game's roster generation decision for Tower recovery.

The original 3CCE50 comparer and mirror copy execute. World lookup and actual
destruction/construction are observation stubs; this does not certify an in-game
respawn and never opens or writes a live process.
"""
import hashlib
from pathlib import Path
import unittest
from mercury_streaming_native import Native, DATA, BASE, IMAGE_SHA256
from unicorn.x86_const import UC_X86_REG_RAX


class TowerRecoveryNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = Path(r"C:\Destiny 2 Development\destiny2_unpacked.bin").read_bytes()
        assert hashlib.sha256(cls.image).hexdigest() == IMAGE_SHA256

    def fixture(self, old_state=0x87, new_state=0x88):
        n = Native(self.image)
        context, delta = DATA, DATA + 0x10000
        root, unrelated, vendor = 0x40AEBF90, 0x12345678, 0x50CC9C7D
        for address, state in ((context + 8, old_state), (delta, new_state)):
            n.put(address, 2)
            n.put(address + 4, root)
            n.put(address + 8, unrelated)
            n.put(address + 0x404, 3)
            n.put(address + 0x424, 2)
            n.put(address + 0x428, state, "B")
            n.put(address + 0x429, 0x89, "B")
            n.put(address + 0x528, 1)
            block = address + 0x52C
            n.put(block, 6)
            n.put(block + 4, 1)
            n.put(block + 8, vendor)
            n.put(block + 0x188, 1)
            n.put(block + 0x194, 1)
            n.put(block + 0x198, 0x81, "B")
        events = []
        n.stubs[0x4294C0] = lambda: DATA + 0x30000
        n.stubs[0x429BA0] = lambda: n.put(n.arg(1), 6) or n.arg(1)
        n.stubs[0x3CA800] = lambda: events.append(("remove", n.get(n.arg(1))))
        n.stubs[0x3CBBE0] = lambda: events.append(("create", n.get(n.arg(1))))
        n.stubs[0x4EF900] = lambda: events.append(("refresh", 0))
        return n, context, delta, root, events

    def test_reinitializes_only_spawn_root_and_is_idempotent(self):
        n, context, delta, root, events = self.fixture()
        n.call(0x3CCE50, context, delta)
        self.assertEqual(events, [("remove", root), ("create", root), ("refresh", 0)])
        self.assertEqual(n.get(context + 0x430, "B"), 0x88)
        events.clear()
        n.call(0x3CCE50, context, delta)
        self.assertEqual(events, [])

    def test_unchanged_roster_does_not_reinitialize(self):
        n, context, delta, _, events = self.fixture(0x87, 0x87)
        n.call(0x3CCE50, context, delta)
        self.assertEqual(events, [])

    def test_biased_generation_wrap_is_a_single_replacement(self):
        n, context, delta, root, events = self.fixture(0xFF, 0x80)
        n.call(0x3CCE50, context, delta)
        self.assertEqual(events, [("remove", root), ("create", root), ("refresh", 0)])

    def test_retained_tower_globals_must_be_seeded_before_any_authority_applies(self):
        # Reproduce the captured 8A4E2843 controls (68/0, 11/1, 53/2).
        # Run original 4D6530: one unseeded global blocks every pending update.
        n = Native(self.image)
        context, directory, tables = DATA, DATA + 0x1000, DATA + 0x2000
        pool, vtable, descriptor = DATA + 0x4000, DATA + 0x6000, DATA + 0x7000
        bitmap, entries = DATA + 0x7800, DATA + 0x8000
        n.put(BASE + 0x2439C70, directory, "Q")
        n.put(directory, tables, "Q")
        n.put(tables + 8, entries, "Q")
        n.put(tables + 0x30, 0x88)
        n.put(tables + 0x34, 0)
        n.put(context + 0x808, pool, "Q")
        n.put(pool, vtable, "Q")
        n.put(vtable + 8, descriptor, "Q")
        n.put(descriptor + 0x10, bitmap, "Q")
        n.put(pool + 8, entries, "Q")
        n.put(pool + 0x1C, 0x80)
        n.put(pool + 0x20, 0x88)
        n.put(pool + 0x24, 0xFF)
        n.put(pool + 0x34, 0)
        n.put(bitmap, 7)
        n.stubs[0x1AB6610] = lambda: 3
        n.stubs[0x34E830] = lambda: 1
        def iterator():
            out = n.arg(0)
            n.put(out, 0)
            n.put(out + 8, bitmap, "Q")
            n.put(out + 0x10, 3)
            n.put(out + 0x14, 7)
        n.stubs[0x35E460] = iterator
        n.stubs[0x4294C0] = lambda: DATA + 0x9000
        n.stubs[0x429BA0] = lambda: n.put(n.arg(1), 0) or n.arg(1)
        for index, kind in enumerate((68, 11, 53)):
            row = entries + index * 0x88
            n.put(row, 0x8A4E2843)
            n.put(row + 4, kind, "H")
            n.put(row + 6, index, "H")
            n.put(row + 0x18, 1, "B")
            n.put(row + 0x68, 0xFFFFFFFF)
            n.put(row + 0x80, 1)
        for initialized in range(4):
            n.call(0x4D6530, context)
            self.assertEqual(n.uc.reg_read(UC_X86_REG_RAX) & 255, int(initialized == 3), initialized)
            if initialized < 3:
                n.put(entries + initialized * 0x88 + 0x18, 0, "B")


if __name__ == "__main__":
    unittest.main()
