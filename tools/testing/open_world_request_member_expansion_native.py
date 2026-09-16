"""Replay native per-category request expansion without claiming actor admission.

The supported executable's original 4EC4C0 routine expands each category count
into selected-member records consumed by 4E2E80. Selection itself is a fixture
boundary here; queue processing and entity creation are intentionally out of
scope.
"""
from pathlib import Path
import argparse
import hashlib
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import mercury_streaming_native as native

    image = args.image.read_bytes()
    assert hashlib.sha256(image).hexdigest() == native.IMAGE_SHA256, "unsupported executable image"
    for requested, expected in ((1, 1), (2, 2), (3, 3), (21, 21), (33, 32)):
        machine = native.Native(image)
        source, directory, registry = native.DATA, native.DATA + 0x2000, native.DATA + 0x3000
        definition, request, output = native.DATA + 0x20000, native.DATA + 0x28000, native.DATA + 0x30000
        machine.put(native.BASE + 0x2439C70, directory, "Q")
        machine.put(directory, registry, "Q")
        machine.put(registry + 8, definition, "Q")
        machine.put(registry + 0x30, 0x1000)
        machine.put(registry + 0x34, 0)
        machine.put(source, 0)
        machine.put(source + 8, 0, "Q")
        machine.put(definition + 0xB0, 0, "Q")
        machine.put(definition + 0xC4, 0, "B")
        machine.put(request, 1)
        machine.put(request + 4, requested)
        selected = []

        def select_member():
            selected.append((machine.arg(1), machine.arg(2)))

        machine.stubs.update({0x4E34C0: select_member, 0x187C480: lambda: None})
        machine.call(0x4EC4C0, source, request, output)
        assert machine.get(output) == expected
        assert len(selected) == expected
        assert [address for _, address in selected] == [output + 8 + index * 40 for index in range(expected)]
        assert all(category == 0 for category, _ in selected)
    print("PASS native open-world request expansion: 5 cases, one selected-member record per request unit, 32-record cap")


if __name__ == "__main__":
    main()
