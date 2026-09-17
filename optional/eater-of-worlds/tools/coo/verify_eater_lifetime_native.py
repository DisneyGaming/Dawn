"""Replay the Eater lifetime navigation lookup that failed during initial load.

The encrypted global-pointer unwrap and game-populated navigation records are modeled.
The pinned image's ordinal packer, unprotected lookup tail, and faulting dereference execute
unchanged in Unicorn.  Preserved crash addresses and event order are checked separately.
No live process is opened or modified.
"""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from unicorn import Uc, UcError, UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_MEM_INVALID, UC_MODE_64
from unicorn.x86_const import (
    UC_X86_REG_EDX,
    UC_X86_REG_RAX,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDX,
    UC_X86_REG_RIP,
    UC_X86_REG_RSP,
    UC_X86_REG_R8,
)

ROOT = Path(__file__).resolve().parents[2]
PIN = "63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e"
BASE = 0x7FF618070000
LIVE_BASE = 0x7FF707B30000
HEAP = 0x10000000
HEAP_SIZE = 0x80000
STACK = HEAP + 0x70000
STOP = HEAP + 0x7F000
TABLE = HEAP + 0x1000
TYPE_ROOT = HEAP + 0x40000
POOLS = HEAP + 0x41000
DATA = HEAP + 0x42000
RECORD = DATA + 0x100
OUT = HEAP + 0x50000

PACKER = 0x4C8E60
LOOKUP = 0x438CA0
LOOKUP_TAIL = 0x438FF1
DEREFERENCE = 0x42B1E0
FAULT = 0x42B1F1
TYPE_TABLE_GLOBAL = 0x2439C70


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def map_image_pages(uc: Uc, image: bytes, *rvas: int) -> None:
    pages = sorted({rva & ~0xFFF for rva in rvas})
    for page in pages:
        uc.mem_map(BASE + page, 0x1000)
        block = image[page : page + 0x1000]
        uc.mem_write(BASE + page, block + bytes(0x1000 - len(block)))


def put(uc: Uc, address: int, fmt: str, *values: int) -> None:
    uc.mem_write(address, struct.pack("<" + fmt, *values))


def get(uc: Uc, address: int, fmt: str):
    return struct.unpack("<" + fmt, uc.mem_read(address, struct.calcsize("<" + fmt)))


def replay_packer(image: bytes, ordinal: int) -> int:
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_image_pages(uc, image, PACKER)
    uc.mem_map(HEAP, HEAP_SIZE)
    put(uc, STACK - 8, "Q", STOP)
    uc.reg_write(UC_X86_REG_RSP, STACK - 8)
    uc.reg_write(UC_X86_REG_RCX, OUT)
    uc.reg_write(UC_X86_REG_EDX, ordinal)
    uc.emu_start(BASE + PACKER, STOP, count=32)
    assert uc.reg_read(UC_X86_REG_RIP) == STOP
    packed = get(uc, OUT, "I")[0]
    assert uc.reg_read(UC_X86_REG_RAX) == OUT
    return packed


def replay_lookup_tail(image: bytes, packed_region: int, present: bool) -> int:
    """Execute 438FF1 onward with the opaque decrypted table base supplied in RBX."""
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_image_pages(uc, image, LOOKUP_TAIL, 0x439000, 0x1A8D4B0, TYPE_TABLE_GLOBAL)
    uc.mem_map(HEAP, HEAP_SIZE)
    entry = TABLE + 0x1E20 + packed_region * 16
    put(uc, entry, "i4xQ", 0 if present else -1, RECORD - DATA if present else 0)
    if present:
        put(uc, BASE + TYPE_TABLE_GLOBAL, "Q", TYPE_ROOT)
        put(uc, TYPE_ROOT, "Q", POOLS)
        put(uc, POOLS + 8, "Q", DATA)
        put(uc, POOLS + 0x30, "Ii", 0x100, 0)
        put(uc, DATA + 8, "Q", 0)
        put(uc, RECORD, "H", 0xBEEF)
    # LOOKUP_TAIL runs after LOOKUP's push/sub. These are its saved RBX, return address,
    # and the original EDX argument copied by the native prologue before that stack change.
    put(uc, STACK + 0x20, "Q", 0)
    put(uc, STACK + 0x28, "Q", STOP)
    put(uc, STACK + 0x38, "I", packed_region)
    uc.reg_write(UC_X86_REG_RSP, STACK)
    uc.reg_write(UC_X86_REG_RBX, TABLE)
    uc.emu_start(BASE + LOOKUP_TAIL, STOP, count=128)
    assert uc.reg_read(UC_X86_REG_RIP) == STOP
    return uc.reg_read(UC_X86_REG_RAX)


