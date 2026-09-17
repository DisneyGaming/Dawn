from pathlib import Path
import difflib
import hashlib
import json
import re
import zipfile
from omega_osiris_hold_patch import apply as align_second_osiris_hold

BASE = Path(__file__).resolve().parent
DEST = BASE.parent.parent / 'Dawn'
REF = BASE / 'src'
BACKUP = zipfile.ZipFile(BASE / 'before-port.zip')
outputs = {}

def old(path):
    return BACKUP.read('src/' + path).decode('utf-8-sig').replace('\r\n', '\n')

def reference(path):
    return (REF / path).read_text(encoding='utf-8-sig')

def put(path, value):
    outputs[path] = value

def replace(path, before, after):
    text = outputs.get(path, reference(path))
    if text.count(before) != 1:
        raise ValueError((path, 'replacement is not unique', before[:100], text.count(before)))
    put(path, text.replace(before, after))

def merge_ranges(path, ranges):
    """Keep reviewed, one-based original regions; take the reference elsewhere."""
    a, b = old(path).splitlines(True), reference(path).splitlines(True)
    result = []
    for tag, i, j, k, l in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        keep = tag != 'equal' and j > i and any(i < end and j >= start for start, end in ranges)
        result.extend(a[i:j] if keep else b[k:l])
    put(path, ''.join(result))

def additive(path):
    """Preserve reviewed independent local additions while adding reference changes."""
    a, b = old(path).splitlines(True), reference(path).splitlines(True)
    result = []
    for tag, i, j, k, l in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        result.extend(a[i:j] if tag == 'delete' else b[k:l])
    put(path, ''.join(result))

# These differences contain other-mission features, diagnostics, or extra catalog
# metadata. Omega's new schema catalog is integrated separately below.
KEEP = '''
client/content/scenarios/internal.h
client/content/scenarios/scenario_build.cpp
client/content/scenarios/scenario_roster_build.cpp
client/content/scenarios/scenario_roster_publish.cpp
client/content/scenarios/scenario_slot_classification.cpp
client/hooking/detour.cpp
client/hooking/detour.h
client/hooks/bootflow/activity_spawner_chain_probe.cpp
client/hooks/bootflow/prologue_filler_ready.cpp
client/hooks/bootflow/spawn_hold.cpp
client/hooks/bootflow/spawn_hold_policy.h
client/hooks/retail_log/retail_log_enqueue_observer.cpp
client/hooks/retail_log/retail_log_enqueue_observer.h
state/activity/runtime.h
state/activity/activity_world_arrival.cpp
state/activity/forced/definition.h
state/build_data/build_data_runtime.cpp
state/build_data/runtime/build_data_roster_runtime.cpp
state/build_data/cache/records/cache_scenario_records.cpp
state/build_data/cache/records/format.h
state/build_data/scenarios/definition.h
state/build_data/scenarios/scenario_catalog.cpp
state/build_data/scenarios/scenario_catalog.h
middleware/content/packages/tables/scenario_reader.cpp
middleware/content/packages/tables/scenario_reader.h
middleware/content/packages/tables/slot_descriptor_reader.cpp
middleware/content/packages/tables/slot_descriptor_reader.h
client/hooks/bootflow/omega_directive_presentation.cpp
server/bap/encrypted/push/activity/activity_roster_push.cpp
'''.split()
for path in KEEP:
    put(path, old(path))

# Preserve Towerfall's lifecycle owner while installing every archive Omega hook.
merge_ranges('client/hooks/bootflow/internal.h', [(335, 397)])
merge_ranges('client/hooks/bootflow/bootflow_hook_lifecycle.cpp', [(220, 224), (279, 335)])
p = 'client/hooks/bootflow/bootflow_hook_lifecycle.cpp'
replace(p, '    const bool fade = install_fade_release();',
        '    const bool towerfallExecutor = install_towerfall_executor_bootstrap();\n    const bool fade = install_fade_release();')
replace(p, '|| worldStep || spawn || fade;', '|| worldStep || spawn || towerfallExecutor || fade;')
replace(p, '&& worldStep && spawn && fade;', '&& worldStep && spawn && towerfallExecutor && fade;')
replace(p, '    quiesce_towerfall_executor_bootstrap();',
        '    quiesce_towerfall_executor_bootstrap();\n' +
        reference(p).split('    quiesce_spawn_hold();\n', 1)[1].split('    quiesce_type31_objective_capture();', 1)[0])

