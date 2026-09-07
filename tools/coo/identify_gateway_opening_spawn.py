"""Identify Gateway's opening spawn set from installed monitor, volume and map tags.

This is an offline read. It does not select a destination or change game memory.
"""
import hashlib
import json
import struct
from pathlib import Path

import package_read as packages

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/coo/gateway-research/gateway-opening-spawn-evidence.json"


def array(data, offset, stride):
    count = struct.unpack_from("<Q", data, offset)[0]
    header = offset + 8 + struct.unpack_from("<q", data, offset + 8)[0]
    assert count == struct.unpack_from("<Q", data, header)[0]
    start = header + 16
    assert 0 <= start <= start + count * stride <= len(data)
    return range(start, start + count * stride, stride)


def inside(polygon, point):
    x, y, _ = point
    result = False
    previous = polygon[-1]
    for current in polygon:
        ax, ay, _ = previous
        bx, by, _ = current
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            result = not result
        previous = current
    return result


def main():
    monitor_tag, volume_tag = 0x80F473D0, 0x80F470E5
    monitor_class, monitor = packages.read(monitor_tag)
    volume_class, volume = packages.read(volume_tag)
    assert monitor_class == volume_class == 0x80809C36
    assert b"leadup_lz_start_player_monitor\0" in monitor
    binding = monitor[0x270:0x278]
    registry, packed_slot = struct.unpack("<II", binding)
    assert (registry, packed_slot) == (0x85742F3E, 0x0167003C)
    candidates = []
    cursor = 0
    while (offset := volume.find(binding, cursor)) >= 0:
        cursor = offset + 1
        base = offset - 12
        if base < 0:
            continue
        name_offset = base + struct.unpack_from("<q", volume, base)[0]
        if 0 <= name_offset < len(volume):
            name = volume[name_offset:].split(b"\0", 1)[0]
            if name == b"leadup_lz_start_trigger_volume":
                candidates.append(base)
    assert len(candidates) == 1, candidates
    base = candidates[0]
    lower = struct.unpack_from("<3f", volume, base + 0xB0)
    upper = struct.unpack_from("<3f", volume, base + 0xC0)
    polygon = [struct.unpack_from("<3f", volume, offset)
               for offset in array(volume, base + 0xD0, 16)]
    matches = []
    point_count = 0
    for pid in (0x0356, 0x03A6, 0x03A7, 0x03A8):
        path, package_data, table, count, _ = packages.package(pid)
        for index in range(count):
            if struct.unpack_from("<I", package_data, table + 16 * index)[0] != 0x80809162:
                continue
            tag = 0x80800000 + (pid << 13) + index
            _, data = packages.read(tag)
            for point_index, offset in enumerate(array(data, 8, 48)):
                point_count += 1
                position = struct.unpack_from("<3f", data, offset + 16)
                if not (lower[2] <= position[2] <= upper[2] and inside(polygon, position)):
                    continue
                matches.append({"hash": f"{struct.unpack_from('<I', data, offset + 32)[0]:08X}",
                                "position": position,
                                "rotation": struct.unpack_from("<4f", data, offset),
                                "pointIndex": point_index, "offset": hex(offset),
                                "tag": f"{tag:08X}", "package": path.name,
                                "tagSha256": hashlib.sha256(data).hexdigest()})
    assert len(matches) == 3, matches
    assert {point["hash"] for point in matches} == {"69F52B3E"}
    evidence = {
        "status": "package-verified opening-area match; live arrival pending",
        "method": "Named opening monitor -> native trigger volume -> containment of native map spawn points",
        "selection": {"activity": "mission_abs", "bubble": 15, "sliceSet": 120,
                      "spawnSetHash": "69F52B3E"},
        "monitor": {"tag": f"{monitor_tag:08X}", "referenceOffset": "0x270",
                    "name": "leadup_lz_start_player_monitor",
                    "sha256": hashlib.sha256(monitor).hexdigest()},
        "volume": {"tag": f"{volume_tag:08X}", "offset": hex(base),
                   "registry": f"{registry:08X}", "type": packed_slot & 0xFFFF,
                   "slot": packed_slot >> 16, "name": "leadup_lz_start_trigger_volume",
                   "boundsMin": lower, "boundsMax": upper, "polygon": polygon,
                   "sha256": hashlib.sha256(volume).hexdigest()},
        "scannedMapPackages": ["0356", "03A6", "03A7", "03A8"],
        "scannedSpawnPointCount": point_count,
        "matchingSpawnPoints": matches,
        "limits": "Spatial authoring evidence, not a recovered explicit mission-start command or a live spawn receipt."
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"Matched 69F52B3E: {len(matches)} points inside the named opening volume; scanned {point_count} native map spawn points.")
    print(OUT)


if __name__ == "__main__":
    main()
