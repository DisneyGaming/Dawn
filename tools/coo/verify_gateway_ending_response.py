"""Freeze the bounded Gateway beam/entry responsiveness fix without deployment."""
import json, zipfile
from pathlib import Path
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-ending-response'
BASE=ROOT/'build/coo/validation-gateway-ending-integration'
CHANGED={
 'Sunrise/scripts/gateway.json',
 'Sunrise/src/server/bap/encrypted/push/activity/activity_keepalive_push.cpp',
 *{'Sunrise/src/state/activity/gateway/'+n for n in ('controller.cpp','profile.h','runtime.cpp','runtime.h')},
 'Sunrise/unit/gateway_opening_tests.cpp','Sunrise/docs/GATEWAY-RECONSTRUCTION.md'
}
def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists()
 archive=BASE/'candidate-source.zip'
 assert verify.digest(archive)=='13ba7302685a484615b7b23ed3adac7b62b46573d2e703d518adc708a001c7c8'
 assert verify.digest(ROOT/'steam_api64.dll')=='eea6afd1846947fe55f46796e699bb675b061c4bf3aa07405e15efb540a57861'
 protected=0
 with zipfile.ZipFile(archive) as z:
  names=z.namelist()
  for name in names:
   if name not in CHANGED:
    assert (ROOT/name).read_bytes()==z.read(name),'Unrelated change: '+name
    protected+=1
  name='Sunrise/src/server/bap/encrypted/push/activity/activity_keepalive_push.cpp'
  before=z.read(name).decode().replace('\r\n','\n');after=(ROOT/name).read_text()
  after=after.replace('#include "../../../../../state/activity/gateway/runtime.h"\n','').replace('\n                || state::activity::gateway::publication_due(now)','')
  assert before==after,'Keepalive change exceeds the existing Gateway wake-up condition'
  old=json.loads(z.read('Sunrise/scripts/gateway.json'))
 current=json.loads((ROOT/'Sunrise/scripts/gateway.json').read_text())
 for field in ('presentation','bindings','assets','roles','entry','modules','observations'):
  assert old[field]==current[field],field
 for graph in ('composition','opening'):
  assert old['graphs'][graph]==current['graphs'][graph],graph
 a=old['graphs']['ending']['steps'];b=current['graphs']['ending']['steps']
 assert len(a)==len(b)==19 and a[:11]==b[:11] and a[15:]==b[15:]
 assert b[11]['commands'][0]['binding']=='dialogue.come_closer' and b[11]['after']==[b[10]['id']]
 assert b[12]['after']==[b[10]['id']] and b[13]['after']==[b[11]['id']]
 assert b[13]['commands'][0]['binding']=='dialogue.old_place' and b[14]['after']==[b[12]['id'],b[13]['id']]
 names+=['Sunrise/src/state/activity/gateway/ending_cadence.h','tools/coo/verify_gateway_ending_response.py']
 manifest={name:verify.digest(ROOT/name) for name in names}
 results=[]
 print(f'Protected {protected} accepted files; existing transport and opening verified.',flush=True)
 for configuration in ('Debug','Release'):
  for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',configuration))
 print('Regression checks passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==digest for name,digest in manifest.items()),'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
  for name in manifest:z.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 (OUT/'review.json').write_text(json.dumps({'protectedFiles':protected,'cadenceMs':100,'scope':'Gateway module exposure through completion only','timingEvidenceSha256':verify.digest(ROOT/'build/coo/gateway-ending-response-20260907/accepted-run-timing.log'),'nativeValidation':'pending fresh run'},indent=2)+'\n')
 print('Gateway ending response candidate passed and frozen.',flush=True)
if __name__=='__main__':main()