# The setup/world callbacks in this file are Towerfall-specific.
put('client/hooks/bootflow/activity_script_upstream_probe.cpp', old('client/hooks/bootflow/activity_script_upstream_probe.cpp'))

merge_ranges('state/activity/forced/activity_forced_destination.h', [(64, 79)])
additive('state/activity/forced/activity_forced_destination.cpp')
# Sequence matching can combine the identical ready-latch tail of two functions.
merge_ranges('state/activity/forced/activity_forced_destination.cpp', [(5, 9), (166, 180), (258, 280)])

additive('state/build_data/runtime/build_data_catalog_runtime.cpp')
# The local catalog places the Forest generator outside the archive's index window.
# Its existing key lookup publishes the same group/mask and survives catalog reorderings.
p = 'state/build_data/runtime/build_data_catalog_runtime.cpp'
text = outputs[p]
start = text.index('/**\n * Publishes the Infinite Forest map-generator group')
end = text.index('/** Publishes the global participation group', start)
put(p, text[:start] + text[end:])
additive('server/ui/activity_override/activity_override_panel.cpp')
p = 'server/bap/encrypted/push/activity/activity_keepalive_push.cpp'
text = reference(p)
previous = old(p)
start = previous.index('    const ActivitySensorObservation& cues =')
end = previous.index('    const bool burstDue', start)
text = text.replace('    const bool burstDue', previous[start:end] + '    const bool burstDue', 1)
text = text.replace('|| omegaOpeningDue)', '|| omegaOpeningDue || towerWatchDue)')
text = text.replace('#include "activity_keepalive_push.h"', '#include "activity_keepalive_push.h"\n#include "../../../../../state/activity/runtime.h"')
put(p, text)
put('client/content/scenarios/scenario_roster_groups.cpp', old('client/content/scenarios/scenario_roster_groups.cpp'))
p = 'client/content/scenarios/scenario_roster_groups.cpp'
replace(p, '#include "internal.h"', '#include "../../../state/build_data/scenarios/omega_schema_catalog.h"\n#include "internal.h"')
snippet = reference(p).split('    candidate.objectTag = objectTag;\n', 1)[1].split('    for (std::size_t index = 0; index < storage.groupCount;', 1)[0]
replace(p, '    candidate.objectTag = objectTag;\n', '    candidate.objectTag = objectTag;\n' + snippet)
replace(p, '&& (memo.authoredRootCueMask & 0x04U) != 0;',
        '&& context.scenarioTag != 0x80F47522U\n        && (memo.authoredRootCueMask & 0x04U) != 0;')
put('state/build_data/cache/records/version.h', old('state/build_data/cache/records/version.h').replace('kCacheFormatVersion = 51', 'kCacheFormatVersion = 52'))

# Keep the original protocol for other missions. The archive codecs have separate
# entry points, with one explicit destination flag selected by the roster owner.
p = 'middleware/bap/activity_message/sensor_auth_update.h'
merge_ranges(p, [(55, 55), (147, 156), (161, 172), (177, 181), (188, 209)])
text = outputs[p]
text = text.replace('#include "../../../state/activity/omega_crown_respawn_authority.h"',
    '#include "../../../state/activity/omega/omega_mission_state.h"\n#include "../../../state/activity/omega_crown_respawn_authority.h"')
text = text.replace('    bool omegaEndingSelected{}, omegaEndingPlay{};', '    bool omegaEndingSelected{};')
for declaration in ['bool omegaForestVexEncounters{};', 'std::uint32_t omegaBossGeneration{};',
                    'std::uint32_t omegaEndingRevision{};', 'bool omegaPortalEntry{};', 'bool omegaPortalPlayerHash{};']:
    first = text.find('    ' + declaration)
    if first >= 0:
        text = text[:first + len('    ' + declaration)] + text[first + len('    ' + declaration):].replace('    ' + declaration + '\n', '')
text = text.replace('struct Snapshot {', 'struct Snapshot {\n    /** Selects the archive protocol only for mission_scot. */\n    bool archiveOmega{};')
if 'bool archiveOmega' not in text:
    text = text.replace('struct Snapshot final {', 'struct Snapshot final {\n    /** Selects the archive protocol only for mission_scot. */\n    bool archiveOmega{};')
