"""Verify Eater's authored platform volume and captured native membership edges.

This reads installed package data, the pinned flat client image, and a compact
ReadProcessMemory capture excerpt. It never invokes game code or writes process
memory. The result proves a solo occupancy input; it does not claim recovery of
the original type-34 all-player collection producer.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

import verify_eater_reactor_platform_bindings as bindings


ROOT = Path(__file__).resolve().parents[2]
INPUT = ROOT / "docs/raids/eater-of-worlds/evidence/platform-volume-live-input.json"
EVIDENCE = ROOT / "docs/raids/eater-of-worlds/evidence/platform-volume-native-proof.json"
ENTITY = 0x80F42FCD
CONFIG = 0x80F42FB5
KIND = 0x8080929E
OFFSET = 0x1B0
SUBTYPE = 0x808099CF
INTERFACE = 0x808092A4
BUILD_ORDINAL = 18
CYLINDER_VTABLE_RVA = 0x1BA5EE8


def rtti_name(image: bytes, vtable: int) -> str:
    base = 0x7FF618070000  # relocation base recorded in the pinned flat image
    locator = struct.unpack_from("<Q", image, vtable - 8)[0] - base
    signature, _, _, type_descriptor, _, self_rva = struct.unpack_from("<6I", image, locator)
    assert signature == 1 and self_rva == locator
    return image[type_descriptor + 16:type_descriptor + 128].split(b"\0", 1)[0].decode("ascii")


def selected(samples: list[dict], elapsed: float) -> dict:
    matches = [sample for sample in samples if math.isclose(sample["elapsed"], elapsed,
                                                              rel_tol=0, abs_tol=1e-9)]
    assert len(matches) == 1
    return matches[0]


def volume_contact(sample: dict, body: str) -> dict | None:
    matches = [row for row in sample["contacts"] if row["body"].lower() == body.lower()]
    assert len(matches) <= 1
    return matches[0] if matches else None


def recover() -> str:
    image = bindings.IMAGE.read_bytes()
    assert hashlib.sha256(image).hexdigest() == bindings.EXPECTED_IMAGE_SHA256
    entity_class, entity = bindings.eater.packages.read(ENTITY)
    assert entity_class == 0x80809C0F
    header = bindings.i64(entity, 0x18)
    assert struct.unpack_from("<III", entity, header + 40 + BUILD_ORDINAL * 12) == (CONFIG, 0, 0)
    count, rows_offset = bindings.standard_array(entity, 0x68, 0x70, 24, 0x80809C20)
    rows = [struct.unpack_from("<6I", entity, rows_offset + index * 24)
            for index in range(count)]
    exact = [row for row in rows if row[0] == CONFIG and row[4] == BUILD_ORDINAL]
    assert exact and all(row in ((CONFIG, SUBTYPE, 0xA0, 0, BUILD_ORDINAL, 0),
                                 (CONFIG, INTERFACE, 0xD0, 0, BUILD_ORDINAL, 0))
                         for row in exact)
    assert (CONFIG, SUBTYPE, 0xA0, 0, BUILD_ORDINAL, 0) in exact
    assert (CONFIG, INTERFACE, 0xD0, 0, BUILD_ORDINAL, 0) in exact

    config_class, config = bindings.eater.packages.read(CONFIG)
    assert config_class == 0x80809C36 and len(config) == 0x3E0
    assert struct.unpack_from("<IIQ", config, 0xA0) == (CONFIG, KIND, OFFSET)
    assert struct.unpack_from("<III", config, OFFSET) == (CONFIG, SUBTYPE, 0xA0)
    assert struct.unpack_from("<III", config, 0x200) == (CONFIG, INTERFACE, 0xD0)
    assert rtti_name(image, CYLINDER_VTABLE_RVA) == ".?AVhkpCylinderShape@@"
    # Original 11DE80 looks up bodyB's collidable (+20) in bodyA's +90 array,
    # then returns the contact manager through row.first + 8.
    collision_lookup = image[0x11DE80:0x11DEBD]
    assert hashlib.sha256(collision_lookup).hexdigest() == \
        "3d31eeba114f967d985db5216950ea1bacaece6632c951a0567273c2816322b7"

    live = json.loads(INPUT.read_text(encoding="utf-8"))
    assert live["schema"] == "eater-platform-volume-live-input-v1"
    assert live["platform"] == {
        "entity": "0x69FAA16A", "definition": "0x80F42FCD",
        "volumeConfig": "0x80F42FB5", "volumeKind": "0x8080929E",
        "volumeOffset": "0x1B0", "volumeBodyField": "0x88",
        "volumeBody": "0x1A3A46AE9B0", "shapeVtableRva": "0x1BA5EE8",
    }
    samples = live["selected"]
    body = live["platform"]["volumeBody"]
    centre = volume_contact(selected(samples, 0.00027810002211481333), body)
    raised = volume_contact(selected(samples, 2.9400314000085928), body)
    edge = volume_contact(selected(samples, 4.510853399988264), body)
    leaving = volume_contact(selected(samples, 24.919788200000767), body)
    outside = volume_contact(selected(samples, 24.970492199994624), body)
    returned = volume_contact(selected(samples, 29.971534200012684), body)
    final_edge = volume_contact(selected(samples, 53.46145060000708), body)
    assert centre and centre["count"] == 1 and centre["points"][0][6] == -1.0
    assert raised and raised["count"] == 1 and raised["points"][0][6] == 1.0
    assert edge and edge["count"] == 1 and edge["points"][0][6] == 0.0
    assert leaving and leaving["count"] == 0 and not leaving["points"]
    assert outside is None
    assert returned and returned["count"] == 1 and returned["points"][0][6] == 0.0
    assert final_edge and final_edge["count"] == 1 and final_edge["points"][0][6] == 0.0

    result = {
        "schema": "eater-platform-volume-native-proof-v1",
        "clientImageSha256": bindings.EXPECTED_IMAGE_SHA256,
        "platformVolume": {
            "entity": f"{ENTITY:08X}", "buildOrdinal": BUILD_ORDINAL,
            "config": f"{CONFIG:08X}", "runtimeKind": f"{KIND:08X}",
            "runtimeOffset": OFFSET, "bodyField": 0x88,
            "shapeRtti": "hkpCylinderShape", "shapeVtableRva": f"{CYLINDER_VTABLE_RVA:X}",
            "interface": f"{INTERFACE:08X}",
            "configSha256": hashlib.sha256(config).hexdigest(),
        },
        "nativeMembershipJoin": {
            "collisionLookupRva": "11DE80", "playerCollisionArrayOffset": "90",
            "partnerCollidableOffset": "20", "managerLinkOffset": "8",
            "contactManagerAndPointAbi": "platform-contact-native-proof.json",
        },
        "liveCapture": {
            "sourceCapture": live["sourceCapture"], "sourceSha256": live["sourceSha256"],
            "sourceSamples": live["sourceSamples"], "selectedInputSha256": hashlib.sha256(
                INPUT.read_bytes()).hexdigest(),
            "observations": [
                "centre membership has a negative-Z normal",
                "edge membership has a horizontal normal",
                "membership is empty after leaving the cylinder during the jump",
                "membership returns on re-entry to the cylinder",
            ],
        },
        "provedPolicy": "stable exact authored platform-volume membership plus solo dwell",
        "notProved": ["walkable ground support", "original type-34 all-player collection semantics"],
    }
    return json.dumps(result, indent=2) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    evidence = recover()
    if args.check:
        assert EVIDENCE.read_text(encoding="utf-8") == evidence
    else:
        EVIDENCE.write_text(evidence, encoding="utf-8", newline="\n")
    print("Verified exact Eater platform cylinder and captured native volume membership")


if __name__ == "__main__":
    main()
