"""Execute pinned native detach/delete and source-budget routines in Unicorn.

No live process access. The allocator, scheduler, and unrelated world services
are fixtures; the native facet walk, saved-state cleanup, and spawn arithmetic
execute from the unmodified supported executable image.
"""
import argparse
import hashlib
import struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

IMAGE_SHA256 = "63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e"
BASE, DATA, STACK, END = 0x140000000, 0x50000000, 0x60000000, 0x600FF000


class Native:
    def __init__(self, image):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(BASE, (len(image) + 4095) & ~4095)
        self.uc.mem_write(BASE, image)
        self.uc.mem_map(DATA, 0x400000)
        self.uc.mem_map(STACK, 0x100000)
        self.stubs, self.calls = {}, []
        self.uc.hook_add(UC_HOOK_CODE, self.hook)

    def put(self, address, value, fmt="I"):
        self.uc.mem_write(address, struct.pack("<" + fmt, value))

    def get(self, address, fmt="I"):
        return struct.unpack("<" + fmt, self.uc.mem_read(address, struct.calcsize(fmt)))[0]

    def hook(self, uc, address, size, _):
        if address == END:
            uc.emu_stop()
            return
        rva = address - BASE
        if rva in self.stubs:
            self.calls.append(rva)
            result = self.stubs[rva]()
            if result is not None:
                uc.reg_write(UC_X86_REG_RAX, result)
            sp = uc.reg_read(UC_X86_REG_RSP)
            uc.reg_write(UC_X86_REG_RIP, self.get(sp, "Q"))
            uc.reg_write(UC_X86_REG_RSP, sp + 8)

    def arg(self, index):
        return self.uc.reg_read([UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9][index])

    def call(self, rva, *args):
        sp = STACK + 0xEFFF8
        self.put(sp, END, "Q")
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        for register, value in zip([UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9], args):
            self.uc.reg_write(register, value)
        self.uc.emu_start(BASE + rva, END, count=200000)
        assert self.uc.reg_read(UC_X86_REG_RIP) == END, "native routine did not return"