assert 'bool archiveOmega' in text
functions = ['pad_bits', 'write_bubble_block', 'write_roster_delta', 'write_object_block', 'auth_body_bits', 'write_auth_body', 'encode_sensor_auth_update']
# Duplicate the public codec declarations with explicit other-mission names.
tail = text[text.index('[[nodiscard]] bool pad_bits'):] if '[[nodiscard]] bool pad_bits' in text else text[text.index('bool pad_bits'):]
tail = tail[:tail.rfind('} // namespace')]
tail += '\n' + re.search(r'\[\[nodiscard\]\] bool encode_sensor_auth_update\([\s\S]*?noexcept;', text).group(0) + '\n'
for name in functions:
    tail = re.sub(r'\b' + name + r'\b', 'legacy_' + name, tail)
text = text.replace('} // namespace dawn::middleware::bap::activity_message::sensor_auth_update',
    '\n// Existing codecs remain isolated from Omega archive serialization.\n' + tail +
    '\n} // namespace dawn::middleware::bap::activity_message::sensor_auth_update')
put(p, text)

for function, arguments in [
    ('auth_body_bits(const Snapshot& snapshot', 'snapshot, key, slotType, slotIndex, carriesPlayerKey'),
    ('write_auth_body(bits::Writer& writer', 'writer, snapshot, key, slotType, slotIndex, carriesPlayerKey')]:
    p = 'middleware/bap/activity_message/activity_sensor_auth_bodies.cpp'
    text = outputs.get(p, reference(p))
    start = text.index(function)
    body = text.index('{', start)
    name = function.split('(')[0]
    text = text[:body+1] + f'\n    if (!snapshot.archiveOmega) {{ return legacy_{name}({arguments}); }}\n' + text[body+1:]
    put(p, text)
for stem in ['activity_sensor_auth_bodies', 'activity_sensor_auth_blocks', 'activity_sensor_auth_encoder']:
    p = 'middleware/bap/activity_message/' + stem + '.cpp'
    legacy = old(p)
    for name in functions:
        legacy = re.sub(r'\b' + name + r'\b', 'legacy_' + name, legacy)
    put('middleware/bap/activity_message/' + stem + '_other_missions.cpp', legacy)
# The new encoder is unchanged except for its entry dispatch and original bound.
p = 'middleware/bap/activity_message/activity_sensor_auth_encoder.cpp'
text = reference(p)
marker = 'bool encode_sensor_auth_update('
start = text.index(marker)
body = text.index('{', start)
text = text[:body+1] + '\n    if (!snapshot.archiveOmega) { return legacy_encode_sensor_auth_update(snapshot, output, written); }\n' + text[body+1:]
text = text.replace('snapshot.roster.groupCount > kGroupCapacity', 'snapshot.roster.groupCount > 15U')
put(p, text)

# Shared decoded storage is a superset. No non-Omega caller is switched to the
# archive's deliberately narrower parser.
p = 'middleware/bap/activity_message/sense_update.h'
merge_ranges(p, [(9, 9), (19, 22), (63, 77)])
text = outputs[p].replace('    /** Remaining MSB-first chunks;', '    std::uint64_t bodyFourth{};\n    /** Remaining MSB-first chunks;')
text = text.replace('    bool inferredBodyWidth{};', '    bool inferredBodyWidth{};\n    bool hasDelta{};')
decl = text[text.index('[[nodiscard]] bool parse_sense_update('):text.index('} // namespace')]
text = text.replace('} // namespace', decl.replace('bool parse_sense_update(', 'bool parse_omega_sense_update(') + '\n} // namespace', 1)
put(p, text)
p = 'middleware/bap/activity_message/activity_sense_update_parser.cpp'
put('middleware/bap/activity_message/activity_sense_update_parser_other_missions.cpp', old(p).replace('kCapturedBodyBitCapacity', '256U'))
text = reference(p).replace('bool parse_sense_update(', 'bool parse_omega_sense_update(')
text = text.replace('kSenseGroupCapacity', '16U').replace('kSenseObjectCapacity', '64U')
put(p, text)

# Shared session fields required by the existing Tower Watch interpreter.
merge_ranges('server/bap/internal.h', [(78, 82), (92, 116)])
replace('server/bap/internal.h', 'struct ActivitySensorObservation {',
        'struct ActivitySensorObservation {\n    state::activity::omega_ikora_lattice::State omegaIkoraLattice{};')
