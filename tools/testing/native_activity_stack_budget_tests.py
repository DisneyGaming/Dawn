"""Check the optimized DLL's Tower-loading call chain using its PDB and x64 unwind data.

Run on Windows with a matching steam_api64.dll/PDB pair. This checks the compiled
frames, including inlined temporaries, rather than the larger-stack unit harness.
The recorded crash chain must leave at least 1 MiB of the game's 4 MiB stack for
native callers and callees; this is a regression bound, not a whole-program proof.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct


class Symbol(c.Structure):
    _fields_ = [('SizeOfStruct', w.ULONG), ('TypeIndex', w.ULONG), ('Reserved', c.c_ulonglong * 2),
                ('Index', w.ULONG), ('Size', w.ULONG), ('ModBase', c.c_ulonglong), ('Flags', w.ULONG),
                ('Value', c.c_ulonglong), ('Address', c.c_ulonglong), ('Register', w.ULONG),
                ('Scope', w.ULONG), ('Tag', w.ULONG), ('NameLen', w.ULONG), ('MaxNameLen', w.ULONG), ('Name', c.c_char * 1)]


class Image:
    def __init__(self, path):
        self.data = path.read_bytes()
        pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        assert self.data[pe:pe+4] == b'PE\0\0'
        count = struct.unpack_from('<H', self.data, pe+6)[0]
        optional_size = struct.unpack_from('<H', self.data, pe+20)[0]
        optional = pe+24
        assert struct.unpack_from('<H', self.data, optional)[0] == 0x20b
        self.size = struct.unpack_from('<I', self.data, optional+56)[0]
        self.sections = []
        for index in range(count):
            offset = optional+optional_size+40*index
            virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', self.data, offset+8)
            self.sections.append((rva, max(virtual_size, raw_size), raw))
        table, length = struct.unpack_from('<II', self.data, optional+112+3*8)
        self.functions = [struct.unpack_from('<III', self.read(table+offset, 12))
                          for offset in range(0, length, 12)]

    def read(self, rva, count):
        for start, size, raw in self.sections:
            if start <= rva and rva+count <= start+size:
                offset = raw+rva-start
                return self.data[offset:offset+count]
        raise ValueError(f'unmapped RVA {rva:x}')

    def unwind(self, rva):
        header = self.read(rva, 4)
        flags = header[0] >> 3
        count = header[2]
        codes = self.read(rva+4, count*2)
        index = 0
        stack = 0
        while index < count:
            op, info = codes[index*2+1] & 15, codes[index*2+1] >> 4
            slots = 1
            if op == 0:
                stack += 8
            elif op == 1:
                slots = 2 if info == 0 else 3
                stack += (struct.unpack_from('<H', codes, (index+1)*2)[0]*8 if info == 0
                          else struct.unpack_from('<I', codes, (index+1)*2)[0])
            elif op == 2:
                stack += info*8+8
            elif op in (4, 8):
                slots = 2
            elif op in (5, 9):
                slots = 3
            elif op == 10:
                stack += 48 if info else 40
            elif op != 3:
                raise ValueError(f'unsupported unwind operation {op}')
            index += slots
        if flags & 4:
            chain = rva+4+((count+1)&~1)*2
            stack += self.unwind(struct.unpack_from('<III', self.read(chain, 12))[2])
        return stack

    def frame(self, address):
        for start, end, unwind in self.functions:
            if start <= address < end:
                return self.unwind(unwind)+8  # return address
        raise ValueError(f'no unwind data for {address:x}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dll', type=Path)
    args = parser.parse_args()
    path = args.dll.resolve()
    pe = Image(path)
    kernel = c.WinDLL('kernel32')
    kernel.GetCurrentProcess.restype = w.HANDLE
    process = kernel.GetCurrentProcess()
    dbg = c.WinDLL('dbghelp', use_last_error=True)
    dbg.SymSetOptions(0x2 | 0x4)
    dbg.SymInitialize.argtypes = [w.HANDLE, c.c_char_p, w.BOOL]
    dbg.SymLoadModuleEx.argtypes = [w.HANDLE, w.HANDLE, c.c_char_p, c.c_char_p, c.c_ulonglong, w.DWORD, c.c_void_p, w.DWORD]
    dbg.SymLoadModuleEx.restype = c.c_ulonglong
    dbg.SymFromName.argtypes = [w.HANDLE, c.c_char_p, c.POINTER(Symbol)]
    dbg.SymCleanup.argtypes = [w.HANDLE]
    assert dbg.SymInitialize(process, str(path.parent).encode(), False)
    base = dbg.SymLoadModuleEx(process, None, str(path).encode(), None, 0x180000000, pe.size, None, 0)
    assert base, c.get_last_error()
    prefix = 'dawn::server::bap::encrypted::'
    activity = prefix+'push::activity::'
    names = [
        'dawn::steam::run_callbacks',
        'dawn::server::transport::service',
        "dawn::server::transport::`anonymous namespace'::service_peer",
        'dawn::server::bap::consume', prefix+'consume_deferred',
        activity+'consume_activity_keepalive',
        prefix+'activity_transaction::stage_periodic_notifications',
        activity+'build_periodic_region_snapshot',
        activity+"`anonymous namespace'::finalize_snapshot",
        activity+"`anonymous namespace'::finalize_roster",
        activity+'build_roster_snapshot',
        'dawn::server::runtime::activity::native_activity::update',
    ]
    rows = []
    try:
        for name in names:
            buffer = c.create_string_buffer(c.sizeof(Symbol)+2048)
            symbol = c.cast(buffer, c.POINTER(Symbol))
            symbol.contents.SizeOfStruct = c.sizeof(Symbol)
            symbol.contents.MaxNameLen = 2048
            assert dbg.SymFromName(process, name.encode(), symbol), (name, c.get_last_error())
            rva = symbol.contents.Address-base
            rows.append({'function': name, 'rva': hex(rva), 'stack_bytes': pe.frame(rva)})
    finally:
        dbg.SymCleanup(process)
    total = sum(row['stack_bytes'] for row in rows)
    passed = total <= 3*1024*1024 and rows[-1]['stack_bytes'] <= 1024*1024
    print(json.dumps({'passed': passed, 'chain_bytes': total, 'chain_limit': 3*1024*1024,
                      'frames': rows}, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
