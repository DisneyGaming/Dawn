"""Validate and stage Gateway traversal; do not install or launch the game."""
import json,subprocess,sys,zipfile
from pathlib import Path
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-traversal'
CHANGED={
 'Sunrise/scripts/gateway.json',
 'Sunrise/src/client/hooks/bootflow/activity_schema_decode_probe.cpp',
 'Sunrise/src/client/hooks/bootflow/omega_enemy_lair_receipts.cpp',
 'Sunrise/src/state/activity/coo/population_service.h',
 'Sunrise/src/state/activity/gateway/authority.h',
 'Sunrise/src/state/activity/gateway/controller.cpp',
 'Sunrise/src/state/activity/gateway/controller.h',
 'Sunrise/src/state/activity/gateway/frame.h',
 'Sunrise/src/state/activity/gateway/profile.h',
 'Sunrise/src/state/activity/gateway/runtime.cpp',
 'Sunrise/src/state/activity/gateway/runtime.h',
 'Sunrise/src/state/activity/omega_combatant_authority.h',
 'Sunrise/src/state/activity/omega_crown_transit_authority.h',
 'Sunrise/src/state/activity/omega_first_mancannon_authority.h',
}

def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(), 'Preserve installed candidate evidence'
 accepted=ROOT/'build/coo/validation-gateway-foundation/candidate-source.zip'
 assert verify.digest(accepted)=='b950b68af1afce806f1ed6edc4035f7e23fd8a12644d8981606bb4f6a727bbfd'
 protected=0
 with zipfile.ZipFile(accepted) as archive:
  for name in archive.namelist():
   if (name.startswith('Sunrise/src/') or name.startswith('Sunrise/scripts/')) and name not in CHANGED:
    assert (ROOT/name).read_bytes()==archive.read(name), 'Accepted source/script changed: '+name
    protected+=1
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_catalog.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_traversal_bindings.py'),'--check'],cwd=ROOT,check=True)
 print('Protected accepted source/script files:',protected,flush=True)
 paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor')
  for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths}
 results=[]
 for config in ('Debug','Release'):
  for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_script_tests','mission_prelaunch_tests','coo_shared_tests','coo_combat_tests','coo_combat_runtime_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
 print('Regression tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
  for name in manifest:archive.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway traversal candidate built and verified.',flush=True)
if __name__=='__main__':main()
