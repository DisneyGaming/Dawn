import sys,json
from pathlib import Path
sys.path.insert(0,'tools/coo')
import verify
verify.OUT=Path('build/coo/validation-deadly-trial-candidate-20260907').resolve()
assert not (verify.OUT/'installation.json').exists(),'Preserve installation evidence; choose a new validation directory.'
results=[]
projects=['deadly_trial_tests','gateway_opening_tests','coo_universal_services_tests','coo_executor_tests','coo_mission_script_tests','coo_shared_tests','coo_combat_tests','coo_ending_tests','coo_forest_tests','coo_opening_tests','other_mission_protocol_tests','omega_archive_protocol_tests']
try:
 for name in projects:
  results.append(verify.build(Path('Sunrise/unit/'+name+'.vcxproj').resolve(),'Release'))
 results.append(verify.build(Path('Sunrise/unit/deadly_trial_tests.vcxproj').resolve(),'Debug'))
 (verify.OUT/'tests.json').write_text(json.dumps(results,indent=2)+'\n')
 result=verify.build(Path('Sunrise/Sunrise.vcxproj').resolve(),'Release')
 (verify.OUT/'candidate-build.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
except Exception as error:print(str(error));raise SystemExit(1)
