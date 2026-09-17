"""Export complete outer-entrance source tags without loading or changing the game.

Recheck the five client descriptors against installed packages, follow their
typed wrapper chains, and decode the two declared type-60 volume definitions.
Only the requested output directory is written. Package key material is never
printed or persisted by the imported package reader.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import extract_eater_of_worlds as eater

ROOT = eater.ROOT
DEFAULT_OUT = ROOT/'build/coo/eater-live-inspection-20260913/entrance-offline-recovery'
GROUP_TAGS = (0x80B49EB8,0x80B49EDA)
VOLUMES = (
    (0x8155C000,0xA9E6185F,4,'tv_entry_door'),
    (0x8155C009,0xB270AC62,1,'tv_phase_berth_traversal'),
)

def recover():
    inventory=json.loads((ROOT/'docs/raids/eater-of-worlds/evidence/native-inventory.json').read_text())
    groups=[eater.recover_group(tag) for tag in GROUP_TAGS]
    expected_names={'seq_berth_music','d_entry_door','m_engagement_sensor','pm_entry_door','pt_phase_berth_traversal'}
    assert {d['name'] for g in groups for d in g['descriptors']}==expected_names
    assert sum(g['declaredSlotCount'] for g in groups)==7
    assert sum(g['resolvedClientDescriptorCount'] for g in groups)==5
    for g in groups:
        old=next(x for x in inventory['groups'] if x['objectTag']==g['objectTag'])
        for key in ('registryKey','declaredSlots','descriptors','bubbleBindings'):
            assert g[key]==old[key], (g['objectTag'],key)
    bubble=next(b for b in inventory['bubbles'] if b['ordinal']==2)
    state=bubble['states'][0]
    assert (state['mapBubbleIndex'],state['sliceSetIndex'])==(15,16)
    blobs={}
    assets=[]
    visited=set()
    def collect(tag):
        if tag in visited:return
        assert len(visited)<80, 'Unexpected wrapper expansion'
        visited.add(tag)
        cls,data=eater.packages.read(tag)
        relative=f'tags/{tag:08X}.{cls:08X}.bin'
        blobs[relative]=data
        refs=[]
        if cls==0x80809B14:
            refs=[eater.u32(data,12)]
        elif cls==0x80809468:
            refs=[eater.u32(data,p) for p in eater.array(data,16,4)]
        assets.append({'tag':eater.hx(tag),'class':eater.hx(cls),'byteLength':len(data),'sha256':hashlib.sha256(data).hexdigest(),'file':relative,'typedWrapperTargets':[eater.hx(r) for r in refs]})
        for ref in refs:collect(ref)
    collect(eater.SCENARIO)
    collect(int(state['entryTag'],16))
    collect(int(state['registryTag'],16))
    for g in groups:
        collect(int(g['objectTag'],16))
        for b in g['bubbleBindings']:
            for tag in b['handles']:collect(int(tag,16))
        for d in g['descriptors']:collect(int(d['sourceTag'],16))
    volumes=[]
    for tag,registry,slot,wanted_name in VOLUMES:
        collect(tag)
        cls,data=eater.packages.read(tag)
        assert cls==0x80809C36
        binding=struct.pack('<IHH',registry,60,slot)
        matches=[]
        for at in range(12,len(data)-0xE0,4):
            if data[at:at+8]!=binding:continue
            base=at-12
            name_at=base+eater.i64(data,base)
            if not 0<=name_at<len(data):continue
            name=data[name_at:].split(b'\0',1)[0].decode(errors='replace')
            if name!=wanted_name:continue
            vertices=[struct.unpack_from('<3f',data,p) for p in eater.array(data,base+0xD0,16,0x80800094)]
            lower=struct.unpack_from('<3f',data,base+0xB0)
            upper=struct.unpack_from('<3f',data,base+0xC0)
            assert len(vertices)>=3 and all(lo<=hi for lo,hi in zip(lower,upper))
            matches.append({'tag':eater.hx(tag),'offset':base,'registry':eater.hx(registry),'type':60,'slot':slot,'name':name,'min':lower,'max':upper,'vertices':vertices})
        assert len(matches)==1, (eater.hx(tag),len(matches))
        volumes.extend(matches)
    # Prove the door monitor references this volume by its serialized identity.
    _,monitor=eater.packages.read(0x8155C006)
    assert struct.unpack_from('<IHH',monitor,0x270)==(0xA9E6185F,60,4)
    bindings=[{'sourceTag':'0x8155C006','sourceOffset':0x270,'name':'pm_entry_door','targetRegistry':'0xA9E6185F','targetType':60,'targetSlot':4,'targetVolume':'tv_entry_door'}]
    cache=eater.CACHE.read_bytes()
    eater.cache_domains(cache)
    result={'schema':'eater-outer-entrance-offline-v1','method':'Fresh installed-package extraction and exact comparison with the five known client descriptors; typed wrapper traversal; named type-60 volume decoding.','scenario':'raid_envy_v310','scenarioTag':eater.hx(eater.SCENARIO),'bubble':bubble,'clientDescriptorCount':5,'declaredSlotCount':7,'recoveredVolumeCount':2,'groups':groups,'volumes':volumes,'bindings':bindings,'assets':sorted(assets,key=lambda a:a['tag']),'sourceCacheVersion':54,'sourceCacheSha256':hashlib.sha256(cache).hexdigest(),'liveStatus':'Entrance launch at bubble 2 / slice 16 / spawn 8BA80878 failed during native sobject creation; no successful entrance runtime capture is claimed.','limits':'Exports source definitions, wrappers, group/bubble metadata and volumes. Does not recursively export every model/audio dependency or reconstruct server-only execution, a successful object creation, or the cause of the BIRD failure.'}
    blobs['entrance-data.json']=(json.dumps(result,indent=2)+'\n').encode()
    return result,blobs

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=DEFAULT_OUT)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    result,blobs=recover()
    manifest={name:hashlib.sha256(data).hexdigest() for name,data in sorted(blobs.items())}
    blobs['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    for relative,data in blobs.items():
        path=args.output/relative
        if args.check:
            assert path.read_bytes()==data, f'Saved extraction differs: {path}'
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(data)
    print(json.dumps({'mode':'checked' if args.check else 'exported','clientDescriptors':5,'volumes':2,'nativeTagFiles':len(result['assets']),'nativeTagBytes':sum(a['byteLength'] for a in result['assets']),'output':str(args.output)}))

if __name__=='__main__':main()