def replay_dereference(image: bytes, packed_region: int, lookup_result: int) -> dict:
    """Run original 42B1E0; model only its call to the separately replayed lookup."""
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_image_pages(uc, image, DEREFERENCE, LOOKUP)
    uc.mem_map(HEAP, HEAP_SIZE)
    put(uc, STACK - 8, "Q", STOP)
    if lookup_result:
        put(uc, lookup_result, "H", 0xBEEF)
    invalid = []

    def return_from_lookup() -> None:
        rsp = uc.reg_read(UC_X86_REG_RSP)
        return_address = get(uc, rsp, "Q")[0]
        uc.reg_write(UC_X86_REG_RAX, lookup_result)
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)
        uc.reg_write(UC_X86_REG_RIP, return_address)

    def on_code(_uc, address, _size, _user):
        if address == BASE + LOOKUP:
            assert uc.reg_read(UC_X86_REG_EDX) == packed_region
            return_from_lookup()

    def on_invalid(_uc, access, address, size, value, _user):
        invalid.append(
            {
                "access": access,
                "address": f"0x{address:X}",
                "size": size,
                "value": value,
                "rva": f"0x{uc.reg_read(UC_X86_REG_RIP) - BASE:X}",
            }
        )
        return False

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_INVALID, on_invalid)
    uc.reg_write(UC_X86_REG_RSP, STACK - 8)
    uc.reg_write(UC_X86_REG_RCX, BASE + 0x1F8DD00)
    uc.reg_write(UC_X86_REG_RDX, OUT)
    uc.reg_write(UC_X86_REG_R8, packed_region)
    error = None
    try:
        uc.emu_start(BASE + DEREFERENCE, STOP, count=64)
    except UcError as exc:
        error = str(exc)
    if lookup_result:
        assert error is None and not invalid
        assert uc.reg_read(UC_X86_REG_RIP) == STOP
        assert get(uc, OUT, "H")[0] == 0xBEEF
        assert uc.reg_read(UC_X86_REG_RAX) == OUT
    else:
        assert error is not None and len(invalid) == 1
        assert invalid[0]["address"] == "0x0" and invalid[0]["rva"] == "0x42B1F1"
    return {
        "faulted": bool(invalid),
        "fault": invalid[0] if invalid else None,
        "copiedWord": None if invalid else f"0x{get(uc, OUT, 'H')[0]:04X}",
    }


def instruction_map(image: bytes, start: int, end: int) -> dict[int, tuple[str, str]]:
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    return {item.address: (item.mnemonic, item.op_str) for item in decoder.disasm(image[start:end], start)}


def verify_control_flow(image: bytes) -> None:
    pack = instruction_map(image, PACKER, PACKER + 12)
    assert pack[0x4C8E60] == ("and", "edx, 0x3f")
    assert pack[0x4C8E66] == ("shl", "edx, 3")
    assert pack[0x4C8E69] == ("mov", "dword ptr [rcx], edx")

    lifetime = instruction_map(image, 0x100A6E0, 0x100AD18)
    assert lifetime[0x100A715] == ("call", "0x4ffbb0")
    assert lifetime[0x100AC17] == ("cmp", "dword ptr [rbp - 0x65], 0x3f")
    assert lifetime[0x100AD04] == ("mov", "ecx, dword ptr [rbp - 0x65]")
    assert lifetime[0x100AD0E] == ("call", "0xa262a0")

    navigator = instruction_map(image, 0xA262A0, 0xA26300)
    assert navigator[0xA262CF] == ("call", "0x4c8e60")
    assert navigator[0xA262DC] == ("mov", "r8d, dword ptr [rbx]")
    assert navigator[0xA262E7] == ("call", "0x42b1e0")

    dereference = instruction_map(image, DEREFERENCE, DEREFERENCE + 0x20)
    assert dereference[0x42B1EC] == ("call", "0x438ca0")
    assert dereference[FAULT] == ("movzx", "ecx, word ptr [rax]")

    tail = instruction_map(image, LOOKUP_TAIL, 0x439065)
    assert tail[0x438FF6] == ("call", "0x1a8d4b0")
    assert tail[0x439008] == ("mov", "edx, dword ptr [rbx + r8*8]")
    assert tail[0x43900C] == ("cmp", "edx, -1")
    assert tail[0x43905D] == ("xor", "eax, eax")


