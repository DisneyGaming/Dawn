"""Validate and stage Gateway native AI and Forest lattice; do not install or launch the game."""
import json,subprocess,sys,zipfile
from pathlib import Path
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-ai'
CHANGED={
 'Dawn/src/state/activity/gateway/authority.h',
 'Dawn/src/state/activity/coo/native_device_authority.h',
}

def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(), 'Preserve installed candidate evidence'
 accepted=ROOT/'build/coo/validation-gateway-mainland/candidate-source.zip'
 assert verify.digest(accepted)=='8f7634a2d02bdcb744b8e42b36222635fcde734823a7aa8f81b2c58ddf279156'
 protected=0
 with zipfile.ZipFile(accepted) as archive:
  for name in archive.namelist():
   if (name.startswith('Dawn/src/') or name.startswith('Dawn/scripts/')) and name not in CHANGED:
    assert (ROOT/name).read_bytes()==archive.read(name), 'Accepted source/script changed: '+name
    protected+=1
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_ai.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/generate_gateway_catalog.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_traversal_bindings.py'),'--check'],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/coo/verify_gateway_mainland_bindings.py'),'--check'],cwd=ROOT,check=True)
 move=(ROOT/'Dawn/src/client/hooks/teleport/teleport_move.cpp').read_text()
 owns=move[move.index('bool owns_player('):move.index('/** @param reason',move.index('bool owns_player('))]
 assert 'read_local_player_entity(component,entity)' in owns and 'kHandleIndexMask' not in owns
 assert 'identity::current(before,owner,after)' in move
 print('Protected accepted source/script files:',protected,flush=True)
 paths=sorted(p for folder in ('Dawn/src','Dawn/unit','Dawn/resources','Dawn/scripts','Dawn/vendor')
  for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Dawn/Dawn.vcxproj',ROOT/'Dawn/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Dawn/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths}
 results=[]
 for config in ('Debug','Release'):
  for name in ('player_position_tests','gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_script_tests','mission_prelaunch_tests','coo_shared_tests','coo_combat_tests','coo_combat_runtime_tests'):
   results.append(verify.build(ROOT/f'Dawn/unit/{name}.vcxproj',config))
 print('Regression tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Dawn/Dawn.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()), 'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
  for name in manifest:archive.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway native AI and Forest lattice candidate built and verified.',flush=True)
if __name__=='__main__':main()
