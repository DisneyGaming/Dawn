"""Validate the Gateway return-dialogue cue against the accepted installed build."""
import json,zipfile,concurrent.futures
from pathlib import Path
import verify
ROOT=verify.ROOT
BASE=ROOT/'build/coo/validation-component-aliases'
OUT=ROOT/'build/coo/validation-return-cue'
CHANGED={'Sunrise/scripts/gateway.json','Sunrise/src/state/activity/gateway/controller.cpp','Sunrise/src/state/activity/gateway/controller.h','Sunrise/src/state/activity/gateway/profile.h','Sunrise/src/state/activity/gateway/frame.h','Sunrise/src/state/activity/gateway/ending_cadence.h','Sunrise/unit/gateway_opening_tests.cpp','Sunrise/docs/GATEWAY-RECONSTRUCTION.md'}
ADDED={'tools/coo/verify_gateway_return_cue.py','build/coo/gateway-return-cue/cue-evidence.json','build/coo/gateway-return-cue/native-word-timing.json','build/coo/install_gateway_return_cue.ps1'}

def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists(),'Preserve installation evidence'
 manifest=json.loads((BASE/'candidate-source.json').read_text());installation=json.loads((BASE/'installation.json').read_text(encoding='utf-8-sig'))
 assert verify.digest(ROOT/'steam_api64.dll')==installation['dllSha256']
 for name,sha in installation['protectedFiles'].items():assert verify.digest(ROOT/name)==sha
 protected=0
 for name,sha in manifest.items():
  if name not in CHANGED:assert verify.digest(ROOT/name)==sha,'Unrelated change: '+name;protected+=1
 with zipfile.ZipFile(BASE/'candidate-source.zip') as z:
  before=json.loads(z.read('Sunrise/scripts/gateway.json'));after=json.loads((ROOT/'Sunrise/scripts/gateway.json').read_text())
  cue=after['bindings'].pop('vance.return_cue');assert cue=={'capability':'vance.return_cue','operation':'eventAfter','asset':'dialogue','argument':8960,'wait':'observed'}
  assert after['graphs']['opening']['steps'][28]['commands'].pop()=={'id':'vance.return_cue','binding':'vance.return_cue'}
  assert after['graphs']['opening']['receipts'].pop('vance.return_cue')=='vance.return_cue'
  assert before==after,'Unrelated mission definition changed'
  assert (OUT/'previous-gateway.json').read_bytes()==z.read('Sunrise/scripts/gateway.json')
 paths=set(manifest)|ADDED;frozen={name:verify.digest(ROOT/name) for name in sorted(paths)}
 print(f'Protected {protected} files; only the Gateway return cue changed.',flush=True)
 def suite(name):return [verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config) for config in ('Debug','Release')]
 results=[]
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
  for rows in pool.map(suite,('gateway_opening_tests','coo_universal_services_tests','other_mission_protocol_tests','omega_archive_protocol_tests')):results.extend(rows)
 for row in results:
  if row['project']=='omega_archive_protocol_tests':assert 'digest=A5BE474333FF4DDF' in row['output']
 print('Cue boundaries, authored timing, delayed dispatch, reset, ending and wire regressions passed. Building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in frozen.items()),'Source changed during build'
 (OUT/'candidate-source.json').write_text(json.dumps(frozen,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
  for name in frozen:z.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 (OUT/'review.json').write_text(json.dumps({'protectedFiles':protected,'changed':sorted(CHANGED),'added':sorted(ADDED),'cueMs':8960,'cueOrigin':'accepted native row5 submission','tests':len(results)-1,'sharedExecutorChanged':False,'freshIntegratedRunValidated':False},indent=2)+'\n')
 print('Validated Gateway return cue is ready to install.',flush=True)
if __name__=='__main__':main()
