"""Validate and stage Gateway foundation; do not install or launch the game."""
import json,subprocess,sys,zipfile
from pathlib import Path
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-foundation'
CHANGED={
 'Sunrise/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp',
 'Sunrise/src/client/player/player_position.cpp',
 'Sunrise/src/middleware/bap/activity_message/activity_sensor_auth_bodies_other_missions.cpp',
 'Sunrise/src/middleware/bap/activity_message/sensor_auth_update.h',
 'Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp',
}
def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(), 'Preserve installed candidate evidence'
 accepted=ROOT/'build/coo/validation-gateway-spawn/candidate-source.zip'
 assert verify.digest(accepted)=='3485a84b60f599e92db1c6e12c144c6eaf9d831994fa66dc06dcb17dd9a6d527'
 protected=0
 with zipfile.ZipFile(accepted) as archive:
  for name in archive.namelist():
   if (name.startswith('Sunrise/src/') or name.startswith('Sunrise/scripts/')) and name not in CHANGED:
    assert (ROOT/name).read_bytes()==archive.read(name), 'Accepted source/script changed: '+name
    protected+=1
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_catalog.py'),'--check'],cwd=ROOT,check=True)
 print('Protected accepted source/script files:',protected,flush=True)
 paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor')
  for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths}
 results=[]
 for config in ('Debug','Release'):
  for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_script_tests','mission_prelaunch_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
 print('Regression tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
  for name in manifest:archive.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway foundation candidate built and verified.',flush=True)
if __name__=='__main__':main()
