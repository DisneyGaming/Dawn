"""Recover and verify Eater's reactor platform output bindings offline.

The verifier reads installed Tiger packages and the pinned unpacked executable. It never
starts or attaches to Destiny. The generated header exposes only package identities and
the generic-device output join; it deliberately does not assign occupancy semantics to
the authority-only type-34 collections.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs/raids/eater-of-worlds/tools"))
import extract_eater_of_worlds as eater  # type: ignore  # noqa: E402


INVENTORY = ROOT / "docs/raids/eater-of-worlds/evidence/native-inventory.json"
EVIDENCE = ROOT / "docs/raids/eater-of-worlds/evidence/reactor-platform-bindings.json"
HEADER = ROOT / "Dawn/src/state/activity/eater_of_worlds/mechanisms.h"
IMAGE = ROOT / "destiny2_unpacked.bin"

EXPECTED_IMAGE_SHA256 = "63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e"
REACTOR_REGISTRY = 0x686321C8
PLATFORM_ENTITY = 0x80F42FCD
GENERIC_DEFINITION = 0x80C3EE24
GENERIC_BUILD_ORDINAL = 6
GENERIC_DEFINITION_SUBTYPE = 0x8080390E
GENERIC_DECLARED_SIZE = 0x90
GENERIC_RUNTIME_KIND = 0x80803910
GENERIC_RUNTIME_OFFSET = 0xA78
GENERIC_AUTH_SCHEMA = 0x80805063
TYPE4_COMPONENT = 0x80809927
TYPE4_SENSE = 0x8080992E
TYPE4_AUTH = 0x8080992F
TYPE34_COMPONENT = 0x80809568
TYPE34_AUTH = 0x8080956A
PATHS = {"first": 0, "second": 1, "third": 2, "fourth": 3}
PATH_COUNTS = (13, 11, 13, 19)
PLATFORM_PATTERN = re.compile(
    r"u_falling_platforms_(first|second|third|fourth)_path\[(\d+)]\.o_platform"
)
COLLECTION_PATTERN = re.compile(
    r"u_falling_platforms_(first|second|third|fourth)_path\[(\d+)]\.of_all_players"
)


def number(value: int | str) -> int:
    return int(value, 16) if isinstance(value, str) else value


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def u64(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", blob, offset)[0]


def i64(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<q", blob, offset)[0]


def package_name(blob: bytes, pattern: re.Pattern[str]) -> tuple[int, int, str]:
    matches = []
    for match in re.finditer(rb"[A-Za-z_][A-Za-z0-9_.\[\]]{4,}", blob):
        text = match.group().decode("ascii")
        parsed = pattern.fullmatch(text)
        if parsed:
            matches.append((PATHS[parsed.group(1)], int(parsed.group(2)), text))
    assert len(matches) == 1
    return matches[0]


def standard_array(blob: bytes, count_offset: int, relative_offset: int,
                   stride: int, expected_class: int) -> tuple[int, int]:
    count = u64(blob, count_offset)
    header = relative_offset + i64(blob, relative_offset)
    assert count > 0 and u32(blob, header - 4) == 0x80809FBD
    assert u64(blob, header) == count and u32(blob, header + 8) == expected_class
    data = header + 16
    assert data + count * stride <= len(blob)
    return count, data


def recover_generic_join() -> dict[str, object]:
    entity_class, entity = eater.packages.read(PLATFORM_ENTITY)
    assert entity_class == 0x80809C0F
    build_count = u64(entity, 0x10)
    build_header = i64(entity, 0x18)
    assert u32(entity, build_header + 20) == 0x80809FBD
    assert u64(entity, build_header + 24) == build_count
    assert u32(entity, build_header + 32) == 0x80809C04
    build_data = build_header + 40
    builds = [struct.unpack_from("<III", entity, build_data + i * 12)
              for i in range(build_count)]
    assert builds[GENERIC_BUILD_ORDINAL] == (GENERIC_DEFINITION, 0, 0)

    metadata_count, metadata_data = standard_array(
        entity, 0x68, 0x70, 24, 0x80809C20
    )
    matching = []
    for i in range(metadata_count):
        row = struct.unpack_from("<6I", entity, metadata_data + i * 24)
        if row[0] == GENERIC_DEFINITION and row[4] == GENERIC_BUILD_ORDINAL:
            matching.append(row)
    assert matching
    assert all(row == (GENERIC_DEFINITION, GENERIC_DEFINITION_SUBTYPE,
                       GENERIC_DECLARED_SIZE, 0, GENERIC_BUILD_ORDINAL, 0)
               for row in matching)

    definition_class, definition = eater.packages.read(GENERIC_DEFINITION)
    assert definition_class == 0x80809C36 and len(definition) == 0x1280
    # The package's generic component block and its runtime implementation marker.
    assert u32(definition, GENERIC_RUNTIME_OFFSET - 4) == GENERIC_RUNTIME_KIND
    assert u32(definition, GENERIC_RUNTIME_OFFSET) == GENERIC_DEFINITION
    assert u32(definition, GENERIC_RUNTIME_OFFSET + 4) == GENERIC_DEFINITION_SUBTYPE

    return {
        "entity": f"0x{PLATFORM_ENTITY:08X}",
        "entityClass": "0x80809C0F",
        "entitySha256": hashlib.sha256(entity).hexdigest(),
        "definition": f"0x{GENERIC_DEFINITION:08X}",
        "definitionClass": "0x80809C36",
        "definitionSha256": hashlib.sha256(definition).hexdigest(),
        "buildOrdinal": GENERIC_BUILD_ORDINAL,
        "definitionSubtype": f"0x{GENERIC_DEFINITION_SUBTYPE:08X}",
        "declaredRuntimeBytes": GENERIC_DECLARED_SIZE,
        "runtimeKind": f"0x{GENERIC_RUNTIME_KIND:08X}",
        "runtimeOffset": GENERIC_RUNTIME_OFFSET,
        "dynamicAuthSchema": f"0x{GENERIC_AUTH_SCHEMA:08X}",
    }


def recover_platforms() -> list[dict[str, object]]:
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    group = next(g for g in inventory["groups"]
                 if number(g["registryKey"]) == REACTOR_REGISTRY)
    platforms: dict[tuple[int, int], dict[str, object]] = {}
    collections: dict[tuple[int, int], dict[str, object]] = {}
    for descriptor in group["descriptors"]:
        typ = descriptor["type"]
        if typ not in (4, 34):
            continue
        tag = number(descriptor["sourceTag"])
        offset = number(descriptor["sourceOffset"])
        source_class, blob = eater.packages.read(tag)
        assert source_class == 0x80809C36
        assert struct.unpack_from("<IHH", blob, offset + 0x30) == (
            REACTOR_REGISTRY, typ, descriptor["index"]
        )
        if typ == 4:
            try:
                path, index, name = package_name(blob, PLATFORM_PATTERN)
            except AssertionError:
                continue
            assert descriptor["componentClass"] == f"0x{TYPE4_COMPONENT:08X}"
            assert descriptor["senseSchema"] == f"0x{TYPE4_SENSE:08X}"
            assert descriptor["authSchema"] == f"0x{TYPE4_AUTH:08X}"
            assert u32(blob, offset + 0xB8) == PLATFORM_ENTITY
            position = struct.unpack_from("<3f", blob, offset + 0xD8)
            platforms[path, index] = {
                "path": path,
                "index": index,
                "name": name,
                "sourceTag": f"0x{tag:08X}",
                "sourceSlot": descriptor["index"],
                "sourceOffset": offset,
                "position": list(position),
            }
        else:
            try:
                path, index, name = package_name(blob, COLLECTION_PATTERN)
            except AssertionError:
                continue
            assert descriptor["componentClass"] == f"0x{TYPE34_COMPONENT:08X}"
            assert descriptor["senseSchema"] == "0xFFFFFFFF"
            assert descriptor["authSchema"] == f"0x{TYPE34_AUTH:08X}"
            collections[path, index] = {
                "name": name,
                "sourceTag": f"0x{tag:08X}",
                "slot": descriptor["index"],
                "sourceOffset": offset,
            }

    expected = {(path, index) for path, count in enumerate(PATH_COUNTS)
                for index in range(count)}
    assert set(platforms) == expected and set(collections) == expected
    rows = []
    for key in sorted(expected):
        row = platforms[key]
        row["playerCollection"] = collections[key]
        rows.append(row)
    # Package order swaps the two source slots; the logical name join must preserve it.
    assert rows[16]["sourceSlot"] == 55 and rows[16]["playerCollection"]["slot"] == 131
    assert rows[17]["sourceSlot"] == 54 and rows[17]["playerCollection"]["slot"] == 132
    return rows


def recover_behaviors() -> list[dict[str, object]]:
    rows = [
        (0x80F42FAA, 0x81558181, 0x05F911E1, "increment"),
        (0x80F42FAC, 0x81558182, 0xD5ACFF36, "set_one"),
        (0x80F42FAE, 0x81558183, 0x5650B4C4, "set_one"),
    ]
    result = []
    for root, owner, target, operation in rows:
        root_class, root_blob = eater.packages.read(root)
        owner_class, owner_blob = eater.packages.read(owner)
        assert root_class == 0x8080941E and owner_class == 0x80809C36
        assert struct.pack("<I", root) in owner_blob
        assert struct.pack("<I", 0x6D408B83) in root_blob
        result.append({
            "root": f"0x{root:08X}",
            "ownerConfig": f"0x{owner:08X}",
            "rootSha256": hashlib.sha256(root_blob).hexdigest(),
            "condition": "device_position > 0",
            "channelHash": "0x6D408B83",
            "comparisonMode": 5,
            "targetHash": f"0x{target:08X}",
            "operation": operation,
        })
    return result


def float_literal(value: float) -> str:
    text = format(value, ".9g")
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return text + "F"


def render_header(platforms: list[dict[str, object]]) -> str:
    lines = [
        "// Generated by tools/coo/verify_eater_reactor_platform_bindings.py. Do not edit by hand.",
        "#pragma once",
        "#include \"catalog.h\"",
        "#include \"../../../middleware/bap/activity_message/native/generic_device_authority.h\"",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "namespace dawn::state::activity::eater_of_worlds {",
        "struct GenericDeviceBinding {",
        "    std::uint32_t entity,definition;std::uint8_t buildOrdinal;",
        "    std::uint32_t definitionSubtype,declaredBytes,runtimeKind,runtimeOffset,dynamicSchema;",
        "};",
        f"inline constexpr GenericDeviceBinding kReactorPlatformDevice{{0x{PLATFORM_ENTITY:08X}U,0x{GENERIC_DEFINITION:08X}U,{GENERIC_BUILD_ORDINAL},0x{GENERIC_DEFINITION_SUBTYPE:08X}U,0x{GENERIC_DECLARED_SIZE:X}U,0x{GENERIC_RUNTIME_KIND:08X}U,0x{GENERIC_RUNTIME_OFFSET:X}U,0x{GENERIC_AUTH_SCHEMA:08X}U}};",
        "struct ReactorPlatformBinding {",
        "    coo::Asset source,playerCollection;Point position;std::uint8_t path,index;",
        "};",
        "// playerCollection is an exact package name/ordinal join only. Type34 has no Sense",
        "// schema here; no occupancy producer or retain-player meaning is inferred from it.",
        f"inline constexpr std::array<ReactorPlatformBinding,{len(platforms)}> kReactorPlatforms{{{{",
    ]
    for row in platforms:
        collection = row["playerCollection"]
        position = row["position"]
        lines.append(
            "    {{0x%08XU,0x%sU,4,%d},{0x%08XU,0x%sU,34,%d},{%s,%s,%s},%d,%d},"
            % (REACTOR_REGISTRY, str(row["sourceTag"])[2:], row["sourceSlot"],
               REACTOR_REGISTRY, str(collection["sourceTag"])[2:], collection["slot"],
               float_literal(position[0]), float_literal(position[1]), float_literal(position[2]),
               row["path"], row["index"])
        )
    lines += [
        "}};",
        "inline constexpr std::array<std::uint8_t,4> kReactorPathLengths{13,11,13,19};",
        "[[nodiscard]] constexpr const ReactorPlatformBinding* reactor_platform(std::uint8_t path,std::uint8_t index) noexcept {",
        "    for(const auto& value:kReactorPlatforms) if(value.path==path && value.index==index) return &value;",
        "    return nullptr;",
        "}",
        "// All three exact object behaviors gate on this object-local channel being positive.",
        "inline constexpr std::uint32_t kDevicePositionChannel=0x6D408B83U;",
        "inline constexpr float kPlatformBehaviorActivationExclusive=0.0F;",
        "static_assert(kReactorPlatforms.size()==56 && kReactorPlatforms[16].source.slot==55",
        "    && kReactorPlatforms[17].source.slot==54);",
        "static_assert(middleware::bap::activity_message::native::generic_device::kSchema",
        "    ==kReactorPlatformDevice.dynamicSchema);",
        "} // namespace dawn::state::activity::eater_of_worlds",
        "",
    ]
    return "\n".join(lines)


def recover() -> tuple[str, str]:
    image = IMAGE.read_bytes()
    assert hashlib.sha256(image).hexdigest() == EXPECTED_IMAGE_SHA256
    # Exact registered generic-device dynamic-record consumer and producer.
    assert struct.unpack_from("<Q", image, 0x1D07180)[0] == 0x7FF618E66510
    assert struct.unpack_from("<Q", image, 0x1D07190)[0] == 0x7FF618E64020
    assert image[0xDF6510:0xDF651A] == bytes.fromhex("40534883ec600fb7412c")
    assert image[0xDF4020:0xDF402A] == bytes.fromhex("40534883ec504533c94d")

    generic = recover_generic_join()
    platforms = recover_platforms()
    behaviors = recover_behaviors()
    evidence = {
        "schema": "eater-reactor-platform-bindings-v1",
        "input": {
            "nativeInventorySha256": hashlib.sha256(INVENTORY.read_bytes()).hexdigest(),
            "clientImageSha256": EXPECTED_IMAGE_SHA256,
        },
        "registry": f"0x{REACTOR_REGISTRY:08X}",
        "genericDevice": generic,
        "platformCount": len(platforms),
        "pathLengths": list(PATH_COUNTS),
        "platforms": platforms,
        "positiveBehaviorGates": behaviors,
        "provedOutput": (
            "A fresh type-4 optional 0x80805063 position record reaches the spawned "
            "platform entity's exact 0x8080390E generic-device component. Any positive "
            "device_position satisfies all three recovered authored behavior predicates."
        ),
        "openBoundaries": [
            "The intended final motion endpoint is not recovered; a positive predicate does not name a presentation mode.",
            "Type-34 of_all_players entries are authority-only and have no proved native occupancy producer.",
            "Publishing or retaining a player in type-34 is not justified by this evidence.",
        ],
    }
    return render_header(platforms), json.dumps(evidence, indent=2) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    header, evidence = recover()
    if args.check:
        assert HEADER.read_text(encoding="utf-8") == header, str(HEADER)
        assert EVIDENCE.read_text(encoding="utf-8") == evidence, str(EVIDENCE)
    else:
        HEADER.write_text(header, encoding="utf-8", newline="\n")
        EVIDENCE.write_text(evidence, encoding="utf-8", newline="\n")
    print("Verified 56 Eater reactor platforms and the exact generic-device output join")


if __name__ == "__main__":
    main()
