# Ghidra script: locate the code that populates the AI actor's firing-position block,
# and decompile the three functions the runtime probe implicated.
#
# Run from Ghidra's Script Manager (or Window > Python) with destiny2_unpacked.bin open.
# Writes: C:\Destiny 2 Development\ghidra_out.txt
#
# Compatible with both Jython 2.7 and PyGhidra (no f-strings).

from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.scalar import Scalar
from ghidra.util.task import ConsoleTaskMonitor

OUT = r"C:\Destiny 2 Development\ghidra_out.txt"

# Struct offsets seen in the runtime probe and the disassembly.
#   0x6434 firing-position count (evaluator bails when zero)
#   0x63F0 firing-position usability bitmask
#   0x6460 firing-position origin
#   0x2620 field the per-actor update compares
#   0x05C0 chained state handle on record A
TARGETS = [0x6434, 0x63F0, 0x6460, 0x2620]

# Functions worth reading in full.
DECOMPILE = [
    ("actor_update", 0x7ff618ce2850),
    ("fp_evaluator", 0x7ff618b54280),
    ("fp_select_job", 0x7ff618b64eb0),
]

prog = currentProgram
fm = prog.getFunctionManager()
af = prog.getAddressFactory()
listing = prog.getListing()
out = open(OUT, "w")


def emit(text):
    out.write(text + "\n")
    print(text)


def fname(addr):
    f = fm.getFunctionContaining(addr)
    if f is None:
        return "?"
    return "{} @ {}".format(f.getName(), f.getEntryPoint())


emit("=" * 70)
emit("SCALAR REFERENCES")
emit("=" * 70)

hits = {}
for t in TARGETS:
    hits[t] = []

count = 0
insts = listing.getInstructions(True)
while insts.hasNext():
    inst = insts.next()
    count += 1
    for i in range(inst.getNumOperands()):
        for obj in inst.getOpObjects(i):
            if isinstance(obj, Scalar):
                v = obj.getUnsignedValue()
                if v in hits:
                    hits[v].append((inst.getAddress(), inst.toString()))

emit("scanned {} instructions".format(count))
for t in TARGETS:
    rows = hits[t]
    emit("")
    emit("--- 0x{:X}: {} site(s) ---".format(t, len(rows)))
    for addr, text in rows[:40]:
        emit("  {}  {}   [{}]".format(addr, text, fname(addr)))
    if len(rows) > 40:
        emit("  ... {} more".format(len(rows) - 40))

emit("")
emit("=" * 70)
emit("DECOMPILATION")
emit("=" * 70)

dec = DecompInterface()
dec.openProgram(prog)
monitor = ConsoleTaskMonitor()

for label, addr in DECOMPILE:
    a = af.getAddress(hex(addr).rstrip("L"))
    f = fm.getFunctionContaining(a)
    emit("")
    emit("-" * 70)
    if f is None:
        emit("{}: no function at {} (try Disassemble there first)".format(label, a))
        continue
    emit("{}: {} @ {}".format(label, f.getName(), f.getEntryPoint()))
    emit("-" * 70)
    res = dec.decompileFunction(f, 120, monitor)
    if res is not None and res.decompileCompleted():
        emit(res.getDecompiledFunction().getC())
    else:
        emit("  decompilation failed")

out.close()
print("WROTE " + OUT)