def verify_preserved_incident(directory: Path) -> dict:
    log = directory / "dawn.log"
    crash = directory / "crash_folder_20764_20260913_140101" / "crash_info.txt"
    log_text = log.read_text(errors="replace")
    crash_text = crash.read_text(errors="replace")
    assert "t=104437" in log_text and "initial_slice_set_instantion" in log_text
    assert re.search(r"t=104453 .*stage=push result=ok type=5 .*body=10123", log_text)
    assert "publication_region=16" in log_text
    assert "EXCEPTION_ACCESS_VIOLATION at 00007FF707F5B1F1" in crash_text
    assert "tried to read address 0000000000000000" in crash_text
    frames = [int(value, 16) for value in re.findall(r"^([0-9A-F]{16}) <unknown>$", crash_text, re.MULTILINE)]
    assert len(frames) >= 3
    assert frames[0] - LIVE_BASE == 0x100AD13
    assert frames[2] - LIVE_BASE == 0x10091A1
    return {
        "logSha256": sha256(log),
        "crashInfoSha256": sha256(crash),
        "initialSliceInstantiationMs": 104437,
        "fullType5PublicationMs": 104453,
        "publicationPackedRegion": 16,
        "exception": "0x7FF707F5B1F1",
        "exceptionRva": f"0x{0x7FF707F5B1F1 - LIVE_BASE:X}",
        "nullRead": True,
        "stackLinks": [
            {
                "address": f"0x{frames[0]:X}",
                "rva": f"0x{frames[0] - LIVE_BASE:X}",
                "meaning": "return after the call at 100AD0E to A262A0",
            },
            {
                "address": f"0x{frames[2]:X}",
                "rva": f"0x{frames[2] - LIVE_BASE:X}",
                "meaning": "return after the call at 100919C to 100A6E0",
            },
        ],
    }


def replay_case(image: bytes, ordinal: int, present: bool) -> dict:
    packed = replay_packer(image, ordinal)
    lookup_result = replay_lookup_tail(image, packed, present)
    assert lookup_result == (RECORD if present else 0)
    dereference = replay_dereference(image, packed, lookup_result)
    return {
        "authorityCScenarioOrdinal": ordinal,
        "packedRegion": packed,
        "tableOffset": f"0x{0x1E20 + packed * 16:X}",
        "modeledEntryPresent": present,
        "lookupResult": "modeled loaded record" if lookup_result else "null",
        "nativeDereference": dereference,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--incident",
        type=Path,
        default=ROOT / "build" / "coo" / "eater-load-crash-20260913",
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    image_path = ROOT / "destiny2_unpacked.bin"
    image = image_path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == PIN
    verify_control_flow(image)
    result = {
        "schemaVersion": 1,
        "imageSha256": PIN,
        "preservedIncident": verify_preserved_incident(args.incident),
        "nativePath": {
            "lifetimeApply": "100A6E0 reads authority+C through 4FFBB0; 100AD04 passes ordinals <=63 to A262A0 when navigation is needed",
            "packer": "4C8E60 computes (scenarioOrdinal & 63) << 3",
            "lookup": "438CA0 indexes decodedTable + 0x1E20 + packedRegion*16; handle -1 returns null",
            "consumer": "42B1E0 unconditionally reads the returned record word at 42B1F1",
        },
        "executedOriginal": ["4C8E60-4C8E6B", "438FF1-439064", "42B1E0-42B1FF"],
        "modeledBoundaries": [
            "438CA0-438FEB encrypted global table-pointer unwrap; replay supplies its decoded RBX at 438FF1",
            "game-populated navigation table entries and handle-pool storage",
            "42B1E0 call boundary receives the result independently produced by the original lookup-tail replay",
        ],
        "cases": [
            replay_case(image, 0, False),
            replay_case(image, 2, True),
        ],
        "conclusion": (
            "The captured region 16 requires lifetime authority scenario ordinal 2. Native packing "
            "selects packed region 16/table offset 0x1F20. Generic ordinal 0 selects offset 0x1E20; "
            "a missing entry returns null and reproduces the captured read at 42B1F1. This proves the "
            "lookup contract and sequencing; loaded/missing table contents are explicitly modeled."
        ),
    }
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    print(rendered, end="")


if __name__ == "__main__":
    main()
