#pragma once
#include "native_activity_definition.h"
#include "round_activity_runtime.h"
#include "round_environment_service.h"
#include "../../../middleware/bap/activity_message/native/forest_switches.h"
#include "../../../middleware/bap/activity_message/native/status_effect_authority.h"
#include "../../../state/activity/coo/native_player_trigger.h"
#include "../../../state/activity/coo/lifecycle_service.h"
#include "../../../state/activity/native_population_events.h"
#include <algorithm>
#include <memory>
#include <limits>
#include <optional>
#include <cstdio>
#include <windows.h>

namespace sunrise::server::runtime::activity {
/**
 * True while a death in generated traversal must fall back to the client's native safe-position
 * history instead of an authored spawn override.
 *
 * The last credited kill flips the phase to toEncounter while the player is still standing in the
 * branch, so scoping this to Phase::traversal alone leaves the whole toEncounter window publishing
 * the destination's arrival set - and, once the arena is latched, the arena itself. The suppression
 * therefore has to hold until the travel arrival actually qualifies. It cannot break arena entry:
 * entry is the native ho_teleporter prefab physically moving the player, never a spawn-point pick.
 */
[[nodiscard]] constexpr bool traversal_respawn_suppressed(bool started,timed_round::Phase phase,
    bool travelArrivalQualified) noexcept {
    // Before expiry, retain the current branch's authored entry spawn.
    (void)started;(void)phase;(void)travelArrivalQualified;
    return false;
}
struct NativeActivityFrame final {
    population::wire::Batch populations{};
    placement::wire::Batch placements{};
    npc_animation::wire::Batch animations{};
    adventure::OpeningFrame opening{};
    activity_clock::Publication clock{};
    forest_generator::wire::Batch generators{};
    cue_presentation::wire::Batch cues{};
    world_device::wire::Batch devices{};
    public_event::engagement_feedback::wire::Batch engagements{};
    adventure::dialogue_feedback::wire::Batch dialogues{};
    public_event::sequence::wire::Batch sequences{};
    public_event::participant_feedback::wire::Batch eventParticipants{};
    music::wire::Batch music{};
    middleware::bap::activity_message::native::status_effect::Batch statusEffects{};
    state::activity::coo::native_player_trigger::Batch playerTriggers{};
    middleware::bap::activity_message::native::forest_switches::Batch nativeForestSwitches{};
    coo::CompletionPublication completion{};
    std::uint64_t endEpoch{};
    bool restricted{};
    bool nativeTraversalRespawn{};
    // Optional music is presentation-only. A set bit suppresses its body while
    // retaining the rest of the round frame and gameplay state.
    bool optionalPresentationFailure{};
};
// One authoritative activity incarnation. The document outlives both executors
// and their commands. Graph completion leaves persistent services active; it
// does not end free roam or manufacture a native completion receipt.
class PersistentActivity final {
public:
    using Document=coo::script::MissionDocument;
    [[nodiscard]] static const NativeAction* action(const NativeActivityDefinition& definition,
        const coo::CommandSpec& spec) noexcept {
        const NativeAction* found{};
        for(const auto& candidate:definition.actions) {
            const auto& expected=candidate.command;
            if(expected.operation==spec.operation && expected.asset==spec.asset
                && expected.argument==spec.argument && expected.wait==spec.wait) {
                if(found) return nullptr;found=&candidate;
            }
        }
        return found;
    }
    [[nodiscard]] static bool valid(const NativeActivityDefinition& definition,const Document& document) noexcept {
        const auto& views=document.views();
        if(definition.rounds) {
            return round_activity::Runtime::valid(*definition.rounds,definition,document);
        }
        if(!definition.profile || definition.bubble>63 || definition.placements.size()>32 || !views.valid
            || views.missionId!=definition.activity || views.profileId!=definition.profile->id
            || views.mission.modules.size()!=1 || !views.mission.observations.empty()
            || views.mission.modules[0].asset!=definition.persistentModule.asset
            || views.mission.modules[0].id!=definition.persistentModule.id) return false;
        const auto* graph=views.role("persistent");
        if(!graph || graph->domain!="nativeActivity" || graph->definition.schema!=definition.profile->schema
            || !coo::MissionRuntime::valid(views.mission)) return false;
        std::array<ambient_population::InitialPolicy,32> initial{};std::size_t initialCount{};
        if(!ambient_population::configure_initial(definition,document,definition.ambientInitial,initial,initialCount)) return false;
        if(!public_event::RallyRuntime::valid(definition,document))return false;
        if(!public_event::InitialRuntime::valid(definition,document))return false;
        if(!adventure::OpeningRuntime::valid(definition,document))return false;
        if(!native_capture::Runtime::valid(definition,document))return false;
        if(!definition.directives.empty()) {
            if(definition.directiveSource.type!=68 || !definition.directiveSource.definition)return false;
            cue_presentation::Service probe;
            if(!probe.begin({1,{1}},1,definition.directives))return false;
            for(const auto& item:definition.directives) {
                if(item.presentation.registry!=definition.directiveSource.registry
                    || item.presentation.slot!=definition.directiveSource.slot
                    || (item.requiredEvidence&~3U)
                    || ((item.requiredEvidence&1U) && definition.captures.empty())
                    || ((item.requiredEvidence&2U) && definition.generators.empty())
                    || (item.timer!=cue_presentation::TimerOperation::absent && definition.clockFrequencyParameter.empty()))return false;
            }
        }
        if(definition.devices.size()>world_device::wire::kCapacity)return false;
        for(const auto& capability:definition.devices) {
            if(!world_device::valid(capability) || capability.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==capability.registry)registered=true;
            if(!registered)return false;
        }
        if(definition.generators.size()>4)return false;
        for(const auto& capability:definition.generators) {
            if(!forest_generator::valid(capability) || capability.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==capability.registry)registered=true;
            if(!registered || (!capability.seedParameter.empty() && !views.parameter(capability.seedParameter)))return false;
            forest_generator::AnchorConfiguration coordinates{};
            for(std::size_t i=0;i<capability.anchorParameters.size();++i) {
                const auto& names=capability.anchorParameters[i];
                if(!names.column.empty()) {
                    const auto* value=views.parameter(names.column);
                    if(!value)return false;
                    coordinates[i].column=value->value;
                }
                if(!names.height.empty()) {
                    const auto* value=views.parameter(names.height);
                    if(!value)return false;
                    coordinates[i].height=value->value;
                }
            }
            if(!forest_generator::valid_coordinates(capability,coordinates))return false;
        }
        if(!definition.clockFrequencyParameter.empty()) {
            const auto* frequency=views.parameter(definition.clockFrequencyParameter);
            if(!frequency || !frequency->value || frequency->value>1000 || definition.registries.empty())return false;
        }
        if(definition.occupancyWaits.size()>32)return false;
        for(const auto& binding:definition.occupancyWaits) {
            if(!occupancy_wait::valid(binding) || binding.registry->bubble!=definition.bubble)return false;
            bool registered{};
            for(const auto& item:definition.registries)if(&item==binding.registry)registered=true;
            if(!registered)return false;
        }
        if(definition.animations.size()>16) return false;
        for(const auto& capability:definition.animations) {
            if(!npc_animation::valid(capability) || capability.registry->bubble!=definition.bubble) return false;
            // The controller must belong to the same admitted registry profile.
            bool registered{};
            for(const auto& registry:definition.registries)
                if(&registry==capability.registry) registered=true;
            if(!registered) return false;
        }
        std::array<coo::Asset,32> seen{};std::array<bool,4> seenCaptures{};std::size_t used{};
        for(const auto& step:graph->definition.steps) for(const auto& spec:step.commands) {
            const auto* route=action(definition,spec);
            if(!route || used==seen.size()) return false;
            if(spec.operation==coo::Operation::observation) {
                if(spec.wait!=coo::Wait::observed || route->capability>=definition.occupancyWaits.size()
                    || !route->countParameter.empty() || spec.argument!=1)return false;
                const auto& binding=definition.occupancyWaits[route->capability];
                if(spec.asset.registry!=binding.registry->key || spec.asset.slot!=binding.slot || spec.asset.type!=30)return false;
                bool descriptor{};
                for(const auto& slot:binding.registry->slots)if(slot.index==binding.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor)return false;
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==4) {
                if(spec.wait!=coo::Wait::completed || route->capability>=definition.captures.size()
                    || seenCaptures[route->capability] || spec.argument!=1 || !route->countParameter.empty())return false;
                seenCaptures[route->capability]=true;
                const auto& binding=definition.captures[route->capability];
                const auto& placement=definition.placements[binding.placement];
                if(spec.asset.registry!=placement.registry->key || spec.asset.slot!=placement.slot)return false;
                bool descriptor{};for(const auto& slot:placement.registry->slots)if(slot.index==placement.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor)return false;
            } else if(spec.wait!=coo::Wait::requested)return false;
            for(std::size_t i=0;i<used;++i) if(seen[i]==spec.asset
                && !(spec.operation==coo::Operation::mechanic && spec.asset.type==4)
                && spec.operation!=coo::Operation::objective
                && !(spec.operation==coo::Operation::device && spec.asset.type==23)) return false;
            seen[used++]=spec.asset;
            if(spec.operation==coo::Operation::device && spec.asset.type==4) {
                if(route->capability>=definition.placements.size() || !route->countParameter.empty()) return false;
                const auto& cap=definition.placements[route->capability];placement::wire::Batch output;
                if(!placement::project({&cap,1},definition.bubble,output) || output.count!=1
                    || spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot || spec.asset.type!=4) return false;
                bool descriptor{};
                for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor) return false;
            } else if(spec.operation==coo::Operation::device && spec.asset.type==23) {
                if(route->capability>=definition.devices.size() || !route->countParameter.empty())return false;
                const auto& capability=definition.devices[route->capability];
                if(spec.asset.registry!=capability.registry->key || spec.asset.slot!=capability.slot)return false;
                bool descriptor{},supported{};
                for(const auto& slot:capability.registry->slots)if(slot.index==capability.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:capability.actions)if(candidate.id==spec.argument)supported=true;
                if(!descriptor || !supported)return false;
            } else if(spec.operation==coo::Operation::population) {
                if(route->capability>=definition.populations.size()) return false;
                const auto& cap=definition.populations[route->capability];
                const auto* count=views.parameter(route->countParameter);
                if(!population::valid(cap) || cap.registry->bubble!=definition.bubble || !count || !count->value || count->value>63
                    || spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot || spec.asset.type!=1) return false;
                bool descriptor{};
                for(const auto& slot:cap.registry->slots) if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                if(!descriptor) return false;
            } else if(spec.operation==coo::Operation::objective) {
                if(spec.asset!=definition.directiveSource || route->capability>=definition.directives.size()
                    || spec.argument!=definition.directives[route->capability].id)return false;
                if(!route->countParameter.empty()) {
                    const auto* duration=views.parameter(route->countParameter);
                    if(!duration || !duration->value || duration->value>3600000
                        || definition.directives[route->capability].timer!=cue_presentation::TimerOperation::start)return false;
                }
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==37) {
                if(route->capability>=definition.generators.size() || !route->countParameter.empty())return false;
                const auto& cap=definition.generators[route->capability];
                if(spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot)return false;
                bool descriptor{},supported{};
                for(const auto& slot:cap.registry->slots)if(slot.index==cap.slot)
                    descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:cap.actions)if(candidate.id==spec.argument)supported=true;
                if(!descriptor || !supported)return false;
            } else if(spec.operation==coo::Operation::mechanic && spec.asset.type==42) {
                if(route->capability>=definition.animations.size() || !route->countParameter.empty()) return false;
                const auto& cap=definition.animations[route->capability];
                if(spec.asset.registry!=cap.registry->key || spec.asset.slot!=cap.slot) return false;
                bool descriptor{},action{};
                for(const auto& slot:cap.registry->slots)
                    if(slot.index==cap.slot) descriptor=slot.descriptorTag==spec.asset.definition;
                for(const auto& candidate:cap.actions)
                    if(candidate.id==spec.argument && candidate.retailVerified) action=true;
                if(!descriptor || !action) return false;
            } else if(spec.operation!=coo::Operation::observation
                && !(spec.operation==coo::Operation::mechanic && spec.asset.type==4))return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(population::Owner owner,const NativeActivityDefinition& definition,
        std::shared_ptr<const Document> document,std::uint64_t boot) noexcept {
        if(document_ || !document || !valid(definition,*document)) return false;
        if(definition.triggeredPlacementPoses.size()>triggeredPoses_.size())return false;
        for(std::size_t i=0;i<definition.triggeredPlacementPoses.size();++i) {
            const auto& b=definition.triggeredPlacementPoses[i];
            if(!b.registry || b.triggerSlot>32767 || !std::isfinite(b.closed) || !std::isfinite(b.opened))return false;
            unsigned matches{};
            for(const auto& c:definition.placements)
                if(c.registry && c.registry->key==b.registry && c.slot==b.placementSlot && c.generation)++matches;
            if(matches!=1)return false;
            unsigned triggers{};
            for(const auto& group:definition.registries)if(group.key==b.registry)
                for(const auto& slot:group.slots)if(slot.index==b.triggerSlot && slot.type==31)++triggers;
            if(triggers!=1)return false;
            for(std::size_t j=0;j<i;++j) {
                const auto& old=definition.triggeredPlacementPoses[j];
                if(old.registry==b.registry && (old.placementSlot==b.placementSlot || old.triggerSlot==b.triggerSlot))return false;
            }
        }
        if(definition.rounds) return begin_round(owner,definition,std::move(document),boot);
        population::Service population;
        npc_animation::Service animation;
        ambient_population::InitialActivation<> ambient;
        public_event::RallyRuntime rally;
        public_event::InitialRuntime publicInitial;
        adventure::OpeningRuntime opening;
        occupancy_wait::Service occupancy;
        native_capture::Runtime capture;
        forest_generator::Service generator;
        world_device::Service device;
        cue_presentation::Service presentation;
        std::array<cue_presentation::Action,8> directives{};
        if(definition.directives.size()>directives.size())return false;
        std::copy(definition.directives.begin(),definition.directives.end(),directives.begin());
        for(const auto& route:definition.actions)if(route.command.operation==coo::Operation::objective && !route.countParameter.empty()) {
            const auto* duration=document->views().parameter(route.countParameter);
            if(route.capability>=definition.directives.size() || !duration
                || !activity_clock::wire::from_milliseconds(duration->value,directives[route.capability].durationTicks))return false;
        }
        std::array<std::uint32_t,4> seeds{};
        std::array<forest_generator::AnchorConfiguration,4> anchors{};
        for(std::size_t i=0;i<definition.generators.size();++i) {
            const auto name=definition.generators[i].seedParameter;
            if(!name.empty())seeds[i]=document->views().parameter(name)->value;
            for(std::size_t j=0;j<definition.generators[i].anchorParameters.size();++j) {
                const auto& names=definition.generators[i].anchorParameters[j];
                if(!names.column.empty())anchors[i][j].column=document->views().parameter(names.column)->value;
                if(!names.height.empty())anchors[i][j].height=document->views().parameter(names.height)->value;
            }
        }
        std::array<ambient_population::InitialPolicy,32> initial{};std::size_t initialCount{};
        if(!population.begin(owner,definition.populations,boot)
            || (!definition.animations.empty() && !animation.begin(owner,boot,definition.animations))
            || !ambient_population::configure_initial(definition,*document,definition.ambientInitial,initial,initialCount)
            || !rally.begin(owner,boot,definition,*document)
            || !publicInitial.begin(owner,boot,definition,*document)
            || !opening.begin(owner,boot,definition,*document)
            || !occupancy.begin(owner,boot,definition.occupancyWaits)
            || !capture.begin(definition,*document)
            || !generator.begin(owner,boot,definition.generators,std::span(seeds).first(definition.generators.size()),
                std::span(anchors).first(definition.generators.size()))
            || !device.begin(owner,boot,definition.devices)
            || (!definition.directives.empty() && !presentation.begin(owner,boot,std::span(directives).first(definition.directives.size())))
            || (initialCount && !ambient.begin(owner,boot,std::span(initial).first(initialCount)))) return false;
        population_=std::move(population);animation_=std::move(animation);ambient_=std::move(ambient);rally_=std::move(rally);
        opening_=std::move(opening);publicInitial_=std::move(publicInitial);
        occupancy_=std::move(occupancy);
        capture_=std::move(capture);
        generator_=std::move(generator);
        device_=std::move(device);
        presentation_=std::move(presentation);
        definition_=&definition;document_=std::move(document);return true;
    }
    [[nodiscard]] NativeActivityFrame update(std::uint32_t bubble,bool arrived,
        const adventure_start::wire::Request& selected={},bool openingAdmissionReady=true,
         activity_clock::Publication clock={},public_event::participant_feedback::LocalIdentity local={}) noexcept {
        if(!document_)return {};
        if(clock && (clock.domain.owner!=population_.owner() || clock.domain.boot!=population_.boot()
            || clock.domain.bubble!=definition_->bubble || definition_->registries.empty()
             || clock.domain.scenario!=definition_->registries.front().scenario))return {};
        if(roundMode_) return update_round(bubble,arrived,clock);
        NativeActivityFrame frame{};
        capture_.clock(clock);
        clock_=clock;
        if(arrived && (bubble==definition_->bubble || definition_->activateAcrossRegions)) {
            Ports ports(*this);
            const coo::MissionInput input{population_.owner().incarnation.value,0,static_cast<int>(bubble*8U),true,true};
            frame=composition_.update<NativeActivityFrame>(document_->views().mission,input,ports);
        } else if(definition_->retainRosterOrdinals) {
            // A region selection is not activity retirement. Publishing empty
            // bodies here resets native source generations and placement state
            // even when their roster descriptors and owners are still live.
            if(!project_authority(frame))return {};
        }
        frame.opening=opening_.update(bubble,arrived && openingAdmissionReady,selected);frame.clock=clock;
        if(!publicInitial_.update(bubble,arrived,openingAdmissionReady,selected,population_,frame.placements,frame.cues,frame.engagements,rally_.used(),clock,local))return {};
        if(!publicInitial_.append_participants(frame.eventParticipants))return {};
        if(!publicInitial_.append_dialogues(frame.dialogues))return {};
        if(!publicInitial_.append_sequences(frame.sequences))return {};
        if(!publicInitial_.append_music(frame.music))return {};
        if(!publicInitial_.append_world(frame.placements,frame.devices))return {};
        return frame;
    }
    // Named-point-dependent sources are owned by the finite initial gate. A
    // development command must not bypass its parameter, occupancy or readiness.
    [[nodiscard]] population::Result request_population(const population::Command& command,std::uint32_t bubble) noexcept {
        if(!definition_ || publicInitial_.owns(command.registry,command.slot))return population::Result::unsupported;
        for(const auto& binding:definition_->ambientInitial) {
            if(!binding.namedDependency || binding.capability>=definition_->populations.size())continue;
            const auto& capability=definition_->populations[binding.capability];
            if(capability.registry->key==command.registry && capability.slot==command.slot)
                return population::Result::unsupported;
        }
        return population_.request(command,bubble);
    }
    [[nodiscard]] population::Service& population() noexcept {return population_;}
    [[nodiscard]] const population::Service& population() const noexcept {return population_;}
    [[nodiscard]] npc_animation::Service& animation() noexcept {return animation_;}
    [[nodiscard]] const npc_animation::Service& animation() const noexcept {return animation_;}
    [[nodiscard]] ambient_population::InitialActivation<>& ambient() noexcept {return ambient_;}
    [[nodiscard]] const ambient_population::InitialActivation<>& ambient() const noexcept {return ambient_;}
    [[nodiscard]] const public_event::RallyRuntime& rally() const noexcept {return rally_;}
    [[nodiscard]] public_event::InitialRuntime& public_initial() noexcept {return publicInitial_;}
    [[nodiscard]] const public_event::InitialRuntime& public_initial() const noexcept {return publicInitial_;}
    [[nodiscard]] const adventure::OpeningRuntime& opening() const noexcept {return opening_;}
    [[nodiscard]] const NativeActivityDefinition* definition() const noexcept {return definition_;}
    [[nodiscard]] const native_capture::Runtime& capture() const noexcept {return capture_;}
    [[nodiscard]] const forest_generator::Service& generator() const noexcept {return generator_;}
    [[nodiscard]] const world_device::Service& device() const noexcept {return device_;}
    [[nodiscard]] const cue_presentation::Service& presentation() const noexcept {return presentation_;}
    [[nodiscard]] bool observe_occupancy(population::Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const ambient_population::sense::SenseObject& object) noexcept {
        return occupancy_.observe(owner,boot,bubble,object,[this](coo::Event event) noexcept {
            return roundMode_ ? rounds_.enqueue(event) : executor_.enqueue(event);
        });
    }
    [[nodiscard]] bool retirement_requested(std::uint16_t index) const noexcept {
        if (!definition_ || !definition_->rounds) return false;
        for (std::size_t i = 0; i < rounds_.encounter_population_count(); ++i)
            if (rounds_.encounter_population_index(i) == index) return true;
        return false;
    }
    [[nodiscard]] bool request_mid_populations() noexcept {
        if (!roundMode_ || !rounds_.mid_ready()
            || rounds_.selected_encounter() >= definition_->rounds->encounters.size()) return false;
        const auto& encounter = definition_->rounds->encounters[rounds_.selected_encounter()];
        bool accepted = true;
        for (const auto index : encounter.midPopulationIndices)
            accepted &= request_round_population_index(index);
        if (accepted) rounds_.mark_mid_requested();
        return accepted;
    }
    [[nodiscard]] bool pause_round_hud() noexcept {
        return definition_ && definition_->rounds
            && request_round_presentation(definition_->rounds->hud.pauseAction,0);
    }
    /** Pure projection of requests already accepted by this activity's script. */
    [[nodiscard]] placement::wire::Batch placements() const noexcept {
        placement::wire::Batch output{};
        if(!definition_) return output;
        std::array<placement::Capability,32> active{};std::size_t count{};
        for(std::size_t i=0;i<definition_->placements.size();++i)
            if(placements_[i]) active[count++]=definition_->placements[i];
        static_cast<void>(placement::project(std::span(active).first(count),definition_->bubble,output));
        if(!rally_.append(definition_->bubble,output))return {};
        if(!capture_.append(output) || !publicInitial_.append(output) || !append_triggered_poses(output))return {};
        return output;
    }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept {return document_?document_->fingerprint():0;}
    [[nodiscard]] coo::Diagnostics diagnostics() const noexcept {
        return roundMode_ ? rounds_.diagnostics() : executor_.diagnostics();
    }
    [[nodiscard]] round_activity::Runtime& round() noexcept {return rounds_;}
    [[nodiscard]] const round_activity::Runtime& round() const noexcept {return rounds_;}
    [[nodiscard]] activity_clock::Publication clock() const noexcept {return clock_;}
    void release_owner() noexcept {environment_.release_owner();roundMusic_.retire();transitEffects_.release_owner();}
    /**
     * Admits the round definition's experimental reward placements for this run. Off unless the
     * host switch is on: native placement authority validates one frame as a whole, so a single
     * roster slot without the auth flag voids every placement in it, the authored chest included.
     */
    void admit_experimental_reward_placements(bool enabled) noexcept {
        experimentalRewardPlacements_=enabled;
    }
    [[nodiscard]] bool observe_transit_effect(population::Owner owner,std::uint64_t boot,
        std::uint32_t bubble,const ambient_population::sense::SenseObject& object) noexcept {
        return transitEffects_.observe(owner,boot,bubble,object,clock_?clock_.elapsedTicks:UINT64_MAX);
    }
    [[nodiscard]] std::size_t pending_transit_targets(
        std::span<transit_effect::TargetRequest> output) const noexcept {
        return transitEffects_.pending_targets(output);
    }
    [[nodiscard]] bool observe_transit_target(
        const transit_effect::TargetRequest& request) noexcept {
        return transitEffects_.observe_target(request);
    }
    [[nodiscard]] transit_effect::Diagnostics transit_diagnostics() const noexcept {
        return transitEffects_.diagnostics();
    }
    /** Bits: 1 prepared, 2 targets ready, 4 trigger ok, 8 respawn destination set, 16 pulsed. */
    [[nodiscard]] std::uint8_t transit_gates() const noexcept {return transitGates_;}
    /** The exact predicate published as NativeActivityFrame::nativeTraversalRespawn. */
    [[nodiscard]] bool traversal_respawn() const noexcept {
        return rounds_.started()
            && traversal_respawn_suppressed(true,rounds_.round_snapshot().phase,
                rounds_.travel_arrival_qualified());
    }
    /** True once a qualified travel arrival latched this round's respawn destination. */
    [[nodiscard]] bool respawn_latched() const noexcept {return respawnLatched_;}
    [[nodiscard]] bool observe_player_trigger(population::Owner owner,std::uint64_t boot,
        std::uint32_t registry,std::uint16_t slot,std::uint32_t object) noexcept {
        if(owner!=population_.owner() || boot!=population_.boot())return false;
        bool accepted=transitEffects_.observe_player_trigger(registry,slot,object);
        if(!definition_ || !object)return accepted;
        for(std::size_t i=0;i<definition_->triggeredPlacementPoses.size();++i) {
            const auto& binding=definition_->triggeredPlacementPoses[i];
            if(binding.registry!=registry || binding.triggerSlot!=slot)continue;
            const auto projected=placements();
            if(placement::wire::find(projected,registry,4,binding.placementSlot)) {
                triggeredPoses_[i]=true;accepted=true;
            }
        }
        return accepted;
    }
    [[nodiscard]] bool publication_pending() const noexcept {
        return definition_ && definition_->rounds && rounds_.started()
            && transitEffects_.native_targets()
            && (rounds_.travel_pending()
                || (rounds_.travel_arrival_pending() && !transitEffects_.arrival_qualified()));
    }
private:
    [[nodiscard]] bool append_triggered_poses(placement::wire::Batch& output) const noexcept {
        if(!definition_ || output.count>output.entries.size())return false;
        for(std::size_t i=0;i<definition_->triggeredPlacementPoses.size();++i) {
            const auto& binding=definition_->triggeredPlacementPoses[i];
            for(std::size_t j=0;j<output.count;++j) {
                auto& request=output.entries[j];
                if(request.registry!=binding.registry || request.slot!=binding.placementSlot)continue;
                if(request.pose || !request.generation)return false;
                middleware::bap::activity_message::native::generic_device::State pose{};
                pose.position={triggeredPoses_[i]?2:1,-1,triggeredPoses_[i]?binding.opened:binding.closed};
                request.pose=pose;
            }
        }
        return true;
    }
    // Project accepted owner-scoped requests without running the graph, starting
    // a population, or consuming a regional trigger in a different bubble.
    [[nodiscard]] bool project_authority(NativeActivityFrame& frame) const noexcept {
        frame.populations=population_.project(definition_->bubble);
        frame.animations=animation_.project(definition_->bubble);
        frame.generators=generator_.project(definition_->bubble);
        frame.devices=device_.project(definition_->bubble);
        frame.cues=presentation_.project();
        // Hide only the presentation timer. Retain the service clock so later objective actions
        // can still retain/pause it without restarting or invalidating the round.
        if(rounds_.started() && rounds_.round_snapshot().expired)
            for(std::size_t i=0;i<frame.cues.count;++i)frame.cues.entries[i].hasTimer=false;
        frame.statusEffects=environment_.project(definition_->bubble);
        if(!transitEffects_.append(frame.statusEffects,definition_->bubble))return false;
        frame.nativeForestSwitches={};
        const auto forestSwitches=environment_.forestSwitches();
        if(forestSwitches.size()>frame.nativeForestSwitches.entries.size())return false;
        for(const auto& authored:forestSwitches) {
            auto& entry=frame.nativeForestSwitches.entries[frame.nativeForestSwitches.count++];
            entry={authored.key,authored.value};
        }
        frame.placements={};
        std::array<placement::Capability,32> active{};std::size_t count{};
        for(std::size_t i=0;i<definition_->placements.size();++i)
            if(placements_[i])active[count++]=definition_->placements[i];
        if(!placement::project(std::span(active).first(count),definition_->bubble,frame.placements)
            || !rally_.append(definition_->bubble,frame.placements) || !capture_.append(frame.placements))return false;
        if(!transitEffects_.append_targets(frame.placements,definition_->bubble)
            || !transitEffects_.append_trigger(frame.playerTriggers)
            || !append_triggered_poses(frame.placements))return false;
        for(std::size_t i=0;i<definition_->triggeredPlacementPoses.size();++i) {
            const auto& binding=definition_->triggeredPlacementPoses[i];
            if(!triggeredPoses_[i] && placement::wire::find(frame.placements,binding.registry,4,binding.placementSlot)
                && !coo::native_player_trigger::append(frame.playerTriggers,binding.registry,
                    binding.triggerSlot,definition_->bubble,population_.boot()))return false;
        }
        return true;
    }
    struct Driver final:coo::Services {
        PersistentActivity& owner;explicit Driver(PersistentActivity& value):owner(value){}
        bool publish(const coo::Command& command) noexcept override {
            const auto* route=action(*owner.definition_,command.spec);if(!route) return false;
            if(command.spec.operation==coo::Operation::observation)
                return owner.occupancy_.arm(route->capability,command.token);
            if(command.spec.operation==coo::Operation::device && command.spec.asset.type==23) {
                auto& device=owner.device_;
                if(device.last_request()==UINT64_MAX)return false;
                const auto result=device.request({device.owner(),device.boot(),device.revision(),
                    device.last_request()+1,command.spec.asset.registry,command.spec.argument,command.spec.asset.slot},
                    owner.definition_->bubble);
                return result==world_device::Result::accepted || result==world_device::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::device) {
                owner.placements_[route->capability]=true;return true;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==4)
                return owner.capture_.start(route->capability,command.token);
            if(command.spec.operation==coo::Operation::objective) {
                auto& presentation=owner.presentation_;
                if(presentation.last_request()==UINT64_MAX)return false;
                std::uint32_t evidence{};
                bool captured=owner.capture_.size()!=0;
                for(std::size_t i=0;i<owner.capture_.size();++i)captured&=owner.capture_.state(i).completed;
                if(captured)evidence|=1U;
                const auto generators=owner.generator_.project(owner.definition_->bubble);
                bool requested=generators.count!=0 && generators.count==owner.definition_->generators.size();
                for(std::size_t i=0;i<generators.count;++i)requested&=generators.entries[i].state.primary.enabled;
                if(requested)evidence|=2U;
                const auto result=presentation.request({presentation.owner(),presentation.boot(),presentation.revision(),
                    presentation.last_request()+1,command.spec.argument,evidence,
                    owner.clock_?owner.clock_.elapsedTicks:UINT64_MAX});
                return result==cue_presentation::Result::accepted || result==cue_presentation::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==37) {
                auto& generator=owner.generator_;
                if(generator.last_request()==UINT64_MAX)return false;
                const auto result=generator.request({generator.owner(),generator.boot(),generator.revision(),
                    generator.last_request()+1,command.spec.asset.registry,command.spec.argument,command.spec.asset.slot},
                    owner.definition_->bubble);
                return result==forest_generator::Result::accepted || result==forest_generator::Result::unchanged;
            }
            if(command.spec.operation==coo::Operation::mechanic && command.spec.asset.type==42) {
                auto& animation=owner.animation_;
                if(animation.last_request()==UINT64_MAX) return false;
                const npc_animation::Command request{animation.owner(),animation.boot(),animation.revision(),
                    animation.last_request()+1,command.spec.asset.registry,command.spec.argument,
                    command.spec.asset.slot,false,false};
                return animation.request(request,owner.definition_->bubble)==npc_animation::Result::accepted;
            }
            if(command.spec.operation!=coo::Operation::population) return false;
            const auto* count=owner.document_->views().parameter(route->countParameter);if(!count) return false;
            auto& source=owner.population_;
            if(source.last_request()==UINT64_MAX) return false;
            const auto result=source.request({source.owner(),source.revision(),source.last_request()+1,
                command.spec.asset.registry,command.spec.asset.slot,static_cast<std::uint8_t>(count->value),source.boot()},owner.definition_->bubble);
            return result==population::Result::accepted || result==population::Result::unchanged;
        }
        // Failure cancels future graph requests. Keep already-published native
        // leases until activity teardown; cancellation is not actor retirement.
        void cancel(const coo::Command& command) noexcept override {owner.occupancy_.cancel(command.token);}
    };
    struct RoundPorts final {
        PersistentActivity& owner;
        explicit RoundPorts(PersistentActivity& value) noexcept:owner(value){}
        [[nodiscard]] bool publish(round_activity::Operation operation,const coo::Command& command) noexcept {
            const auto& definition=*owner.definition_;
            const auto& rounds=*definition.rounds;
            const auto platform=owner.rounds_.selected_platform();
            if(platform==round_activity::kMissingIndex || platform>=rounds.platforms.size()) {
                if(operation!=round_activity::Operation::prepareEntry)return false;
            }
            switch(operation) {
            case round_activity::Operation::prepareEntry:
                {
                const auto round=owner.rounds_.round_snapshot().token.round;
                const auto selected=static_cast<std::size_t>((round-1U)%rounds.platforms.size());
                if(!owner.rounds_.select_platform(command.token,static_cast<std::uint16_t>(selected)))return false;
                if(owner.environment_.started()) {
                    const auto result=owner.environment_.apply_round(round);
                    if(result!=round_environment::Result::accepted
                        && result!=round_environment::Result::unchanged)return false;
                }
                for(const auto index:rounds.platforms[selected].placementIndexes)
                    if(index>=owner.placements_.size())return false;else owner.placements_[index]=true;
                const auto action=round==1 ? rounds.hud.initialEnterAction : rounds.hud.enterAction;
                const std::optional<std::int32_t> variant=owner.environment_.started()
                    ? std::optional<std::int32_t>{owner.environment_.entryHudVariant()}
                    : std::nullopt;
                return owner.request_round_device(rounds.platforms[selected].deviceIndex,1)
                    && owner.request_round_presentation(action,0,variant);
                }
            case round_activity::Operation::plateOccupied:
                return owner.occupancy_.delivered(rounds.platforms[platform].occupancyIndex)
                    ? owner.occupancy_.rearm(rounds.platforms[platform].occupancyIndex,command.token)
                    : owner.occupancy_.arm(rounds.platforms[platform].occupancyIndex,command.token);
            case round_activity::Operation::capture:
                {
                const auto index=rounds.platforms[platform].captureIndex;
                const auto& state=owner.capture_.state(index);
                const auto armed=state.completed
                    ? owner.capture_.rearm(index,command.token,state.ticket.generation+1U)
                    : owner.capture_.start(index,command.token);
                return armed && owner.request_round_device(rounds.platforms[platform].deviceIndex,2);
                }
            case round_activity::Operation::startTraversal:
                if(!owner.round_capture_completed(platform))return false;
                if(!owner.request_round_generator(rounds.generatorEnableAction))return false;
                if(!owner.round_generator_enabled())return false;
                {
                const auto round=owner.rounds_.round_snapshot().token.round;
                const auto action=round==1 ? rounds.hud.startAction : rounds.hud.resumeAction;
                if(!owner.request_round_presentation(action,3U))return false;
                const auto variant=static_cast<std::int64_t>(round)<rounds.hud.maximumBranchVariant
                    ? static_cast<std::int32_t>(round) : rounds.hud.maximumBranchVariant;
                return owner.update_round_presentation_variant(variant);
                }
            case round_activity::Operation::progressFull:
                return owner.rounds_.arm_progress(command.token);
            case round_activity::Operation::travelEncounter:
                {
                const auto seed=owner.round_generator_seed();
                if(!seed || !owner.rounds_.prepare_travel(command.token,seed))return false;
                // Keep the visible floor until the cohort has arrived at the
                // arena. encounter_arrived publishes disable, offscreen;
                // return travel waits for that native cleanup to finish.
                return true;
                }
            case round_activity::Operation::spawnEncounter:
                if(!owner.rounds_.arm_spawn(command.token))return false;
                for(std::size_t i=0;i<rounds.encounters[owner.rounds_.selected_encounter()].introPopulationIndices.size();++i)
                    if(!owner.request_round_population_index(rounds.encounters[owner.rounds_.selected_encounter()].introPopulationIndices[i]))return false;
                return owner.request_round_population_index(
                    rounds.encounters[owner.rounds_.selected_encounter()].bossPopulationIndex);
            case round_activity::Operation::retireEncounter:
                if(owner.rounds_.round_snapshot().phase==timed_round::Phase::rewards) {
                    if(owner.environment_.started()) {
                        const auto result=owner.environment_.release();
                        if(result!=round_environment::Result::accepted
                            && result!=round_environment::Result::unchanged)return false;
                    }
                    for(const auto index:rounds.rewardPlacementIndices)
                        if(index>=owner.placements_.size())return false;else owner.placements_[index]=true;
                    if(owner.experimentalRewardPlacements_) {
                        // [owner] All five authored coffers spawn every run; each opens with a Cipher
                        // Decoder. The count does not depend on branches cleared (owner, 15 Sep 2026).
                        for(const auto index:rounds.experimentalRewardPlacementIndices)
                            if(index>=owner.placements_.size())return false;else owner.placements_[index]=true;
                    }
                    // The chest plinth device: the authored prefab's only drivable part.
                    if(rounds.rewardDeviceIndex!=round_activity::kMissingIndex
                        && !owner.request_round_device(rounds.rewardDeviceIndex,1))return false;
                    for(const auto index:rounds.rewardPopulationIndices)
                        if(!owner.request_round_population_index(index,false))return false;
                    if(!owner.resultsPublished_) {
                        const auto completed=owner.rounds_.round_snapshot().completedRounds;
                        const auto last=rounds.hud.resultsVariants.size()-1U;
                        const auto variant=rounds.hud.resultsVariants.empty()
                            ? std::optional<std::int32_t>{}
                            : std::optional<std::int32_t>{rounds.hud.resultsVariants[
                                static_cast<std::size_t>(completed<last?completed:last)]};
                        if(!owner.request_round_presentation(rounds.hud.resultsAction,0,variant))return false;
                        owner.resultsPublished_=true;
                    }
                }
                if(!owner.rounds_.begin_retirement(command.token))return false;
                for(std::size_t i=0;i<owner.rounds_.encounter_population_count();++i) {
                    const auto index=owner.rounds_.encounter_population_index(i);
                    if(index>=owner.definition_->populations.size())return false;
                    auto& service=owner.population_;
                    if(service.last_request()==UINT64_MAX)return false;
                    const auto& capability=owner.definition_->populations[index];
                    const auto result=service.request_retirement(service.owner(),service.boot(),service.revision(),
                        service.last_request()+1,capability.registry->key,capability.slot);
                    if(result!=population::Result::accepted && result!=population::Result::duplicate)return false;
                }
                return true;
            case round_activity::Operation::travelEntry:
                {
                const auto round=owner.rounds_.round_snapshot().token.round;
                const auto& landing=rounds.platforms[static_cast<std::size_t>(round%rounds.platforms.size())];
                // Arrival cannot activate the floor: the player needs it before
                // the native teleport fires. Transit separately qualifies its
                // committed placement children before publishing the effect.
                for(const auto index:landing.placementIndexes)
                    if(index>=owner.placements_.size())return false;else owner.placements_[index]=true;
                const auto destination=landing.transitDestinationId;
                if(!owner.rounds_.prepare_return_travel(command.token,destination))return false;
                return true; // Retried after the offscreen generator reset drains.
                }
            case round_activity::Operation::travelArrived:
                return owner.rounds_.arm_travel_arrival(command.token);
            case round_activity::Operation::travelRewards:
                {
                if(!owner.rounds_.prepare_reward_travel(command.token,rounds.rewardDestinationId))return false;
                return true;
                }
            case round_activity::Operation::bossDead:
                return owner.rounds_.arm_boss_dead(command.token);
            case round_activity::Operation::complete:
                return owner.rounds_.request_completion(GetTickCount64(),
                    owner.clock_ ? owner.clock_.elapsedTicks : (std::numeric_limits<std::uint64_t>::max)());
            }
            return false;
        }
        void cancel(round_activity::Operation operation,const coo::Command& command) noexcept {
            if(operation==round_activity::Operation::plateOccupied)owner.occupancy_.cancel(command.token);
        }
        [[nodiscard]] bool encounter_arrived() noexcept {
            const auto& rounds=*owner.definition_->rounds;
            // The arena objective is the Terror whether or not the clock has expired. Re-issuing
            // the collapse cue with variant 0 on an expired arrival left the objective unchanged in
            // game (2026-09-14): the Terror cue appeared only when the boss death paused it.
            return owner.request_round_generator(rounds.generatorDisableAction)
                && owner.request_round_presentation(rounds.hud.terrorAction,0,std::nullopt);
        }
    };
    struct Ports final:coo::MissionPorts<NativeActivityFrame> {
        PersistentActivity& owner;explicit Ports(PersistentActivity& value):owner(value){}
        void update_module(std::uint32_t module,const coo::MissionInput& input,NativeActivityFrame& frame) noexcept override {
            if(module!=owner.definition_->persistentModule.id) return;
            if(!owner.started_) {
                owner.started_=owner.executor_.start(owner.document_->views().role("persistent")->definition,input.run);
                if(!owner.started_) return;
            }
            Driver driver(owner);
            if(!owner.capture_.poll([&](coo::Event event) noexcept {return owner.executor_.enqueue(event);})) {
                owner.executor_.cancel(driver);return;
            }
            owner.executor_.update(driver);
            for(std::size_t i=0;i<owner.ambient_.size();++i)
                if(owner.ambient_.state(i)==ambient_population::InitialState::eligible)
                    static_cast<void>(owner.ambient_.activate(i,owner.population_,owner.definition_->bubble));
            if(!owner.rally_.update(owner.definition_->bubble,frame.placements)
                || !owner.project_authority(frame))frame={};
        }
        std::uint32_t observations(std::uint64_t,const NativeActivityFrame&) noexcept override {return 0;}
    };
    [[nodiscard]] bool begin_round(population::Owner owner,const NativeActivityDefinition& definition,
        std::shared_ptr<const Document> document,std::uint64_t boot) noexcept {
        population::Service population;
        native_capture::Runtime capture;
        forest_generator::Service generator;
        world_device::Service device;
        cue_presentation::Service presentation;
        occupancy_wait::Service occupancy;
        round_environment::Service environment;
        round_music::Service roundMusic;
        transit_effect::Service transitEffects;
        std::array<std::uint32_t,4> seeds{};
        std::array<forest_generator::AnchorConfiguration,4> anchors{};
        for(std::size_t i=0;i<definition.generators.size();++i) {
            const auto name=definition.generators[i].seedParameter;
            if(!name.empty())seeds[i]=document->views().parameter(name)->value;
            for(std::size_t j=0;j<definition.generators[i].anchorParameters.size();++j) {
                const auto& names=definition.generators[i].anchorParameters[j];
                if(!names.column.empty())anchors[i][j].column=document->views().parameter(names.column)->value;
                if(!names.height.empty())anchors[i][j].height=document->views().parameter(names.height)->value;
            }
        }
        std::array<cue_presentation::Action,8> directives{};
        if(definition.directives.size()>directives.size())return false;
        std::copy(definition.directives.begin(),definition.directives.end(),directives.begin());
        if(!round_activity::bind_timer(*definition.rounds,*document,
            std::span(directives).first(definition.directives.size())))return false;
        if(!population.begin(owner,definition.populations,boot))return false;
        if(!capture.begin(definition,*document,capture_feedback::RunIdentity::activitySession))return false;
        if(!occupancy.begin(owner,boot,definition.occupancyWaits))return false;
        if(!native_activity_transit::bind(owner,boot,definition.rounds->transitDestinations))return false;
        if(definition.rounds->environment
            && !environment.begin(owner,boot,*definition.rounds->environment))return false;
        if(!generator.begin(owner,boot,definition.generators,
            std::span(seeds).first(definition.generators.size()),
            std::span(anchors).first(definition.generators.size())))return false;
        if(!device.begin(owner,boot,definition.devices))return false;
        if(!definition.directives.empty() && !presentation.begin(owner,boot,
            std::span(directives).first(definition.directives.size())))return false;
        if(!roundMusic.configure(definition.rounds->music))return false;
        if(definition.rounds->transitEffects
            && !transitEffects.begin(owner,boot,*definition.rounds->transitEffects))return false;
        population_=std::move(population);capture_=std::move(capture);occupancy_=std::move(occupancy);
        generator_=std::move(generator);
        device_=std::move(device);presentation_=std::move(presentation);
        environment_=std::move(environment);
        roundMusic_=std::move(roundMusic);
        transitEffects_=std::move(transitEffects);
        roundSequences_={};
        definition_=&definition;document_=std::move(document);presentationProgress_=UINT32_MAX;
        resultsPublished_=false;
        roundMode_=true;return true;
    }
    [[nodiscard]] bool request_round_device(std::uint16_t index,std::uint32_t action) noexcept {
        if(index>=definition_->devices.size())return false;
        auto& service=device_;if(service.last_request()==UINT64_MAX)return false;
        const auto& capability=definition_->devices[index];
        const auto result=service.request({service.owner(),service.boot(),service.revision(),service.last_request()+1,
            capability.registry->key,action,capability.slot},definition_->bubble);
        return result==world_device::Result::accepted || result==world_device::Result::unchanged;
    }
    [[nodiscard]] bool request_round_presentation(std::uint32_t action,std::uint32_t evidence,
        std::optional<std::int32_t> variant=std::nullopt) noexcept {
        if(!action || presentation_.last_request()==UINT64_MAX)return false;
        const auto result=presentation_.request({presentation_.owner(),presentation_.boot(),presentation_.revision(),
            presentation_.last_request()+1,action,evidence,clock_?clock_.elapsedTicks:UINT64_MAX});
        if(result!=cue_presentation::Result::accepted && result!=cue_presentation::Result::unchanged)return false;
        if(!variant || !presentation_.presentation()
            || presentation_.presentation()->variant==*variant)return true;
        return update_round_presentation_variant(*variant);
    }
    [[nodiscard]] bool update_round_presentation_variant(std::int32_t variant) noexcept {
        const auto* current=presentation_.presentation();
        if(!current || presentation_.last_request()==UINT64_MAX)return false;
        const auto result=presentation_.update({presentation_.owner(),presentation_.boot(),presentation_.revision(),
            presentation_.last_request()+1,presentation_.current_action(),variant,std::nullopt});
        return result==cue_presentation::Result::accepted || result==cue_presentation::Result::unchanged;
    }
    [[nodiscard]] std::uint32_t round_generator_seed() const noexcept {
        if(!definition_ || !definition_->rounds
            || definition_->rounds->generatorIndex>=definition_->generators.size())return 0;
        const auto& capability=definition_->generators[definition_->rounds->generatorIndex];
        const auto projected=generator_.project(definition_->bubble);
        for(std::size_t i=0;i<projected.count;++i) {
            const auto& request=projected.entries[i];
            if(request.registry==capability.registry->key && request.slot==capability.slot)
                return request.state.primary.seed;
        }
        return 0;
    }
    [[nodiscard]] bool round_generator_enabled() const noexcept {
        if(!definition_ || !definition_->rounds
            || definition_->rounds->generatorIndex>=definition_->generators.size())return false;
        const auto& capability=definition_->generators[definition_->rounds->generatorIndex];
        const auto projected=generator_.project(definition_->bubble);
        for(std::size_t i=0;i<projected.count;++i) {
            const auto& request=projected.entries[i];
            if(request.registry==capability.registry->key && request.slot==capability.slot)
                return request.state.primary.enabled;
        }
        return false;
    }
    [[nodiscard]] bool round_travel_ready() const noexcept {
        if(rounds_.round_snapshot().phase!=timed_round::Phase::returning)return true;
        if(round_generator_enabled())return false;
        const auto& capability=definition_->generators[definition_->rounds->generatorIndex];
        return state::activity::native_population::generator_drained(population_.owner(),population_.boot(),
            capability.registry->key,capability.slot,round_generator_seed());
    }
    [[nodiscard]] bool round_travel_effect_ready() noexcept {
        if(rounds_.defeat_reward_travel()) {
            // Membership travel must select the reward spawn, not the previous native
            // teleporter's latched branch-entry spawn.
            native_activity_transit::clear_respawn_destination(population_.owner(),population_.boot());
            respawnLatched_=false;
            return true;
        }
        if(!definition_->rounds->transitEffects)return true;
        const auto cohort=rounds_.travel_cohort();
        if(transitEffects_.native_targets()) {
            // Each gate is recorded so the runtime can log which one held the pulse back.
            std::uint8_t gates{};
            const bool prepared=transitEffects_.prepare(cohort,rounds_.travel_destination());
            if(prepared && preparedCohort_!=cohort) {
                // Travel-prepare edge for a new leg. The previous leg's respawn set is retained by
                // the transit service until teardown, so drop it here or it stays authoritative
                // for the whole of this round.
                preparedCohort_=cohort;respawnLatched_=false;
                native_activity_transit::clear_respawn_destination(
                    population_.owner(),population_.boot());
            }
            gates|=prepared?1U:0U;
            const bool targets=prepared && transitEffects_.target_ready(cohort);
            gates|=targets?2U:0U;
            const bool trigger=targets
                && (!transitEffects_.trigger_required() || transitEffects_.trigger_armed());
            gates|=trigger?4U:0U;
            // Bit 8 now reports "a qualified arrival latched the respawn set", not "the pulse is
            // about to fire": latching here published the arena as the respawn destination ~2.3 s
            // before the player left the branch. The pulse keeps the same fail-closed destination
            // validation without the side effect, so an unknown id still holds the pulse and still
            // shows up as a missing bit 16.
            gates|=respawnLatched_?8U:0U;
            const bool pulsed=trigger
                && native_activity_transit::destination_bound(population_.owner(),
                    population_.boot(),rounds_.travel_destination())
                && transitEffects_.pulse(cohort);
            gates|=pulsed?16U:0U;
            transitGates_=gates;
            return pulsed;
        }
        return transitEffects_.request(cohort,rounds_.travel_destination())
            && transitEffects_.ready(cohort,clock_?clock_.elapsedTicks:UINT64_MAX);
    }
    [[nodiscard]] bool round_capture_completed(std::uint16_t platform) const noexcept {
        if(!definition_ || !definition_->rounds || platform>=definition_->rounds->platforms.size())return false;
        const auto index=definition_->rounds->platforms[platform].captureIndex;
        if(index>=capture_.size())return false;
        const auto& state=capture_.state(index);
        return state.requested && state.completed
            && state.ticket.token==rounds_.capture_token();
    }
    [[nodiscard]] bool update_round_presentation(std::uint32_t progress) noexcept {
        const auto* current=presentation_.presentation();
        if(!current || presentation_.last_request()==UINT64_MAX)return false;
        if(current->hasProgress && current->progress.current==static_cast<std::int32_t>(progress)) {
            presentationProgress_=progress;return true;
        }
        const auto result=presentation_.update({presentation_.owner(),presentation_.boot(),presentation_.revision(),
            presentation_.last_request()+1,presentation_.current_action(),{},
            cue_presentation::wire::Progress{static_cast<std::int32_t>(progress),
                static_cast<std::int32_t>(definition_->rounds->hud.progressTarget)}});
        if(result!=cue_presentation::Result::accepted && result!=cue_presentation::Result::unchanged)return false;
        presentationProgress_=progress;return true;
    }
    [[nodiscard]] bool request_round_generator(std::uint32_t action) noexcept {
        auto& service=generator_;if(service.last_request()==UINT64_MAX)return false;
        const auto& capability=definition_->generators[definition_->rounds->generatorIndex];
        const auto round=rounds_.round_snapshot().token.round;
        const auto result=(action==definition_->rounds->generatorDisableAction || round==1)
            ? service.request({service.owner(),service.boot(),service.revision(),service.last_request()+1,
                capability.registry->key,action,capability.slot},definition_->bubble)
            : service.begin_cycle({service.owner(),service.boot(),service.revision(),service.last_request()+1,
                capability.registry->key,capability.slot,action,round},definition_->bubble);
        return result==forest_generator::Result::accepted || result==forest_generator::Result::unchanged;
    }
    [[nodiscard]] bool request_round_population(const coo::Command& command) noexcept {
        const auto* route=action(*definition_,command.spec);if(!route)return false;
        if(route->capability>=definition_->populations.size())return false;
        const auto* count=document_->views().parameter(route->countParameter);if(!count)return false;
        auto& service=population_;if(service.last_request()==UINT64_MAX)return false;
        const auto& capability=definition_->populations[route->capability];
        const auto result=service.request({service.owner(),service.revision(),service.last_request()+1,
            capability.registry->key,capability.slot,static_cast<std::uint8_t>(count->value),service.boot()},definition_->bubble);
        return result==population::Result::accepted || result==population::Result::unchanged;
    }
    [[nodiscard]] bool request_round_population_index(std::uint16_t index,bool encounter=true) noexcept {
        if (!definition_ || !definition_->rounds || index >= definition_->populations.size()) return false;
        auto& source=population_;if(source.last_request()==UINT64_MAX)return false;
        const auto& capability=definition_->populations[index];
        const auto* status=source.status(capability.registry->key,capability.slot);
        if(!status)return false;
        const auto request=source.last_request()+1;
        const auto round=rounds_.round_snapshot().token.round;
        if(!round || round>UINT32_MAX)return false;
        const auto result=(status->phase==population::Phase::retired
                && status->retirementAcknowledged)
            ? source.begin_cycle(source.owner(),source.boot(),source.revision(),request,
                capability.registry->key,capability.slot,static_cast<std::uint32_t>(round),1)
            : source.request({source.owner(),source.revision(),request,capability.registry->key,
                capability.slot,1,source.boot()},definition_->bubble);
        if(result!=population::Result::accepted && result!=population::Result::unchanged)return false;
        return !encounter || rounds_.mark_encounter_population_requested(index);
    }
    [[nodiscard]] NativeActivityFrame update_round(std::uint32_t bubble,bool arrived,
        activity_clock::Publication clock) noexcept {
        NativeActivityFrame frame{};capture_.clock(clock);clock_=clock;
        if(clock && !rounds_.started()) {
            std::uint64_t ticks{};
            if(!round_activity::duration_ticks(*definition_->rounds,*document_,ticks)
                || !rounds_.begin(population_.owner(),population_.boot(),*definition_->rounds,*document_,ticks))return {};
        }
        if(rounds_.started()) {
            if(!capture_.poll([&](coo::Event event) noexcept {return rounds_.enqueue(event);}))return {};
            RoundPorts ports(*this);
            if(rounds_.travel_pending() && round_travel_ready() && round_travel_effect_ready()) {
                // Native-local routes already pulse the authored activity effect. Their
                // arrival receipt is the travel boundary; membership travel would cause
                // the roster wait and whole-region transition this profile is avoiding.
                const auto requested=transitEffects_.native_targets() && !rounds_.defeat_reward_travel()
                    ? true
                    : native_activity_transit::request_all(population_.owner(),population_.boot(),
                        rounds_.travel_cohort(),rounds_.travel_destination());
                static_cast<void>(rounds_.mark_travel_request(requested));
            }
            if(!rounds_.update(ports,clock?clock.elapsedTicks:UINT64_MAX,
                arrived && bubble==definition_->bubble))return {};
            if(rounds_.travel_arrival_pending() && definition_->rounds->transitEffects
                && transitEffects_.native_targets() && !rounds_.defeat_reward_travel()) {
                // Arrival, not the pulse, is the respawn boundary. Until the player is actually at
                // the destination a death in the branch must not resolve there.
                const bool qualified=transitEffects_.arrival_qualified();
                if(qualified && !respawnLatched_)
                    respawnLatched_=native_activity_transit::set_respawn_destination(
                        population_.owner(),population_.boot(),rounds_.travel_destination());
                static_cast<void>(rounds_.observe_travel(qualified,qualified));
            } else if(rounds_.travel_arrival_pending()) {
                const auto status=native_activity_transit::snapshot(population_.owner(),population_.boot(),
                    rounds_.travel_cohort());
                static_cast<void>(rounds_.observe_travel(status.arrived,status.released));
            }
            const auto status=rounds_.round_snapshot();
            if(rounds_.collapse_due() && definition_->rounds->hud.collapseAction) {
                const auto action=definition_->rounds->hud.collapseAction;
                std::int32_t variant{}; // Authored zero describes the Terror.
                if(status.phase!=timed_round::Phase::encounter) {
                    for(const auto& cue:definition_->directives)if(cue.id==action && cue.maximumVariant)
                        variant=static_cast<std::int32_t>((std::min)(status.token.round,
                            static_cast<std::uint64_t>(*cue.maximumVariant)));
                }
                if(!request_round_presentation(action,0,variant))return {};
            }
            // The final credited death enters toEncounter immediately. Publish
            // its 100% while the traversal cue is retained, until real arrival
            // replaces that cue with the Terror objective.
            if((status.phase==timed_round::Phase::traversal
                || status.phase==timed_round::Phase::toEncounter)
                && !update_round_presentation(status.progress))return {};
            static_cast<void>(request_mid_populations());
        }
        const auto musicContext = round_music_context(arrived && bubble == definition_->bubble);
        if(rounds_.started()) {
            const auto status=rounds_.round_snapshot();
            const public_event::sequence::Context sequenceContext{
                population_.owner(),population_.boot(),document_->fingerprint(),document_->fingerprint(),
                status.token.round,clock?clock.elapsedTicks:UINT64_MAX,musicContext.activity,
                bubble,arrived,arrived && bubble==definition_->bubble};
            if(!roundSequences_.update(definition_->rounds->sequences,sequenceContext,
                status.token.round,status.phase) || !roundSequences_.append(frame.sequences))
                frame.optionalPresentationFailure=true;
        }
        const auto completion=rounds_.completion();
        // State6 still permits reward-area gameplay. Keep its native music
        // lease until state7 opens the summary, including pit occupancy changes.
        const auto musicPhase=completion.valid() && completion.state==6
            ? timed_round::Phase::rewards
            : rounds_.started() ? rounds_.round_snapshot().phase : timed_round::Phase::entry;
        if (!roundMusic_.update(musicContext,musicPhase,
            rounds_.started() && rounds_.round_snapshot().expired,
            completion.valid() && completion.state>=7,
            occupancy_.occupied(definition_->rounds->trapOccupancyIndex)))
            frame.optionalPresentationFailure = true;
        if(roundMode_ && rounds_.completion().valid())
            static_cast<void>(rounds_.advance_lifecycle(GetTickCount64()));
        if(!project_authority(frame))return {};
        if(!roundMusic_.append(frame.music)) {
            frame.music = {};
            frame.optionalPresentationFailure = true;
        }
        const auto status=rounds_.snapshot();frame.completion=status.completion;frame.endEpoch=status.endEpoch;
        frame.restricted=status.restricted;frame.clock=clock;
        frame.nativeTraversalRespawn=traversal_respawn();
        return frame;
    }
    [[nodiscard]] music::Context round_music_context(bool admitted) const noexcept {
        const auto revision = document_->fingerprint() ? document_->fingerprint() : 1;
        const auto activity = definition_->activityOrdinals.empty()
            ? std::int16_t{0} : static_cast<std::int16_t>(definition_->activityOrdinals.front());
        const auto owner = population_.owner();
        return {owner, population_.boot(), revision, owner.sessionId, owner.incarnation.value,
            activity, definition_->bubble, admitted, admitted};
    }
    const NativeActivityDefinition* definition_{};
    std::shared_ptr<const Document> document_;
    population::Service population_;
    npc_animation::Service animation_;
    ambient_population::InitialActivation<> ambient_;
    public_event::RallyRuntime rally_;
    public_event::InitialRuntime publicInitial_;
    adventure::OpeningRuntime opening_;
    occupancy_wait::Service occupancy_;
    native_capture::Runtime capture_;
    forest_generator::Service generator_;
    world_device::Service device_;
    cue_presentation::Service presentation_;
    activity_clock::Publication clock_{};
    coo::MissionRuntime composition_;
    coo::Executor executor_;
    std::array<bool,32> placements_{};
    bool started_{};
    round_activity::Runtime rounds_{};
    std::uint32_t presentationProgress_{UINT32_MAX};
    round_environment::Service environment_{};
    round_music::Service roundMusic_{};
    round_sequence::Service roundSequences_{};
    transit_effect::Service transitEffects_{};
    std::array<bool,8> triggeredPoses_{};
    std::uint8_t transitGates_{};
    // Set only by a qualified travel arrival, cleared on the next travel-prepare edge.
    bool respawnLatched_{};
    std::uint64_t preparedCohort_{};
    bool resultsPublished_{};
    bool experimentalRewardPlacements_{};
    bool roundMode_{};
};
}
