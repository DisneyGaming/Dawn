"""Read-only installed-package evidence for exact Haunted Forest activity 78.

Requires the existing offline pkg.py reader; writes only to --output.
No game process, settings, cache, or live package is modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def relative(data, offset):
    delta = struct.unpack_from("<q", data, offset)[0]
    target = offset + delta
    if not delta or not 0 <= target < len(data):
        raise ValueError(f"invalid relative pointer at {offset:X}")
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reader-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir))
    from pkg import Reader
    from arrays import resolve_descriptor

    reader = Reader()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixtures = output / "fixtures"
    fixtures.mkdir(exist_ok=True)
    provenance = {}

    def read(tag, expected=None):
        data, cls = reader.read_tag(tag)
        if data is None or (expected is not None and cls != expected):
            raise ValueError(f"missing/wrong class for {tag:08X}")
        name = f"{tag:08X}.bin"
        (fixtures / name).write_bytes(data)
        provenance[f"{tag:08X}"] = {
            "class": f"{cls:08X}", "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest().upper(),
            "entry": reader.entry_info(tag),
        }
        return data

    def array(data, offset, expected):
        found = resolve_descriptor(data, offset)
        if found is None or found[2] != expected:
            raise ValueError(f"missing/wrong array class at {offset:X}")
        return found[:2]

    public = read(0x81327CF0)
    count, start = array(public, 8, 0x808076FC)
    if count != 1170:
        raise ValueError("public catalog identity changed")
    variants = []
    for ordinal, expected in ((78, 0x56B7B6A5), (79, 0x2A30935A), (80, 0xE6ED0216)):
        entry = start + ordinal * 16
        record = relative(public, entry + 8)
        if u32(public, entry) != expected or u32(public, record) != expected:
            raise ValueError("public ordinal/hash mismatch")
        name_at = relative(public, record + 0x68)
        name = public[name_at:public.index(0, name_at)].decode("ascii")
        variants.append({"ordinal": ordinal, "hash": f"{expected:08X}",
                         "package": name, "native_type": public[record + 0xDA],
                         "destination": public[record + 0xE0],
                         "settings_hash": f"{u32(public, record + 0xDC):08X}",
                         "fixed_record_hex": public[record:record + 0xE4].hex()})

    scenario = read(0x81550015, 0x80809994)
    count, start = array(scenario, 80, 0x8080924D)
    bubbles = []
    for ordinal in range(count):
        bubble = start + ordinal * 24
        states, state_start = array(scenario, bubble + 8, 0x8080924F)
        row = {"ordinal": ordinal, "hash": f"{u32(scenario, bubble):08X}", "states": []}
        for state in range(states):
            at = state_start + state * 76
            entry_tag = u32(scenario, at + 68)
            entry = read(entry_tag, 0x8080925B)
            registry_tag = u32(entry, 20)
            read(registry_tag, 0x8080925E)
            row["states"].append({
                "ordinal": state, "region": ordinal * 8 + state,
                "enabled": scenario[at], "public": scenario[at + 12],
                "state_hash": f"{u32(scenario, at + 4):08X}",
                "state_offset": at,
                "native_slice_descriptor_offset": at + 28,
                "bubble_hash": f"{u32(scenario, at + 64):08X}",
                "map_bubble": u32(scenario, at + 28),
                "entry": f"{entry_tag:08X}", "entry_index": u32(entry, 16),
                "registry": f"{registry_tag:08X}",
            })
        bubbles.append(row)

    read(0x81550000, 0x80808AAE)
    map_root = read(0x8150A9FF, 0x808091DE)
    read(0x8150A9FD, 0x80809962)
    map_count, map_start = array(map_root, 16, 0x80807D53)
    map_links = [{"ordinal": i,
                  "hash64": f"{struct.unpack_from('<Q', map_root, map_start + i * 16 + 8)[0]:016X}",
                  "mappings": []} for i in range(map_count)]
    wanted = {int(row["hash64"], 16): row for row in map_links}
    metadata_provenance = []
    # Header +F0/+F4 bounds the uncompressed package metadata. Its +30 descriptor has
    # a logical count; unlike ordinary arrays, the 80809D02 allocation may have spare rows.
    # Resolve every installed newest package, not just the map's package, before concluding
    # that a map child is unavailable. This reproduces the package Hash64-to-tag join.
    for package_id, (stem, patch) in sorted(reader.latest.items()):
        path = Path(f"{stem}_{patch}.pkg")
        with path.open("rb") as stream:
            header = stream.read(0x180)
            metadata_at, metadata_size = struct.unpack_from("<II", header, 0xF0)
            if not metadata_at or not metadata_size:
                continue
            stream.seek(metadata_at)
            metadata = stream.read(metadata_size)
        if len(metadata) != metadata_size or len(metadata) < 64:
            raise ValueError(f"short metadata in {path.name}")
        logical = struct.unpack_from("<Q", metadata, 48)[0]
        if not logical:
            continue
        allocation = relative(metadata, 56)
        capacity, cls = struct.unpack_from("<QI", metadata, allocation)
        start = allocation + 16
        if cls != 0x80809D02 or capacity < logical or start + capacity * 16 > len(metadata):
            raise ValueError(f"invalid Hash64 metadata in {path.name}")
        for index in range(logical):
            key, tag, expected = struct.unpack_from("<QII", metadata, start + index * 16)
            if key not in wanted:
                continue
            wrapper = read(tag, expected)
            if expected != 0x80807DAE:
                raise ValueError("map link has unexpected wrapper class")
            child = u32(wrapper, 8)
            child_data = read(child, 0x808091E0)
            wanted[key]["mappings"].append({
                "wrapper": f"{tag:08X}", "child": f"{child:08X}",
                "bubble_hash": f"{u32(wrapper, 24):08X}",
                "child_bubble_ordinal": u32(child_data, 8),
                "package": path.name,
                "metadata_row_offset": metadata_at + start + index * 16,
            })
            metadata_provenance.append({"package": path.name,
                "header_sha256": hashlib.sha256(header).hexdigest().upper(),
                "offset": metadata_at, "bytes": metadata_size,
                "sha256": hashlib.sha256(metadata).hexdigest().upper()})
            (fixtures / f"metadata_{package_id:04X}_{patch}.bin").write_bytes(metadata)
    loaded = [row["ordinal"] for row in map_links if row["mappings"]]
    if loaded != [13] or map_links[13]["mappings"][0]["bubble_hash"] != "47EA4CE9":
        raise ValueError("installed loadable Forest mapping changed")
    sets, components, containers = {}, {}, {}
    package_ids = [0x685, 0x6A8]
    for tag, _size in reader.scan_class(0x80809162, package_ids):
        data = read(tag, 0x80809162)
        count, start = array(data, 8, 0x80809164)
        sets[tag] = {"tag": f"{tag:08X}", "points": [], "containers": []}
        for index in range(count):
            at = start + index * 48
            sets[tag]["points"].append({
                "hash": f"{u32(data, at + 32):08X}",
                "position": list(struct.unpack_from("<3f", data, at + 16)),
                "rotation": list(struct.unpack_from("<4f", data, at)),
            })
    for tag, _size in reader.scan_class(0x808099D6, package_ids):
        data, _cls = reader.read_tag(tag)
        resource = u32(data, 216) if data is not None and len(data) >= 220 else 0
        if resource in sets:
            read(tag, 0x808099D6)
            components[tag] = resource
    for tag, _size in reader.scan_class(0x80808A54, package_ids):
        data, _cls = reader.read_tag(tag)
        found = resolve_descriptor(data, 40) if data is not None else None
        if found is None:
            continue
        count, start, cls = found
        if cls != 0x80808BB0:
            raise ValueError("wrong container member class")
        for index in range(count):
            component = u32(data, start + index * 4)
            if component not in components:
                continue
            read(tag, 0x80808A54)
            mask = [bit for bit in range(256) if data[8 + bit // 8] & (1 << (bit % 8))]
            binding = {"tag": f"{tag:08X}", "component": f"{component:08X}",
                       "map_bubbles": mask}
            sets[components[component]]["containers"].append(binding)
            containers[tag] = binding

    # These resource names and references distinguish reward/top placement from an entry point.
    for tag in (0x81550330, 0x81550342, 0x81550345, 0x8155034B,
                0x81550188, 0x81550320, 0x81550328, 0x8155034E):
        read(tag)
    report = {"schema": 2, "variants": variants, "scenario": "81550015",
              "map": "infinite_forest_live", "bubbles": bubbles,
              "spawn_sets": list(sets.values()), "provenance": provenance,
              "map_links": map_links, "metadata_provenance": metadata_provenance,
              "metadata_packages_scanned": len(reader.latest),
              "verified_loadable_regions": [ordinal * 8 for ordinal in loaded],
              "verified_initial_region": 104, "verified_initial_spawn": None}
    if args.log:
        log = args.log.read_bytes()
        report["log"] = {"path": str(args.log.resolve()),
                         "sha256": hashlib.sha256(log).hexdigest().upper()}
        lines = log.decode("utf-8", errors="replace").splitlines()
        selected = [(i + 1, line) for i, line in enumerate(lines)
                    if any(term in line for term in ("activity=78", "infinite_abyss",
                           "cannot be found!", "error:slice_set_not_found", "ev=build_identity"))]
        (output / "haunted_forest_log_excerpt.txt").write_text(
            "\n".join(f"{i}: {line}" for i, line in selected) + "\n", encoding="utf-8")
    (output / "haunted_forest_inventory.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(variants)} identities, {len(bubbles)} bubbles, "
          f"{len(sets)} spawn set(s), {len(provenance)} captured tags. "
          "Only region 104 has an installed map child. Initial spawn remains unverified.")


if __name__ == "__main__":
    main()
