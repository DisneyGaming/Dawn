"""Replay the saved stale-list hang and the production C++ repair, offline.

The supported native list traversal executes in Unicorn. Context-property
lookup is a fixture returning absent; no native handles or list links are
stubbed. Dumps stay local, and no running game is accessed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'.codex-tools/minidump'))
from minidump.minidumpfile import MinidumpFile
from mercury_streaming_native import Native, BASE, IMAGE_SHA256


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('dump',type=Path)
    parser.add_argument('test_binary',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    image=(root/'build/baseline-02fc2c30/destiny2_unpacked.bin').read_bytes()
    assert hashlib.sha256(image).hexdigest()==IMAGE_SHA256
    assert image[0x4D86B0:0x4D86B0+21].hex()=='40564883ec208b01488bf185c07e3048895c243048'
    args.output.mkdir(parents=True,exist_ok=True)
    dump=MinidumpFile.parse(str(args.dump));reader=dump.get_reader()
    game=next(m for m in dump.modules.modules if m.name.lower().endswith('destiny2.exe'))
    def get(address,fmt='Q'):
        return struct.unpack('<'+fmt,reader.read(address,struct.calcsize('<'+fmt)))[0]
    # Identity comes from the captured worker's native frame, never a live address.
    threads=json.loads(args.dump.with_suffix('.threads.json').read_text())
    worker=next(t for t in threads if t['thread']==15164)
    assert worker['frames'][0]['pc']=='destiny2.exe+0x4D878B'
    owner=int(worker['frames'][1]['regs']['Rsi'],16)
    pool=get(owner+0x808);header=get(pool);bitmap=get(header+8)
    capacity=get(header+0x1C,'H');assert 0<capacity<=8192
    elements=get(pool+8);bits=get(bitmap+0x10)
    segments=[(owner,0x810),(pool,0x40),(header,0x20),(bitmap,0x18),
              (bits,((capacity+31)//32)*4),(elements,capacity*0x88)]
    fixture=args.output/'captured-property-list.bin'
    with fixture.open('wb') as f:
        f.write(struct.pack('<IQI',0x504C4731,owner,len(segments)))
        for address,size in segments:
            f.write(struct.pack('<QI',address,size));f.write(reader.read(address,size))
    repaired=args.output/'repaired-property-list.bin'
    result=subprocess.run([str(args.test_binary),str(fixture),str(repaired)],capture_output=True,text=True,check=True)
    (args.output/'cpp-captured.log').write_text(result.stdout+result.stderr)
    directory=get(game.baseaddress+0x2439C70);tables=get(directory)
    # The corrupt links leave the node pool and enter table zero, then table eight.
    # Capture those exact native resolutions to reproduce the observed cycle.
    package=get(tables+8*64+8)+4*get(tables+8*64+0x30,'I')
    native_segments=segments+[(directory,0x20),(tables,916*64),(package,0x140)]
    def replay(fixed):
        n=Native(image);mapped=set()
        for address,size in native_segments:
            for page in range(address&~4095,(address+size+4095)&~4095,4096):
                if page not in mapped:n.uc.mem_map(page,4096);mapped.add(page)
            n.uc.mem_write(address,reader.read(address,size))
        n.put(BASE+0x2439C70,directory,'Q')
        n.stubs[0xA03560]=lambda:0
        if fixed:n.uc.mem_write(owner,repaired.read_bytes())
        try:n.call(0x4D86B0,owner)
        except AssertionError as error:
            assert not fixed and str(error)=='native routine did not return'
            return 'instruction_limit_in_original_loop'
        assert fixed,'Original unexpectedly returned'
        return 'native_return'
    before=replay(False);after=replay(True)
    receipt={'before':before,'after':after,'cpp':result.stdout.strip(),
        'scope':'Captured cleared-slot loop; no live acceptance claimed',
        'test_binary_sha256':hashlib.sha256(args.test_binary.read_bytes()).hexdigest()}
    (args.output/'replay.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))


if __name__=='__main__':main()
