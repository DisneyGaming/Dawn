"""Validate the Gateway display activation without changing spawn/damage code."""
import json,struct,zipfile
from pathlib import Path
import verify
from package_read import read
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-gateway-module-presentation'
CHANGED={'Sunrise/src/state/activity/gateway/authority.h','Sunrise/unit/gateway_opening_tests.cpp','Sunrise/docs/GATEWAY-RECONSTRUCTION.md'}
def main():
 verify.OUT=OUT;OUT.mkdir(parents=True,exist_ok=True)
 assert not (OUT/'installation.json').exists()
 archive=ROOT/'build/coo/validation-gateway-module/candidate-source.zip'
 assert verify.digest(archive)=='9ed15fc3ab0808a75f4fca849ebad486403ca07b32bd19fd2437b2000c0a4fd0'
 protected=0
 with zipfile.ZipFile(archive) as z:
  for name in z.namelist():
   if name not in CHANGED:
    assert (ROOT/name).read_bytes()==z.read(name),'Unrelated change: '+name
    protected+=1
 _,graph=read(0x80F48031)
 assert struct.unpack_from('<IIQ',graph,0x90)==(0x80F48031,0x808084E9,0x2888)
 assert struct.unpack_from('<I',graph,0x7020)[0]==0x6D408B83
 low=struct.unpack_from('<ff',graph,0x3808);positive=struct.unpack_from('<ff',graph,0x3898)
 active=struct.unpack_from('<ff',graph,0x2E78)
 assert low[0]<0<low[1] and not low[0]<=1<=low[1]
 assert positive[0]>0 and positive[0]<1<positive[1]
 assert active[0]<1<active[1] and active[0]>0
 (OUT/'presentation-native.json').write_text(json.dumps({'protectedFiles':protected,'graph':'80F48031','property':'device_position','inactiveRange':low,'positiveRange':positive,'activationRange':active,'visualValidation':'pending'},indent=2))
 paths=sorted(p for folder in ('Sunrise/src','Sunrise/unit','Sunrise/resources','Sunrise/scripts','Sunrise/vendor') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ('.obj','.exe','.pdb','.zip','.pyc'))
 paths += [ROOT/'Sunrise/Sunrise.vcxproj',ROOT/'Sunrise/docs/GATEWAY-RECONSTRUCTION.md',ROOT/'Sunrise/docs/gateway-reconstruction-map.json']
 paths += [p for p in (ROOT/'tools/coo').glob('*') if p.suffix in ('.py','.cpp','.vcxproj','.ps1')]
 manifest={p.relative_to(ROOT).as_posix():verify.digest(p) for p in paths};results=[]
 print(f'Protected {protected} accepted files; native position ranges verified.',flush=True)
 for config in ('Debug','Release'):
  for name in ('gateway_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests'):
   results.append(verify.build(ROOT/f'Sunrise/unit/{name}.vcxproj',config))
 print('Tests passed; building Release DLL.',flush=True)
 results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
 assert all(verify.digest(ROOT/name)==sha for name,sha in manifest.items()),'Source changed during validation'
 (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as z:
  for name in manifest:z.write(ROOT/name,name)
 (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Gateway module presentation candidate verified; native visual test pending.',flush=True)
if __name__=='__main__':main()