def network_case(image, owner=-1):
    n = Native(image)
    manager, pool, context = DATA, DATA + 0x10000, DATA + 0x11000
    root, child, unrelated = 0x100007, 0x100008, 0x100009
    nets = [0x21FBA007, 0x21FBA008, 0x21FBA009]
    rows = [BASE + 0x30B0440 + i * 0x70 for i in [3, 4, 5]]
    n.put(BASE + 0x2037D48, pool, "Q")
    n.put(BASE + 0x2037D50, 12)
    # Clear the global bitmap captured in the image before creating fixtures.
    n.uc.mem_write(BASE + 0x30B0340, bytes(128))
    frees, destroyed_objects, events, released_nets = [], [], [], []
    n.stubs.update({
        0x3F76C0: lambda: context,
        0x16FC5E0: lambda: DATA + 0x20000,
        0x17019E0: lambda: manager,
        0x187C480: lambda: None,  # stack cookie check
        0x4C04F0: lambda: None,  # diagnostic member cleanup
        0x16C9950: lambda: frees.append(n.arg(0)),
        0x171A260: lambda: frees.append(n.arg(0)),
        0x187E862: lambda: n.uc.mem_write(n.arg(0), bytes([n.arg(1) & 255]) * n.arg(2)),
        0x1708A00: lambda: destroyed_objects.append(n.arg(0)),
    })
    # Native unlink is orthogonal to deletion; model its parent-list effect.
    def unlink():
        if n.arg(1) == child:
            n.put(rows[0] + 0x48, 0xFFFFFFFF)
    n.stubs[0x171C360] = unlink
    for handle, net, row, slot in zip([root, child, unrelated], nets, rows, [3, 4, 5]):
        local = handle & 8191
        n.put(manager + 0x114 + local * 6, slot, "h")
        n.put(manager + 0x114 + local * 6 + 2, 0, "B")
        n.put(manager + 0x114 + local * 6 + 3, 15, "B")
        n.put(manager + 0xC520, n.get(manager + 0xC520) | (1 << local))
        n.put(manager + 0xC920, n.get(manager + 0xC920) | (1 << slot))
        n.put(BASE + 0x30B0340, n.get(BASE + 0x30B0340) | (1 << slot))
        n.uc.mem_write(row, bytes(0x70))
        n.put(row + 1, owner, "b")
        n.put(row + 4, net)
        n.put(row + 8, handle)
        n.put(row + 0x48, child if handle == root else 0xFFFFFFFF)
        n.put(row + 0x30, DATA + 0x18000 + slot * 0x100, "Q")
        n.put(row + 0x38, DATA + 0x19000 + slot * 0x100, "Q")
        n.put(row + 0x60, DATA + 0x1A000 + slot * 0x100, "Q")
        n.put(pool + (net & 8191) * 12, handle)
        n.put(pool + (net & 8191) * 12 + 4, 0x12FC4000 + slot)
    if owner == -2:
        # Execute the real unowned-facet serializer, then its native event
        # receiver after cleanup. Only the local event queue is a fixture.
        del n.stubs[0x1708A00]
        n.put(BASE + 0x2037D74, (nets[0] >> 13) & 0x3FF)
        n.stubs[0x187E85C] = lambda: n.uc.mem_write(n.arg(0), bytes(n.uc.mem_read(n.arg(1), n.arg(2))))
        def allocate_event():
            assert n.arg(0) == 5
            event = DATA + 0x28000 + len(events) * 0x1000
            n.put(event + 4, n.arg(1))
            n.put(event + 8, event + 0x100, "Q")
            n.put(n.arg(2), event, "Q")
            return 0
        n.stubs[0x40E7C0] = allocate_event
        n.stubs[0x40F880] = lambda: events.append(n.arg(0))
        n.stubs[0x16FC5A0] = lambda: DATA + 0x20000
        n.stubs[0x16EAE90] = lambda: DATA + 0x21000
        n.stubs[0x12AADF0] = lambda: 0
        n.stubs[0x352400] = lambda: 1
        n.stubs[0x34F790] = lambda: released_nets.append(n.arg(1))
        # Both source-owned objects are already undergoing native teardown.
        n.call(0x1704870, nets[1], 0xFFFFFFFF)
    n.call(0x1704870, nets[0], 0xFFFFFFFF)
    assert n.get(pool + 7 * 12 + 4) == 0xFFFFFFFF
    assert n.get(pool + 7 * 12) == root, "detach alone must leave the saved facet"
    n.call(0x16EE070, nets[0], 0)
    assert n.get(pool + 7 * 12) == n.get(pool + 8 * 12) == 0xFFFFFFFF
    assert n.get(manager + 0xC520) & ((1 << 7) | (1 << 8)) == 0
    assert n.get(BASE + 0x30B0340) & ((1 << 3) | (1 << 4)) == 0
    assert n.get(manager + 0x114 + 7 * 6, "h") == -1
    assert n.get(manager + 0x114 + 8 * 6, "h") == -1
    assert len(frees) == 6 and len(set(frees)) == 6
    assert not destroyed_objects, "local AI teardown must remain with its caller"
    assert n.get(pool + 9 * 12) == unrelated and n.get(BASE + 0x30B0340) & (1 << 5)
    n.call(0x16F0080, nets[0])  # caller's following save call is now a no-op
    assert len(frees) == 6
    if owner == -2:
        assert len(events) == 2
        for event in events:
            n.call(0x1708730, event)
        assert set(released_nets) == set(nets[:2])
        assert len(frees) == 6
    # A delayed detach must not be interpreted as a completed write.
    deferred = []
    n.put(context, 2, "B")
    n.stubs[0x4CB1C0] = lambda: deferred.append(n.arg(2))
    n.call(0x1704870, nets[2], 0xFFFFFFFF)
    assert deferred and n.get(pool + 9 * 12 + 4) != 0xFFFFFFFF
    print(f"PASS native detach/full-delete owner={owner}: descendants, saved buffers, sync state, both allocation maps, unrelated facet, deferred write, native teardown events")


