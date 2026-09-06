"""Freeze the accepted Omega inputs without changing or launching the installation."""
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / 'build/coo/omega-baseline-20260906'
EXPECTED = 'f4ffa03e31ddc8a0f5927d4038448aa71de2870a649e0064873d1eb34f1fe92f'

def digest(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()

def main():
    if BASELINE.exists():
        raise SystemExit('Baseline already exists; verify it instead of overwriting it.')
    if digest(ROOT / 'steam_api64.dll') != EXPECTED:
        raise SystemExit('Installed DLL differs from the accepted reference.')
    files = []
    for folder in ('Sunrise/src', 'Sunrise/resources', 'Sunrise/vendor', 'Sunrise/unit',
                   'Sunrise/cache', 'Sunrise/exports', 'Sunrise/docs',
                   'tools/omega-reference-20260906'):
        files += [p for p in (ROOT / folder).rglob('*') if p.is_file()
                  and p.suffix not in ('.obj', '.pdb', '.exe', '.zip', '.pyc')]
    files += [ROOT / p for p in ('steam_api64.dll', 'Sunrise/Sunrise.vcxproj',
        'Sunrise/settings.json', 'Sunrise/OMEGA-SRC-PORT.md', 'Sunrise/logs/sunrise.log',
        'Sunrise/logs/sunrise.log.old', 'deploy-omega.ps1', 'launch-scot-reveal-debug.cmd')]
    files = sorted(set(files))
    manifest = {p.relative_to(ROOT).as_posix(): digest(p) for p in files}
    head = subprocess.check_output(['git', '-c', f'safe.directory={ROOT.as_posix()}',
                                    'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    BASELINE.mkdir(parents=True)
    archive = BASELINE / 'baseline.zip'
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for path in files:
            output.write(path, path.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(archive) as frozen:
        assert frozen.testzip() is None
        for name, expected in manifest.items():
            assert hashlib.sha256(frozen.read(name)).hexdigest() == expected, name
    receipt = {'gitHead': head, 'installedDllSha256': EXPECTED,
               'archiveSha256': digest(archive), 'files': manifest}
    (BASELINE / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Verified {len(files)} files: {archive}')

if __name__ == '__main__':
    main()
