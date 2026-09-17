"""Validate and stage Gateway arrival recovery against the installed preload.
Never deploy or launch the game. Existing accepted source evidence is immutable.
"""
import hashlib
import json
from pathlib import Path
import zipfile
import verify

ROOT = verify.ROOT
OUT = ROOT / 'build/coo/validation-gateway-arrival'
CHANGED = {
    'Dawn/src/client/hooks/bootflow/spawn_hold.cpp',
    'Dawn/src/client/hooks/bootflow/spawn_hold_policy.h',
    'Dawn/src/client/hooks/bootflow/world_step.cpp',
    'Dawn/src/client/hooks/bootflow/internal.h',
    'Dawn/src/client/hooks/teleport/runtime.h',
    'Dawn/src/client/hooks/teleport/teleport_move.cpp',
}

def main():
    verify.OUT = OUT
    OUT.mkdir(parents=True, exist_ok=True)
    assert not (OUT / 'installation.json').exists(), 'Preserve installed candidate evidence'
    accepted = ROOT / 'build/coo/validation-gateway-prelaunch/candidate-source.zip'
    assert verify.digest(accepted) == 'bbafa43961a683db63262e3f21fc3813650ff60b34437c1d6f0a613e7b8b39da'
    protected = 0
    with zipfile.ZipFile(accepted) as archive:
        for name in archive.namelist():
            if (name.startswith('Dawn/src/') or name.startswith('Dawn/scripts/')) and name not in CHANGED:
                assert (ROOT / name).read_bytes() == archive.read(name), f'Accepted dependency changed: {name}'
                protected += 1
    print(f'Protected accepted source/script files: {protected}', flush=True)
    paths = sorted(p for folder in ('Dawn/src','Dawn/unit','Dawn/resources','Dawn/scripts','Dawn/vendor')
        for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
    paths += [ROOT/'Dawn/Dawn.vcxproj']
    paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
    manifest = {p.relative_to(ROOT).as_posix(): verify.digest(p) for p in paths}
    results=[]
    for config in ('Debug','Release'):
        for name in ('spawn_hold_lifecycle_tests','mission_prelaunch_tests','coo_mission_script_tests','coo_script_tests',
                     'coo_opening_tests','coo_ending_runtime_tests','other_mission_protocol_tests',
                     'omega_forest_roster_tests'):
            project=ROOT/f'Dawn/unit/{name}.vcxproj'
            if f'Include="{config}|x64"' in project.read_text():
                results.append(verify.build(project,config))
    results.append(verify.build(ROOT/'Dawn/Dawn.vcxproj','Release'))
    assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
    (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for name in manifest: archive.write(ROOT/name,name)
    (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Arrival recovery candidate built; native Gateway fade validation is still pending.',flush=True)

if __name__=='__main__': main()
