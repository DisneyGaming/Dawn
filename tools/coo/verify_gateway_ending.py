"""Validate and stage the complete Gateway gameplay ending; do not install or launch the game."""
import json,subprocess,sys,zipfile
from pathlib import Path
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-ending'
CHANGED={
 'Sunrise/scripts/gateway.json',
 'Sunrise/src/client/hooks/bootflow/omega_arc_charge_receipts.cpp',
 'Sunrise/src/client/hooks/bootflow/omega_rescue_scene_receipts.cpp',
 'Sunrise/src/middleware/bap/activity_message/activity_sensor_auth_bodies_other_missions.cpp',
 'Sunrise/src/state/activity/gateway/catalog.h',
 'Sunrise/src/state/activity/gateway/traversal_catalog.h',
 'Sunrise/src/state/activity/gateway/ai_bindings.h',
 'Sunrise/src/state/activity/gateway/profile.h',
 'Sunrise/src/state/activity/gateway/controller.h',
 'Sunrise/src/state/activity/gateway/controller.cpp',
 'Sunrise/src/state/activity/gateway/frame.h',
 'Sunrise/src/state/activity/gateway/preparation.h',
 'Sunrise/src/state/activity/gateway/authority.h',
 'Sunrise/src/state/activity/gateway/runtime.h',
 'Sunrise/src/state/activity/gateway/runtime.cpp',
}


def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(), 'Preserve installed candidate evidence'
 accepted=ROOT/'build/coo/validation-gateway-ai/candidate-source.zip'
 assert verify.digest(accepted)=='0cd25d058971872d7355cfefa2fb7e7de257fd52e84c710c4a44165b3b19361b'
 protected=0
 with zipfile.ZipFile(accepted) as archive:
  for name in archive.namelist():
   if (name.startswith('Sunrise/src/') or name.startswith('Sunrise/scripts/')) and name not in CHANGED:
    assert (ROOT/name).read_bytes()==archive.read(name), 'Accepted source/script changed: '+name
    protected+=1
  old=json.loads(archive.read('Sunrise/scripts/gateway.json'));current=json.loads((ROOT/'Sunrise/scripts/gateway.json').read_text())
  assert old['graphs']['opening']==current['graphs']['opening'], 'Earlier progression changed'
  assert old['presentation']==current['presentation'], 'Earlier dialogue bank changed'
  import re
  for name in ('traversal_catalog.h','ai_bindings.h'):
   path='Sunrise/src/state/activity/gateway/'+name
   rows=lambda text:set(line.strip() for line in text.splitlines() if re.match(r'\s*\{\d',line))
   assert rows(archive.read(path).decode())<=rows((ROOT/path).read_text()), 'Accepted source counts or AI assignments changed'
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_ai.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_catalog.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_traversal_bindings.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_mainland_bindings.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_ending_bindings.py'),'--check'],cwd=ROOT,check=True)
 move=(ROOT/'Sunrise/src/client/hooks/teleport/teleport_move.cpp').read_text()
 owns=move[move.index('bool owns_player('):move.index('/** @param reason',move.index('bool owns_player('))]
 assert 'read_local_player_entity(component,entity)' in owns and 'kHandleIndexMask' not in owns
 assert 'identity::current(before,owner,after)' in move
 print('Protected accepted source/script files:',protected,flush=True)
 paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor')
  for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths}
 results=[]
 for config in ('Debug','Release'):
  for name in ('player_position_tests','gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_script_tests','mission_prelaunch_tests','coo_shared_tests','coo_combat_tests','coo_combat_runtime_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
 print('Regression tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
  for name in manifest:archive.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway complete gameplay candidate built and verified; ending live validation remains pending.',flush=True)
if __name__=='__main__':main()
