"""Read-only native tag search for the seven installed Haunted Forest spawn hashes.

This is evidence collection, not a launch selector. Raw four-byte matches do not establish
field semantics; the result retains offsets and classes for a subsequent structural decode.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reader-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir))
    from pkg import Reader

    reader = Reader()
    targets = (0x79E3AB1F, 0x8BC697B5, 0x69310371, 0x69310372,
               0x69310373, 0x29E00F76, 0xC51AAC9E)
    needles = {value: struct.pack("<I", value) for value in targets}
    results = {f"{value:08X}": [] for value in targets}
    scanned = 0
    size = 0
    errors = []
    args.output.parent.mkdir(parents=True, exist_ok=True)

    def save(complete, packages):
        args.output.write_text(json.dumps({"complete": complete, "native_types": [8, 16],
            "scanned_tags": scanned, "scanned_bytes": size, "packages": packages,
            "installed_packages": len(reader.latest), "errors": errors, "matches": results},
            indent=2) + "\n", encoding="utf-8")

    for package_index, package_id in enumerate(reader.package_ids()):
        for tag, cls, typ, subtype, length in reader.scan_all([package_id]):
            if typ not in (8, 16):
                continue
            try:
                data, actual = reader.read_tag(tag)
            except FileNotFoundError as error:
                errors.append({"package": reader.stem_name(package_id),
                    "first_unread_tag": f"{tag:08X}", "reason": str(error)})
                # Missing an underlying patch prevents a complete package scan. Record this
                # explicitly rather than treating those tags as negative matches.
                break
            if actual != cls or len(data) != length:
                raise ValueError(f"tag identity/length changed: {tag:08X}")
            scanned += 1
            size += len(data)
            for value, needle in needles.items():
                offsets = []
                offset = data.find(needle)
                while offset >= 0:
                    offsets.append(offset)
                    offset = data.find(needle, offset + 1)
                if offsets:
                    results[f"{value:08X}"].append({"tag": f"{tag:08X}",
                        "class": f"{cls:08X}", "type": typ, "subtype": subtype,
                        "bytes": length, "offsets": offsets,
                        "package": reader.stem_name(package_id)})
        reader.package(package_id).close()
        del reader._packages[package_id]
        if package_index % 100 == 0:
            save(False, package_index + 1)
            print(f"Scanned {package_index + 1} packages, {scanned} native tags", flush=True)
    save(True, len(reader.latest))
    print(f"Complete: {scanned} tags / {size} bytes. Wrote {args.output}", flush=True)


if __name__ == "__main__":
    main()