merge_ranges('server/bap/encrypted/push/activity/activity_roster_snapshot.cpp', [(8, 16), (44, 44), (66, 68), (104, 130), (1031, 1140)])
p = 'server/bap/encrypted/push/activity/activity_roster_snapshot.cpp'
text = outputs[p]
text = text.replace('#include "../../../../../client/hooks/bootflow/omega_reveal_native.h"\n', '')
text = text.replace('#include "../../../../../client/player/player_position.h"\n', '')
text = text.replace('#include "omega_lair_roster.h"', '#include "omega_lair_roster.h"\n#include "../../../../../middleware/bap/activity_message/tower_watch_cue_manifest.h"')
# Establish this before serialization regardless of the experiment switches.
anchor = '    snapshot.omegaSceneAuthority ='
idx = text.index(anchor)
text = text[:idx] + '    snapshot.archiveOmega = name == "mission_scot";\n' + text[idx:]
put(p, text)

# The route uses reference Omega readiness/monitor handling and retains the
# independent Tower Watch interpreter and observation mapping.
merge_ranges('server/bap/encrypted/activity_message/activity_message_route.cpp', [(25, 25), (29, 29), (43, 43), (162, 342), (445, 581), (607, 619), (621, 622), (923, 929)])
p = 'server/bap/encrypted/activity_message/activity_message_route.cpp'
text = outputs[p]
obsolete = text.index('/** A native monitor edge can share a valid packet with source/Scene changes. */')
end_obsolete = text.index('[[nodiscard]] bool exact_omega_opening(', obsolete)
text = text[:obsolete] + text[end_obsolete:]
text = text.replace('using omega_ack::exact_body;\nusing omega_ack::exact_omega_initial_report;\n', '')
start = text.index('void report_sense_entries(')
end = text.index('\n}\n', start) + 3
original = reference(p)
ref_start = original.index('void report_sense_entries(')
ref_end = original.index('\n}\n', ref_start) + 3
text = text[:start] + original[ref_start:ref_end] + text[end:]
text = text.replace('service::sense_update::parse_sense_update(',
    '(omega_destination(session.activity.instance) ? service::sense_update::parse_omega_sense_update : service::sense_update::parse_sense_update)(')
put(p, text)

# Dialogue also owns a separate Tower Watch observation path. Restore only its
# original declarations, functions, calls and counters, not the old Omega logic.
merge_ranges('client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp',
    [(589, 601), (613, 874), (1303, 1312), (1339, 1345), (1358, 1390), (1734, 1768)])
p = 'client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp'
replace(p, 'namespace {\n', 'namespace {\nstd::array<std::atomic_uint64_t, 5U> g_towerWatchSlotFingerprints{};\n')
replace(p, '    g_lastScanState.store(UINT64_MAX, std::memory_order_release);',
        '    g_lastScanState.store(UINT64_MAX, std::memory_order_release);\n'
        '    for (std::atomic_uint64_t& fingerprint : g_towerWatchSlotFingerprints) {\n'
        '        fingerprint.store(UINT64_MAX, std::memory_order_release);\n'
        '    }')

# Preserve the user's later visual correction to the second bubble hold.
put('client/hooks/bootflow/omega_ikora_origin_probe.cpp',
    align_second_osiris_hold(reference('client/hooks/bootflow/omega_ikora_origin_probe.cpp')))

# Produce all files only after the transformations above succeed.
manifest = []
for source in REF.rglob('*'):
    if not source.is_file(): continue
    path = source.relative_to(REF).as_posix()
    content = outputs.pop(path, None)
    target = DEST / 'src' / path
    data = content.encode('utf-8') if content is not None else source.read_bytes()
    if content is not None:
        try:
            if content == old(path):
                data = BACKUP.read('src/' + path)
        except KeyError:
            pass
    if not target.exists() or target.read_bytes() != data:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    try:
        previous = BACKUP.read('src/' + path)
    except KeyError:
        previous = None
    if previous != data:
        manifest.append({'path': 'src/' + path, 'referenceIdentical': data == source.read_bytes(),
                         'new': previous is None, 'sha256': hashlib.sha256(data).hexdigest()})
for path, content in outputs.items():
    target = DEST / 'src' / path
    target.parent.mkdir(parents=True, exist_ok=True)
    data = content.encode('utf-8')
    if not target.exists() or target.read_bytes() != data:
        target.write_bytes(data)
    manifest.append({'path': 'src/' + path, 'compatibilityFile': True,
                     'sha256': hashlib.sha256(data).hexdigest()})
(BASE / 'port-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print('Ported', len(manifest), 'files')
