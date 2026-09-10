"""Read-only installed-package evidence for Mercury's unresolved faction battle.

Uses the existing authenticated package Reader. Writes facts and SHA-256 values
only; it never opens the game process, sends a command, or changes an install.
--scan-message-references scans class-8080 metadata (entries <=8 MB), clearing
package caches as it goes. A missing reference is bounded negative evidence,
not proof that native or host-side scheduling cannot use this incident.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def i64(data, offset):
    return struct.unpack_from("<q", data, offset)[0]


def array(data, offset):
    count = struct.unpack_from("<Q", data, offset)[0]
    header = offset + 8 + i64(data, offset + 8)
    if not 0 < count <= 300000 or not 4 <= header <= len(data) - 16:
        raise ValueError("array bounds")
    if (u32(data, header - 4) >> 16) != 0x8080 or struct.unpack_from("<Q", data, header)[0] != count:
        raise ValueError("array marker/count")
    return count, header + 16, u32(data, header + 8)


def localized(container, language, wanted):
    count, data, _ = array(container, 8)
    hashes = [u32(container, data + i * 4) for i in range(count)]
    ordinal = hashes.index(wanted)
    combinations, first, _ = array(language, 72)
    if combinations != count:
        raise ValueError("language cardinality")
    combination = first + ordinal * 16
    part = combination + i64(language, combination)
    parts = i64(language, combination + 8)
    if not 0 <= parts <= 32 or not 0 <= part <= len(language) - 32 * parts:
        raise ValueError("string part bounds")
    output = bytearray()
    for index in range(parts):
        row = part + index * 32
        start = row + 8 + i64(language, row + 8)
        size, shift = (struct.unpack_from("<H", language, row + off)[0] for off in (20, 24))
        if not 0 <= start <= len(language) - size:
            raise ValueError("string byte bounds")
        raw = bytearray(language[start:start + size])
        position = 0
        while position < len(raw):
            value = raw[position]
            width = 1 if value < 0xC0 else 2 if value < 0xE0 else 3 if value < 0xF0 else 4
            if value >= 0xF5 or position + width > len(raw):
                raise ValueError("UTF-8 bounds")
            raw[position + width - 1] = (raw[position + width - 1] + shift) & 255
            position += width
        output.extend(raw)
    return ordinal, output.decode("utf-8"), output.hex()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reader-dir", type=Path, default=Path(r"D:\Sunrise-work\scripts"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scan-message-references", action="store_true")
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir.resolve()))
    from pkg import Reader, TAG_BASE, ENTRY_BITS
    reader = Reader()
    report = {"scenario": "80F4696A", "bubble": 15, "activation_supported": False, "files": {}}

    def read(tag, expected):
        body, cls = reader.read_tag(tag)
        if cls != expected:
            raise ValueError(f"{tag:08X}: class {cls:08X}, expected {expected:08X}")
        report["files"][f"{tag:08X}"] = {"class": f"{cls:08X}", "bytes": len(body),
            "sha256": hashlib.sha256(body).hexdigest()}
        return body

    wanted = 0xA2DD920A
    container = read(0x80B9E37E, 0x80809A88)
    english_tag = u32(container, 24)
    if english_tag != 0x80B34700:
        raise ValueError("English language tag changed")
    english = read(english_tag, 0x80809A8A)
    ordinal, text, encoded = localized(container, english, wanted)
    if ordinal != 25 or encoded != "54686520656e656d79206973206d6f76696e6720616761696e73742065616368206f74686572e280a6":
        raise ValueError("announcement changed")
    incident_rows = []
    from mercury_faction_battle_presentation import incident_presentation, presentation_variant
    for tag in (0x80B9E5BF, 0x81327CD4):
        body = read(tag, 0x80807C9B)
        count = struct.unpack_from("<Q", body, 112)[0]
        matches = [index for index in range(count) if u32(body, 128 + index * 40) == wanted]
        if matches != [4852] or u32(body, 128 + 4852 * 40 + 36) != 0:
            raise ValueError("native incident identity/type changed")
        binding = incident_presentation(body, wanted)
        if binding['presentation_list_index'] != 157 or binding['presentation_hashes'] != ['A2DD920A']:
            raise ValueError('native incident presentation list changed')
        incident_rows.append({"table": f"{tag:08X}", "index": 4852, "offset": 194208, "type": 0,
                              "binding": binding})
    presentation = read(0x80B9E5E3, 0x80806485)
    count, start, cls = array(presentation, 40)
    matches = [index for index in range(count) if u32(presentation, start + index * 24) == wanted]
    if cls != 0x80806489 or matches != [287]:
        raise ValueError("native presentation identity changed")
    variants, body, variant_cls = array(presentation, start + 287 * 24 + 8)
    if variants != 1 or variant_cls != 0x8080648B or u32(presentation, body + 8) != wanted or u32(presentation, body + 24) != 0x80B9E37E:
        raise ValueError("native presentation string binding changed")
    report["announcement"] = {"hash": f"{wanted:08X}", "text": text, "utf8_hex": encoded,
        "container": "80B9E37E", "english": "80B34700", "ordinal": ordinal,
        "incident_rows": incident_rows, "presentation_table": "80B9E5E3",
        "presentation_row": 287, "presentation_variant_offset": body,
        "native_delivery_and_receipt_qualified": False}
    report['announcement']['presentation'] = presentation_variant(presentation, wanted)
    retreat_hash = 0x8753E5BA
    retreat_ordinal, retreat_text, retreat_encoded = localized(container, english, retreat_hash)
    report['adjacent_retreat_announcement'] = {
        'hash': f'{retreat_hash:08X}', 'text': retreat_text, 'utf8_hex': retreat_encoded,
        'container': '80B9E37E', 'ordinal': retreat_ordinal,
        'incident': incident_presentation(read(0x80B9E5BF, 0x80807C9B), retreat_hash),
        'presentation': presentation_variant(presentation, retreat_hash),
        'mercury_completion_trigger': None}
    # Pin the three candidate objects independently of the inventory JSON. This
    # does not assert that any is the requested faction battle.
    report["chest_skirmish_leads"] = []
    for tag, key in ((0x80F5BFA1, 0x0F075D5A), (0x80F5BFF4, 0x0F075D59), (0x80F5E047, 0x0F075D58)):
        body = read(tag, 0x80809462)
        if u32(body, 12) != key:
            raise ValueError("candidate registry changed")
        report["chest_skirmish_leads"].append({"object": f"{tag:08X}", "registry": f"{key:08X}",
            "identified_as_faction_battle": False})
    scenario = read(0x80F4696A, 0x80809994)
    bubbles, first, _ = array(scenario, 80)
    if bubbles <= 15:
        raise ValueError("landing bubble absent")
    states, first, _ = array(scenario, first + 15 * 24 + 8)
    matches = [first + index * 76 for index in range(states)
               if u32(scenario, first + index * 76 + 68) == 0x80F46AD5]
    if len(matches) != 1:
        raise ValueError("landing slice ownership changed")
    entry = read(0x80F46AD5, 0x8080925B)
    if u32(entry, 16) != 15 or u32(entry, 20) != 0x80F46C89:
        raise ValueError("landing registry link changed")
    registry = read(0x80F46C89, 0x8080925E)
    placed = []
    for offset in (8, 24, 40):
        if struct.unpack_from("<Q", registry, offset)[0]:
            count, start, _ = array(registry, offset)
            placed.extend(u32(registry, start + index * 4) for index in range(count))
    if not {0x80F5BFA1, 0x80F5BFF4, 0x80F5E047}.issubset(placed):
        raise ValueError("candidate scenario ownership changed")
    report["candidate_ownership"] = {"scenario": "80F4696A", "bubble": 15,
        "entry": "80F46AD5", "registry": "80F46C89", "activation_policy_recovered": False}
    from mercury_faction_battle_catalog import extract
    report["patrol_catalog"] = extract(read, scenario, placed)
    if args.scan_message_references:
        hits, total, errors = [], 0, []
        needle = struct.pack("<I", wanted)
        for pid in reader.package_ids():
            package = reader.package(pid)
            for index in range(package.header.entry_count):
                cls, _, placement = package.entry(index)
                if cls >> 16 != 0x8080 or package.placement(placement)[2] > 8000000:
                    continue
                tag = TAG_BASE + (pid << ENTRY_BITS) + index
                try:
                    body, _ = reader.read_tag(tag)
                except Exception as error:
                    errors.append({"tag": f"{tag:08X}", "class": f"{cls:08X}", "error": str(error)})
                    continue
                total += 1
                offset = body.find(needle)
                if offset >= 0:
                    hits.append({"tag": f"{tag:08X}", "class": f"{cls:08X}", "offset": offset})
            package.close()
            reader._packages.pop(pid, None)
        report["message_reference_scan"] = {"read_metadata_entries": total, "read_errors": errors, "hits": hits,
            "scope": "class high16=8080; size<=8000000; newest package patch selected by Reader"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Verified native announcement, eight patrol catalog rows and three chest source sets; wrote {args.output}")


if __name__ == "__main__":
    main()
