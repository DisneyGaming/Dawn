from pathlib import Path

from capstone import CS_AC_WRITE, CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_MEM


IMAGE_BASE = 0x7FF618070000
TARGETS = (0x1AF00, 0x1AF04)
data = Path(r"C:\Destiny 2 Development\destiny2_unpacked.bin").read_bytes()
decoder = Cs(CS_ARCH_X86, CS_MODE_64)
decoder.detail = True
hits = {}

for displacement in TARGETS:
    needle = displacement.to_bytes(4, "little")
    cursor = 0
    while True:
        occurrence = data.find(needle, cursor)
        if occurrence < 0:
            break
        cursor = occurrence + 1
        for start in range(max(0, occurrence - 15), occurrence + 1):
            instructions = list(decoder.disasm(data[start : start + 15], IMAGE_BASE + start, 1))
            if not instructions:
                continue
            instruction = instructions[0]
            if instruction.address + instruction.size < IMAGE_BASE + occurrence + 4:
                continue
            for operand in instruction.operands:
                if (operand.type == X86_OP_MEM and operand.mem.disp == displacement
                        and operand.access & CS_AC_WRITE):
                    hits[instruction.address] = (
                        instruction.mnemonic,
                        instruction.op_str,
                        instruction.bytes.hex(),
                    )

for address, (mnemonic, operands, encoded) in sorted(hits.items()):
    print(f"0x{address:016X} rva=0x{address - IMAGE_BASE:X} "
          f"bytes={encoded} {mnemonic} {operands}")
print(f"writers={len(hits)}")
