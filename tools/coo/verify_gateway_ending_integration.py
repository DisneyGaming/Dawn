"""Validate and freeze the live-tested Gateway ending integration; never deploy."""
import json
import struct
import subprocess
import sys
import zipfile
from pathlib import Path
import verify
from package_read import read

ROOT = verify.ROOT
OUT = ROOT / 'build/coo/validation-gateway-ending-integration'
CHANGED = {
    'Sunrise/scripts/gateway.json',
    'Sunrise/src/client/hooks/bootflow/omega_rescue_scene_receipts.cpp',
    'Sunrise/src/state/activity/coo/native_scene_authority.h',
    *{'Sunrise/src/state/activity/gateway/' + name for name in (
        'authority.h', 'controller.cpp', 'controller.h', 'ending_receipts.h',
        'frame.h', 'profile.h', 'runtime.cpp', 'runtime.h')},
    'Sunrise/unit/gateway_opening_tests.cpp',
    'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',
}

def main():
    verify.OUT = OUT
    OUT.mkdir(parents=True, exist_ok=True)
    assert not (OUT / 'installation.json').exists(), 'Preserve installed evidence'
    archive = ROOT / 'build/coo/validation-gateway-module/candidate-source.zip'
    assert verify.digest(archive) == '9ed15fc3ab0808a75f4fca849ebad486403ca07b32bd19fd2437b2000c0a4fd0'
    protected = 0
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if name not in CHANGED:
                assert (ROOT / name).read_bytes() == z.read(name), 'Unrelated change: ' + name
                protected += 1
        old = json.loads(z.read('Sunrise/scripts/gateway.json'))
    document = json.loads((ROOT / 'Sunrise/scripts/gateway.json').read_text())
    assert document['profile'] == 'gateway.ending.v2'
    for field in ('presentation', 'roles', 'entry', 'modules', 'observations'):
        assert document[field] == old[field], field
    for name in ('composition', 'opening'):
        assert document['graphs'][name] == old['graphs'][name], name
    assert document['graphs']['ending']['steps'][:15] == old['graphs']['ending']['steps'][:15]
    assert verify.digest(ROOT / 'steam_api64.dll') == '24931770d3a70b7548e9ff53e03f1e06e295a47cb9f370e883eeb3c89ef05ae2'
    assert verify.digest(ROOT / 'Sunrise/scripts/omega.json') == '39e161e9481a930ea76bdeb5305d2a377c5834b7bbec8dca7d1a05e921dbcfd1'

    # Independent package and recorded native-instance identities.
    _, parent = read(0x80EC0ABC)
    _, child = read(0x80EC0AC5)
    for offset, kind, target in ((0x2548,0x808063A7,0x90),(0x2998,0x808062FD,0x940),
                                 (0x2D28,0x80806306,0x12D0),(0x2DE8,0x80806306,0x1530)):
        assert struct.unpack_from('<IIQ',parent,offset) == (0x80EC0ABC,kind,target)
    assert struct.unpack_from('<IIQ',child,0x27E8) == (0x80EC0AC5,0x808084D7,0x90)
    for event, offset in ((0x3A5C256C,0x3120),(0xC2656F80,0x30C0)):
        assert struct.unpack_from('<I',parent,offset)[0] == event
    evidence = ROOT / 'build/coo/gateway-vance-live-20260907'
    selector = (evidence / 'selector-full.bin').read_bytes()
    native_child = (evidence / 'child-latest.bin').read_bytes()
    handle = struct.unpack_from('<I',selector,0x24)[0]
    assert struct.unpack_from('<IIQ',selector) == (0x80EC0ABC,0x80806384,0x2548)
    assert struct.unpack_from('<Q',selector,0x38)[0] == 12
    for offset,kind,definition in ((0x8B0,0x808062FE,0x2998),(0x1240,0x80806307,0x2D28),(0x14A0,0x80806307,0x2DE8)):
        assert struct.unpack_from('<IIQ',selector,offset) == (0x80EC0ABC,kind,definition)
        assert struct.unpack_from('<IIQ',selector,offset+0x28) == (handle,kind-1,offset)
    assert struct.unpack_from('<IIQ',native_child) == (0x80EC0AC5,0x808084E9,0x27E8)
    replay = json.loads((evidence / 'live-ending-state6-replay.json').read_text())
    events = next(v for v in replay.values() if isinstance(v,list) and any(isinstance(e,dict) and e.get('kind')=='activity_lifetime_readback' for e in v))
    result = next(e for e in events if e['kind']=='activity_lifetime_readback')
    assert result['state']==6 and result['bytes'].startswith('06010000')
    _, module = read(0x80F48031)
    low=struct.unpack_from('<ff',module,0x3808)
    positive=struct.unpack_from('<ff',module,0x3898)
    active=struct.unpack_from('<ff',module,0x2E78)
    assert low[0]<0<low[1] and not low[0]<=1<=low[1]
    assert 0<positive[0]<.75<1<positive[1] and 0<active[0]<1<active[1] and not active[0]<=.75<=active[1]
    # Existing independent reflection oracle also protects every encounter/source fixture.
    subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_ending_bindings.py'),'--check'],cwd=ROOT,check=True)
    (OUT/'native-evidence.json').write_text(json.dumps({
        'protectedFiles':protected,'installedArchiveSha256':verify.digest(archive),
        'liveReplaySha256':verify.digest(evidence/'live-ending-state6-replay.json'),
        'selectorCaptureSha256':verify.digest(evidence/'selector-full.bin'),
        'childCaptureSha256':verify.digest(evidence/'child-latest.bin'),
        'ascentMs':22640,'finishMs':31000,'nativeLifetime':result,
        'userValidation':'User confirmed the native turn, dialogue, ascent and phase-6 replay worked flawlessly before requesting integration.',
        'freshIntegratedBuildValidation':'pending'
    },indent=2)+'\n')
    paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor')
        for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
    paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
    paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
    manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths}
    results=[]
    print(f'Protected {protected} accepted files; native ending identities and unchanged opening verified.',flush=True)
    for config in ('Debug','Release'):
        for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_executor_tests'):
            results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
    print('Tests passed; building Release DLL.',flush=True)
    results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
    assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
    (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name in manifest:z.write(ROOT/name,name)
    (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Gateway ending integration candidate verified and frozen.',flush=True)

if __name__=='__main__':main()
