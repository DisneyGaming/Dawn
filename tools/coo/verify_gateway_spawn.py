"""Build the confirmed Gateway opening spawn preset; do not install or launch."""
import json
from pathlib import Path
import zipfile
import verify

ROOT = verify.ROOT
OUT = ROOT / 'build/coo/validation-gateway-spawn'
CHANGED = {
    'Dawn/src/state/activity/forced/definition.h',
    'Dawn/src/server/ui/activity_override/activity_override_panel.cpp',
}


def main():
    verify.OUT = OUT
    OUT.mkdir(parents=True, exist_ok=True)
    assert not (OUT / 'installation.json').exists(), 'Preserve installed candidate evidence'
    accepted = ROOT / 'build/coo/validation-gateway-arrival/candidate-source.zip'
    assert verify.digest(accepted) == '9887a13cacf8803188cb39a007d8c91aac38085b60e89a69bfd3098203d07b4f'
    protected = 0
    with zipfile.ZipFile(accepted) as archive:
        for name in archive.namelist():
            if (name.startswith('Dawn/src/') or name.startswith('Dawn/scripts/')) and name not in CHANGED:
                assert (ROOT / name).read_bytes() == archive.read(name), f'Accepted dependency changed: {name}'
                protected += 1
    print(f'Protected source/script files: {protected}', flush=True)
    paths = sorted(p for folder in ('Dawn/src','Dawn/unit','Dawn/resources','Dawn/scripts','Dawn/vendor')
        for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
    paths += [ROOT/'Dawn/Dawn.vcxproj', ROOT/'Dawn/docs/GATEWAY-RECONSTRUCTION.md']
    paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
    manifest = {p.relative_to(ROOT).as_posix(): verify.digest(p) for p in paths}
    results = []
    for config in ('Debug', 'Release'):
        for name in ('mission_prelaunch_tests', 'other_mission_protocol_tests'):
            results.append(verify.build(ROOT/f'Dawn/unit/{name}.vcxproj', config))
    print('Routing regressions passed; building the Release DLL.', flush=True)
    results.append(verify.build(ROOT/'Dawn/Dawn.vcxproj', 'Release'))
    assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
    (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for name in manifest: archive.write(ROOT/name,name)
    (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Gateway opening spawn candidate built and verified.', flush=True)


if __name__ == '__main__':
    main()
