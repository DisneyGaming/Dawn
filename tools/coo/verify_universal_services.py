"""Validate and freeze the shared mission services. Never deploy or launch a game."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile
import verify
ROOT=verify.ROOT
OUT=ROOT/'build/coo/validation-universal-services'
CHANGED={
    *{'Sunrise/src/state/activity/coo/'+n for n in ('executor.h','mission_script.cpp','mission_script.h','native_presentation_authority.h','population_service.h','script_views.h')},
    *{'Sunrise/src/state/activity/gateway/'+n for n in ('authority.h','controller.cpp','controller.h','ending_cadence.h','frame.h','profile.h','runtime.cpp','runtime.h')},
    'Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp',
    *{'Sunrise/src/middleware/bap/activity_message/'+n for n in ('activity_sensor_auth_bodies.cpp','activity_sensor_auth_bodies_other_missions.cpp','sensor_auth_update.h')},
    *{'Sunrise/src/client/hooks/bootflow/'+n for n in ('gateway_module_receipts.inl','omega_arc_charge_receipts.cpp','omega_enemy_lair_receipts.cpp')},
    'Sunrise/unit/gateway_opening_tests.cpp','Sunrise/scripts/gateway.json','Sunrise/scripts/FORMAT.md'
}
ADDED={
    *{'Sunrise/src/state/activity/coo/'+n for n in ('event_timeline.h','lifecycle_service.h','objective_service.h','object_service.h','scene_orchestration.h','stall_diagnostics.h')},
    'Sunrise/src/state/activity/gateway/service_bindings.h',
    *{'Sunrise/src/client/hooks/bootflow/'+n for n in ('coo_enemy_readiness.h','coo_native_components.h')},
    'Sunrise/unit/coo_universal_services_tests.cpp','Sunrise/unit/coo_universal_services_tests.vcxproj','Sunrise/unit/fixtures/mission_script_universal.json'
}
PROTECTED={
    'Sunrise/settings.json':'20c92fed04379070b297141155b7b02136eca1273b15c08f7547241c14ee4dd6',
    'launch-destiny.cmd':'8a75e1bf39d5bc5386a7d80a1887b3c4f7198a84f98035c8f6f24380803fcc05',
    'destiny2.exe':'81964380664e7fcee3c620085a157fdeaf91fefacf7214907820f188bbeb4ced',
    'Sunrise/scripts/omega.json':'39e161e9481a930ea76bdeb5305d2a377c5834b7bbec8dca7d1a05e921dbcfd1'
}
SUITES=('coo_universal_services_tests','gateway_opening_tests','coo_mission_script_tests','coo_script_tests','coo_shared_tests','coo_executor_tests','coo_opening_tests','coo_forest_tests','coo_forest_runtime_tests','coo_combat_tests','coo_combat_runtime_tests','coo_ending_tests','coo_ending_runtime_tests','omega_archive_encounter_tests','omega_archive_protocol_tests','omega_forest_roster_tests','activity_sense_update_parser_tests','other_mission_protocol_tests','omega_experiment_settings_tests')

def main():
    verify.OUT=OUT
    assert not (OUT/'installation.json').exists(), 'Preserve installed candidate evidence'
    before=json.loads((OUT/'before.json').read_text())
    changed=[]
    for name,sha in before.items():
        if verify.digest(ROOT/name)!=sha:
            assert name in CHANGED, 'Unrelated change: '+name
            changed.append(name)
    for name,sha in PROTECTED.items():
        assert verify.digest(ROOT/name)==sha, 'Protected file changed: '+name
    assert verify.digest(ROOT/'steam_api64.dll')=='80c423f17166dd15210386e0855c6a370529156432bd258bb5ac135f72e5b4a0'
    for directory in ('Sunrise/src','Sunrise/unit','Sunrise/scripts'):
        for path in (ROOT/directory).rglob('*'):
            if path.is_file() and path.suffix in ('.h','.cpp','.inl','.vcxproj','.json','.md'):
                name=path.relative_to(ROOT).as_posix()
                assert name in before or name in ADDED, 'Unexpected new source: '+name
    baseline=OUT/'baseline'
    with zipfile.ZipFile(OUT/'before.zip') as archive:
        for name,sha in before.items():
            data=archive.read(name)
            assert hashlib.sha256(data).hexdigest()==sha, 'Baseline changed: '+name
            if name.startswith('Sunrise/src/'):
                target=(baseline/name).resolve()
                assert target.is_relative_to(baseline.resolve())
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(data)
        old=json.loads(archive.read('Sunrise/scripts/gateway.json'))
        (OUT/'previous-gateway.json').write_bytes(archive.read('Sunrise/scripts/gateway.json'))
    current=json.loads((ROOT/'Sunrise/scripts/gateway.json').read_text())
    current['presentation'].pop('markers')
    for key in ('vance.ascent_cue','vance.ending_cue','mission.finish'):
        current['bindings'][key]=old['bindings'][key]
    assert current==old, 'Gateway migration changed encounter or dialogue scheduling'
    for name in ADDED:
        if '/coo/' in name and '/gateway/' not in name:
            source=(ROOT/name).read_text(encoding='utf-8')
            assert 'gateway::' not in source and 'namespace gateway' not in source, 'Mission identity leaked into service: '+name
    # Preserve all source inputs used by the compiler, including the existing vendor tree.
    names=set(before)|ADDED|{'Sunrise/Sunrise.vcxproj','tools/coo/verify_universal_services.py','Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md','Sunrise/docs/COO-EXECUTOR.md','Sunrise/docs/GATEWAY-RECONSTRUCTION.md'}
    for directory in ('Sunrise/vendor','Sunrise/resources'):
        names.update(p.relative_to(ROOT).as_posix() for p in (ROOT/directory).rglob('*') if p.is_file() and p.suffix not in ('.obj','.pdb','.exe','.zip','.pyc'))
    manifest={name:verify.digest(ROOT/name) for name in sorted(names)}
    print(f'Protected {len(before)-len(changed)} baseline files; {len(changed)} approved changes, {len(ADDED)} new implementation/test files.',flush=True)
    for name in ('generate_gateway_catalog.py','generate_gateway_ai.py','verify_gateway_ending_bindings.py'):
        result=subprocess.run([sys.executable,str(ROOT/'tools/coo'/name),'--check'],cwd=ROOT,capture_output=True,text=True,timeout=120)
        (OUT/(name+'.log')).write_text(result.stdout+result.stderr)
        assert result.returncode==0,result.stdout+result.stderr
        print(name+': passed',flush=True)
    def suite(name):
        rows=[]
        project=ROOT/f'Sunrise/unit/{name}.vcxproj'
        for config in ('Debug','Release'):
            if f'Include="{config}|x64"' not in project.read_text(): continue
            local=verify.build(project,config);rows.append(local)
            if name in ('omega_archive_protocol_tests','omega_archive_encounter_tests'):
                reference=verify.build(project,config,'before',baseline/'Sunrise/src');rows.append(reference)
                assert local['output']==reference['output'], 'Accepted Omega parity mismatch: '+name
        return rows
    results=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for rows in pool.map(suite,SUITES):
            results.extend(rows)
            (OUT/'progress.json').write_text(json.dumps(results,indent=2)+'\n')
    for config in ('Debug','Release'):
        results.append(verify.build(ROOT/'build/omega-osiris-hold-fix-20260906/omega_osiris_hold_tests.vcxproj',config))
    print('All regressions passed; building Release DLL.',flush=True)
    results.append(verify.build(ROOT/'Sunrise/Sunrise.vcxproj','Release'))
    assert all(verify.digest(ROOT/n)==h for n,h in manifest.items()), 'Source changed during validation'
    assert all(verify.digest(ROOT/n)==h for n,h in PROTECTED.items()), 'Protected file changed during validation'
    (OUT/'candidate-source.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(OUT/'candidate-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for name in manifest: archive.write(ROOT/name,name)
    (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    (OUT/'review.json').write_text(json.dumps({'changed':changed,'added':sorted(ADDED),'protectedFiles':len(before)-len(changed),'checks':len(results)-1,'freshIntegratedRunValidated':False,'actorLimitChanged':False,'scriptEnemyCountsChanged':False,'sourceArchiveSha256':verify.digest(OUT/'candidate-source.zip')},indent=2)+'\n')
    print(f'Validated and frozen {len(results)-1} regression runs plus the Release DLL.',flush=True)
if __name__=='__main__': main()
