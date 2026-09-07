"""Build and verify the Gateway module fix against the restored region candidate."""
import json,subprocess,sys,zipfile
from pathlib import Path
import verify
from verify_gateway_module_native import verify as native_verify
ROOT=verify.ROOT;OUT=ROOT/'build/coo/validation-gateway-module'
CHANGED={
 'Sunrise/src/state/activity/gateway/authority.h',
 'Sunrise/src/state/activity/gateway/controller.h',
 'Sunrise/src/state/activity/gateway/controller.cpp',
 'Sunrise/src/state/activity/gateway/ending_receipts.h',
 'Sunrise/src/state/activity/gateway/runtime.cpp',
 'Sunrise/src/client/hooks/bootflow/gateway_module_receipts.inl',
 'Sunrise/src/client/hooks/bootflow/gateway_module_damage.h',
 'Sunrise/src/client/hooks/bootflow/omega_arc_charge_receipts.cpp',
 'Sunrise/src/client/hooks/bootflow/gateway_module_damage_hooks.inl',
 'Sunrise/unit/gateway_opening_tests.cpp',
 'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',
}
def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(),'Preserve installed candidate evidence'
 archive=ROOT/'build/coo/validation-gateway-region/candidate-source.zip'
 assert verify.digest(archive)=='55f25744f51ea9f2127f5fedc4846c8888cab795c7fdfd939764f69b30edf0fc'
 protected=0
 with zipfile.ZipFile(archive) as z:
  for name in z.namelist():
   if (name.startswith(('Sunrise/src/','Sunrise/unit/','Sunrise/scripts/')) or name=='Sunrise/Sunrise.vcxproj') and name not in CHANGED:
    assert (ROOT/name).read_bytes()==z.read(name),'Accepted file changed: '+name
    protected+=1
 print('Protected accepted files:',protected,flush=True)
 native_verify(OUT)
 for name in ('generate_gateway_ai','generate_gateway_catalog','verify_gateway_traversal_bindings','verify_gateway_mainland_bindings','verify_gateway_ending_bindings'):
  subprocess.run([sys.executable,str(ROOT/f'tools/coo/{name}.py'),'--check'],cwd=ROOT,check=True)
 paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths};results=[]
 for config in ('Debug','Release'):
  for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests','coo_mission_script_tests','coo_combat_tests','coo_combat_runtime_tests','coo_ending_tests','coo_ending_runtime_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
 print('Regression tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()),'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
  for name in manifest:z.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway module candidate built and verified. Fresh native playthrough pending.',flush=True)
if __name__=='__main__':main()
