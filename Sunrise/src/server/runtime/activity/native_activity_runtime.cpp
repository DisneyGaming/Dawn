#include <Windows.h>
#include "native_activity_runtime.h"
#include "native_activity_profiles.h"
#include "mercury_freeroam_runtime.h"
#include "open_world_runtime.h"
#include "../../../state/activity/native_population_events.h"
#include "../../../client/hooks/bootflow/native_local_placement_probe.h"
#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/runtime.h"
#include "../../../state/runtime/runtime.h"
#include "../../../middleware/crypto/random_bytes.h"
#include <cstdio>
#include <mutex>

namespace sunrise::server::runtime::activity::native_activity {
using population::Service;
using population::Command;
using population::Result;
using population::parse;
namespace {
namespace nativeEvents=state::activity::native_population;
namespace placementProbe=sunrise::client::hooks::bootflow::native_local_placement_probe;
namespace gatewayRead=sunrise::client::hooks::bootflow::gateway_native;
struct Entry {
    PersistentActivity activity{};std::uint64_t nextPoll{},nextTransitPoll{},lastText{};
    activity_clock::Service clock{};
    std::array<coo::NativePopulationLedger<128>,128> ledgers{};
    std::array<std::uint32_t,128> sourceHandles{};
    nativeEvents::Lease generatorLease{};
    std::uint32_t generatorSeed{};
    bool generatorBound{};
    mercury::freeroam::Director mercuryFreeroam{};
    open_world::Director openWorld{};
    std::uint8_t lastBubble{};
    bool mercuryFreeroamReady{};
    bool openWorldReady{};
    bool lastArrived{};
    bool observationFailure{};
    std::uint64_t lastTransitKey{UINT64_MAX};
};
std::mutex mutex;
std::array<Entry,16> entries{};
std::uint64_t nextClockEpoch{};
// Activity SOIDs and incarnation clocks may repeat after a process restart.
// A random process token prevents an old mailbox file matching that new lifetime.
std::uint64_t boot_token() noexcept {
    static const auto token=[]() noexcept {
        std::uint64_t value{};
        return middleware::crypto::random::fill(std::as_writable_bytes(std::span{&value,1}))?value:0ULL;
    }();
    return token;
}
// Immutable documents are loaded outside the activity-state mutex. Registered
// filenames stay module-relative; missing/invalid files publish no fallback policy.
std::shared_ptr<const PersistentActivity::Document> document_for(const NativeActivityDefinition& definition) noexcept {
    struct Loaded { const NativeActivityDefinition* definition{};std::shared_ptr<const PersistentActivity::Document> document; };
    static const auto documents=[] {
        std::array<Loaded,kNativeActivityProfiles.size()> result{};
        for(std::size_t i=0;i<result.size();++i) {
            const auto* profile=kNativeActivityProfiles[i];result[i].definition=profile;
            core::path::Buffer path{};std::string error;
            if(core::path::artifact_directory(GetModuleHandleW(L"steam_api64.dll"),path)
                && core::path::append(path,L"\\scripts\\") && core::path::append(path,profile->scriptFile)) {
                auto parsed=PersistentActivity::Document::read_native_policy(path.chars.data(),*profile->profile,error);
                if(parsed && PersistentActivity::valid(*profile,*parsed)) result[i].document=std::move(parsed);
                else if(error.empty()) error="native activity contract mismatch";
            } else error="native activity script path unavailable";
            std::array<char,384> line{};
            const auto size=std::snprintf(line.data(),line.size(),
                "ev=native_activity phase=definition activity=%.*s valid=%u fingerprint=%016llX error=%.180s",
                static_cast<int>(profile->activity.size()),profile->activity.data(),result[i].document?1U:0U,
                result[i].document?result[i].document->fingerprint():0ULL,error.c_str());
            if(size>0 && static_cast<std::size_t>(size)<line.size())
                core::log::write(core::log::Channel::server,result[i].document?core::log::Level::info:core::log::Level::error,
                    {line.data(),static_cast<std::size_t>(size)});
        }
        return result;
    }();
    for(const auto& loaded:documents) if(loaded.definition==&definition) return loaded.document;
    return {};
}
void report(const Service& service,const char* phase,unsigned result=0,
    const Command& command={}) noexcept {
    std::array<char,320> text{};
    const auto owner=service.owner();
    const auto size=std::snprintf(text.data(),text.size(),
        "ev=activity_population phase=%s boot=%016llX owner=%016llX incarnation=%llu revision=%llu request=%llu registry=%08X slot=%u target=%u result=%u development=1",
        phase,service.boot(),owner.sessionId,owner.incarnation.value,service.revision(),command.request,
        command.registry,command.slot,command.requested,result);
    if(size>0 && static_cast<std::size_t>(size)<text.size())
        core::log::write(core::log::Channel::server,core::log::Level::info,{text.data(),static_cast<std::size_t>(size)});
}
void report_animation(const npc_animation::Service& service,const char* phase,
    npc_animation::Result result=npc_animation::Result::accepted,
    const npc_animation::Command& command={}) noexcept {
    std::array<char,384> text{};
    const auto owner=service.owner();
    const auto size=std::snprintf(text.data(),text.size(),
        "ev=activity_npc_animation phase=%s boot=%016llX owner=%016llX incarnation=%llu revision=%llu request=%llu registry=%08X slot=%u action=%u stop=%u result=%u development=%u readiness=unobserved",
        phase,service.boot(),owner.sessionId,owner.incarnation.value,service.revision(),command.request,
        command.registry,command.slot,command.action,command.stop?1U:0U,static_cast<unsigned>(result),command.development?1U:0U);
    if(size>0 && static_cast<std::size_t>(size)<text.size())
        core::log::write(core::log::Channel::server,core::log::Level::info,{text.data(),static_cast<std::size_t>(size)});
}
bool bind_round_generator(Entry& entry,Owner owner,const NativeActivityDefinition& definition,
    const NativeActivityFrame& frame) noexcept {
    if(!definition.rounds || definition.rounds->generatorIndex>=definition.generators.size())return true;
    const auto& capability=definition.generators[definition.rounds->generatorIndex];
    const forest_generator::wire::Request* request{};
    for(std::size_t i=0;i<frame.generators.count;++i) {
        const auto& candidate=frame.generators.entries[i];
        if(candidate.registry==capability.registry->key && candidate.slot==capability.slot) {request=&candidate;break;}
    }
    if(!request || !request->state.primary.enabled)return true;
    const registry::Slot* descriptor{};
    for(const auto& slot:capability.registry->slots)if(slot.index==capability.slot)descriptor=&slot;
    if(!descriptor)return false;
    const auto round=entry.activity.round().round_snapshot().token.round;
    if(!round || round>UINT32_MAX)return false;
    const coo::Asset asset{capability.registry->key,descriptor->descriptorTag,37,capability.slot};
    const coo::PopulationOwner source{owner.sessionId,entry.activity.population().boot(),
        owner.incarnation.value,asset,static_cast<std::uint32_t>(round)};
    const nativeEvents::GeneratorBinding binding{{owner,source,capability.registry->bubble},
        request->state.primary.seed,definition.rounds->generatorPalette.resourceTag,
        definition.rounds->generatorPalette.workerDefinitionTag,
        definition.rounds->generatorPalette.workerDefinitionOffset,
        definition.rounds->generatorPalette.palettes};
    if(!entry.generatorBound) {
        if(!nativeEvents::bind_generator(binding)) return false;
        entry.generatorLease=binding.lease;entry.generatorSeed=binding.seed;
        entry.generatorBound=true;return true;
    }
    if(entry.generatorLease==binding.lease && entry.generatorSeed==binding.seed)
        return nativeEvents::bind_generator(binding);
    if(!entry.activity.round().generated_source_retired(entry.generatorLease.source))
        return true;
    const auto oldSource=entry.generatorLease.source;
    if(!nativeEvents::rebind_generator(entry.generatorLease,binding))return false;
    if(!entry.activity.round().release_retired_generated_source(oldSource,binding.lease.source))
        return false;
    entry.generatorLease=binding.lease;entry.generatorSeed=binding.seed;return true;
}
void poll_transit_targets(Entry& entry,std::uint64_t now) noexcept {
    if(now<entry.nextTransitPoll)return;
    std::array<transit_effect::TargetRequest,18> pending{};
    const auto count=entry.activity.pending_transit_targets(pending);
    if(!count)return;
    entry.nextTransitPoll=now+100;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<count;++i) {
        const auto& target=pending[i];
        const placementProbe::Request request{
            target.registry,target.slot,target.definitionTag,target.definitionOffset,
            target.generation,target.active};
        gatewayRead::Read read{image};
        if(placementProbe::probe(read,request))
            static_cast<void>(entry.activity.observe_transit_target(target));
    }
}
// Logs the native transit chain state whenever any of its gates or receipts change.
void log_transit_state(Entry& entry,Owner owner) noexcept {
    const auto* definition=entry.activity.definition();
    if(!definition || !definition->rounds || !entry.activity.round().started())return;
    const auto d=entry.activity.transit_diagnostics();
    const auto& rt=entry.activity.round();
    const auto r=rt.round_snapshot();
    const auto gates=entry.activity.transit_gates();
    const auto b=[](bool value) noexcept {return static_cast<std::uint64_t>(value?1U:0U);};
    const std::uint64_t key=(static_cast<std::uint64_t>(static_cast<unsigned>(r.phase)&0xFU)<<60)
        |(b(r.expired)<<59)|(b(rt.travel_pending())<<58)|(b(rt.travel_arrival_pending())<<57)
        |(b(rt.travel_arrival_qualified())<<56)|(static_cast<std::uint64_t>(gates)<<48)
        |(static_cast<std::uint64_t>(d.lastFailure)<<40)|(static_cast<std::uint64_t>(d.targetsPending)<<32)
        |(b(d.prepared)<<31)|(b(d.targetReady)<<30)|(b(d.triggerArmed)<<29)|(b(d.requested)<<28)
        |(b(d.applied)<<27)|(b(d.arrivalCandidate)<<26)|(b(d.arrivalQualified)<<25)
        |(static_cast<std::uint64_t>(r.progress&0xFFFFU)<<8)|(r.token.round&0xFFU);
    if(key==entry.lastTransitKey)return;
    entry.lastTransitKey=key;
    std::array<char,448> line{};
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=forest_transit stage=state owner=%016llX round=%llu phase=%u progress=%u expired=%u "
        "travel_pending=%u cohort=%llu destination=%08X effect=%u arrival_kind=%u gates=%02X "
        "prepared=%u targets_pending=%u target_ready=%u trigger_armed=%u requested=%u expected=%d "
        "applied=%u arrival_candidate=%u arrival_qualified=%u arrival_armed=%u arrival_ok=%u fail=%u",
        owner.sessionId,static_cast<unsigned long long>(r.token.round),static_cast<unsigned>(r.phase),
        static_cast<unsigned>(r.progress),r.expired?1U:0U,rt.travel_pending()?1U:0U,
        static_cast<unsigned long long>(d.cohort),d.destination,static_cast<unsigned>(d.effect),
        static_cast<unsigned>(d.arrivalKind),static_cast<unsigned>(gates),d.prepared?1U:0U,
        static_cast<unsigned>(d.targetsPending),d.targetReady?1U:0U,d.triggerArmed?1U:0U,
        d.requested?1U:0U,d.expected,d.applied?1U:0U,d.arrivalCandidate?1U:0U,d.arrivalQualified?1U:0U,
        rt.travel_arrival_pending()?1U:0U,rt.travel_arrival_qualified()?1U:0U,
        static_cast<unsigned>(d.lastFailure));
    if(size>0 && static_cast<std::size_t>(size)<line.size())
        core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
}
// Small explicit developer mailbox. The sender atomically renames a complete
// file into place. Unsupported/partial input does not change retained authority.
// No file watcher can reinterpret an old file as a command for a new activity.
bool read_command(std::array<char,512>& text,std::size_t& length) noexcept {
    static core::path::Buffer path{};
    static const bool ready=[]() noexcept {
        return core::path::artifact_directory(GetModuleHandleW(L"steam_api64.dll"),path)
            && core::path::append(path,L"\\activity-dev.txt");
    }();
    if(!ready) return false;
    const HANDLE file=CreateFileW(path.chars.data(),GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};DWORD read{};
    const bool ok=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<static_cast<LONGLONG>(text.size())
        && ReadFile(file,text.data(),static_cast<DWORD>(size.QuadPart),&read,nullptr)
        && read==size.QuadPart;
    CloseHandle(file);length=ok?read:0;return ok;
}
}
bool optional_registries(const NativeActivityDefinition& definition,
    ambient_population::RegistryBatch& output) noexcept {
    output={};const auto document=document_for(definition);
    return document && ambient_population::optional_registries(definition,*document,output);
}
bool snapshot_clock(Owner owner,activity_clock::Publication& output) noexcept {
    output={};
    if(!owner || !state::activity::contains(owner))return false;
    std::lock_guard lock(mutex);
    for(auto& entry:entries) {
        if(entry.activity.population().owner()!=owner || !entry.activity.definition())continue;
        const auto domain=entry.clock.domain();
        return domain && domain.owner==owner && domain.boot==entry.activity.population().boot()
            && entry.clock.project_retained(owner,domain.boot,domain,GetTickCount64(),output);
    }
    return false;
}
bool snapshot_placements(Owner owner,const NativeActivityDefinition*& definition,
    placement::wire::Batch& output) noexcept {
    definition=nullptr;output={};
    if(!owner || !state::activity::contains(owner)) return false;
    const auto account=state::account_snapshot();
    std::lock_guard lock(mutex);
    for(const auto& entry:entries) {
        if(entry.activity.population().owner()!=owner) continue;
        definition=entry.activity.definition();
        output=entry.activity.placements();
        if (!definition || !equipment_interaction::project(
                definition->equipmentInteractionGates, account, output)) {
            definition=nullptr;output={};return false;
        }
        if(definition && definition->openWorld && (!entry.lastArrived
            || !open_world::append_placements(definition->placements,entry.lastBubble,output))) {
            definition=nullptr;output={};return false;
        }
        return true;
    }
    return false;
}
void observe(Owner owner,std::uint32_t bubble,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept {
    if(!owner || !state::activity::contains(owner) || update.objectCount>update.objects.size()) return;
    std::lock_guard lock(mutex);
    for(auto& entry:entries) {
        if(entry.activity.population().owner()!=owner) continue;
        for(std::size_t i=0;i<update.objectCount;++i) {
            const auto& object=update.objects[i];
            if(entry.activity.observe_transit_effect(owner,entry.activity.population().boot(),bubble,object)) {
                std::array<char,224> line{};
                const auto size=std::snprintf(line.data(),line.size(),
                    "ev=native_transit_effect phase=applied owner=%016llX registry=%08X slot=%u native_revision=%u",
                    owner.sessionId,object.registryKey,object.slotIndex,object.nativeRevision);
                if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                    core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
            } else if(object.slotType==26 && !(entry.activity.transit_diagnostics().applied
                && entry.activity.transit_diagnostics().lastFailure==0)) {
                // Every type-26 receipt that did not qualify, with the fields the matcher reads;
                // echoes after a successful application are expected and not logged.
                const auto d=entry.activity.transit_diagnostics();
                std::array<char,448> line{};
                const auto size=std::snprintf(line.data(),line.size(),
                    "ev=forest_transit stage=sense26 owner=%016llX bubble=%u registry=%08X slot=%u "
                    "has_schema=%u schema=%08X inferred=%u root_delta=%u body_bits=%u first=%016llX "
                    "second=%016llX third=%016llX fourth=%016llX native_revision=%u expected=%d fail=%u",
                    owner.sessionId,bubble,object.registryKey,object.slotIndex,object.hasNativeSchema?1U:0U,
                    object.nativeSchema,object.inferredBodyWidth?1U:0U,object.hasRootDelta?1U:0U,
                    object.bodyBits,static_cast<unsigned long long>(object.bodyFirst),
                    static_cast<unsigned long long>(object.bodySecond),
                    static_cast<unsigned long long>(object.bodyThird),
                    static_cast<unsigned long long>(object.bodyFourth),object.nativeRevision,d.expected,
                    static_cast<unsigned>(d.lastFailure));
                if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                    core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
            }
            if(object.slotType==70 && object.hasNativeSchema)
                static_cast<void>(entry.activity.public_initial().observe(object.registryKey,object.slotIndex,bubble,object.nativeSchema,object.engagement));
            if(entry.activity.observe_occupancy(owner,entry.activity.population().boot(),bubble,object)) {
                std::array<char,256> line{};
                const auto size=std::snprintf(line.data(),line.size(),
                    "ev=native_activity phase=occupancy_receipt owner=%016llX incarnation=%llu registry=%08X slot=%u native_revision=%u",
                    owner.sessionId,owner.incarnation.value,object.registryKey,object.slotIndex,object.nativeRevision);
                if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                    core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
            }
            const auto monitors=entry.activity.ambient().observe(owner,entry.activity.population().boot(),bubble,object);
            if(monitors) {
                ambient_population::MonitorDelta monitor{};
                if(ambient_population::decode_monitor(object,monitor)) {
                    std::array<char,320> line{};
                    const auto size=std::snprintf(line.data(),line.size(),
                        "ev=activity_ambient phase=monitor owner=%016llX incarnation=%llu registry=%08X slot=%u native_revision=%u selected=%d any=%u all=%u authority_token=%d affected=%zu",
                        owner.sessionId,owner.incarnation.value,object.registryKey,object.slotIndex,
                        monitor.revision,monitor.selected,monitor.any?1U:0U,monitor.all?1U:0U,monitor.authorityToken,monitors);
                    if(size>0 && static_cast<std::size_t>(size)<line.size())
                        core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
                }
            }
            const auto priorPopulationRevision=entry.activity.population().revision();
            const auto* mirror=entry.activity.population().observe(bubble,object);
            if(!mirror) continue;
            if(entry.activity.population().revision()!=priorPopulationRevision) {
                const auto populations=entry.activity.population().project_retained();
                for(std::size_t sourceIndex=0;sourceIndex<populations.count;++sourceIndex) {
                    const auto& source=populations.entries[sourceIndex];
                    if(source.source.registry!=object.registryKey || source.slot!=object.slotIndex)continue;
                    std::array<char,384> taskLine{};
                    const auto taskSize=std::snprintf(taskLine.data(),taskLine.size(),
                        "ev=activity_population phase=task_selection owner=%016llX incarnation=%llu registry=%08X slot=%u generation=%u task_row=%d evaluation_revision=%u reported_cost_mask=%06X",
                        owner.sessionId,owner.incarnation.value,object.registryKey,object.slotIndex,
                        source.source.generation,static_cast<int>(source.source.tactical.row),
                        source.source.tactical.revision,object.squadOutput.costMask);
                    if(taskSize>0 && static_cast<std::size_t>(taskSize)<taskLine.size())
                        core::log::write(core::log::Channel::server,core::log::Level::info,
                            {taskLine.data(),static_cast<std::size_t>(taskSize)});
                    break;
                }
            }
            std::array<char,384> line{};
            const auto size=std::snprintf(line.data(),line.size(),
                "ev=activity_population phase=native owner=%016llX incarnation=%llu registry=%08X slot=%u native_revision=%u generation=%u associated=%d consumed=%d known=%02X confirmed_deaths=unknown",
                owner.sessionId,owner.incarnation.value,object.registryKey,object.slotIndex,
                mirror->revision,mirror->scalar[0],(mirror->known&8U)?static_cast<int>(mirror->scalar[3]):-1,
                mirror->consumedKnown && mirror->consumedCount==1?mirror->consumed[0]:-1,mirror->known);
            if(size>0 && static_cast<std::size_t>(size)<line.size())
                core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
        return;
    }
}

bool observe_player_trigger(Owner owner,
    const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept {
    if(!owner || receipt.slot<0)return false;
    const std::lock_guard lock(mutex);
    for(auto& entry:entries) {
        if(entry.activity.population().owner()!=owner)continue;
        return entry.activity.observe_player_trigger(owner,entry.activity.population().boot(),
            receipt.registry,static_cast<std::uint16_t>(receipt.slot),receipt.object);
    }
    return false;
}

bool round_progress(Owner owner,timed_round::Phase& phase,
    std::uint64_t& completedRounds,std::uint64_t& boot) noexcept {
    phase=timed_round::Phase::entry;completedRounds=0;boot=0;
    if(!owner || !state::activity::contains(owner))return false;
    const std::lock_guard lock(mutex);
    for(const auto& entry:entries) {
        if(entry.activity.population().owner()!=owner)continue;
        if(!entry.activity.definition() || !entry.activity.round().started())return false;
        const auto status=entry.activity.round().round_snapshot();
        phase=status.phase;completedRounds=status.completedRounds;boot=status.token.boot;
        return true;
    }
    return false;
}

void observe_player_life(Owner owner,std::uint64_t boot,std::uint32_t entity,bool alive) noexcept {
    if(!owner || !state::activity::contains(owner) || entity==UINT32_MAX)return;
    const std::lock_guard lock(mutex);
    for(auto& entry:entries) {
        if(entry.activity.population().owner()!=owner || entry.activity.population().boot()!=boot
            || !entry.activity.round().started())continue;
        if(native_activity_transit::observed_member_count(owner,boot)!=1)return;
        if(entry.activity.round().observe_player_life(entity,alive,GetTickCount64()))
            core::log::write(core::log::Channel::server,core::log::Level::info,
                "ev=native_round stage=defeat_qualified destination=rewards");
        return;
    }
}

RespawnState respawn_state(Owner owner) noexcept {
    RespawnState result{};
    if(!owner || !state::activity::contains(owner))return result;
    const std::lock_guard lock(mutex);
    for(const auto& entry:entries) {
        if(entry.activity.population().owner()!=owner || !entry.activity.definition())continue;
        if(!entry.activity.round().started())return result;
        result.valid=true;
        result.suppressed=entry.activity.traversal_respawn();
        result.restricted=entry.activity.round().restricted();
        result.travelArrivalQualified=entry.activity.round().travel_arrival_qualified();
        result.latched=entry.activity.respawn_latched();
        result.phase=entry.activity.round().round_snapshot().phase;
        return result;
    }
    return result;
}
bool publication_pending(Owner owner) noexcept {
    if(!owner)return false;
    const std::lock_guard lock(mutex);
    for(const auto& entry:entries)
        if(entry.activity.population().owner()==owner)return entry.activity.publication_pending();
    return false;
}
NativeActivityFrame update(Owner owner,std::uint32_t bubble,bool arrived,
    const NativeActivityDefinition& definition,const adventure_start::wire::Request& selected,bool openingAdmissionReady,
    bool experimentalRewardPlacements) noexcept {
    if(!owner || bubble>63 || !state::activity::contains(owner)) return {};
    const auto account=state::account_snapshot();
    const auto document=document_for(definition);if(!document) return {};
    std::lock_guard lock(mutex);
    Entry* current{};Entry* empty{};
    for(auto& entry:entries) {
        // Detach only when the authoritative activity incarnation was released.
        // This is not a reusable native-source retirement acknowledgement.
        if(entry.activity.population().owner() && !state::activity::contains(entry.activity.population().owner())) {
            entry.activity.release_owner();
            nativeEvents::release(entry.activity.population().owner());
            adventure::native_bridge::release(entry.activity.population().owner());
            adventure::dialogue_bridge::release(entry.activity.population().owner());
            ambient_population::named_points::release(entry.activity.population().owner());
            capture_bridge::release(entry.activity.population().owner());
            public_event::native_bridge::release(entry.activity.population().owner());
            public_event::deferred_bridge::release(entry.activity.population().owner());
            public_event::keys::bridge::release(entry.activity.population().owner());
            public_event::participant_bridge::release(entry.activity.population().owner());
            public_event::engagement_bridge::release(entry.activity.population().owner());
            if(entry.activity.definition() && entry.activity.definition()->rounds)
                native_activity_transit::release(entry.activity.population().owner(),entry.activity.population().boot());
            entry={};
        }
        if(entry.activity.population().owner()==owner) current=&entry;
        if(!entry.activity.population().owner() && !empty) empty=&entry;
    }
    if(!current) {
        if(!empty || !empty->activity.begin(owner,definition,document,boot_token())) return {};
        current=empty;
        if(&definition==&mercury::kActivity) {
            mercury::freeroam::Configuration configuration{};SYSTEMTIME local{};GetLocalTime(&local);
            if(!mercury::freeroam::configure(*document,configuration)
                || !current->mercuryFreeroam.begin(owner,current->activity.population().boot(),
                    static_cast<std::uint8_t>(local.wMinute),configuration)) {*current={};return {};}
            current->mercuryFreeroamReady=true;
        }
        if(definition.openWorld) {
            open_world::Configuration configuration{};
            if(!open_world::configure(*document,configuration)
                || !current->openWorld.begin(owner,current->activity.population().boot(),
                    *definition.openWorld,definition.populations,configuration)) {*current={};return {};}
            current->openWorldReady=true;
        }
        report(current->activity.population(),"ready");
        if(current->activity.animation().owner()) report_animation(current->activity.animation(),"ready");
    }
    if(current->activity.definition()!=&definition) return {};
    current->activity.admit_experimental_reward_placements(experimentalRewardPlacements);
    const bool populationLifecycleReady=current->mercuryFreeroamReady || current->openWorldReady;
    const auto prior=current->activity.population().revision();
    const auto priorAnimation=current->activity.animation().revision();
    const auto priorGenerator=current->activity.generator().revision();
    const auto priorDevice=current->activity.device().revision();
    const auto priorPresentation=current->activity.presentation().revision();
    const auto priorRally=current->activity.rally().diagnostics().phase;
    const auto priorRallyFailure=current->activity.rally().failed();
    std::array<ambient_population::InitialState,128> priorAmbient{};
    for(std::size_t i=0;i<current->activity.ambient().size();++i) priorAmbient[i]=current->activity.ambient().state(i);
    const auto priorOpening=current->activity.opening().frame();
    const auto priorPublicInitial=current->activity.public_initial().state();
    activity_clock::Publication clock{};
    const bool retainedClock=current->clock.domain()
        && (definition.retainRosterOrdinals || current->activity.opening().retains_region(bubble,selected));
    if(!definition.clockFrequencyParameter.empty()
        && (definition.openWorld || bubble==definition.bubble || retainedClock)) {
        const auto now=GetTickCount64();
        if(!current->clock.domain() && arrived) {
            const auto* frequency=document->views().parameter(definition.clockFrequencyParameter);
            if(!frequency || !frequency->value || nextClockEpoch==UINT64_MAX)return {};
            const activity_clock::Policy policy{definition.registries.front().scenario,definition.bubble,
                {false,1000.0F/static_cast<float>(frequency->value)}};
            if(!current->clock.begin(owner,boot_token(),++nextClockEpoch,policy,now))return {};
            std::array<char,256> line{};const auto size=std::snprintf(line.data(),line.size(),
                "ev=native_clock phase=start owner=%016llX incarnation=%llu epoch=%llu scenario=%08X timing_hz=%u policy=reconstructed",
                owner.sessionId,owner.incarnation.value,nextClockEpoch,policy.scenario,frequency->value);
            if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
        // Arrival admits a new clock once. A transient loading/visibility step
        // must not publish legacy zero and rewind an already admitted domain.
        if(current->clock.domain()) {
            const bool projected=(definition.openWorld || retainedClock)
                ?current->clock.project_retained(owner,boot_token(),current->clock.domain(),now,clock)
                :current->clock.project(owner,boot_token(),bubble,now,clock);
            if(!projected)return {};
        }
    }
    // Establish the elapsed-tick publication before native activity intake.
    // Round deaths are queued until this owner has a current clock and round;
    // they are never scored with the prior frame's timestamp.
    std::array<nativeEvents::Event,256> native{};bool overflow{};
    const bool deferRoundEvents=definition.rounds && (!clock || !current->activity.round().started());
    const auto received=deferRoundEvents?0:nativeEvents::drain(owner,native,overflow);
    current->observationFailure|=overflow;
    for(std::size_t e=0;e<received;++e) {
        const auto& event=native[e];
        if(definition.rounds && event.lease.source.source.type==37) {
            bool accepted{};
            if(event.kind==nativeEvents::Kind::admitted)
                accepted=current->activity.round().admit_generated(event.actor,event.lease.source,
                    event.memberPrefabTag,event.completionGroup);
            else if(event.kind==nativeEvents::Kind::died) {
                const auto result=current->activity.round().generated_death(event.actor,event.lease.source,
                    event.memberPrefabTag,event.completionGroup,false,clock.elapsedTicks);
                // Unknown/non-scoring authored prefabs are a bounded diagnostic,
                // not a round fault.
                accepted=result==timed_round::Result::accepted || result==timed_round::Result::unsupported;
            } else accepted=current->activity.round().retire_generated(event.actor,event.lease.source);
            // A generated lease can legitimately report after traversal or
            // after a rebind has begun. The round ledger scores only deaths
            // in the current traversal; late/stale identities remain useful for
            // retirement accounting and never become a global activity fault.
            continue;
        }
        for(std::size_t i=0;i<definition.populations.size();++i) {
            auto& ledger=current->ledgers[i];if(ledger.owner()!=event.lease.source) continue;
            coo::PopulationIntake result=coo::PopulationIntake::conflict;
            if(event.kind==nativeEvents::Kind::sourceRecreated) {
                if(populationLifecycleReady && event.lease.discardStreamedReplicas
                    && (current->sourceHandles[i]==event.previousSourceHandle || current->sourceHandles[i]==0)
                    && ledger.source_recreated(event.lease.source)) {
                    current->sourceHandles[i]=event.sourceHandle;result=coo::PopulationIntake::accepted;
                }
            } else if(current->sourceHandles[i]==0 || current->sourceHandles[i]==event.sourceHandle) {
                if(event.kind==nativeEvents::Kind::admitted) {
                    result=ledger.admitted(event.actor);
                    if(result==coo::PopulationIntake::accepted) current->sourceHandles[i]=event.sourceHandle;
                } else if(event.kind==nativeEvents::Kind::died) result=ledger.died(event.actor);
                else result=ledger.actor_retired(event.actor);
            }
            if(result!=coo::PopulationIntake::accepted && result!=coo::PopulationIntake::duplicate) current->observationFailure=true;
            if(definition.rounds && (result==coo::PopulationIntake::accepted
                || result==coo::PopulationIntake::duplicate)) {
                const bool wasDead=current->activity.round().boss_death_qualified();
                static_cast<void>(current->activity.round().population_event(
                    static_cast<std::uint16_t>(i),event.lease.source,event.actor,event.kind,
                    clock.elapsedTicks));
                if(!wasDead && current->activity.round().boss_death_qualified())
                    static_cast<void>(current->activity.pause_round_hud());
            }
            if(result==coo::PopulationIntake::accepted && event.kind!=nativeEvents::Kind::sourceRecreated)static_cast<void>(current->activity.public_initial().observe_accepted(event,result));
            const auto counts=ledger.counts();
            std::array<char,384> line{};
            const auto size=std::snprintf(line.data(),line.size(),
                "ev=native_population kind=%s owner=%016llX registry=%08X slot=%u generation=%u actor=%08X entity=%08X source=%08X result=%u admitted=%zu alive=%zu dead=%zu resident=%zu incomplete=%u",
                event.kind==nativeEvents::Kind::admitted?"admitted":event.kind==nativeEvents::Kind::died?"died":"retired",owner.sessionId,
                event.lease.source.source.registry,event.lease.source.source.slot,event.lease.source.generation,
                event.actor.actor,event.actor.entity,event.sourceHandle,static_cast<unsigned>(result),counts.admitted,counts.alive,
                counts.dead,counts.resident,current->observationFailure?1U:0U);
            if(size>0 && static_cast<std::size_t>(size)<line.size())
                core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
    }
    if(definition.rounds && (current->activity.round().round_snapshot().phase==timed_round::Phase::returning
        || current->activity.round().round_snapshot().phase==timed_round::Phase::rewards)) {
        bool allActorsRetired=true,allSourcesAcknowledged=true;std::size_t requestedCount{};
        for(std::size_t j=0;j<current->activity.round().encounter_population_count();++j) {
            const auto index=current->activity.round().encounter_population_index(j);
            if(index>=definition.populations.size()) {allActorsRetired=false;allSourcesAcknowledged=false;continue;}
            ++requestedCount;auto& ledger=current->ledgers[index];const auto counts=ledger.counts();
            allActorsRetired&=counts.resident==0;
            const auto& capability=definition.populations[index];
            const auto* status=current->activity.population().status(capability.registry->key,capability.slot);
            bool acknowledged=status && status->retirementAcknowledged
                && status->generation>ledger.owner().generation;
            if(!acknowledged && status && counts.resident==0 && ledger.owner().valid())
                acknowledged=current->activity.population().retirement_acknowledged(owner,
                    current->activity.population().boot(),capability.registry->key,capability.slot,
                    status->generation,true);
            if(acknowledged && !counts.sourceRetired)
                static_cast<void>(ledger.source_retired(ledger.owner()));
            const auto settled=ledger.counts();
            allSourcesAcknowledged&=acknowledged && settled.sourceRetired && status
                && status->phase==population::Phase::retired && status->retirementAcknowledged;
        }
        if(requestedCount) static_cast<void>(current->activity.round().retirement_acknowledged(
            allActorsRetired,allSourcesAcknowledged));
    }
    std::array<std::uint8_t,4> priorCapture{};
    for(std::size_t i=0;i<current->activity.capture().size();++i) {
        const auto& s=current->activity.capture().state(i);priorCapture[i]=(s.requested?1:0)|(s.ready?2:0)|(s.completed?4:0);
    }
    const auto local=public_event::participant_bridge::local_identity(GetTickCount64());
    // Poll authenticated local placement state before the activity update. A
    // successful observation can therefore unlock the one-shot source-26 pulse
    // in this same server frame; publication alone never marks a target ready.
    poll_transit_targets(*current,GetTickCount64());
    auto frame=current->activity.update(bubble,arrived,selected,openingAdmissionReady,clock,local.identity);
    log_transit_state(*current,owner);
    // Bind generator evidence before the final authority projection can expose
    // the generated population request.
    if(definition.rounds && !bind_round_generator(*current,owner,definition,frame))
        current->observationFailure=true;
    std::array<std::uint8_t,128> nativePending{};
    if(populationLifecycleReady) {
        for(std::size_t i=0;i<definition.populations.size();++i) {
            const auto priorOwner=current->ledgers[i].owner();
            if(priorOwner.valid()) nativePending[i]=nativeEvents::pending_lease(
                {owner,priorOwner,static_cast<std::uint8_t>(definition.populations[i].registry->bubble),true})?1U:0U;
        }
    }
    const auto freeroamNow=GetTickCount64();
    if(current->openWorldReady
        && !current->openWorld.update(freeroamNow,bubble,arrived,current->activity.population(),
            std::span<const coo::NativePopulationLedger<128>>(current->ledgers),nativePending))return {};
    if(current->mercuryFreeroamReady) {
        if(!current->mercuryFreeroam.update(GetTickCount64(),bubble,arrived,current->activity.population(),
            std::span<const coo::NativePopulationLedger<128>>(current->ledgers),nativePending))return {};
    }
    if(populationLifecycleReady) {
        // The Director only stages a ticket. Keep publishing the old source
        // until the native bridge can exclude queued, provisional and still
        // unidentified births while atomically swapping the exact lease.
        for(std::size_t i=0;i<definition.populations.size();++i) {
            const auto renewal=current->activity.population().renewal(i);if(!renewal.pending)continue;
            const auto& cap=definition.populations[i];auto& ledger=current->ledgers[i];
            const auto priorOwner=ledger.owner();const auto counts=ledger.counts();
            coo::Asset asset{cap.registry->key,0,1,cap.slot};
            for(const auto& slot:cap.registry->slots)if(slot.index==cap.slot)asset.definition=slot.descriptorTag;
            const coo::PopulationOwner nextOwner{owner.sessionId,current->activity.population().boot(),
                owner.incarnation.value,asset,renewal.generation+1};
            if(!priorOwner.valid() || renewal.generation!=priorOwner.generation || !renewal.target
                || counts.failed || counts.dead>counts.admitted) {
                current->observationFailure=true;return {};
            }
            const bool settled=current->activity.population().consumed(i) && counts.admitted
                && counts.dead==counts.admitted && !counts.alive && !counts.resident;
            // A provisional birth may finish after the Director staged this
            // ticket. Cancel it without failing the activity; the patrol will
            // require the new actor's real death/retirement and a fresh timer.
            if(!settled) {current->activity.population().cancel_renewal(i);continue;}
            auto staged=ledger;
            if(!staged.renew(priorOwner,nextOwner)) {current->observationFailure=true;return {};}
            const auto result=nativeEvents::renew(
                {owner,priorOwner,static_cast<std::uint8_t>(cap.registry->bubble),true},
                {owner,nextOwner,static_cast<std::uint8_t>(cap.registry->bubble),true});
            if(result==nativeEvents::RenewResult::busy)continue;
            if(result!=nativeEvents::RenewResult::renewed
                || !current->activity.population().commit_renewal(i)) {
                current->observationFailure=true;return {};
            }
            ledger=std::move(staged);current->sourceHandles[i]=0;
        }
    }
    if(current->mercuryFreeroamReady) {
        if(arrived && bubble==definition.bubble && current->mercuryFreeroam.wave_clear_pending()) {
            const auto state=current->mercuryFreeroam.diagnostics();
            std::array<nativeEvents::Lease,2> waveLeases{};bool validWave=state.wave<mercury::freeroam::kWaves.size();
            if(validWave)for(std::size_t side=0;side<waveLeases.size();++side) {
                const auto capability=mercury::freeroam::kWaves[state.wave].capabilities[side];
                const auto source=current->ledgers[capability].owner();
                validWave&=source.valid() && current->activity.population().consumed(capability);
                waveLeases[side]={owner,source,static_cast<std::uint8_t>(definition.populations[capability].registry->bubble),true};
            }
            if(!validWave) {current->observationFailure=true;return {};}
            if(nativeEvents::quiescent(waveLeases) && !current->mercuryFreeroam.commit_wave_clear()) {
                current->observationFailure=true;return {};
            }
        }
        std::uint32_t announcement{};
        if(current->mercuryFreeroam.take_announcement(announcement)) {
            const auto state=current->mercuryFreeroam.diagnostics();std::array<char,320> line{};
            const auto size=std::snprintf(line.data(),line.size(),
                "ev=mercury_faction_war phase=start owner=%016llX incarnation=%llu incident=%08X wave=%zu join_eligible=%u presentation=unavailable policy=reconstructed",
                owner.sessionId,owner.incarnation.value,announcement,state.wave,state.joinEligible?1U:0U);
            if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
    }
    if(const auto status=current->activity.public_initial().state();status!=priorPublicInitial) {
        std::array<char,320> line{};const auto n=std::snprintf(line.data(),line.size(),
            "ev=public_event_initial owner=%016llX incarnation=%llu boot=%016llX state=%u prior=%u development=1 completion=unimplemented schedule=development_probe",
            owner.sessionId,owner.incarnation.value,current->activity.population().boot(),status,priorPublicInitial);
        if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
    if(current->activity.presentation().revision()!=priorPresentation) {
        const auto* cue=current->activity.presentation().presentation();
        if(cue) {
            std::array<char,384> line{};const auto size=std::snprintf(line.data(),line.size(),
                "ev=native_directive phase=requested owner=%016llX incarnation=%llu registry=%08X slot=%u event=%08X variant=%d timer=%u remaining_ticks=%llu anchor=%llu",
                owner.sessionId,owner.incarnation.value,cue->registry,cue->slot,cue->event,cue->variant,
                cue->hasTimer?1U:0U,cue->timer.remaining,cue->timer.anchor);
            if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
                core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
    }
    if(current->activity.device().revision()!=priorDevice)for(std::size_t i=0;i<frame.devices.count;++i) {
        const auto& request=frame.devices.entries[i];
        std::array<char,384> line{};const auto size=std::snprintf(line.data(),line.size(),
            "ev=native_device phase=requested owner=%016llX incarnation=%llu registry=%08X slot=%u position=%.3f position_revision=%d power_revision=%d lock_revision=%d native_applied=unobserved",
            owner.sessionId,owner.incarnation.value,request.registry,request.slot,
            static_cast<double>(request.state.position.value),static_cast<int>(request.state.position.revision),
            static_cast<int>(request.state.power.revision),static_cast<int>(request.state.lock.revision));
        if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
            core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    if(current->activity.generator().revision()!=priorGenerator)for(std::size_t i=0;i<frame.generators.count;++i) {
        const auto& request=frame.generators.entries[i];
        std::array<char,384> line{};const auto size=std::snprintf(line.data(),line.size(),
            "ev=forest_generator phase=requested owner=%016llX incarnation=%llu registry=%08X slot=%u revision=%llu seed=%u enabled=%u native_ready=unobserved completion=unobserved",
            owner.sessionId,owner.incarnation.value,request.registry,request.slot,current->activity.generator().revision(),
            request.state.primary.seed,request.state.primary.enabled?1U:0U);
        if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
            core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    for(std::size_t i=0;i<current->activity.capture().size();++i) {
        const auto& s=current->activity.capture().state(i);
        const auto flags=(s.requested?1:0)|(s.ready?2:0)|(s.completed?4:0);if(flags==priorCapture[i])continue;
        std::array<char,384> line{};const auto size=std::snprintf(line.data(),line.size(),
            "ev=native_capture owner=%016llX incarnation=%llu registry=%08X slot=%u requested=%u native_ready=%u completed=%u entity=%08X progress=%.3f sequence=%llu",
            owner.sessionId,owner.incarnation.value,s.ticket.source.registry,s.ticket.source.slot,s.requested?1U:0U,
            s.ready?1U:0U,s.completed?1U:0U,s.last.entityHandle,static_cast<double>(s.last.progress),s.last.sequence);
        if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
            core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    if(frame.opening.requested!=priorOpening.requested || frame.opening.nativeReady!=priorOpening.nativeReady
        || frame.opening.conflictingSelection!=priorOpening.conflictingSelection
        || frame.opening.dialogueRequested!=priorOpening.dialogueRequested || frame.opening.dialogueSubmitted!=priorOpening.dialogueSubmitted
        || frame.opening.gatewayRequested!=priorOpening.gatewayRequested) {
        const auto& ticket=current->activity.opening().ticket();std::array<char,416> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=adventure_opening owner=%016llX incarnation=%llu boot=%016llX target=%d selection_revision=%llu requested=%u native_ready=%u selection_conflict=%u registry=%08X event=%08X dialogue_requested=%u dialogue_submitted=%u gateway_requested=%u completion=opening_only",
            owner.sessionId,owner.incarnation.value,ticket.boot,ticket.activity,ticket.selectionRevision,
            frame.opening.requested?1U:0U,frame.opening.nativeReady?1U:0U,frame.opening.conflictingSelection?1U:0U,
            ticket.request.registry,ticket.request.event,frame.opening.dialogueRequested?1U:0U,frame.opening.dialogueSubmitted?1U:0U,frame.opening.gatewayRequested?1U:0U);
        if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
            core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    const auto& rally=current->activity.rally();const auto rallyState=rally.diagnostics();
    if(rallyState.phase!=priorRally || rally.failed()!=priorRallyFailure) {
        const auto& receipt=rally.last_observation();
        std::array<char,512> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=public_event phase=%u failed=%u owner=%016llX incarnation=%llu boot=%016llX revision=%llu event=%llu development=1 receipt=placement_only receipt_registry=%08X source=%08X entity=%08X sequence=%llu",
            static_cast<unsigned>(rallyState.phase),rally.failed()?1U:0U,owner.sessionId,owner.incarnation.value,
            rallyState.lease.boot,rallyState.lease.revision,rallyState.lease.event,receipt.receipt.asset.registry,
            receipt.source.member,receipt.entity,receipt.sequence);
        if(size>0 && static_cast<std::size_t>(size)<line.size())core::log::write(core::log::Channel::server,
            rally.failed()?core::log::Level::error:core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    bool ambientDevelopmentRequested{};
    for(std::size_t i=0;i<current->activity.ambient().size();++i) {
        const auto status=current->activity.ambient().diagnostics(i);
        if(status.state!=ambient_population::InitialState::published || priorAmbient[i]==status.state) continue;
        const auto& policy=*current->activity.ambient().policy(i);
        ambientDevelopmentRequested|=policy.development;
        std::array<char,384> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=activity_ambient phase=requested boot=%016llX owner=%016llX incarnation=%llu registry=%08X slot=%u target=%u monitor=%u native_revision=%u selected=%d tactical_slot=%u tactical_row=%d development=%u",
            current->activity.population().boot(),owner.sessionId,owner.incarnation.value,policy.source->registry->key,
            policy.source->slot,policy.initialRequests,policy.monitorSlot,status.nativeRevision,status.selectedPlayers,
            policy.source->tactical.slot,policy.source->tactical.row,policy.development?1U:0U);
        if(size>0 && static_cast<std::size_t>(size)<line.size())
            core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    if(current->activity.animation().revision()!=priorAnimation)
        report_animation(current->activity.animation(),"script");
    if(current->activity.population().revision()!=prior) {
        std::array<char,256> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=native_activity phase=script owner=%016llX incarnation=%llu revision=%llu graph=%u placements=%zu sources=%zu development=%u",
            owner.sessionId,owner.incarnation.value,current->activity.population().revision(),
            static_cast<unsigned>(current->activity.diagnostics().phase),frame.placements.count,frame.populations.count,
            ambientDevelopmentRequested?1U:0U);
        if(size>0 && static_cast<std::size_t>(size)<line.size())
            core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
    const auto now=GetTickCount64();
    if(arrived && now>=current->nextPoll) {
        current->nextPoll=now+500;
        std::array<char,512> text{};std::size_t length{};
        if(read_command(text,length)) {
            std::uint64_t hash=14695981039346656037ULL;
            for(std::size_t i=0;i<length;++i) {hash^=static_cast<unsigned char>(text[i]);hash*=1099511628211ULL;}
            if(hash!=current->lastText) {
                current->lastText=hash;
                const std::string_view content{text.data(),length};
                const auto prefix=content.find_first_not_of(" \t\r\n");
                if(prefix!=std::string_view::npos && content.substr(prefix).starts_with("npc1")) {
                    npc_animation::Command command{};
                    if(!npc_animation::parse(content,command))
                        report_animation(current->activity.animation(),"command",npc_animation::Result::invalid);
                    else if(command.owner==owner) {
                        const auto result=current->activity.animation().owner()
                            ?current->activity.animation().request(command,bubble):npc_animation::Result::unsupported;
                        report_animation(current->activity.animation(),"command",result,command);
                    }
                } else {
                    Command command{};
                    if(!parse(content,command)) report(current->activity.population(),"command",static_cast<unsigned>(Result::invalid));
                    else if(command.owner==owner) {
                        const auto result=current->activity.request_population(command,bubble);
                        report(current->activity.population(),"command",static_cast<unsigned>(result),command);
                    }
                }
            }
        }
    }
    if(definition.openWorld) {
        // The retained roster still owns sources in previously visited zones.
        // Publish their unchanged bodies too: omission resets their native
        // objective/row while resident enemies can remain visible and firing.
        // Activation stays in start_bubble(); targets in unvisited zones are zero.
        frame.populations=current->activity.population().project_retained();
        frame.animations=current->activity.animation().project(bubble);
        if(arrived && !open_world::append_placements(definition.placements,bubble,frame.placements))return {};
    } else if(definition.retainRosterOrdinals || (arrived && (bubble==definition.bubble || definition.activateAcrossRegions))) {
        // The mailbox may have changed a request after update(). Projection is
        // tied to the live activity lease, while command admission stays scoped.
        frame.populations=current->activity.population().project(definition.bubble);
        frame.animations=current->activity.animation().project(definition.bubble);
    }
    if (!equipment_interaction::project(definition.equipmentInteractionGates, account,
            frame.placements)) return {};
    // Publish the observation capability before the authority frame can create
    // actors. Native callbacks enqueue values; this owner drains them in order.
    for(std::size_t r=0;r<frame.populations.count;++r) {
        const auto& request=frame.populations.entries[r];
        for(std::size_t i=0;i<definition.populations.size();++i) {
            const auto& cap=definition.populations[i];
            if(cap.registry->key!=request.source.registry || cap.slot!=request.slot) continue;
            coo::Asset asset{cap.registry->key,0,1,cap.slot};
            for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) asset.definition=slot.descriptorTag;
            const coo::PopulationOwner source{owner.sessionId,current->activity.population().boot(),
                owner.incarnation.value,asset,request.source.generation};
            if(populationLifecycleReady) {
            auto& ledger=current->ledgers[i];const auto priorOwner=ledger.owner();
            if(priorOwner.valid() && priorOwner!=source){current->observationFailure=true;return {};}
            if(!priorOwner.valid() && (!ledger.begin(source)
                || !nativeEvents::bind({owner,source,static_cast<std::uint8_t>(cap.registry->bubble),populationLifecycleReady}))) {
                current->observationFailure=true;return {};
            }
            } else {
            const auto priorSource=current->ledgers[i].owner();
            const bool newCycle=priorSource.valid() && priorSource.source==source.source
                && source.generation>priorSource.generation;
            // The source authority accepts retirement before native actors and
            // the new generation acknowledge it. Put the old ledger into that
            // lifecycle now; source_retired correctly rejects an active ledger.
            if(newCycle && request.source.retireOwned
                && current->ledgers[i].phase()==coo::PopulationPhase::active
                && !current->ledgers[i].retiring(priorSource)) current->observationFailure=true;
            if(newCycle && !request.source.retireOwned) {
                const auto counts=current->ledgers[i].counts();
                if(counts.resident==0 && !current->observationFailure) {
                    const nativeEvents::Lease oldLease{owner,priorSource,cap.registry->bubble};
                    const nativeEvents::Lease newLease{owner,source,cap.registry->bubble};
                    if(!nativeEvents::rebind(oldLease,newLease)) current->observationFailure=true;
                    else {
                        current->ledgers[i]={};current->sourceHandles[i]=0;
                        if(!current->ledgers[i].begin(source)) current->observationFailure=true;
                    }
                } else current->observationFailure=true;
            } else if(!priorSource.valid() && !current->ledgers[i].begin(source)) current->observationFailure=true;
            if(!(newCycle && request.source.retireOwned)
                && !nativeEvents::bind({owner,source,cap.registry->bubble})) current->observationFailure=true;
            }
        }
    }
    current->lastBubble=static_cast<std::uint8_t>(bubble);current->lastArrived=arrived;
    return frame;
}
} // namespace sunrise::server::runtime::activity::native_activity
