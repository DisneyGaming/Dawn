import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
configuration = sys.argv[1] if len(sys.argv) > 1 else 'Release'
project = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else root / 'Sunrise/Sunrise.vcxproj'
name = project.stem
variant = sys.argv[3] if len(sys.argv) > 3 else 'local'
folder = root / 'build/omega-src-port' / (name + '-' + variant if name != 'Sunrise' else '')
log = Path(__file__).with_name('build-' + (name + '-' + variant + '-' if name != 'Sunrise' else '') + configuration.lower() + '.log')
args = [r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        str(project), '/nologo', '/m:2', '/v:minimal',
        '/p:Configuration=' + configuration, '/p:Platform=x64', '/p:CL_MPCount=4',
        '/p:OutDir=' + str(folder / configuration) + '\\',
        '/p:IntDir=' + str(folder / 'obj' / configuration) + '\\']
if variant == 'reference':
    args += ['/p:OmegaSourceRoot=' + str(Path(__file__).resolve().parent / 'src'), '/p:OmegaVariant=reference']
# The host can supply both PATH and Path. .NET Framework MSBuild rejects that.
env = {key.upper(): value for key, value in os.environ.items()}
with log.open('w') as stream:
    result = subprocess.run(args, env=env, stdout=stream, stderr=subprocess.STDOUT)
lines = log.read_text(errors='replace').splitlines()
errors = [line for line in lines if ': error ' in line]
print('\n'.join(errors if errors else lines[-12:]))
sys.exit(result.returncode)
