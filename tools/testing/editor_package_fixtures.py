"""Read installed item/name/artwork tags into ignored fixtures for native editor validation.

No account is opened or modified. Package assets remain local and must not be committed.
"""
from pathlib import Path
import argparse, importlib.util, json, struct, sys

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('installed_package_reader', args.game/'tools/coo/package_read.py')
    reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    u16 = lambda b,o: struct.unpack_from('<H',b,o)[0]
    u32 = lambda b,o: struct.unpack_from('<I',b,o)[0]
    i64 = lambda b,o: struct.unpack_from('<q',b,o)[0]
    def array(b,o=8):
        count = i64(b,o); header = o+8+i64(b,o+8)
        assert 0 <= count <= 300000 and header >= 0 and header+16 <= len(b)
        return count, header+16
    classes = json.loads((out/'manifest.json').read_text())['classes'] if (out/'manifest.json').exists() else {}
    def read(tag):
        if not 0x80800000 <= tag < 0x82000000: raise ValueError('not a tag')
        path = out/f'{tag:08X}.bin'
        if path.exists() and str(tag) in classes: return path.read_bytes()
        cls,data = reader.read(tag)
        if not path.exists(): path.write_bytes(data)
        classes[str(tag)] = cls
        return data
    cache_paths = [p for p in args.game.glob('*/cache/build_data.bin')]
    assert cache_paths, 'No installed build-data cache found'
    cache = cache_paths[0].read_bytes()
    candidates=[]; at=0
    while (at:=cache.find(b'investment_globals\0',at)) >= 0:
        tag=u32(cache,at+132)
        if 0x80800000<=tag<0x82000000: candidates.append(tag)
        at+=18
    assert candidates
    for globals_tag in candidates:
        try:
            globals_data = read(globals_tag)
            root=read(u32(globals_data,16))
            items=read(u32(root,8+48*16)); n,rows=array(items)
            if n > 1000: break
        except (ValueError,AssertionError,struct.error): continue
    else: raise ValueError('No readable investment root')
    string_table=read(u32(globals_data,16+33*16)); ns,srows=array(string_table)
    localized=read(u32(globals_data,16+72*16)); nl,lrows=array(localized)
    icons=read(u32(globals_data,16+75*16)); ni,irows=array(icons)
    plug_table=read(u32(root,8+51*16))
    read(u32(root,8+11*16))  # Installed investment constants, including armor stat rows.
    entry_lists=read(u32(root,8+97*16)); ne,erows=array(entry_lists)
    for i in range(ne):
        try:
            entry_list=read(u32(entry_lists,erows+i*24+16))
            if i in (1,2,3,5,6,7,9,10,11):
                en,entries=array(entry_list,16)
                for e in range(en):
                    try: read(u32(entry_list,entries+e*64+56))
                    except (ValueError,AssertionError,struct.error): pass
        except (ValueError,AssertionError,struct.error): pass
    names={}; local_cache={}; icon_by_hash={}
    def name(data,offset):
        idx,h=struct.unpack_from('<II',data,offset)
        if idx>=nl: return ''
        if idx not in local_cache:
            header=read(u32(localized,lrows+idx*8+4)); raw=read(u32(header,24))
            nh,hr=array(header); np,pr=array(raw); nc,cr=array(raw,0x48); assert nh==nc
            values={}
            for x in range(nc):
                combo=cr+x*16; start=combo+i64(raw,combo); count=i64(raw,combo+8)
                text=''
                if count<0 or start<pr or start+count*32>pr+np*32: continue
                for p in range(count):
                    part=start+p*32; begin=part+8+i64(raw,part+8)
                    length=u16(raw,part+0x14); shift=u16(raw,part+0x18)
                    text+=''.join(chr(ord(c)+shift) for c in raw[begin:begin+length].decode('utf-8',errors='replace'))
                values[u32(header,hr+x*4)]=text
            local_cache[idx]=values
        return local_cache[idx].get(h,'')
    for i in range(n):
        read(u32(items,rows+i*24+16))
        if i%2000==0: print(f'Item definitions {i}/{n}',flush=True)
    for i in range(ns):
        h=u32(string_table,srows+i*24)
        try:
            data=read(u32(string_table,srows+i*24+16))
            names[h]=name(data,0x84)
            name(data,0x90); name(data,0x98)
            icon=u16(data,0x80)
            if icon<ni: icon_by_hash[h]=u32(icons,irows+icon*24+16)
        except (ValueError,AssertionError,struct.error): continue
        if i%3000==0: print(f'Item names {i}/{ns}',flush=True)
    display_table=read(u32(globals_data,16+61*16)); nd,drows=array(display_table)
    for idx in (1,2,3,5,6,7,9,10,11):
        record=read(u32(display_table,drows+idx*24+16))
        for offset in range(16,len(record)-3,4):
            tag=u32(record,offset)
            if not 0x80800000<=tag<0x82000000: continue
            try:
                display=read(tag)
                if classes[str(tag)]==0x80805c49: name(display,160)
            except (ValueError,KeyError,AssertionError,struct.error): pass
    # Artwork coverage spans weapon/armor types and a sample of perk icons for the visual run.
    wanted=[]
    for i in range(n):
        h=u32(items,rows+i*24); data=read(u32(items,rows+i*24+16))
        if len(data)<188 or not names.get(h): continue
        if h in icon_by_hash: wanted.append(h)
    preferred=('Ace of Spades','Thorn','Riskrunner','The Recluse','Izanagi','Celestial','St0mp','Dunemarchers','Lunafaction','One-Eyed','Insurmountable','Liar','Orpheus','Heart of Inmost','Peacekeepers','Synthoceps')
    wanted += [h for h,nm in names.items() if any(v in nm for v in preferred)]
    for h in dict.fromkeys(wanted):
        try:
            container=read(icon_by_hash[h])
            for offset in (0x1c,0x14,0x20,0x24):
                try:
                    layer=read(u32(container,offset)); resource=0x10+i64(layer,0x10)
                    count,lanes=array(layer,resource); textures,trows=array(layer,lanes)
                    texture=u32(layer,trows); read(texture); read(classes[str(texture)])
                except (ValueError,AssertionError,struct.error): pass
        except (ValueError,KeyError,AssertionError,struct.error): pass
    (out/'manifest.json').write_text(json.dumps({'globals':globals_tag,'count':n,'classes':classes,'names':names,'icons':icon_by_hash},indent=2))
    (out/'globals.txt').write_text(str(globals_tag))
    (out/'classes.tsv').write_text('\n'.join(f'{k}\t{v}' for k,v in classes.items()))
    print(f'Exported {n} installed definitions and {len(classes)} tag fixtures. No save changes.',flush=True)

if __name__ == '__main__': main()
