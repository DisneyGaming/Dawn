#include <Windows.h>
#include "native_activity_runtime.h"
#include "native_activity_profiles.h"
#include "mercury_freeroam_runtime.h"
#include "open_world_runtime.h"
#include "lost_sector_runtime.h"
#include "moon_lost_sector_runtime.h"
#include "open_world_census.h"
#include "../../../state/activity/native_population_events.h"
#include "../../../state/activity/vendors/catalog.h"
#include "../../../state/activity/open_world_member_observations.h"
#include "../../../state/activity/coo/open_world_member_catalog.h"
#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/runtime.h"
#include "../../../middleware/crypto/random_bytes.h"
#include <cstdio>
#include <memory>
#include <mutex>

namespace sunrise::server::runtime::activity::native_activity {
using population::Service;
using population::Command;
using population::Result;
using population::parse;
namespace {
namespace nativeEvents=state::activity::native_population;
namespace members=state::activity::open_world_members;
struct Entry {
    PersistentActivity activity{};std::uint64_t nextPoll{},lastText{};
    activity_clock::Service clock{};
    // A loose request can create an authored group. The largest Mercury wave
    // raises one source by five requests before renewal, so retain a bounded
    // cohort rather than assuming one request equals one actor.
    std::array<coo::NativePopulationLedger<128>,population::kSourceCapacity> ledgers{};
    std::array<std::uint32_t,population::kSourceCapacity> sourceHandles{};
    mercury::freeroam::Director mercuryFreeroam{};
    open_world::Director openWorld{};
    lost_sector::Director lostSectors{};
    moon_lost_sector::Director moonLostSectors{};
    bool moonLostSectorsReady{};
    std::uint8_t lastBubble{};
    bool mercuryFreeroamReady{};
    bool openWorldReady{};
    bool lostSectorsReady{};
    bool lastArrived{};
    bool observationFailure{};
    struct CensusSource final {
        std::uint32_t generation{},observationRevision{},tacticalRevision{};std::uint64_t renewalRequest{};
        std::uint32_t first{},second{};std::int8_t tacticalRow{-1};bool seen{},observationSeen{};
        bool renewalBusy{};
    };
    std::array<CensusSource,population::kSourceCapacity> censusSources{};
};
std::mutex mutex;
std::array<Entry,16> entries{};
std::uint64_t nextClockEpoch{};
void reset_entry(Entry& entry) noexcept {
    // Entry is process-static and now retains up to 384 actor ledgers. Rebuild it
    // in its own storage so reset never needs a multi-megabyte stack temporary.
    std::destroy_at(&entry);std::construct_at(&entry);
}
open_world_census::Record census_record(const NativeActivityDefinition& definition,
    const Service& service,std::size_t index,std::uint32_t bubble,bool arrived,
    open_world_census::Event event,std::string_view reason={}) noexcept {
    open_world_census::Record record{};record.event=event;record.destination=definition.activity;
    record.reason=reason;record.bubble=bubble;record.arrived=arrived;
    if(index<definition.populations.size()) {
        const auto& cap=definition.populations[index];record.scenario=cap.registry->scenario;
        record.registry=cap.registry->key;record.sourceBubble=cap.registry->bubble;record.sourceSlot=cap.slot;
        record.ruleSlot=cap.rule;record.hasRule=cap.hasRule;
        record.generation=service.generation(index);record.requestSequence=service.source_request(index);
        record.requestedFirst=service.target(index);record.requestedSecond=service.second_target(index);
        const auto tactical=service.tactical(index);record.tacticalProvider=tactical.registry;
        record.tacticalSlot=tactical.slot;record.tacticalRow=tactical.row;record.tacticalRevision=tactical.revision;
        if(const auto* observation=service.observation(index);observation && observation->consumedKnown) {
            record.consumedKnown=true;record.consumedFirst=observation->consumed[0];
            record.consumedSecond=observation->consumedCount>1?observation->consumed[1]:0;
        }
    } else if(!definition.registries.empty())record.scenario=definition.registries.front().scenario;
    return record;
}
void attach_member_census(open_world_census::Record& record,const nativeEvents::Event& event) noexcept {
    if(!members::enabled() || event.kind!=nativeEvents::Kind::admitted)return;
    record.member.evidence="not_captured";
    members::Observation observation;
    // The binding nonce protects release/identical-rebind ABA. The sidecar must
    // also match both salted actor handles, source handle, and full source lease.
    const auto taken=members::take(nativeEvents::capture(event.lease),event,observation);
    if(taken==members::Take::busy){record.member.evidence="capture_contention_lost";return;}
    if(taken!=members::Take::found)return;
    if(observation.conflicted){record.member.evidence="capture_conflict";return;}
    const auto& source=event.lease.source.source;
    if(observation.member.resource!=source.definition) {
        record.member.evidence="member_resource_mismatch";return;
    }
    const auto* choice=members::lookup(source.definition,source.registry,source.slot,observation.member.offset);
    if(!choice){record.member.evidence="member_offset_unmatched";return;}
    record.member.known=true;record.member.evidence="exact_native_member_offset";
    record.member.category=choice->category;record.member.categoryKey=choice->categoryKey;
    record.member.entity=choice->entity;record.member.variant=choice->variant;
    record.member.choice=choice->choice;record.member.weight=choice->weight;
    record.member.kind=observation.member.kind;record.member.offset=choice->memberOffset;
}
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
    std::lock_guard lock(mutex);
    for(const auto& entry:entries) {
        if(entry.activity.population().owner()!=owner) continue;
        definition=entry.activity.definition();output=entry.activity.placements();
        if(definition && definition->openWorld && (!entry.lastArrived
            || !open_world::append_placements(definition->placements,entry.lastBubble,output))) {
            definition=nullptr;output={};return false;
        }
        if(definition)adventure_start::restrict_banners(definition->startRoutes,output);
        return definition!=nullptr;
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
            const auto* mirror=(entry.mercuryFreeroamReady || entry.openWorldReady || entry.lostSectorsReady)
                ?entry.activity.population().observe_retained(object)
                :entry.activity.population().observe(bubble,object);
            if(!mirror) continue;
            const auto* definition=entry.activity.definition();
            std::size_t censusIndex=definition?definition->populations.size():0;
            if(definition)for(std::size_t candidate=0;candidate<definition->populations.size();++candidate) {
                const auto& capability=definition->populations[candidate];
                if(capability.registry->key==object.registryKey && capability.slot==object.slotIndex) {
                    censusIndex=candidate;break;
                }
            }
            if(definition && censusIndex<definition->populations.size()
                && (!entry.censusSources[censusIndex].observationSeen
                    || entry.censusSources[censusIndex].observationRevision!=mirror->revision)) {
                auto record=census_record(*definition,entry.activity.population(),censusIndex,bubble,entry.lastArrived,
                    open_world_census::Event::sourceObservation,"native_source_mirror");
                open_world_census::emit(record);
                entry.censusSources[censusIndex].observationRevision=mirror->revision;
                entry.censusSources[censusIndex].observationSeen=true;
            }
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
                    if(definition && censusIndex<definition->populations.size())
                        open_world_census::emit(census_record(*definition,entry.activity.population(),censusIndex,bubble,entry.lastArrived,
                            open_world_census::Event::taskSelection,"native_cost_selection"));
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
lost_sector::RewardTicket lost_sector_reward(Owner owner,std::uint32_t registryKey,
    std::uint16_t slot,std::uint32_t generation) noexcept {
    if(!owner || !registryKey || !generation || !state::activity::contains(owner))return {};
    std::lock_guard lock(mutex);
    const auto* entry=detail::find_owned_entry(entries,owner,
        [](const Entry& candidate) noexcept {return candidate.activity.population().owner();});
    if(!entry || !entry->lostSectorsReady || !entry->lastArrived)return {};
    const auto ticket=entry->lostSectors.reward_ticket(registryKey,slot,generation);
    if(ticket && ticket.bubble==entry->lastBubble)return ticket;
    return {};
}
lost_sector::RewardTicket lost_sector_reward_current(Owner owner,std::uint32_t registryKey,
    std::uint16_t slot,std::uint32_t generation) noexcept {
    if(!owner || !registryKey || !generation || !state::activity::contains(owner))return {};
    std::lock_guard lock(mutex);
    const auto* entry=detail::find_owned_entry(entries,owner,
        [](const Entry& candidate) noexcept {return candidate.activity.population().owner();});
    if(!entry || !entry->lostSectorsReady)return {};
    return entry->lostSectors.reward_ticket(registryKey,slot,generation);
}
moon_lost_sector::DestructibleRequest lost_sector_object_request(Owner owner,std::uint32_t definition) noexcept {
 if(!owner || !state::activity::contains(owner))return {};
 std::lock_guard lock(mutex);
 auto* entry=detail::find_owned_entry(entries,owner,[](const Entry& value){return value.activity.population().owner();});
 if(!entry || !entry->moonLostSectorsReady || !entry->lastArrived
    || entry->lastBubble!=moon_lost_sector::moon::kRegistries[1].bubble)return {};
 return entry->moonLostSectors.object_request(definition);
}
bool lost_sector_object_observed(const moon_lost_sector::ObjectReceipt& receipt,bool dead,bool replaced) noexcept {
 if(!receipt.valid() || !state::activity::contains(receipt.activity))return false;
 std::lock_guard lock(mutex);
 auto* entry=detail::find_owned_entry(entries,receipt.activity,[](const Entry& value){return value.activity.population().owner();});
 if(!entry || !entry->moonLostSectorsReady || !entry->lastArrived
    || entry->lastBubble!=moon_lost_sector::moon::kRegistries[1].bubble)return false;
 return entry->moonLostSectors.observe_object(receipt,dead,replaced);
}

NativeActivityFrame update(Owner owner,std::uint32_t bubble,bool arrived,
    const NativeActivityDefinition& definition,const adventure_start::wire::Request& selected,
    bool openingAdmissionReady,std::uint32_t populationPrefetchBubble) noexcept {
    if(!owner || bubble>63 || !state::activity::contains(owner)) return {};
    const auto document=document_for(definition);if(!document) return {};
    std::lock_guard lock(mutex);
    Entry* current{};Entry* empty{};
    for(auto& entry:entries) {
        // Detach only when the authoritative activity incarnation was released.
        // This is not a reusable native-source retirement acknowledgement.
        if(entry.activity.population().owner() && !state::activity::contains(entry.activity.population().owner())) {
            nativeEvents::release(entry.activity.population().owner());
            members::release(entry.activity.population().owner());
            adventure::native_bridge::release(entry.activity.population().owner());
            adventure::dialogue_bridge::release(entry.activity.population().owner());
            ambient_population::named_points::release(entry.activity.population().owner());
            capture_bridge::release(entry.activity.population().owner());
            public_event::native_bridge::release(entry.activity.population().owner());
            public_event::deferred_bridge::release(entry.activity.population().owner());
            public_event::keys::bridge::release(entry.activity.population().owner());
            public_event::participant_bridge::release(entry.activity.population().owner());
            public_event::engagement_bridge::release(entry.activity.population().owner());reset_entry(entry);
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
                    static_cast<std::uint8_t>(local.wMinute),configuration)) {reset_entry(*current);return {};}
            current->mercuryFreeroamReady=true;
        }
        if(definition.openWorld) {
            open_world::Configuration configuration{};
            const auto ordinaryCount=definition.openWorld->authored->populations.size();
            if(ordinaryCount>definition.populations.size()) {reset_entry(*current);return {};}
            if(!open_world::configure(*document,configuration)
                || !current->openWorld.begin(owner,current->activity.population().boot(),
                    *definition.openWorld,definition.populations.first(ordinaryCount),configuration)) {
                reset_entry(*current);return {};
            }
            current->openWorldReady=true;
        }
        if(definition.lostSectors) {
            const auto base=definition.lostSectors->capabilityBase;
            if(base>definition.populations.size()
                || !current->lostSectors.begin(owner,current->activity.population().boot(),
                    *definition.lostSectors,definition.populations.subspan(base))) {
                reset_entry(*current);return {};
            }
            current->lostSectorsReady=true;
            if(definition.activity=="luna_freeroam") {
                if(!current->moonLostSectors.begin(owner,current->activity.population().boot(),definition.lostSectors->capabilityBase))return {};
                current->moonLostSectorsReady=true;
            }
        }
        report(current->activity.population(),"ready");
        open_world_census::emit(census_record(definition,current->activity.population(),definition.populations.size(),
            bubble,arrived,open_world_census::Event::runStart,"activity_admitted"));
        if(current->activity.animation().owner()) report_animation(current->activity.animation(),"ready");
    }
    if(current->activity.definition()!=&definition) return {};
    if(current->lastBubble!=bubble || current->lastArrived!=arrived) {
        auto boundary=census_record(definition,current->activity.population(),definition.populations.size(),bubble,arrived,
            open_world_census::Event::boundary,arrived?(current->lastArrived?"bubble_transition":"arrive_or_return"):"leave");
        boundary.priorBubble=current->lastBubble;boundary.priorArrived=current->lastArrived;
        open_world_census::emit(boundary);
        for(std::size_t i=0;i<definition.populations.size();++i)if(current->activity.population().target(i)) {
            auto snapshot=census_record(definition,current->activity.population(),i,bubble,arrived,
                open_world_census::Event::sourceSnapshot,"boundary_retained_state");
            snapshot.counts=current->ledgers[i].counts();snapshot.hasCounts=current->ledgers[i].owner().valid();
            snapshot.failed=current->observationFailure;open_world_census::emit(snapshot);
        }
    }
    const bool populationLifecycleReady=current->mercuryFreeroamReady || current->openWorldReady
        || current->lostSectorsReady;
    // Apply native observations before building this authority frame. Otherwise
    // an accepted death waits for another periodic snapshot to reach the HUD.
    std::array<nativeEvents::Event,256> native{};bool overflow{};
    const auto received=nativeEvents::drain(owner,native,overflow);current->observationFailure|=overflow;
    if(overflow) {
        auto failed=census_record(definition,current->activity.population(),definition.populations.size(),bubble,arrived,
            open_world_census::Event::failure,"native_receipt_overflow");failed.failed=true;open_world_census::emit(failed);
    }
    for(std::size_t e=0;e<received;++e) {
        const auto& event=native[e];
        for(std::size_t i=0;i<definition.populations.size();++i) {
            auto& ledger=current->ledgers[i];if(ledger.owner()!=event.lease.source) continue;
            coo::PopulationIntake result=coo::PopulationIntake::conflict;
            const auto& cap=definition.populations[i];
            if(event.kind==nativeEvents::Kind::sourceRecreated) {
                auto stagedLedger=ledger;std::array<std::uint8_t,8> streamedSurvivors{};
                if(populationLifecycleReady && event.lease.discardStreamedReplicas
                    && (current->sourceHandles[i]==event.previousSourceHandle || current->sourceHandles[i]==0)
                    && stagedLedger.source_recreated(event.lease.source,streamedSurvivors)) {
                    const auto replacements=detail::streamed_replacements(current->activity.population(),i,
                        stagedLedger,streamedSurvivors,definition.populations[i].categories,
                        definition.populations[i].recurringRehydration);
                    bool any{};
                    for(std::size_t category=0;category<streamedSurvivors.size();++category) {
                        if(category>=definition.populations[i].categories && streamedSurvivors[category]) {
                            current->observationFailure=true;return {};
                        }
                        any=any || replacements[category]!=0;
                    }
                    auto stagedPopulation=current->activity.population();
                    if(any) {
                        if(stagedPopulation.last_request()==UINT64_MAX) {current->observationFailure=true;return {};}
                        population::Command command{owner,stagedPopulation.revision(),
                            stagedPopulation.last_request()+1,cap.registry->key,cap.slot,
                            replacements[0],stagedPopulation.boot(),replacements[1]};
                        for(std::size_t category=2;category<streamedSurvivors.size();++category)
                            command.additionalRequested[category-2]=replacements[category];
                        command.categoryCount=cap.categories;
                        if(stagedPopulation.rehydrate(command,cap.registry->bubble)!=population::Result::accepted) {
                            current->observationFailure=true;return {};
                        }
                    }
                    current->activity.population()=std::move(stagedPopulation);ledger=std::move(stagedLedger);
                    current->sourceHandles[i]=event.sourceHandle;result=coo::PopulationIntake::accepted;
                }
            } else if(current->sourceHandles[i]==0 || current->sourceHandles[i]==event.sourceHandle) {
                if(event.kind==nativeEvents::Kind::admitted) {
                    result=ledger.admitted(event.actor,event.memberCategory);
                    if(result==coo::PopulationIntake::accepted) current->sourceHandles[i]=event.sourceHandle;
                } else if(event.kind==nativeEvents::Kind::died) result=ledger.died(event.actor);
                else result=ledger.actor_retired(event.actor);
            }
            if(result!=coo::PopulationIntake::accepted && result!=coo::PopulationIntake::duplicate) current->observationFailure=true;
            if(result==coo::PopulationIntake::accepted && event.kind!=nativeEvents::Kind::sourceRecreated)static_cast<void>(current->activity.public_initial().observe_accepted(event,result));
            const auto counts=ledger.counts();
            std::array<char,384> line{};
            const auto size=std::snprintf(line.data(),line.size(),
                "ev=native_population kind=%s owner=%016llX registry=%08X slot=%u generation=%u actor=%08X source=%08X result=%u admitted=%zu alive=%zu dead=%zu resident=%zu incomplete=%u",
                event.kind==nativeEvents::Kind::admitted?"admitted":event.kind==nativeEvents::Kind::died?"died":
                    event.kind==nativeEvents::Kind::sourceRecreated?"source_recreated":"retired",owner.sessionId,
                event.lease.source.source.registry,event.lease.source.source.slot,event.lease.source.generation,
                event.actor.actor,event.sourceHandle,static_cast<unsigned>(result),counts.admitted,counts.alive,
                counts.dead,counts.resident,current->observationFailure?1U:0U);
            if(size>0 && static_cast<std::size_t>(size)<line.size())
                core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
            auto census=census_record(definition,current->activity.population(),i,bubble,arrived,
                event.kind==nativeEvents::Kind::admitted?open_world_census::Event::actorAdmitted:
                event.kind==nativeEvents::Kind::died?open_world_census::Event::actorDied:
                event.kind==nativeEvents::Kind::sourceRecreated?open_world_census::Event::sourceRecreated:
                open_world_census::Event::actorRetired,"native_receipt");
            census.counts=counts;census.hasCounts=true;census.failed=current->observationFailure;
            // Consume even a rejected/duplicate admission's sidecar, but only an
            // accepted ledger transition below emits it as an admitted actor.
            attach_member_census(census,event);
            if(result==coo::PopulationIntake::accepted)open_world_census::emit(census);
            if(result!=coo::PopulationIntake::accepted && result!=coo::PopulationIntake::duplicate) {
                census.event=open_world_census::Event::failure;census.reason="native_receipt_rejected";census.failed=true;
                open_world_census::emit(census);
            }
        }
    }
    const auto prior=current->activity.population().revision();
    const auto priorAnimation=current->activity.animation().revision();
    const auto priorGenerator=current->activity.generator().revision();
    const auto priorDevice=current->activity.device().revision();
    const auto priorPresentation=current->activity.presentation().revision();
    const auto priorRally=current->activity.rally().diagnostics().phase;
    const auto priorRallyFailure=current->activity.rally().failed();
    std::array<ambient_population::InitialState,32> priorAmbient{};
    for(std::size_t i=0;i<current->activity.ambient().size();++i) priorAmbient[i]=current->activity.ambient().state(i);
    const auto priorOpening=current->activity.opening().frame();
    const auto priorPublicInitial=current->activity.public_initial().state();
    activity_clock::Publication clock{};
    const bool retainedClock=current->clock.domain()
        && (definition.retainRosterOrdinals || current->activity.opening().retains_region(bubble,selected));
    if(!definition.clockFrequencyParameter.empty()
        && (definition.openWorld || definition.lostSectors || bubble==definition.bubble || retainedClock)) {
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
            const bool projected=(definition.openWorld || definition.lostSectors || retainedClock)
                ?current->clock.project_retained(owner,boot_token(),current->clock.domain(),now,clock)
                :current->clock.project(owner,boot_token(),bubble,now,clock);
            if(!projected)return {};
        }
    }
    std::array<std::uint8_t,4> priorCapture{};
    for(std::size_t i=0;i<current->activity.capture().size();++i) {
        const auto& s=current->activity.capture().state(i);priorCapture[i]=(s.requested?1:0)|(s.ready?2:0)|(s.completed?4:0);
    }
    const auto local=public_event::participant_bridge::local_identity(GetTickCount64());
    auto frame=current->activity.update(bubble,arrived,selected,openingAdmissionReady,clock,local.identity);
    std::array<std::uint8_t,population::kSourceCapacity> nativePending{};
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
            std::span<const coo::NativePopulationLedger<128>>(current->ledgers),nativePending,
            populationPrefetchBubble))return {};
    if(current->mercuryFreeroamReady) {
        if(!current->mercuryFreeroam.update(GetTickCount64(),bubble,arrived,current->activity.population(),
            std::span<const coo::NativePopulationLedger<128>>(current->ledgers),nativePending,
            populationPrefetchBubble))return {};
    }
    if(current->lostSectorsReady) {
        const auto base=definition.lostSectors->capabilityBase;
        const auto quiescent=[&](std::size_t,std::uint16_t,const lost_sector::Stage& stage) noexcept {
            std::array<nativeEvents::Lease,64> leases{};
            if(stage.count>leases.size())return false;
            for(std::size_t local=0;local<stage.count;++local) {
                const auto capability=std::size_t(base)+stage.first+local;
                if(capability>=definition.populations.size() || nativePending[capability]
                    || !current->activity.population().consumed(capability))return false;
                const auto source=current->ledgers[capability].owner();
                const auto counts=current->ledgers[capability].counts();
                if(!lost_sector::source_defeated(nativePending[capability]!=0,
                    current->activity.population().consumed(capability),source.valid(),counts.failed,
                    counts.admitted,counts.dead,counts.alive))return false;
                leases[local]={owner,source,
                    static_cast<std::uint8_t>(definition.populations[capability].registry->bubble),true};
            }
            return nativeEvents::quiescent(std::span(leases).first(stage.count));
        };
        if(!current->lostSectors.update(bubble,arrived,current->activity.population(),quiescent,
            populationPrefetchBubble))return {};
        if(!current->lostSectors.append_rewards(bubble,arrived,frame.placements))return {};
    }
    if(current->moonLostSectorsReady) {
        const auto defeated=[&](std::size_t index,std::uint32_t generation) noexcept {
            if(index>=definition.populations.size() || nativePending[index]
                || current->activity.population().generation(index)!=generation
                || !current->activity.population().consumed(index))return false;
            const auto source=current->ledgers[index].owner();const auto counts=current->ledgers[index].counts();
            const nativeEvents::Lease lease{owner,source,
                static_cast<std::uint8_t>(definition.populations[index].registry->bubble),true};
            return source.valid() && source.generation==generation && !counts.failed && counts.admitted
                && counts.dead==counts.admitted && !counts.alive
                && nativeEvents::quiescent(std::span(&lease,1));
        };
        moon_lost_sector::ShieldBatch shields{};
        middleware::bap::activity_message::native::world_device::Batch traversalDevices{};
        if(!current->moonLostSectors.update(bubble,arrived,current->lostSectors,
            current->activity.population(),frame.placements,shields,traversalDevices,defeated)
            || frame.lostSectorShields.count+shields.count>frame.lostSectorShields.entries.size()
            || frame.devices.count+traversalDevices.count>frame.devices.entries.size())return {};
        for(std::size_t i=0;i<shields.count;++i) {
            const auto& source=shields.entries[i];
            frame.lostSectorShields.entries[frame.lostSectorShields.count++]={source.registry->key,
                source.effectSlot,source.filterSlot,source.protectedSource,source.registry->bubble,source.enabled};
        }
        for(std::size_t i=0;i<traversalDevices.count;++i)
            frame.devices.entries[frame.devices.count++]=traversalDevices.entries[i];
    }
    for(std::size_t i=0;i<definition.populations.size();++i) {
        const auto generation=current->activity.population().generation(i);
        const auto first=current->activity.population().target(i);
        const auto second=current->activity.population().second_target(i);
        const auto tactical=current->activity.population().tactical(i);
        auto& priorSource=current->censusSources[i];
        if(generation && first && (!priorSource.seen || priorSource.generation!=generation || priorSource.first!=first
            || priorSource.second!=second || priorSource.tacticalRow!=tactical.row
            || priorSource.tacticalRevision!=tactical.revision)) {
            const auto reason=!priorSource.seen
                ?(arrived && bubble==definition.populations[i].registry->bubble?"arrival_activation"
                    :populationPrefetchBubble==definition.populations[i].registry->bubble
                        ?"prefetch_activation":"retained_authority")
                :priorSource.generation!=generation?"renewal_generation"
                :(priorSource.first!=first || priorSource.second!=second)?"target_changed":"tactical_selection";
            open_world_census::emit(census_record(definition,current->activity.population(),i,bubble,arrived,
                open_world_census::Event::sourceRequest,reason));
            priorSource.seen=true;priorSource.generation=generation;priorSource.first=first;priorSource.second=second;
            priorSource.tacticalRow=tactical.row;priorSource.tacticalRevision=tactical.revision;
        }
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
                auto failed=census_record(definition,current->activity.population(),i,bubble,arrived,
                    open_world_census::Event::failure,"renewal_precondition");failed.counts=counts;failed.hasCounts=true;failed.failed=true;
                open_world_census::emit(failed);
                current->observationFailure=true;return {};
            }
            auto renewalRecord=census_record(definition,current->activity.population(),i,bubble,arrived,
                open_world_census::Event::renewalStarted,"director_ticket");renewalRecord.counts=counts;
            renewalRecord.hasCounts=true;renewalRecord.priorGeneration=renewal.generation;
            renewalRecord.nextGeneration=renewal.generation+1;renewalRecord.requestSequence=renewal.request;
            if(current->censusSources[i].renewalRequest!=renewal.request) {
                open_world_census::emit(renewalRecord);current->censusSources[i].renewalRequest=renewal.request;
                current->censusSources[i].renewalBusy=false;
            }
            const bool settled=current->activity.population().consumed(i) && counts.admitted
                && counts.dead==counts.admitted && !counts.alive && !counts.resident;
            // A provisional birth may finish after the Director staged this
            // ticket. Cancel it without failing the activity; the patrol will
            // require the new actor's real death/retirement and a fresh timer.
            if(!settled) {
                renewalRecord.event=open_world_census::Event::renewalCancelled;renewalRecord.reason="source_not_quiescent";
                open_world_census::emit(renewalRecord);current->activity.population().cancel_renewal(i);
                current->censusSources[i].renewalRequest=0;current->censusSources[i].renewalBusy=false;continue;
            }
            auto staged=ledger;
            if(!staged.renew(priorOwner,nextOwner)) {current->observationFailure=true;return {};}
            const auto result=nativeEvents::renew(
                {owner,priorOwner,static_cast<std::uint8_t>(cap.registry->bubble),true},
                {owner,nextOwner,static_cast<std::uint8_t>(cap.registry->bubble),true});
            if(result==nativeEvents::RenewResult::busy) {
                renewalRecord.event=open_world_census::Event::renewalBusy;renewalRecord.reason="native_mailbox_busy";
                if(!current->censusSources[i].renewalBusy)open_world_census::emit(renewalRecord);
                current->censusSources[i].renewalBusy=true;continue;
            }
            if(result!=nativeEvents::RenewResult::renewed
                || !current->activity.population().commit_renewal(i)) {
                current->observationFailure=true;return {};
            }
            ledger=std::move(staged);current->sourceHandles[i]=0;
            members::release_source(priorOwner);
            renewalRecord=census_record(definition,current->activity.population(),i,bubble,arrived,
                open_world_census::Event::renewalCommitted,"native_and_service_committed");
            renewalRecord.counts=ledger.counts();renewalRecord.hasCounts=true;
            renewalRecord.priorGeneration=renewal.generation;renewalRecord.nextGeneration=renewal.generation+1;
            open_world_census::emit(renewalRecord);
            current->censusSources[i].renewalRequest=0;current->censusSources[i].renewalBusy=false;
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
    if(definition.openWorld || current->lostSectorsReady) {
        // The retained roster still owns sources in previously visited zones.
        // Publish their unchanged bodies too: omission resets their native
        // objective/row while resident enemies can remain visible and firing.
        // Activation stays in the directors; targets are nonzero only after an
        // actual visit or a separately qualified incoming-region prewarm.
        frame.populations=current->activity.population().project_retained();
        frame.animations=current->activity.animation().project(bubble);
        if(definition.openWorld && arrived
            && !open_world::append_placements(definition.placements,bubble,frame.placements))return {};
    } else if(definition.retainRosterOrdinals || (arrived && bubble==definition.bubble)) {
        // The mailbox may have changed a request after update(). Projection is
        // tied to the live activity lease, while command admission stays scoped.
        frame.populations=current->activity.population().project(definition.bubble);
        frame.animations=current->activity.animation().project(definition.bubble);
    }
    adventure_start::restrict_banners(definition.startRoutes,frame.placements);
    // Mercury's authored script keeps stable capability indices, including
    // Vance. Remove vendor-owned output before either observation binding or
    // publication; the vendor lifetime alone admits and retires those actors.
    const auto vendorOwned=[&](std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
        for(const auto& registry:definition.registries)
            if(registry.key==key && state::activity::vendors::owns(registry.scenario,key,type,slot))
                return true;
        return false;
    };
    std::size_t kept{};
    for(std::size_t index=0;index<frame.populations.count;++index) {
        const auto& request=frame.populations.entries[index];
        if(!vendorOwned(request.source.registry,1,request.slot))
            frame.populations.entries[kept++]=request;
    }
    frame.populations.count=kept;kept=0;
    for(std::size_t index=0;index<frame.animations.count;++index) {
        const auto& request=frame.animations.entries[index];
        if(!vendorOwned(request.registry,42,request.slot))
            frame.animations.entries[kept++]=request;
    }
    frame.animations.count=kept;
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
            auto& ledger=current->ledgers[i];const auto priorOwner=ledger.owner();
            if(priorOwner.valid() && priorOwner!=source){current->observationFailure=true;return {};}
            if(!priorOwner.valid() && (!ledger.begin(source)
                || !nativeEvents::bind({owner,source,static_cast<std::uint8_t>(cap.registry->bubble),populationLifecycleReady}))) {
                current->observationFailure=true;return {};
            }
        }
    }
    current->lastBubble=static_cast<std::uint8_t>(bubble);current->lastArrived=arrived;
    return frame;
}
} // namespace sunrise::server::runtime::activity::native_activity
