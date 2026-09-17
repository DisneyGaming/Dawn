"""Prepare an INERT integration patch for review; never modify production files.

Historical preparation utility for the pre-integration source. The user approved
and the integration was applied on 8 September 2026. This utility refuses to run
on integrated source so it cannot overwrite the original review evidence.
"""
import difflib
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/coo/beyond-infinity-implementation-tests'
originals={}
changes={}

def edit(path,old,new):
    if path not in originals: originals[path]=(ROOT/path).read_text(encoding='utf-8')
    text=changes.get(path,originals[path])
    assert old in text,(path,old[:100])
    changes[path]=text.replace(old,new)

def include(path,anchor,new):
    edit(path,f'#include "{anchor}"',f'#include "{anchor}"\n#include "{new}"')

def main():
    if "beyond_infinity::Frame" in (ROOT/"Dawn/src/middleware/bap/activity_message/sensor_auth_update.h").read_text():
        raise SystemExit("Beyond Infinity is already integrated; preserve the reviewed patch and inspect git diff.")
    p='Dawn/src/middleware/bap/activity_message/sensor_auth_update.h'
    include(p,'../../../state/activity/gateway/frame.h','../../../state/activity/beyond_infinity/frame.h')
    edit(p,'state::activity::gateway::Frame gateway{};','state::activity::gateway::Frame gateway{};\n    state::activity::beyond_infinity::Frame beyond_infinity{};')
    # The primary codec already delegates non-Omega messages to this codec.
    p='Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies_other_missions.cpp'
    include(p,'../../../state/activity/gateway/authority.h','../../../state/activity/beyond_infinity/authority.h')
    anchor='    if(const auto count=state::activity::gateway::body_bits(snapshot.gateway,key,slotType,slotIndex)) { return count; }'
    edit(p,anchor,anchor+'\n    if(const auto count=state::activity::beyond_infinity::body_bits(snapshot.beyond_infinity,key,slotType,slotIndex)) { return count; }')
    anchor='    if(state::activity::gateway::body_bits(snapshot.gateway,key,slotType,slotIndex)) {'
    edit(p,anchor,'    if(state::activity::beyond_infinity::body_bits(snapshot.beyond_infinity,key,slotType,slotIndex)) {\n        return state::activity::beyond_infinity::write_body(writer,snapshot.beyond_infinity,key,slotType,slotIndex);\n    }\n'+anchor)
    p='Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp'
    include(p,'deadly_trial_roster.h','beyond_infinity_roster.h')
    include(p,'beyond_infinity_roster.h','../../../../../state/activity/beyond_infinity/runtime.h')
    anchor='    const auto& omegaExperiments = core::settings::get().omegaExperiments;'
    edit(p,anchor,'    const bool beyondDestination=name=="adventure_vod" && !session.activity.joinedForeignSession;\n    const bool beyondPrepared=state::activity::beyond_infinity::prepare(state::activity::mission_run_generation(),beyondDestination);\n    if(beyondDestination && !beyondPrepared) { return RosterOutcome::noGroups; }\n'+anchor)
    anchor='    if(trialPrepared && !deadly_trial_roster::prepare_layout(layout,'
    edit(p,anchor,'    if(beyondPrepared && !beyond_infinity_roster::prepare_layout(layout,\n        [](std::size_t index,layouts::RosterGroup& group) noexcept { return state::build_data::find_roster_group(index,group); })) { return RosterOutcome::noGroups; }\n'+anchor)
    anchor='    if(snapshot.omegaEndingRetire) {'
    edit(p,anchor,'''    if(beyondPrepared) {
        std::uint32_t failedKey{};
        const bool admitted=beyond_infinity_roster::admit(layout,scratch,snapshot.roster,
            [](std::size_t index,layouts::RosterGroup& group) noexcept { return state::build_data::find_roster_group(index,group); },&failedKey);
        if(!admitted) {
            std::array<char,160> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=roster result=failed registry=%08X",failedKey);
            core::log::write(core::log::Channel::server,core::log::Level::error,line.data());return RosterOutcome::noGroups;
        }
        snapshot.beyond_infinity=state::activity::beyond_infinity::snapshot(state::activity::mission_run_generation(),GetTickCount64(),state::activity::mission_seed_armed());
        if(snapshot.beyond_infinity.enabled) { snapshot.missionCompletion=snapshot.beyond_infinity.completion; }
    }
'''+anchor)
    p='Dawn/src/server/bap/encrypted/push/activity/activity_keepalive_push.cpp'
    include(p,'../../../../../state/activity/deadly_trial/runtime.h','../../../../../state/activity/beyond_infinity/runtime.h')
    edit(p,'state::activity::deadly_trial::publication_due(now)','state::activity::deadly_trial::publication_due(now) || state::activity::beyond_infinity::publication_due(now)')
    p='Dawn/src/client/player/player_position.cpp'
    include(p,'../../state/activity/deadly_trial/runtime.h','../../state/activity/beyond_infinity/runtime.h')
    anchor='    state::activity::deadly_trial::observe_position(position[0],position[1],position[2]);'
    edit(p,anchor,anchor+'\n    state::activity::beyond_infinity::observe_position(position[0],position[1],position[2]);')
    p='Dawn/src/state/activity/forced/prelaunch_profile.h'
    anchor='[[nodiscard]] constexpr const Profile* find'
    edit(p,anchor,'inline constexpr Profile kBeyondInfinity{"adventure_vod",294,0x3E9433BDU,\n    0x03632571U,0x80F46000U,0x80F9FDD2U,"beyond_infinity_direct"};\n\n'+anchor)
    edit(p,'    return nullptr;','    if (package == kBeyondInfinity.package) { return &kBeyondInfinity; }\n    return nullptr;')
    p='Dawn/src/state/activity/forced/activity_forced_destination.cpp'
    edit(p,'profile==&prelaunch::kGateway || profile==&prelaunch::kDeadlyTrial','profile==&prelaunch::kGateway || profile==&prelaunch::kDeadlyTrial || profile==&prelaunch::kBeyondInfinity')
    p='Dawn/src/state/activity/forced/definition.h'
    edit(p,'} // namespace profiles','''// Recovered opening spawn; orientation and landing require live acceptance.
constexpr ForcedDestination beyond_infinity_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="adventure_vod";
    for(std::size_t i=0;i<sizeof(name)-1;++i) { v.packageName[i]=name[i]; }
    v.packageNameLength=sizeof(name)-1;v.bubble=15;v.sliceSet=120;v.spawnSetHash=0x26B11B02U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kBeyondInfinityOpening=beyond_infinity_opening();
static_assert(active(kBeyondInfinityOpening));
} // namespace profiles''')
    p='Dawn/src/server/ui/activity_override/activity_override_panel.cpp'
    anchor='    if (ImGui::Button("Gateway opening")) {'
    edit(p,anchor,'    if (ImGui::Button("Beyond Infinity opening")) {\n        changed = apply_opening_profile(value, rows, forced::profiles::kBeyondInfinityOpening) || changed;\n    }\n'+anchor)
    p='Dawn/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp'
    include(p,'../../../state/activity/gateway/runtime.h','../../../state/activity/beyond_infinity/runtime.h')
    anchor='        original(component, index);\n        if (component != nullptr && index >= 0 && index < 34) {'
    edit(p,anchor,'''        original(component, index);
        bool beyondDispatch{};
        if(component!=nullptr && index>=0 && index<49) {
            std::uint32_t self{};std::int64_t offset{};
            const auto bank=resolve_bank_handle(component,self,offset);
            if(bank==state::activity::beyond_infinity::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::beyond_infinity::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
        }
        if (component != nullptr && index >= 0 && index < 34) {''')
    edit(p,'if (bank != kDialogueBankHandle && bank != 0x80F1FC9EU && bank != 0x80F1F086U)','if (bank != kDialogueBankHandle && bank != 0x80F1FC9EU && bank != 0x80F1F086U && !beyondDispatch)')
    edit(p,'        } else if (component != nullptr) {\n            log_reject("dispatch_index"','        } else if (component != nullptr && !beyondDispatch) {\n            log_reject("dispatch_index"')
    p='Dawn/src/client/hooks/bootflow/omega_rescue_scene_receipts.cpp'
    include(p,'gateway_vance_native_path.h','beyond_infinity_native_receipts.h')
    include(p,'beyond_infinity_native_receipts.h','../../../state/activity/beyond_infinity/runtime.h')
    anchor='__declspec(noinline) void __fastcall tick(void* raw) noexcept {'
    edit(p,anchor,'#include "beyond_infinity_scene_receipts.inl"\n'+anchor)
    edit(p,'    original(raw);\n    if(gatewayOwned','''    beyond_native::SceneSample beyondBefore{};Read beyondRead{};
    state::activity::coo::Generation beyondOwner{};bool beyondOwned{};
    if(gate.accepts_side_effects() && read_beyond_scene(beyondRead,component,beyondBefore)) {
        const auto request=beyond::request();beyond::SceneReceipt receipt{};
        beyondOwned=beyond_native::scene_sample(beyondBefore,request.frame,request.owner,receipt);beyondOwner=request.owner;
    }
    original(raw);
    if(beyondOwned && gate.accepts_side_effects()) { finish_beyond_scene(beyondRead,component,beyondBefore,beyondOwner); }
    if(gatewayOwned''')
    anchor='        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&tick)},'
    edit(p,anchor,anchor+'\n        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&read_beyond_scene)},\n        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&finish_beyond_scene)},')
    p='Dawn/src/client/hooks/bootflow/omega_arc_charge_receipts.cpp'
    include(p,'gateway_module_native_path.h','beyond_infinity_native_receipts.h')
    include(p,'beyond_infinity_native_receipts.h','../../../state/activity/beyond_infinity/runtime.h')
    edit(p,'#include "gateway_module_receipts.inl"','#include "beyond_infinity_object_receipts.inl"\n#include "gateway_module_receipts.inl"')
    anchor='{reinterpret_cast<void*>(&gateway_sense_hook)}, {reinterpret_cast<void*>(&observe_gateway_module)},'
    edit(p,anchor,anchor+'\n        {reinterpret_cast<void*>(&observe_beyond_object)},')
    p='Dawn/src/client/hooks/bootflow/gateway_module_receipts.inl'
    edit(p,'    observe_gateway_object(raw);','    observe_beyond_object(raw);\n    observe_gateway_object(raw);')
    p='Dawn/Dawn.vcxproj'
    anchor='    <ClCompile Include="src\\state\\activity\\gateway\\controller.cpp" />'
    edit(p,anchor,anchor+'\n    <ClCompile Include="src\\state\\activity\\beyond_infinity\\runtime.cpp"><ExceptionHandling>Sync</ExceptionHandling></ClCompile>\n    <ClCompile Include="src\\state\\activity\\beyond_infinity\\controller.cpp" />')
    headers=['src/state/activity/beyond_infinity/'+n for n in ('catalog.h','native_catalog.h','bindings.h','frame.h','controller.h','runtime.h','authority.h')]
    headers+=['src/state/activity/coo/native_scene_cast_authority.h','src/server/bap/encrypted/push/activity/beyond_infinity_roster.h']
    headers+=['src/client/hooks/bootflow/beyond_infinity_'+n for n in ('native_receipts.h','scene_receipts.inl','object_receipts.inl')]
    anchor='    <None Include="scripts\\gateway.lua" />'
    edit(p,anchor,anchor+'\n    <None Include="scripts\\beyond_infinity.lua" />\n'+'\n'.join('    <ClInclude Include="'+n.replace('/','\\')+'" />' for n in headers))
    p='Dawn/unit/player_position_tests.cpp'
    edit(p,'namespace deadly_trial { void observe_position','namespace beyond_infinity { void observe_position(float,float,float) noexcept {} }\nnamespace deadly_trial { void observe_position')
    p='tools/coo/verify_lua.py'
    edit(p,'    "coo_ending_runtime_tests",','    "coo_ending_runtime_tests", "beyond_infinity_catalog_tests", "beyond_infinity_tests",')
    p='tools/coo/package_lua.py'
    edit(p,"SCRIPTS = ('omega.lua', 'deadly_trial.lua', 'gateway.lua')","SCRIPTS = ('omega.lua', 'deadly_trial.lua', 'gateway.lua', 'beyond_infinity.lua')")
    p='tools/coo/install_candidate.ps1'
    edit(p,"'Dawn/scripts/gateway.lua')","'Dawn/scripts/gateway.lua', 'Dawn/scripts/beyond_infinity.lua')")
    edit(p,'$manifest.buildsAndTests -ne 34','$manifest.buildsAndTests -ne 38')
    OUT.mkdir(parents=True,exist_ok=True)
    patch=''.join('diff --git a/'+p+' b/'+p+'\n'+''.join(difflib.unified_diff(originals[p].splitlines(True),s.splitlines(True),fromfile='a/'+p,tofile='b/'+p)) for p,s in changes.items())
    (OUT/'production-integration.patch').write_text(patch,encoding='utf-8')
    receipt={'status':'UNAPPLIED; explicit approval required after automatic-review rejection','files':list(changes),
             'productionFilesUnchanged':all((ROOT/p).read_text(encoding='utf-8')==s for p,s in originals.items()),
             'baseSha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in changes},
             'patchSha256':hashlib.sha256(patch.encode()).hexdigest(),'wireCapacityChanged':False}
    (OUT/'integration-review.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    print(f'Prepared UNAPPLIED patch: {len(changes)} files, {len(patch.splitlines())} lines. Production sources unchanged: {receipt["productionFilesUnchanged"]}')

if __name__=='__main__': main()
