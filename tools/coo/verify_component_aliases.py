"""Replay captured native controller aliases, validate scope, build without deployment."""
import json,zipfile,concurrent.futures
from pathlib import Path
import verify
ROOT=verify.ROOT
BASE=ROOT/'build/coo/validation-universal-services'
OUT=ROOT/'build/coo/validation-component-aliases'
CHANGED={'Sunrise/src/client/hooks/bootflow/coo_native_components.h','Sunrise/unit/coo_universal_services_tests.cpp','Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md'}
ADDED={'Sunrise/unit/fixtures/gateway_controller_components.bin','tools/coo/verify_component_aliases.py'}

def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(),'Preserve installation evidence'
 assert verify.digest(BASE/'candidate-source.zip')=='d9280684193f36ec97bd0296c4269bf7eadf7cfe5a9346fee8de69c2d252e544'
 manifest=json.loads((BASE/'candidate-source.json').read_text());protected=0
 for name,sha in manifest.items():
  if name not in CHANGED:
   assert verify.digest(ROOT/name)==sha,'Unrelated change: '+name
   protected+=1
 with zipfile.ZipFile(BASE/'candidate-source.zip') as archive:
  name='Sunrise/src/client/hooks/bootflow/coo_native_components.h'
  expected=archive.read(name).decode().replace('\r\n','\n')
  expected=expected.replace('// must resolve back to itself and the expected entity. Ambiguous matches fail.','// must resolve back to itself and the expected entity. Reflected base/interface\n// rows may alias that same component; only distinct matching components conflict.')
  expected=expected.replace('|| !read.resolve(self,resolved) || resolved!=address || result)','|| !read.resolve(self,resolved) || resolved!=address || (result && result!=address))')
  assert (ROOT/name).read_text()==expected,'Lookup change exceeds identity-aware alias handling'
 evidence=json.loads((ROOT/'build/coo/gateway-controller-binding-20260907/fixture.json').read_text())
 assert evidence['processModified']==False and len(evidence['cases'])==3
 assert verify.digest(ROOT/evidence['fixture'])==evidence['sha256']=='92a2271ee45dd3e6dedc4614fc3827cfde07bd6a2b32d9e710a47540bc5b35ab'
 assert all(c['aliases']==10 for c in evidence['cases'])
 assert 'FAIL line' in (OUT/'regression-before.log').read_text()
 installation=json.loads((BASE/'installation.json').read_text(encoding='utf-8-sig'))
 assert verify.digest(ROOT/'steam_api64.dll')==installation['dllSha256']
 for name,sha in installation['protectedFiles'].items(): assert verify.digest(ROOT/name)==sha
 paths=set(manifest)|ADDED
 frozen={name:verify.digest(ROOT/name) for name in sorted(paths)}
 print(f'Protected {protected} files; one native lookup condition changed. Captured aliases verified.',flush=True)
 def suite(name):
  return [verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config) for config in ('Debug','Release')]
 results=[]
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
  for rows in pool.map(suite,('coo_universal_services_tests','gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests')): results.extend(rows)
 for result in results:
  if result['project']=='omega_archive_protocol_tests': assert 'digest=A5BE474333FF4DDF' in result['output']
 print('Captured-state replay and regressions passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in frozen.items()),'Source changed during build'
 (OUT/'candidate-source.json').write_text(json.dumps(frozen,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
  for name in frozen: archive.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 (OUT/'review.json').write_text(json.dumps({'protectedFiles':protected,'changed':sorted(CHANGED),'added':sorted(ADDED),'capturedFixture':evidence,'checks':len(results)-1,'nativeReadinessBypassed':False,'scriptsChanged':False,'freshIntegratedRunValidated':False},indent=2)+'\n')
 print('Validated controller-alias fix is frozen and ready to install.',flush=True)
if __name__=='__main__':main()
