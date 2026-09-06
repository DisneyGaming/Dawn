"""Build and run CoO/Omega parity checks; never deploy or launch Destiny."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/coo/validation-opening'
MSBUILD = Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe')
BASELINE = ROOT / 'build/coo/omega-baseline-20260906'

# Explicit boundary of the shared-service extraction; the latest accepted
# candidate protects every other source, including native hooks and codecs.
SHARED_HOST_FILES = {
    'Sunrise/src/state/activity/coo/omega_adapter.h',
    'Sunrise/src/state/activity/coo/omega_adapter.cpp',
    'Sunrise/src/state/activity/coo/omega_definition.h',
    'Sunrise/src/state/activity/omega_presentation_rules.h',
    'Sunrise/src/state/activity/coo/omega_opening.h',
    'Sunrise/src/state/activity/coo/omega_forest.h',
    'Sunrise/src/state/activity/coo/omega_forest_controller.h',
    'Sunrise/src/state/activity/coo/omega_reveal.h',
    'Sunrise/src/state/activity/coo/omega_ending_controller.h',
    'Sunrise/src/state/activity/omega_first_lair_encounter.h',
}


# Explicit boundary for named receipts, generic compilation and the Omega profile.
GENERIC_HOST_FILES = {'Sunrise/src/state/activity/' + name for name in (
    'coo/executor.h', 'coo/script_views.h', 'coo/omega_script.h', 'coo/omega_script.cpp',
    'coo/omega_adapter.cpp', 'coo/omega_opening.h', 'coo/omega_forest.h', 'coo/omega_reveal.h',
    'coo/omega_ending_definition.h', 'coo/omega_ending_controller.h',
    'coo/omega_combat_definition.h', 'omega_first_lair_encounter.h', 'omega_presentation_rules.h')}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def frozen_source():
    manifest = json.loads((BASELINE / 'manifest.json').read_text())
    archive = BASELINE / 'baseline.zip'
    assert digest(archive) == manifest['archiveSha256'], 'Baseline archive changed'
    target = OUT / 'baseline'
    with zipfile.ZipFile(archive) as source:
        for name, expected in manifest['files'].items():
            if not name.startswith(('Sunrise/src/', 'Sunrise/unit/')):
                continue
            data = source.read(name)
            assert hashlib.sha256(data).hexdigest() == expected, name
            path = (target / name).resolve()
            assert path.is_relative_to(target.resolve()), name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    # Both accepted fixes and all codec bodies must remain identical to the
    # corrected baseline. The adapter changes only the publication boundary.
    protected = [name for name in manifest['files'] if name.startswith(
        'Sunrise/src/middleware/bap/activity_message/activity_sensor_auth_')]
    protected += ['Sunrise/src/client/hooks/bootflow/omega_ikora_origin_probe.cpp',
                  'Sunrise/src/state/build_data/roster/build_data_roster_runtime.cpp']
    # Locate the registry lookup by its actual recorded path, not a guessed folder.
    protected = [name for name in protected if name in manifest['files']]
    protected += [name for name in manifest['files'] if name.endswith('/build_data_roster_runtime.cpp')]
    protected += ['Sunrise/src/server/bap/encrypted/activity_message/omega_roster_readiness.h',
                  'Sunrise/src/server/bap/encrypted/activity_message/omega_monitor_edges.h',
                  'Sunrise/src/state/activity/omega_ikora_lattice.h']
    for name in protected:
        assert digest(ROOT / name) == manifest['files'][name], f'Protected reference changed: {name}'
    # The replay oracle is the frozen production branch, with only logging and
    # the external quiescence read substituted. Reject accidental oracle edits.
    route_name = 'Sunrise/src/server/bap/encrypted/activity_message/activity_message_route.cpp'
    route = (target / route_name).read_text(encoding='utf-8')
    first = route.index('    if (observerReset) {', route.index('void report_sense_update('))
    last = route.index('    const bool latched =', first)
    policy = route[first:last].replace('core::log::write(', 'discard_log(').replace(
        'state::activity::omega_authority_quiesced()', 'quiesced')
    fixture = ROOT / 'Sunrise/unit/fixtures/coo_opening_legacy_policy.h'
    assert policy in fixture.read_text(encoding='utf-8'), 'Frozen opening oracle changed'
    # Each replay packet must still match a complete packet in the accepted log.
    accepted = ROOT / 'build/coo/accepted-adapter-20260906-121452/sunrise.log'
    captures = (ROOT / 'Sunrise/unit/fixtures/coo_opening_accepted_capture.h').read_text(encoding='utf-8')
    assert digest(accepted) in captures, 'Accepted replay log changed'
    lines = accepted.read_text(encoding='utf-8').splitlines()
    records = re.findall(r'\{(\d+), (\d+), (\d+), "([0-9A-F]+)"\}', captures)
    assert len(records) == 40
    for line, tick, packet, payload in records:
        fields = dict(re.findall(r'(\w+)=([^ ]+)', lines[int(line)-1]))
        assert fields['t'] == tick and fields['packet'] == packet and fields['hex'] == payload
        assert int(fields['bytes']) == int(fields['capture_bytes']) == len(payload)//2
    # Native receipts are consumed before a region-debt snapshot can copy the
    # session. Snapshot construction itself cannot consume the queue.
    keeper = (ROOT / 'Sunrise/src/server/bap/encrypted/push/activity/activity_keepalive_push.cpp').read_text()
    keeper = keeper[keeper.index('bool consume_activity_keepalive('):]
    assert keeper.index('opening::update_observer') < keeper.index('if (session.activity.regionDebt.present)')
    assert 'update_observer' not in (ROOT / 'Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp').read_text()
    return target / 'Sunrise/src'

def frozen_forest():
    # Last candidate accepted through Ikora -> Forest. The oracle is immutable;
    # native generation, enemy/gate/navigation hooks and roster admission remain protected.
    base = ROOT / 'build/coo/validation-opening-loadfix'
    archive = base / 'candidate-source.zip'
    assert digest(archive) == '8119298484a6eb763940d66de9b1b86cb78979356c9cfcd9386d78e086fe1a1c'
    with zipfile.ZipFile(archive) as source:
        name = 'Sunrise/src/state/activity/omega_presentation_rules.h'
        text = source.read(name).decode().replace('\r\n', '\n')
        text = text.replace('#include "omega_intro_rules.h"', '#include "state/activity/omega_intro_rules.h"')
        text = text.replace('#include "omega_first_lair_encounter.h"', '#include "state/activity/omega_first_lair_encounter.h"')
        text = text.replace('namespace sunrise::state::activity::omega_presentation {',
            'namespace sunrise::state::activity::frozen_presentation {\nusing namespace sunrise::state::activity::omega_presentation;')
        expected = '// Frozen accepted opening candidate; only includes/namespace adapted for the oracle.\n' + text
        assert (ROOT / 'Sunrise/unit/fixtures/coo_forest_legacy_presentation.h').read_text() == expected
        protected = ['Sunrise/src/client/hooks/bootflow/' + name for name in (
            'omega_forest_recipe.h', 'omega_navigation.cpp', 'omega_navigation_rules.h',
            'omega_enemy_forest_receipts_runtime.h', 'omega_dialogue_dispatch_probe.cpp')]
        protected += ['Sunrise/src/state/activity/' + name for name in (
            'omega_presentation_volumes.h', 'omega_intro_rules.h')]
        protected += ['Sunrise/src/server/bap/encrypted/push/activity/' + name for name in (
            'activity_roster_snapshot.cpp', 'activity_keepalive_push.cpp')]
        for name in protected:
            if name not in (SHARED_HOST_FILES | GENERIC_HOST_FILES):
                assert (ROOT / name).read_bytes() == source.read(name), f'Accepted Forest dependency changed: {name}'
    runtime = (ROOT / 'Sunrise/src/state/activity/omega_presentation.cpp').read_text()
    # Only the normal owner updates drain receipts; no native observer calls an
    # adapter/executor update or acquires the adapter's lock under presentation's lock.
    assert runtime.count('drain_locked();') == 2
    callbacks = runtime[runtime.index('void observe_position('):runtime.index('Navigation navigation()')]
    assert 'coo::omega::' not in callbacks and 'drain_locked' not in callbacks
    assert 'executor_.update' not in callbacks and 'g_run.enter' not in callbacks


def frozen_combat():
    archive = ROOT / 'build/coo/validation-forest/candidate-source.zip'
    assert digest(archive) == 'ede6909e9d5a4d7a81eac0a05b1e3c8d057823e8c0f2bcc1762d8d490c804a3b'
    allowed = {'Sunrise/src/state/activity/' + name for name in (
        'coo/omega_adapter.h', 'coo/omega_adapter.cpp', 'coo/omega_forest_controller.h',
        'omega_first_lair_encounter.h', 'omega_first_lair_runtime.cpp', 'omega_first_lair_runtime.h',
        'omega_presentation_rules.h', 'omega_ending.cpp', 'omega_ending.h')}
    allowed.add('Sunrise/src/client/hooks/bootflow/omega_activity_handoff.inl')
    with zipfile.ZipFile(archive) as source:
        # Preserve every accepted native hook, catalog, codec and unrelated
        # mission path. Only the explicit host sequencing boundary may change.
        for name in source.namelist():
            if name.startswith('Sunrise/src/') and name not in allowed and name not in (SHARED_HOST_FILES | GENERIC_HOST_FILES):
                assert (ROOT / name).read_bytes() == source.read(name), f'Protected combat dependency changed: {name}'
        original = source.read('Sunrise/src/state/activity/omega_first_lair_encounter.h').decode().replace('\r\n','\n')
        for dep in ('omega_enemy_chase_catalog.h', 'omega_enemy_crown_catalog.h',
                    'omega_enemy_crown_waves.h', 'omega_rescue_scene_authority.h'):
            original = original.replace('"'+dep+'"', '"state/activity/'+dep+'"')
        original = original.replace('namespace sunrise::state::activity::omega_first_lair {',
                                    'namespace sunrise::state::activity::frozen_combat {')
        expected = '// Frozen accepted Forest candidate; only includes/namespace adapted.\n' + original
        assert (ROOT / 'Sunrise/unit/fixtures/coo_combat_legacy.h').read_text() == expected
        name = 'Sunrise/src/state/activity/omega_first_lair_runtime.cpp'
        old = source.read(name).decode().replace('\r\n','\n')
        new = (ROOT / name).read_text()
        def projection(text):
            return text[text.index('            output.generation=generation;'):text.index('            g_published=g_revision;')]
        assert projection(old) == projection(new), 'Accepted combat authority projection changed'


def frozen_ending():
    archive = ROOT / 'build/coo/validation-lair-crown/candidate-source.zip'
    assert digest(archive) == '89828c36b338790208ec7c3b5ae9dd95446f8a0924125f69ba77c4b1e8fabf8f'
    allowed = {'Sunrise/src/state/activity/' + name for name in (
        'coo/omega_adapter.h', 'coo/omega_adapter.cpp', 'omega_ending.cpp', 'omega_ending.h')}
    handoff = 'Sunrise/src/client/hooks/bootflow/omega_activity_handoff.inl'
    allowed.add(handoff)
    with zipfile.ZipFile(archive) as source:
        for name in source.namelist():
            if name.startswith('Sunrise/src/') and name not in allowed and name not in (SHARED_HOST_FILES | GENERIC_HOST_FILES):
                assert (ROOT / name).read_bytes() == source.read(name), f'Accepted Lair/Crown dependency changed: {name}'
        original = source.read('Sunrise/src/state/activity/omega_ending_rules.h').decode().replace('\r\n','\n')
        original = original.replace('namespace sunrise::state::activity::omega_ending {',
                                    'namespace sunrise::state::activity::frozen_ending {')
        assert (ROOT / 'Sunrise/unit/fixtures/coo_ending_legacy.h').read_text() == '// Frozen accepted Lair/Crown candidate; only namespace adapted.\n' + original
        # Preserve native launch construction, nonce, guards, cleanup and loading
        # behavior. Only three explicit owner drains were added to this frame path.
        original = source.read(handoff).decode().replace('\r\n','\n')
        original = original.replace('inline void poll() noexcept {\n', 'inline void poll() noexcept {\n    ending::update();\n')
        original = original.replace('ending::note_handoff_result(token,false);report', 'ending::note_handoff_result(token,false);ending::update();report')
        original = original.replace('ending::note_handoff_result(token,true);', 'ending::note_handoff_result(token,true);\n    ending::update();')
        assert (ROOT / handoff).read_text() == original, 'Native handoff behavior changed beyond owner dispatch'



def frozen_shared():
    archive = ROOT / 'build/coo/validation-ending/candidate-source.zip'
    assert digest(archive) == 'a5f35172716b323e93c574c013230acf2bed4d9f38b1d6114e9bc57108fc519e'
    with zipfile.ZipFile(archive) as source:
        for name in source.namelist():
            if name.startswith('Sunrise/src/') and name not in (SHARED_HOST_FILES | GENERIC_HOST_FILES):
                assert (ROOT / name).read_bytes() == source.read(name), f'Accepted native dependency changed: {name}'
        original = source.read('Sunrise/src/state/activity/omega_presentation_rules.h').decode().replace('\r\n','\n')
        for dependency in ('omega_intro_rules.h', 'coo/omega_reveal.h', 'omega_first_lair_encounter.h'):
            original = original.replace('"'+dependency+'"', '"state/activity/'+dependency+'"')
        original = original.replace('namespace sunrise::state::activity::omega_presentation {',
            'namespace sunrise::state::activity::frozen_shared_presentation {\nusing namespace sunrise::state::activity::omega_presentation;')
        assert (ROOT / 'Sunrise/unit/fixtures/coo_shared_legacy_presentation.h').read_text() == '// Frozen accepted ending candidate; only includes/namespace adapted.\n' + original
        adapter = source.read('Sunrise/src/state/activity/coo/omega_adapter.h').decode().replace('\r\n','\n')
        definition = source.read('Sunrise/src/state/activity/coo/omega_definition.h').decode().replace('\r\n','\n')
        definition = definition[definition.index('// First migration stage:'):definition.index('} // namespace')]
        adapter = adapter[adapter.index('struct Input final'):adapter.index('// Admitted local')]
        expected = '// Frozen accepted ending candidate; original definition and adapter bodies.\n#pragma once\n#include "state/activity/coo/omega_adapter.h"\nnamespace sunrise::state::activity::coo::frozen_composition {\n' + definition + adapter + '\n}\n'
        assert (ROOT / 'Sunrise/unit/fixtures/coo_shared_legacy_composition.h').read_text() == expected
    for name in ('mission_runtime.h', 'native_services.h', 'receipt_queue.h', 'dialogue_service.h', 'presentation_cues.h', 'scene_service.h', 'population_service.h'):
        shared = (ROOT / 'Sunrise/src/state/activity/coo' / name).read_text()
        assert 'omega_' not in shared and 'Schema::omegaArchive' not in shared, f'Omega dependency in shared service: {name}'


def frozen_script():
    archive = ROOT / 'build/coo/validation-shared/candidate-source.zip'
    assert digest(archive) == 'f44c69d87d57f8593972b55cf3eaf1ab87095d5e1d483f02332dd2cb61d68b62'
    allowed = {'Sunrise/src/state/activity/' + name for name in (
        'coo/omega_adapter.cpp', 'coo/omega_opening.h', 'coo/omega_forest.h',
        'coo/omega_reveal.h', 'coo/omega_ending_controller.h',
        'omega_first_lair_encounter.h', 'omega_presentation_rules.h')}
    with zipfile.ZipFile(archive) as source:
        for name in source.namelist():
            if name.startswith('Sunrise/src/') and name not in (allowed | GENERIC_HOST_FILES):
                assert (ROOT / name).read_bytes() == source.read(name), f'Accepted script dependency changed: {name}'
    # Session values remain allocation-free; the only owning script document is
    # held by the once-only loader, never by a copied native session/controller.
    for name in allowed - {'Sunrise/src/state/activity/coo/omega_adapter.cpp'}:
        text = (ROOT / name).read_text()
        assert 'unique_ptr' not in text and 'std::vector' not in text, name



def frozen_generic():
    archive = ROOT / 'build/coo/validation-scripts/candidate-source.zip'
    assert digest(archive) == '59bfdd59700104158d5783cf24e3d65d525fcd159910d5badbda30fccc2159d2'
    with zipfile.ZipFile(archive) as source:
        for name in source.namelist():
            if name.startswith('Sunrise/src/') and name not in GENERIC_HOST_FILES:
                assert (ROOT / name).read_bytes() == source.read(name), f'Accepted JSON dependency changed: {name}'
    for name in ('mission_script.h', 'mission_script.cpp', 'script_views.h'):
        text = (ROOT / 'Sunrise/src/state/activity/coo' / name).read_text()
        assert 'omega' not in text.lower(), f'Mission-specific dependency in generic loader: {name}'
    for name in ('omega_opening.h', 'omega_forest.h', 'omega_reveal.h', 'omega_ending_controller.h'):
        text = (ROOT / 'Sunrise/src/state/activity/coo' / name).read_text()
        assert not re.search(r'executor_\.token\([^)]*,', text), f'Positional native receipt remains: {name}'
    project = (ROOT / 'Sunrise/unit/coo_mission_script_tests.vcxproj').read_text()
    assert 'omega_script.cpp' not in project and 'coo_script_bootstrap' not in project, 'Independent loader test links Omega'


def build(project, configuration, variant='local', source=None):
    name = project.stem
    directory = OUT / f'{name}-{variant}' / configuration
    directory.mkdir(parents=True, exist_ok=True)
    log = directory / 'build.log'
    args = [str(MSBUILD), str(project), '/nologo', '/m:2', '/v:minimal',
            f'/p:Configuration={configuration}', '/p:Platform=x64', '/p:CL_MPCount=4',
            f'/p:OutDir={directory}\\', f'/p:IntDir={directory / "obj"}\\']
    if source:
        args += [f'/p:OmegaSourceRoot={source}']
    with log.open('w') as output:
        result = subprocess.run(args, cwd=ROOT,
            env={k.upper(): v for k, v in os.environ.items()}, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(log.read_text(errors='replace'))
    warnings = re.findall(r'^.*: warning .*$', log.read_text(errors='replace'), re.MULTILINE)
    if warnings:
        raise RuntimeError('\n'.join(warnings))
    if name == 'Sunrise':
        return {'project': name, 'configuration': configuration, 'variant': variant,
                'dll': str(directory / 'steam_api64.dll'), 'sha256': digest(directory / 'steam_api64.dll')}
    binary = directory / f'{name}.exe'
    result = subprocess.run([str(binary)], cwd=ROOT, text=True, capture_output=True, timeout=60)
    (directory / 'test.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{binary}\n{result.stdout}\n{result.stderr}')
    if name == 'coo_script_tests':
        rejected = subprocess.run([str(binary), '--invalid-admission'], cwd=ROOT, text=True, capture_output=True, timeout=60)
        (directory / 'invalid-admission.log').write_text(rejected.stdout + rejected.stderr)
        assert rejected.returncode == 0, rejected.stdout + rejected.stderr
    print(f'{name} {configuration} {variant}: {result.stdout.strip()}', flush=True)
    return {'project': name, 'configuration': configuration, 'variant': variant,
            'output': result.stdout, 'binarySha256': digest(binary)}

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=OUT)
    options = parser.parse_args()
    OUT = options.out.resolve()
    assert OUT.is_relative_to((ROOT / 'build/coo').resolve()), 'Output must stay under build/coo'
    assert not (OUT / 'installation.json').exists(), 'Preserve installed candidate evidence; choose a new output directory'
    OUT.mkdir(parents=True, exist_ok=True)
    source = frozen_source()
    frozen_forest()
    frozen_combat()
    frozen_ending()
    frozen_shared()
    frozen_script()
    frozen_generic()
    results = []
    for configuration in ('Debug', 'Release'):
        for name in ('coo_mission_script_tests', 'coo_script_tests', 'coo_shared_tests', 'coo_executor_tests', 'coo_opening_tests', 'coo_forest_tests', 'coo_forest_runtime_tests', 'coo_combat_tests', 'coo_combat_runtime_tests', 'coo_ending_tests', 'coo_ending_runtime_tests', 'omega_archive_encounter_tests', 'omega_archive_protocol_tests',
                     'omega_forest_roster_tests', 'activity_sense_update_parser_tests',
                     'other_mission_protocol_tests', 'omega_experiment_settings_tests'):
            project = ROOT / f'Sunrise/unit/{name}.vcxproj'
            if f'Include="{configuration}|x64"' not in project.read_text():
                print(f'{name}: no {configuration} configuration in existing project', flush=True)
                continue
            local = build(project, configuration)
            results.append(local)
            if name in ('omega_archive_encounter_tests', 'omega_archive_protocol_tests'):
                reference = build(project, configuration, 'baseline', source)
                assert local['output'] == reference['output'], f'Baseline mismatch: {name}'
                results.append(reference)
        results.append(build(ROOT / 'build/omega-osiris-hold-fix-20260906/omega_osiris_hold_tests.vcxproj', configuration))
    source_files = sorted(p for p in (ROOT / 'Sunrise/src').rglob('*') if p.is_file())
    source_files += [p for p in (ROOT / 'Sunrise/vendor').rglob('*') if p.is_file()
                     and p.suffix not in ('.obj', '.pdb', '.exe', '.zip', '.pyc')]
    source_files += [ROOT / 'Sunrise/Sunrise.vcxproj']
    source_files += sorted(p for p in (ROOT / 'Sunrise/scripts').rglob('*') if p.is_file())
    source_files += sorted(p for p in (ROOT / 'Sunrise/unit').rglob('*')
                           if p.is_file() and p.suffix in ('.h', '.cpp', '.vcxproj', '.json'))
    source_files += sorted(p for p in (ROOT / 'tools/coo').glob('*') if p.suffix in ('.py', '.ps1'))
    source_files += sorted(p for p in (ROOT / 'Sunrise/resources').rglob('*') if p.is_file())
    source_manifest = {p.relative_to(ROOT).as_posix(): digest(p) for p in source_files}
    results.append(build(ROOT / 'Sunrise/Sunrise.vcxproj', 'Release'))
    assert all(digest(ROOT / name) == sha for name, sha in source_manifest.items()), 'Source changed during DLL build'
    (OUT / 'candidate-source.json').write_text(json.dumps(source_manifest, indent=2) + '\n')
    with zipfile.ZipFile(OUT / 'candidate-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in source_manifest:
            archive.write(ROOT / name, name)
    (OUT / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'All checks passed; DLL staged under {OUT / "Sunrise-local/Release"}.', flush=True)

if __name__ == '__main__':
    main()
