"""Verify the platform physics join and execute original contact-normal code offline.

Unicorn executes only the pinned sphere/sphere contact calculation, stopping before
the contact-manager call. This does not attach to or change a running game and does
not claim that the live player/platform pair has been observed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import (UC_X86_REG_RBX, UC_X86_REG_RBP, UC_X86_REG_RSP,
                              UC_X86_REG_R12, UC_X86_REG_R15, UC_X86_REG_RDX,
                              UC_X86_REG_R8, UC_X86_REG_RIP)
import verify_eater_reactor_platform_bindings as bindings

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "docs/raids/eater-of-worlds/evidence/platform-contact-native-proof.json"


def original_normal(image: bytes, a_z: float, b_z: float,
                    tolerance: float = 0.1) -> dict[str, object]:
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(0, (len(image) + 4095) & ~4095)
    uc.mem_write(0, image)
    stack, data = 0x10000000, 0x20000000
    uc.mem_map(stack, 0x10000)
    uc.mem_map(data, 0x10000)
    a, b = data, data + 0x100
    transform_a, transform_b = data + 0x200, data + 0x300
    shape_a, shape_b = data + 0x400, data + 0x500
    agent, collision_input = data + 0x600, data + 0x700
    output, point, manager, vtable = data + 0x800, data + 0x900, data + 0xA00, data + 0xB00
    def put(address: int, fmt: str, *values: object) -> None:
        uc.mem_write(address, struct.pack(fmt, *values))
    put(a, "<Q", shape_a); put(a + 0x10, "<Q", transform_a)
    put(b, "<Q", shape_b); put(b + 0x10, "<Q", transform_b)
    put(transform_a + 0x30, "<4f", 0, 0, a_z, 0)
    put(transform_b + 0x30, "<4f", 0, 0, b_z, 0)
    put(shape_a + 0x20, "<f", 1); put(shape_b + 0x20, "<f", 1)
    put(agent + 0x18, "<H", 0xFFFF); put(agent + 0x10, "<Q", manager)
    put(collision_input + 0x10, "<f", tolerance)
    put(output, "<Q", point); put(manager, "<Q", vtable)
    put(vtable + 0x18, "<Q", 0x127870)
    rsp = stack + 0x8000
    put(rsp + 0x90, "<Q", output)
    for register, value in ((UC_X86_REG_RSP, rsp), (UC_X86_REG_RBP, a),
                            (UC_X86_REG_R15, b), (UC_X86_REG_R12, collision_input),
                            (UC_X86_REG_RBX, agent)):
        uc.reg_write(register, value)
    # Enter after the TLS profiling prologue; stop before invoking the native
    # manager. Every arithmetic instruction and contact write is original code.
    within_tolerance = abs(a_z - b_z) < 2.0 + tolerance
    stop = 0x18FAFC8 if within_tolerance else 0x18FAFE4
    uc.emu_start(0x18FAE8A, stop, count=300)
    if not within_tolerance:
        assert uc.reg_read(UC_X86_REG_RIP) == 0x18FAFE4
        return {"aZ": a_z, "bZ": b_z, "tolerance": tolerance,
                "contactProduced": False}
    assert uc.reg_read(UC_X86_REG_RIP) == 0x18FAFC8
    assert uc.reg_read(UC_X86_REG_RDX) == a and uc.reg_read(UC_X86_REG_R8) == b
    assert struct.unpack("<Q", uc.mem_read(rsp + 0x30, 8))[0] == point
    values = struct.unpack("<8f", uc.mem_read(point, 32))
    expected = 1.0 if a_z > b_z else -1.0
    assert abs(values[4]) < 1e-6 and abs(values[5]) < 1e-6
    assert math.isclose(values[6], expected, abs_tol=1e-6)
    assert math.isclose(values[2], b_z + expected, abs_tol=1e-6)
    assert math.isclose(values[7], abs(a_z - b_z) - 2.0, abs_tol=1e-6)
    return {"aZ": a_z, "bZ": b_z, "normalZ": round(values[6], 6),
            "positionZ": round(values[2], 6), "distance": round(values[7], 6),
            "managerReceivesSameBodyOrder": True, "tolerance": tolerance,
            "contactProduced": True}


def recover() -> str:
    image = bindings.IMAGE.read_bytes()
    assert hashlib.sha256(image).hexdigest() == bindings.EXPECTED_IMAGE_SHA256
    entity_class, entity = bindings.eater.packages.read(0x80F42FCD)
    assert entity_class == 0x80809C0F
    header = bindings.i64(entity, 0x18)
    assert struct.unpack_from("<III", entity, header + 40 + 2 * 12) == (0x80F42FA5, 0, 0)
    count, offset = bindings.standard_array(entity, 0x68, 0x70, 24, 0x80809C20)
    rows = [struct.unpack_from("<6I", entity, offset + i * 24) for i in range(count)]
    matches = [row for row in rows if row[0] == 0x80F42FA5 and row[4] == 2]
    assert matches and all(row == (0x80F42FA5, 0x808092D8, 0xC0, 0, 2, 0) for row in matches)
    config_class, config = bindings.eater.packages.read(0x80F42FA5)
    assert config_class == 0x80809C36 and len(config) == 2176
    assert struct.unpack_from("<IIQ", config, 0xC0) == (0x80F42FA5, 0x80808A0C, 0x378)
    assert struct.unpack_from("<III", config, 0x378) == (0x80F42FA5, 0x808092D8, 0xC0)
    # Exact original manager copies both contact vectors unchanged to its atom.
    assert image[0x12792C:0x12793A] == bytes.fromhex("0f28030f29010f284b100f294910")
    # World constructor and getCinfo preserve the collision-input tolerance.
    assert image[0x108B3F:0x108B46] == bytes.fromhex("4c89b6b8000000")
    assert image[0x108B54:0x108B5A] == bytes.fromhex("f3410f114610")
    assert image[0x10925C:0x10926B] == bytes.fromhex("488b81b8000000448b401044894250")
    result = {
        "schema": "eater-platform-contact-native-proof-v1",
        "clientImageSha256": bindings.EXPECTED_IMAGE_SHA256,
        "platformPhysics": {"entity": "80F42FCD", "buildOrdinal": 2,
                            "config": "80F42FA5", "runtimeKind": "80808A0C",
                            "runtimeOffset": 0x378,
                            "configSha256": hashlib.sha256(config).hexdigest()},
        "originalContactProducerRva": "18FAE30",
        "emulatedRange": ["18FAE8A", "18FAFC8"],
        "contactManagerRva": "127870",
        "normalConvention": "B to A; manager receives the same ordered bodies and contact",
        "cases": [original_normal(image, 1.0, -1.0), original_normal(image, -1.0, 1.0)],
        "collisionTolerance": {"worldVtableRva": "1BAAC28", "worldInputOffset": "B8",
                               "inputToleranceOffset": "10", "constructorRva": "107D50",
                               "getCinfoRva": "109220"},
        "toleranceCases": [original_normal(image, 1.0625, -1.0, 0.125),
                           original_normal(image, 1.125, -1.0, 0.125),
                           original_normal(image, 1.25, -1.0, 0.125)],
        "livePlayerPlatformContactVerified": False,
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
    print("Verified exact platform physics component and original B-to-A contact normals in both body orders")


if __name__ == "__main__":
    main()