def source_cases(image):
    n = Native(image)
    source, authority, directory, registry, definition = [DATA + x for x in [0, 0x1000, 0x2000, 0x3000, 0x20000]]
    n.put(BASE + 0x2439C70, directory, "Q")
    n.put(directory, registry, "Q")
    n.put(registry + 8, definition, "Q")
    n.put(registry + 0x30, 0x1000)
    n.put(registry + 0x34, 0)
    n.put(source, 0)
    n.put(source + 8, 0, "Q")
    n.put(definition + 0xA8, 1)
    alive, requests = [0], []
    def living():
        n.uc.mem_write(n.arg(1), struct.pack("<8i", alive[0], *([0] * 7)))
    def dispatch():
        requests.append(n.get(n.arg(2) + 4))
    n.stubs.update({0x4E3410: living, 0x187C480: lambda: None,
                    0x4C4B90: lambda: None, 0x4E2E80: dispatch})
    for requested, consumed, living_count, pending, mode, expected in [
        (4, 0, 0, 0, 0, 4), (4, 1, 0, 0, 0, 3),
        (4, 1, 2, 0, 0, 1), (4, 1, 0, 1, 2, 2),
        (4, 4, 0, 0, 0, 0), (4, 0, 0, 0, 1, 0),
        (4, 0, 0, 0, 3, 0), (4, 0, 0, 0, 4, 0),
    ]:
        alive[0] = living_count
        n.put(source + 0x650, requested)
        n.put(source + 0x268, consumed)
        n.put(source + 0x670, pending)
        n.put(authority + 0xBD, mode, "B")
        requests.clear()
        n.call(0x4E4580, source, 0, authority)
        assert sum(requests) == expected, (requested, consumed, living_count, pending, mode, requests)
        assert n.get(source + 0x650) == requested and n.get(authority + 0xBD, "B") == mode
    # These cumulative totals exceed the former uint8 policy boundary. Keep
    # only a small residual deficit so this proves the original routine reads
    # the full signed 32-bit counters without authorizing a large spawn burst.
    wide = [
        (64, 61, 0, 0, 3),
        (256, 252, 1, 0, 3),
        (65536, 65530, 2, 1, 3),
        (0x7FFFFFFF, 0x7FFFFFFC, 0, 0, 3),
        (0x7FFFFFFF, 0x7FFFFFF9, 2, 1, 3),
        # Pending births reduce, rather than inflate, the dispatched deficit.
        (64, 60, 1, 2, 1),
        (256, 250, 2, 3, 1),
        (65536, 65531, 1, 3, 1),
    ]
    for requested, consumed, living_count, pending, expected in wide:
        alive[0] = living_count
        n.put(source + 0x650, requested)
        n.put(source + 0x268, consumed)
        n.put(source + 0x670, pending)
        n.put(authority + 0xBD, 0, "B")
        requests.clear()
        n.call(0x4E4580, source, 0, authority)
        assert sum(requests) == expected, (requested, consumed, living_count, pending, requests)
        assert all(0 < request <= 3 for request in requests), requests
        assert n.get(source + 0x650) == requested, "native target counter truncated"
        assert n.get(source + 0x268) == consumed, "fixture dispatch unexpectedly consumed work"
        assert n.get(source + 0x670) == pending, "fixture dispatch unexpectedly changed pending births"
    # Native deactivation consumes the remaining budget. A pre-deactivation
    # checkpoint is necessary: copying its final counter would suppress AI.
    alive[0] = 0
    n.put(source + 0x650, 4)
    n.put(source + 0x268, 1)
    n.call(0x4E8270, source, 0)
    assert n.get(source + 0x268) == 4
    print("PASS native source arithmetic: fresh AI, partial kills, residents, pending births, exhausted quota, all authority modes, 32-bit cumulative boundaries with bounded deficits, deactivation")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    image = args.image.read_bytes()
    assert hashlib.sha256(image).hexdigest() == IMAGE_SHA256, "unsupported executable image"
    network_case(image)
    network_case(image, -2)
    source_cases(image)
