"""Validate and stage Gateway launch support against the accepted generic Omega build.
Never deploy or launch the game. Existing accepted source evidence is immutable.
"""
import hashlib
import json
from pathlib import Path
import zipfile
import verify

ROOT = verify.ROOT
OUT = ROOT / 'build/coo/validation-gateway-prelaunch'
CHANGED = {
    'Sunrise/src/client/hooks/bootflow/towerfall_executor_bootstrap.cpp',
    'Sunrise/src/server/ui/activity_override/activity_override_panel.cpp',
    'Sunrise/src/state/activity/forced/activity_forced_destination.cpp',
    'Sunrise/src/state/activity/forced/activity_forced_destination.h',
    'Sunrise/src/state/activity/forced/definition.h',
}

def main():
    verify.OUT = OUT
    OUT.mkdir(parents=True, exist_ok=True)
    assert not (OUT / 'installation.json').exists(), 'Preserve installed candidate evidence'
    accepted = ROOT / 'build/coo/validation-generic/candidate-source.zip'
    assert verify.digest(accepted) == '093c360c9296e382b762a9431b0504cab8e9b4704e71864b6fede102512fce57'
    protected = 0
    with zipfile.ZipFile(accepted) as archive:
        for name in archive.namelist():
            if (name.startswith('Sunrise/src/') or name.startswith('Sunrise/scripts/')) and name not in CHANGED:
                assert (ROOT / name).read_bytes() == archive.read(name), f'Accepted dependency changed: {name}'
                protected += 1
    print(f'Protected accepted source/script files: {protected}', flush=True)
    paths = sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor')
        for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
    paths += [ROOT/'Sunrise/Sunrise.vcxproj']
    paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
    manifest = {p.relative_to(ROOT).as_posix(): verify.digest(p) for p in paths}
    results=[]
    for config in ('Debug','Release'):
        for name in ('mission_prelaunch_tests','coo_mission_script_tests','coo_script_tests',
                     'coo_opening_tests','coo_ending_runtime_tests','other_mission_protocol_tests',
                     'omega_forest_roster_tests'):
            project=ROOT/f'Sunrise/unit/{name}.vcxproj'
            if f'Include="{config}|x64"' in project.read_text():
                results.append(verify.build(project,config))
    results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
    assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
    (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for name in manifest: archive.write(ROOT/name,name)
    (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Gateway preload candidate built; native load and spawn validation are still pending.',flush=True)

if __name__=='__main__': main()
