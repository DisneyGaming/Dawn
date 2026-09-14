"""Capture a read-only Eater reactor-platform standing sample.

This tool never invokes game code and never writes process memory.  It snapshots the
authenticated Sunrise local-player cache, the complete physics-component arrays, the
Havok rigid body's collision entries, and bounded memory around every collision partner.
The optional position gate is only a capture aid; it is not an occupancy predicate.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
from pathlib import Path
import struct
import time


PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

EXPECTED_DLL_SHA256 = "7e430774ee2685cb63b85a0b3086d0a23b70c03804adb1edde65457c87a024f2"
SEQUENCE_RVA = 0x7CC84A8
COMPONENT_RVA = 0x7CC84B0
POSITION_RVA = 0x7CC84B8
PRESENT_RVA = 0x73471A9
# Each layout is pinned to its installed DLL and was resolved from its matching
# PDB. Keeping the previous layout permits reproducible reads of the prior build.
PLAYER_CACHE_LAYOUTS = {
    EXPECTED_DLL_SHA256: (SEQUENCE_RVA, COMPONENT_RVA, POSITION_RVA, PRESENT_RVA),
    "ceb6fc096d1bd59e13f276e7155bce5774906ff0ac140d8e55e92aa010c303bb":
        (0x7CD02D0, 0x7CD02D8, 0x7CD02E0, 0x734EFC9),
}
ENTITY_TABLE_RVA = 0x1F93428
ENTITY_STRIDE_RVA = 0x1F93430
HANDLE_DIRECTORY_RVA = 0x2439C70
RIGID_BODY_VTABLE_RVA = 0x1BAC0D8
CONTACT_MANAGER_VTABLE_RVA = 0x1BAC4C0


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD), ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD), ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD), ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD), ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256), ("szExePath", wintypes.WCHAR * 260),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.ReadProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPCVOID,
                                      wintypes.LPVOID, ctypes.c_size_t,
                                      ctypes.POINTER(ctypes.c_size_t)]
kernel32.ReadProcessMemory.restype = wintypes.BOOL
kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32FirstW.restype = wintypes.BOOL
kernel32.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32NextW.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]


def modules(pid: int) -> dict[str, tuple[int, int, Path]]:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), "CreateToolhelp32Snapshot")
    result: dict[str, tuple[int, int, Path]] = {}
    try:
        row = MODULEENTRY32W(); row.dwSize = ctypes.sizeof(row)
        ok = kernel32.Module32FirstW(snapshot, ctypes.byref(row))
        while ok:
            result[row.szModule.lower()] = (
                ctypes.cast(row.modBaseAddr, ctypes.c_void_p).value or 0,
                row.modBaseSize, Path(row.szExePath),
            )
            ok = kernel32.Module32NextW(snapshot, ctypes.byref(row))
    finally:
        kernel32.CloseHandle(snapshot)
    return result


class Reader:
    def __init__(self, pid: int):
        self.handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), "OpenProcess")

    def close(self) -> None:
        if self.handle:
            kernel32.CloseHandle(self.handle); self.handle = None

    def read(self, address: int, size: int) -> bytes | None:
        if address < 0x10000 or size <= 0 or size > 0x800000 or address + size > 0x800000000000:
            return None
        data = ctypes.create_string_buffer(size); copied = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(self.handle, ctypes.c_void_p(address), data,
                                          size, ctypes.byref(copied)) or copied.value != size:
            return None
        return data.raw

    def unpack(self, fmt: str, address: int):
        data = self.read(address, struct.calcsize(fmt))
        return None if data is None else struct.unpack(fmt, data)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save_blob(directory: Path, name: str, address: int, data: bytes,
              inventory: list[dict[str, object]], reason: str) -> None:
    path = directory / name
    path.write_bytes(data)
    inventory.append({"file": name, "address": f"0x{address:X}", "bytes": len(data),
                      "sha256": hashlib.sha256(data).hexdigest(), "reason": reason})


def stable_player(reader: Reader, dll: int, layout=None) -> tuple[int, tuple[float, float, float]] | None:
    sequence_rva, component_rva, position_rva, present_rva = layout or (
        SEQUENCE_RVA, COMPONENT_RVA, POSITION_RVA, PRESENT_RVA)
    first = reader.unpack("<I", dll + sequence_rva)
    component = reader.unpack("<Q", dll + component_rva)
    position = reader.unpack("<3f", dll + position_rva)
    present = reader.unpack("<B", dll + present_rva)
    second = reader.unpack("<I", dll + sequence_rva)
    if not first or not component or not position or not present or not second:
        return None
    if (first[0] != second[0] or first[0] & 1 or not present[0] or not component[0]
            or not all(math.isfinite(value) for value in position)):
        return None
    return component[0], position


def pointer_pages(reader: Reader, seeds: list[tuple[int, str]], directory: Path,
                  inventory: list[dict[str, object]]) -> list[dict[str, object]]:
    """Save at most 192 unique readable pages, following one bounded pointer layer."""
    queue = list(seeds); seen: set[int] = set(); rows: list[dict[str, object]] = []
    while queue and len(seen) < 192:
        pointer, reason = queue.pop(0)
        if pointer < 0x10000 or pointer >= 0x800000000000:
            continue
        page = pointer & ~0xFFF
        if page in seen:
            continue
        data = reader.read(page, 0x1000)
        if data is None:
            continue
        seen.add(page)
        save_blob(directory, f"page-{page:012X}.bin", page, data, inventory, reason)
        pointers = []
        for offset in range(0, len(data), 8):
            value = struct.unpack_from("<Q", data, offset)[0]
            if 0x10000 <= value < 0x800000000000:
                pointers.append(value)
        rows.append({"page": f"0x{page:X}", "reason": reason,
                     "pointerCount": len(pointers)})
        # Only collision-root pages expand. This retains shapes, agents, owners and
        # property arrays without walking an unbounded game heap.
        if reason.startswith("collision"):
            queue.extend((value, f"child of {reason}") for value in pointers[:32])
    return rows


def capture(args: argparse.Namespace) -> Path:
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError("capture output must be empty; existing evidence is never overwritten")
    found = modules(args.pid)
    if "destiny2.exe" not in found or "steam_api64.dll" not in found:
        raise RuntimeError("Destiny and the installed Sunrise module must both be loaded")
    image, image_size, _ = found["destiny2.exe"]
    dll, dll_size, dll_path = found["steam_api64.dll"]
    installed_sha = sha256(dll_path)
    layout = PLAYER_CACHE_LAYOUTS.get(installed_sha)
    if layout is None:
        raise RuntimeError(f"unsupported steam_api64.dll SHA-256 {installed_sha}")

    reader = Reader(args.pid)
    try:
        deadline = time.monotonic() + args.wait_seconds
        selected = None
        # A zero wait still requests one immediate sample. Comparing the clock
        # before that attempt otherwise expires even before the first read.
        while True:
            candidate = stable_player(reader, dll, layout)
            if candidate:
                _, position = candidate
                if args.near is None or all(abs(position[i] - args.near[i]) <= args.capture_tolerance
                                            for i in range(3)):
                    selected = candidate; break
            if time.monotonic() >= deadline:
                break
            time.sleep(0.05)
        if selected is None:
            raise RuntimeError("no stable local-player sample matched the capture gate")
        component, position = selected
        component_bytes = reader.read(component, 0x800)
        if component_bytes is None:
            raise RuntimeError("local physics component retired during capture")
        owner = struct.unpack_from("<I", component_bytes, 0x2C)[0]
        body_array = struct.unpack_from("<Q", component_bytes, 0x190)[0]
        body_count = struct.unpack_from("<i", component_bytes, 0x198)[0]
        body_index = struct.unpack_from("<i", component_bytes, 0x204)[0]
        if owner == 0xFFFFFFFF or body_count <= 0 or body_count > 64 or not 0 <= body_index < body_count:
            raise RuntimeError("invalid authenticated physics body array")
        body_rows = reader.read(body_array, body_count * 0x50)
        if body_rows is None:
            raise RuntimeError("physics body array retired during capture")
        body = struct.unpack_from("<Q", body_rows, body_index * 0x50 + 0x20)[0]
        body_bytes = reader.read(body, 0x800)
        if body_bytes is None:
            raise RuntimeError("player rigid body retired during capture")
        body_vtable = struct.unpack_from("<Q", body_bytes)[0]
        if body_vtable != image + RIGID_BODY_VTABLE_RVA:
            raise RuntimeError(f"unexpected rigid-body vtable 0x{body_vtable:X}")

        collision = struct.unpack_from("<Q", body_bytes, 0x90)[0]
        collision_count = struct.unpack_from("<i", body_bytes, 0x98)[0]
        collision_capacity = struct.unpack_from("<I", body_bytes, 0x9C)[0] & 0x3FFFFFFF
        if collision_count < 0 or collision_count > 64 or collision_capacity < collision_count:
            raise RuntimeError("invalid Havok collision-entry array")
        collision_bytes = reader.read(collision, collision_count * 16) if collision_count else b""
        if collision_bytes is None:
            raise RuntimeError("collision-entry array retired during capture")

        output.mkdir(parents=True, exist_ok=True)
        inventory: list[dict[str, object]] = []
        save_blob(output, "player-component.bin", component, component_bytes, inventory,
                  "authenticated Sunrise player component")
        save_blob(output, "physics-body-array.bin", body_array, body_rows, inventory,
                  "component +190 body array")
        save_blob(output, "player-rigid-body.bin", body, body_bytes, inventory,
                  "selected hkpRigidBody")
        if collision_bytes:
            save_blob(output, "collision-entries.bin", collision, collision_bytes, inventory,
                      "hkpRigidBody +90 collision entries")

        component_list = struct.unpack_from("<Q", component_bytes, 0x1D0)[0]
        component_list_count = struct.unpack_from("<i", component_bytes, 0x1D8)[0]
        if 0 < component_list_count <= 256:
            # Save a deliberately generous 0x80 bytes per entry. Native 45A540 is
            # still required to interpret it; this capture does not assign semantics.
            component_list_bytes = reader.read(component_list, component_list_count * 0x80)
            if component_list_bytes:
                save_blob(output, "physics-component-list.bin", component_list,
                          component_list_bytes, inventory, "component +1D0 auxiliary body list")

        seeds: list[tuple[int, str]] = []
        collision_rows = []
        contact_records = []
        for index in range(collision_count):
            first, second = struct.unpack_from("<QQ", collision_bytes, index * 16)
            collision_rows.append({"index": index, "first": f"0x{first:X}",
                                   "second": f"0x{second:X}"})
            # Native 11DE80: a collision row's first pointer owns the manager
            # pointer at +8; second is the partner world object's collidable +20.
            manager_pointer = reader.unpack("<Q", first + 8)
            if manager_pointer:
                manager = manager_pointer[0]
                manager_bytes = reader.read(manager, 0xB0)
                if (manager_bytes is not None
                        and struct.unpack_from("<Q", manager_bytes)[0]
                        == image + CONTACT_MANAGER_VTABLE_RVA):
                    save_blob(output, f"contact-manager-{index}.bin", manager,
                              manager_bytes, inventory, "native simple-constraint contact manager")
                    atom = struct.unpack_from("<Q", manager_bytes, 0x68)[0]
                    pair = struct.unpack_from("<QQ", manager_bytes, 0xA0)
                    atom_header = reader.read(atom, 0x30)
                    record = {"collisionIndex": index, "manager": f"0x{manager:X}",
                              "bodies": [f"0x{value:X}" for value in pair],
                              "partner": f"0x{second - 0x20:X}" if second >= 0x20 else None,
                              "atom": f"0x{atom:X}"}
                    if atom_header:
                        count = struct.unpack_from("<H", atom_header, 4)[0]
                        record["contactCount"] = count
                        if count <= 256:
                            atom_bytes = reader.read(atom, 0x30 + count * 0x20)
                            if atom_bytes:
                                save_blob(output, f"contact-atom-{index}.bin", atom,
                                          atom_bytes, inventory, "native contact positions and normals")
                                record["contacts"] = [list(struct.unpack_from("<8f", atom_bytes,
                                                          0x30 + contact * 0x20))
                                                      for contact in range(count)]
                    final_manager = reader.read(manager, 0xB0)
                    record["managerStable"] = final_manager == manager_bytes
                    contact_records.append(record)
            for label, value in (("first", first), ("second", second)):
                seeds.append((value, f"collision[{index}].{label}"))
                # hkpLinkedCollidable partners commonly point inside their owner.
                # Save candidate owner neighborhoods; validation remains offline.
                for delta in (0x10, 0x20, 0x30, 0x40):
                    if value > delta:
                        seeds.append((value - delta,
                                      f"collision[{index}].{label}-0x{delta:X}"))
        pages = pointer_pages(reader, seeds, output, inventory)

        entity_table = reader.unpack("<Q", image + ENTITY_TABLE_RVA)
        entity_stride = reader.unpack("<I", image + ENTITY_STRIDE_RVA)
        entity_snapshot = None
        if entity_table and entity_stride and 0x50 <= entity_stride[0] <= 0x1000:
            entity_snapshot = reader.read(entity_table[0], entity_stride[0] * 8192)
            if entity_snapshot:
                save_blob(output, "entity-table.bin", entity_table[0], entity_snapshot,
                          inventory, "complete 8192-row world entity table")

        directory_pointer = reader.unpack("<Q", image + HANDLE_DIRECTORY_RVA)
        handle_directory = None
        registry = None
        if directory_pointer:
            handle_directory = reader.read(directory_pointer[0], 0x40)
            if handle_directory:
                save_blob(output, "handle-directory.bin", directory_pointer[0], handle_directory,
                          inventory, "native object-handle directory")
                registry_address = struct.unpack_from("<Q", handle_directory)[0]
                registry = reader.read(registry_address, 0x40 * 65536)
                if registry:
                    save_blob(output, "handle-registry.bin", registry_address, registry,
                              inventory, "native handle bucket registry")

        # The initial cache seqlock does not cover the physics arrays. Preserve
        # all bytes, but separately qualify the exact owner/body/array identities
        # again after the snapshot. No asynchronous capture is called atomic.
        final_player = stable_player(reader, dll, layout)
        final_component = reader.read(component, 0x210)
        final_body = reader.read(body, 0xA0)
        final_body_rows = reader.read(body_array, body_count * 0x50)
        final_collision_rows = reader.read(collision, collision_count * 16) if collision_count else b""
        stable_fields = ((0, 0x30), (0x190, 0x1A0), (0x204, 0x20C))
        stability = {
            "sameLocalComponent": bool(final_player and final_player[0] == component),
            "sameComponentIdentityAndBodySelection": bool(final_component and all(
                final_component[start:end] == component_bytes[start:end]
                for start, end in stable_fields)),
            "sameSelectedBodyPointers": bool(final_body_rows and all(
                final_body_rows[i * 0x50 + 0x20:i * 0x50 + 0x28]
                == body_rows[i * 0x50 + 0x20:i * 0x50 + 0x28] for i in range(body_count))),
            "sameRigidBodyAndCollisionArray": bool(final_body
                and final_body[:8] == body_bytes[:8]
                and final_body[0x90:0xA0] == body_bytes[0x90:0xA0]),
            "sameCollisionRows": final_collision_rows == collision_bytes,
        }

        manifest = {
            "schema": "eater-platform-live-capture-v2",
            "captureSemantics": "read-only evidence; near/tolerance is not gameplay occupancy",
            "process": {"pid": args.pid, "imageBase": f"0x{image:X}",
                        "imageSize": image_size, "steamApiBase": f"0x{dll:X}",
                        "steamApiSize": dll_size, "steamApiSha256": installed_sha},
            "playerCacheLayoutRvas": dict(zip(
                ("sequence", "component", "position", "present"),
                (f"0x{value:X}" for value in layout))),
            "player": {"component": f"0x{component:X}", "ownerHandle": f"0x{owner:08X}",
                       "position": list(position), "bodyArray": f"0x{body_array:X}",
                       "bodyCount": body_count, "bodyIndex": body_index,
                       "body": f"0x{body:X}", "bodyVtable": f"0x{body_vtable:X}"},
            "havokCollisionEntries": {"address": f"0x{collision:X}",
                                       "count": collision_count,
                                       "capacity": collision_capacity,
                                       "stride": 16, "rows": collision_rows},
            "componentAuxiliaryList": {"address": f"0x{component_list:X}",
                                       "count": component_list_count},
            "nativeContactRecords": contact_records,
            "stability": stability,
            "identityStableAcrossCapture": all(stability.values()),
            "pointerPages": pages,
            "files": inventory,
        }
        path = output / "capture.json"
        path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        return path
    finally:
        reader.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--wait-seconds", type=float, default=55.0)
    parser.add_argument("--near", type=float, nargs=3)
    parser.add_argument("--capture-tolerance", type=float, default=2.5)
    parser.add_argument("--samples", type=int, default=2,
                        help="independent full snapshots one second apart (1 or 2)")
    args = parser.parse_args()
    if not 0 <= args.wait_seconds <= 55 or args.capture_tolerance <= 0:
        parser.error("wait must be 0..55 seconds and capture tolerance must be positive")
    if args.samples not in (1, 2):
        parser.error("samples must be 1 or 2")
    root = args.output
    for index in range(args.samples):
        args.output = root if args.samples == 1 else root / f"sample-{index + 1}"
        if index:
            time.sleep(1)
            args.wait_seconds = 0
        print(capture(args), flush=True)


if __name__ == "__main__":
    main()
