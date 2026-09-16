#pragma once

#include "round_activity_definition.h"
#include "timed_round_service.h"
#include "round_wipe_service.h"
#include "../../../state/activity/coo/lifecycle_service.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace sunrise::server::runtime::activity::round_activity {

namespace coo = state::activity::coo;
using Owner = timed_round::Owner;
using Document = coo::script::MissionDocument;

struct Snapshot final {
    timed_round::Snapshot round{};
    coo::Diagnostics executor{};
    coo::CompletionPublication completion{};
    std::uint64_t selectedBoss{};
    std::uint64_t endEpoch{};
    bool restricted{};
    bool rewardArrived{};
    bool rewardsAccepted{};
    bool failed{};
    std::uint32_t unknownScoreEvents{};
    std::uint16_t selectedPlatform{kMissingIndex};
    std::uint32_t selectedEncounter{};
    std::uint64_t travelCohort{};
    bool travelRequested{};
    bool travelArrivalQualified{};
};

// The runtime stores only value state and pointers to immutable authored data. Native services
// are supplied as short-lived ports on the owner update, so this class cannot become a second
// population, capture, generator, occupancy, cue, device, or transit owner.
class Runtime final {
public:
    static constexpr std::size_t kPhaseCount = 6;

    template <class NativeDefinition>
    [[nodiscard]] static bool valid(const Definition& definition, const NativeDefinition& native,
        const Document& document) noexcept {
        return round_activity::valid(definition, native, document);
    }

    [[nodiscard]] static bool valid(const Definition& definition, const Document& document) noexcept {
        if (!round_activity::valid(definition) || !document.views().valid) return false;
        const auto roles = phase_roles(definition.roles);
        for (const auto role : roles) {
            const auto* graph = document.views().role(role);
            if (!graph || graph->domain != "nativeActivity" || !coo::Executor::valid(graph->definition))
                return false;
            for (const auto& step : graph->definition.steps) for (const auto& command : step.commands) {
                if (!find_binding(definition, command)) return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool begin(Owner owner, std::uint64_t boot, const Definition& definition,
        const Document& document, std::uint64_t traversalTicks) noexcept {
        if (owner_ || !owner || !boot || !traversalTicks || !valid(definition, document)
            || !lifecycle_.begin(owner.sessionId)) return false;
        owner_ = owner;
        boot_ = boot;
        definition_ = &definition;
        document_ = &document;
        if (!round_.begin(owner, boot, {traversalTicks, definition.hud.progressTarget,
                timed_round::ActiveCombatPhases})) {
            reset_value();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool started() const noexcept { return static_cast<bool>(owner_); }
    [[nodiscard]] bool graph_complete() const noexcept {
        return executor_.diagnostics().phase == coo::Phase::complete;
    }
    [[nodiscard]] bool update_pending() const noexcept {
        const auto phase = round_.snapshot().phase;
        if (!started() || failed_ || phase == timed_round::Phase::complete) return false;
        // Phase commits happen after graph execution. Keep the owner scheduled
        // to start the new graph (and publish its objective) on the next update.
        return defeatPending_ || executor_.update_pending()
            || (graphPhase_ != phase && executor_.diagnostics().phase != coo::Phase::running);
    }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] timed_round::Snapshot round_snapshot() const noexcept { return round_.snapshot(); }
    [[nodiscard]] coo::Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    [[nodiscard]] std::uint64_t selected_boss() const noexcept { return selectedBoss_; }
    [[nodiscard]] std::uint16_t selected_platform() const noexcept { return selectedPlatform_; }
    [[nodiscard]] coo::Token capture_token() const noexcept { return captureToken_; }
    [[nodiscard]] const coo::CompletionPublication& completion() const noexcept {
        return completion_;
    }

    template <class Ports>
    [[nodiscard]] bool update(Ports& ports, std::uint64_t clockTicks,
        bool graphAllowed = true) noexcept {
        if (!started() || failed_) return false;
        const auto effectiveClock=effective_clock(clockTicks);
        // The timed service owns the clock independently from graph execution.
        // This is deliberately done for every update, including transit and
        // the post-graph completion states.
        if (valid_clock(effectiveClock)) {
            const auto tick = round_.tick(round_.snapshot().token, effectiveClock);
            if (tick == timed_round::Result::exhausted
                || tick == timed_round::Result::invalidClock) return false;
        }
        if (defeatPending_ && valid_clock(effectiveClock)) {
            const auto result=round_.all_players_defeated(round_.snapshot().token,effectiveClock,1);
            if(result!=timed_round::Result::accepted)return false;
            // A wipe terminates the active wait (progress or boss death), not the activity.
            // The existing rewards graph owns retirement, travel, chest and timed completion.
            Driver<Ports> driver(*this,ports);executor_.cancel(driver);
            reset_travel();retirementToken_={};retirementQualified_=false;
            defeatPending_=false;defeatRewards_=true;
        }
        if (graphAllowed && round_.snapshot().phase != timed_round::Phase::complete) {
            if (!ensure_phase(ports)) return false;
            Driver<Ports> driver(*this, ports);
            executor_.update(driver);
            // Publishing runs before the executor marks a command requested. Deliver the
            // empty encounter receipt afterwards, just like a native retirement receipt.
            if(retirementToken_.run && !retirementQualified_ && !encounterRequestedCount_)
                static_cast<void>(retirement_acknowledged(true,true));
            deliver_early_boss_death();
        }
        if (captureReceipt_ && !pendingCapture_
            && round_.snapshot().phase == timed_round::Phase::entry
            && valid_clock(clockTicks)) {
            pendingCapture_ = true;
            pendingCaptureClock_ = clockTicks;
        }
        if (executor_.diagnostics().phase == coo::Phase::failed) {
            failed_ = true;
            return false;
        }
        if (!commit_pending_transition(ports, effective_clock(clockTicks))) return false;
        return true;
    }

    // Forward a qualified native observation to the active phase executor. The executor retains
    // the exact phase-generation token and rejects late observations itself.
    [[nodiscard]] bool enqueue(coo::Event event) noexcept {
        if (!executor_.enqueue(event)) return false;
        if (event.token == captureToken_ && event.milestone == coo::Milestone::completed)
            captureReceipt_ = true;
        return true;
    }

    [[nodiscard]] bool arm_progress(coo::Token token) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::traversal
            || !token.run || token.run != owner_.sessionId || !token.incarnation || progressArmed_) return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::progressFull) return false;
        progressToken_ = token;
        progressArmed_ = true;
        progressDelivered_ = false;
        return true;
    }

    [[nodiscard]] bool select_platform(coo::Token token, std::uint16_t platform) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::entry
            || token.run != owner_.sessionId || !token.incarnation) return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::prepareEntry
            || platform >= definition_->platforms.size()) return false;
        const auto roundNumber = round_.snapshot().token.round;
        if (selectedPlatformRound_ == roundNumber)
            return selectedPlatform_ == platform;
        selectedPlatformRound_ = roundNumber;
        selectedPlatform_ = platform;
        return true;
    }

    [[nodiscard]] timed_round::Result capture_complete(std::uint64_t clockTicks) noexcept {
        return capture_complete(captureToken_, clockTicks);
    }

    // Native capture completion may arrive while the entry graph is still
    // waiting. Keep the exact receipt token and defer the round transition
    // until that graph has consumed all of its waits.
    [[nodiscard]] timed_round::Result capture_complete(coo::Token token,
        std::uint64_t clockTicks) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::entry
            || token != captureToken_ || !captureReceipt_)
            return timed_round::Result::unsupported;
        if (pendingCapture_) return timed_round::Result::duplicate;
        if (!valid_clock(clockTicks)) return timed_round::Result::invalidClock;
        pendingCapture_ = true;
        pendingCaptureClock_ = clockTicks;
        return timed_round::Result::accepted;
    }

    // Compatibility overload for callers that still carry the native metadata
    // on a death. The admitted record, not this late metadata, is authoritative.
    [[nodiscard]] timed_round::Result generated_death(std::uint64_t identity,
        std::uint32_t memberPrefabTag, std::uint32_t completionGroup, bool elite,
        std::uint64_t clockTicks) noexcept {
        static_cast<void>(memberPrefabTag);static_cast<void>(completionGroup);static_cast<void>(elite);
        auto* record = generated(identity);
        if (!record || record->dead || record->retired || !progressArmed_ || progressDelivered_)
            return timed_round::Result::unsupported;
        // Native admission can precede the traversal graph (including the
        // plate's final update). Qualify credit at death, against this branch,
        // rather than permanently excluding an early but valid admission.
        if (round_.snapshot().phase != timed_round::Phase::traversal
            || record->source.generation != round_.snapshot().token.round)
            return timed_round::Result::unsupported;
        if (!record->scored) return timed_round::Result::unsupported;
        const auto weight = record->weight;
        if (!weight) return timed_round::Result::unsupported;
        const auto result = round_.add_progress(round_.snapshot().token, clockTicks, identity, weight);
        if (result == timed_round::Result::accepted) record->dead = true;
        if (result == timed_round::Result::accepted
            && round_.snapshot().progress >= definition_->hud.progressTarget) {
            progressDelivered_ = true;
            if (!enqueue({progressToken_, coo::Milestone::observed})) return timed_round::Result::stale;
        }
        return result;
    }

    [[nodiscard]] timed_round::Result generated_death(std::uint64_t identity,
        std::uint64_t clockTicks) noexcept {
        return generated_death(identity, 0, 0, false, clockTicks);
    }

    [[nodiscard]] timed_round::Result generated_death(const coo::PopulationActor& actor,
        const coo::PopulationOwner& source, std::uint32_t memberPrefabTag,
        std::uint32_t completionGroup, bool elite, std::uint64_t clockTicks) noexcept {
        if (!actor.valid() || actor.owner != source) return timed_round::Result::unsupported;
        const auto* record = generated(qualified_identity(actor));
        if (!record || record->source != source) return timed_round::Result::unsupported;
        return generated_death(qualified_identity(actor), memberPrefabTag, completionGroup, elite, clockTicks);
    }

    // Fixed population events are already authenticated by the population
    // ledger in the native activity owner. This method only keeps the
    // selected boss/intro/mid identities and forwards the actual boss proof.
    [[nodiscard]] bool population_event(std::uint16_t populationIndex,
        const coo::PopulationOwner& source, const coo::PopulationActor& actor,
        native_population::Kind kind, std::uint64_t clockTicks) noexcept {
        if (!actor.valid() || actor.owner != source || !source.valid()
            || source.activity != owner_.sessionId || source.run != boot_
            || source.incarnation != owner_.incarnation.value) return false;
        const auto sourceIndex = encounter_source(populationIndex);
        if (!sourceIndex) return false;
        auto& record = encounterSources_[*sourceIndex];
        if (!record.source.valid()) record.source = source;
        if (record.source != source) return false;
        if (kind == native_population::Kind::admitted) {
            const auto identity = qualified_identity(actor);
            for (std::size_t i = 0; i < record.admitted; ++i)
                if (record.actorIdentities[i] == identity) return true;
            if (record.admitted < record.actorIdentities.size())
                record.actorIdentities[record.admitted++] = identity;
            if (populationIndex == selectedBossPopulation_) {
                return boss_admitted(actor, source);
            }
        } else if (kind == native_population::Kind::died) {
            if (populationIndex == selectedBossPopulation_) {
                const auto result=boss_dead(actor, source, clockTicks);
                if (result!=timed_round::Result::accepted
                    && result!=timed_round::Result::duplicate)return false;
            }
            if (record.dead < record.admitted) ++record.dead;
        }
        if (record.intro && record.admitted && record.dead >= record.admitted)
            update_mid_readiness();
        return true;
    }

    [[nodiscard]] bool arm_spawn(coo::Token token) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::toEncounter
            || selectedEncounter_ == (std::numeric_limits<std::uint32_t>::max)()
            || token.run != owner_.sessionId || !token.incarnation)
            return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::spawnEncounter) return false;
        if (spawnArmed_) return spawnToken_ == token;
        if (selectedEncounter_ >= definition_->encounters.size()) return false;
        selectedBossPopulation_ = definition_->encounters[selectedEncounter_].bossPopulationIndex;
        spawnToken_ = token;spawnArmed_ = true;spawnCompleted_ = false;
        return deliver_spawn_receipt();
    }

    [[nodiscard]] bool boss_admitted(const coo::PopulationActor& actor,
        const coo::PopulationOwner& source) noexcept {
        if (!actor.valid() || actor.owner != source || selectedBossPopulation_ == kMissingIndex
            || source.source.type != 1) return false;
        if (!bossSource_.valid()) bossSource_ = source;
        if (bossSource_ != source) return false;
        const auto identity = qualified_identity(actor);
        if (bossIdentity_ == identity) return true;
        if (bossIdentity_) return false;
        bossIdentity_ = identity;selectedBoss_ = identity;bossAdmitted_ = true;
        return deliver_spawn_receipt();
    }

    [[nodiscard]] bool begin_retirement(coo::Token token) noexcept {
        if (!started() || (round_.snapshot().phase != timed_round::Phase::returning
            && round_.snapshot().phase != timed_round::Phase::rewards)
            || !token.run || token.run != owner_.sessionId || !token.incarnation)
            return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::retireEncounter) return false;
        if (retirementToken_.run && retirementToken_ != token) return false;
        if(retirementQualified_)return true;
        retirementToken_ = token;
        return true;
    }

    [[nodiscard]] bool arm_boss_dead(coo::Token token) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::encounter
            || !token.run || token.run != owner_.sessionId || !token.incarnation)
            return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::bossDead) return false;
        bossDeadToken_ = token;deliver_early_boss_death();return true;
    }

    [[nodiscard]] bool mark_encounter_population_requested(std::uint16_t index) noexcept {
        if (index == kMissingIndex || encounterRequestedCount_ == encounterRequested_.size())
            return false;
        for (std::size_t i = 0; i < encounterRequestedCount_; ++i)
            if (encounterRequested_[i] == index) return true;
        encounterRequested_[encounterRequestedCount_++] = index;
        return true;
    }
    [[nodiscard]] std::size_t encounter_population_count() const noexcept {
        return encounterRequestedCount_;
    }
    [[nodiscard]] std::uint16_t encounter_population_index(std::size_t index) const noexcept {
        return index < encounterRequestedCount_ ? encounterRequested_[index] : kMissingIndex;
    }
    [[nodiscard]] bool mid_ready() const noexcept { return midReady_ && !midRequested_; }
    void mark_mid_requested() noexcept { midRequested_ = true; }

    [[nodiscard]] bool generated_source_retired(const coo::PopulationOwner& source) const noexcept {
        bool found{};
        for (std::size_t i = 0; i < generatedCount_; ++i) {
            const auto& record = generated_[i];
            if (record.source != source) continue;
            found = true;
            if (!record.retired) return false;
        }
        return found || source.valid();
    }

    [[nodiscard]] bool release_retired_generated_source(
        const coo::PopulationOwner& oldSource,
        const coo::PopulationOwner& replacementSource) noexcept {
        if (oldSource == coo::PopulationOwner{}) return true;
        const auto round = round_.snapshot().token.round;
        const auto ownerMatches = [this](const coo::PopulationOwner& source) noexcept {
            return source.valid() && source.activity == owner_.sessionId
                && source.run == boot_ && source.incarnation == owner_.incarnation.value
                && source.source.type == 37;
        };
        if (!started() || !ownerMatches(oldSource) || !ownerMatches(replacementSource)
            || oldSource.source != replacementSource.source
            || oldSource.generation >= replacementSource.generation
            || replacementSource.generation != round)
            return false;
        for (std::size_t i = 0; i < generatedCount_; ++i)
            if (generated_[i].source == oldSource && !generated_[i].retired) return false;

        std::size_t retained{};
        for (std::size_t i = 0; i < generatedCount_; ++i) {
            if (generated_[i].source == oldSource) continue;
            if (retained != i) generated_[retained] = generated_[i];
            ++retained;
        }
        for (std::size_t i = retained; i < generatedCount_; ++i) generated_[i] = {};
        generatedCount_ = retained;
        return true;
    }

    [[nodiscard]] bool admit_generated(const coo::PopulationActor& actor,
        const coo::PopulationOwner& source, std::uint32_t memberPrefabTag = 0,
        std::uint32_t completionGroup = (std::numeric_limits<std::uint32_t>::max)()) noexcept {
        if (!actor.valid() || actor.owner != source || source.activity != owner_.sessionId
            || source.run != boot_ || source.incarnation != owner_.incarnation.value
            || source.source.type != 37 || source.generation > round_.snapshot().token.round)
            return false;
        const auto identity = qualified_identity(actor);
        if (generated(identity)) return true;
        if (generatedCount_ == generated_.size()) return false;
        GeneratedRecord record{identity, source, memberPrefabTag, completionGroup, 0, false, false, false, false};
        const auto* score = find_score(memberPrefabTag);
        if (score) {
            record.elite = definition_->eliteCompletionGroup != 0
                && completionGroup == definition_->eliteCompletionGroup;
            record.weight = record.elite ? score->eliteWeight : score->normalWeight;
            record.scored = record.weight != 0;
        } else if (unknownScoreEvents_ != (std::numeric_limits<std::uint32_t>::max)()) {
            ++unknownScoreEvents_;
        }
        generated_[generatedCount_++] = record;
        return true;
    }

    [[nodiscard]] bool retire_generated(const coo::PopulationActor& actor,
        const coo::PopulationOwner& source) noexcept {
        if (!actor.valid() || actor.owner != source) return false;
        auto* record = generated(qualified_identity(actor));
        if (!record || record->source != source || record->retired) return false;
        record->retired = true;
        return true;
    }

    [[nodiscard]] timed_round::Result encounter_arrived(std::uint64_t clockTicks) noexcept {
        if (!travelArrivalQualified_ || round_.snapshot().phase != timed_round::Phase::toEncounter)
            return timed_round::Result::unsupported;
        const auto result = round_.encounter_arrived(round_.snapshot().token, clockTicks);
        if (result == timed_round::Result::accepted) reset_travel();
        return result;
    }

    [[nodiscard]] bool select_encounter(std::uint64_t roundNumber, std::uint64_t selectionSeed,
        std::uint64_t selectedIdentity) noexcept {
        if (!selectedIdentity || !selectionSeed || roundNumber != round_.snapshot().token.round) return false;
        if (selectedRound_ == roundNumber) return selectedBoss_ == selectedIdentity;
        selectedRound_ = roundNumber;
        selectionSeed_ = selectionSeed;
        selectedBoss_ = selectedIdentity;
        bossAdmitted_ = false;
        return true;
    }

    [[nodiscard]] bool prepare_travel(coo::Token token, std::uint32_t generatorSeed) noexcept {
        if (!started() || round_.snapshot().phase != timed_round::Phase::toEncounter
            || token.run != owner_.sessionId || !token.incarnation || !definition_
            || !definition_->encounterDestinationId || !generatorSeed)
            return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::travelEncounter) return false;
        const auto roundNumber = round_.snapshot().token.round;
        if (travelIntent_) return travelToken_ == token && travelRound_ == roundNumber;
        if (requestCounter_ == (std::numeric_limits<std::uint64_t>::max)()) return false;
        if (selectedRound_ != roundNumber) {
            selectedRound_ = roundNumber;
            selectionSeed_ = generatorSeed;
            selectedEncounter_ = static_cast<std::uint32_t>(mix_selection(generatorSeed, roundNumber)
                % definition_->encounters.size());
            selectedBoss_ = 0;
        } else if (selectionSeed_ != generatorSeed) return false;
        travelToken_ = token;
        travelRound_ = roundNumber;
        travelCohort_ = ++requestCounter_;
        travelDestination_ = definition_->encounterDestinationId;
        travelOperation_ = Operation::travelEncounter;
        travelIntent_ = true;
        travelRequested_ = false;
        return true;
    }

    [[nodiscard]] bool mark_travel_request(bool nativeAccepted) noexcept {
        if (!travelIntent_) return false;
        travelRequested_ |= nativeAccepted;
        return true;
    }

    [[nodiscard]] bool arm_travel_arrival(coo::Token token) noexcept {
        if (!travelIntent_ || (round_.snapshot().phase != timed_round::Phase::toEncounter
            && round_.snapshot().phase != timed_round::Phase::returning
            && round_.snapshot().phase != timed_round::Phase::rewards)
            || token.run != owner_.sessionId || !token.incarnation) return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != Operation::travelArrived) return false;
        if (travelArrivalArmed_) return travelArrivalToken_ == token;
        travelArrivalToken_ = token;travelArrivalArmed_ = true;
        travelSawArrival_ = travelArrivalQualified_ = false;return true;
    }

    [[nodiscard]] bool observe_travel(bool arrived, bool released) noexcept {
        if (!travelArrivalArmed_) return false;
        if (arrived) travelSawArrival_ = true;
        if (!released || !travelSawArrival_ || travelArrivalQualified_) return false;
        if (!enqueue({travelArrivalToken_, coo::Milestone::observed})) return false;
        travelArrivalQualified_ = true;
        if (round_.snapshot().phase == timed_round::Phase::rewards) {
            // The placement set was authored before the reward travel request;
            // the real released arrival is the acceptance edge for it.
            rewardArrived_ = true;rewardsAccepted_ = true;
        }
        return true;
    }

    [[nodiscard]] bool travel_pending() const noexcept { return travelIntent_ && !travelRequested_; }
    [[nodiscard]] std::uint64_t travel_cohort() const noexcept { return travelCohort_; }
    [[nodiscard]] std::uint32_t travel_destination() const noexcept { return travelDestination_; }
    [[nodiscard]] bool travel_arrival_pending() const noexcept { return travelArrivalArmed_; }
    [[nodiscard]] bool travel_arrival_qualified() const noexcept { return travelArrivalQualified_; }
    [[nodiscard]] std::uint32_t selected_encounter() const noexcept { return selectedEncounter_; }
    [[nodiscard]] std::uint32_t unknown_score_events() const noexcept { return unknownScoreEvents_; }
    [[nodiscard]] bool collapse_due() noexcept {
        const auto phase = round_.snapshot().phase;
        if (!round_.snapshot().expired || (phase != timed_round::Phase::traversal
            && phase != timed_round::Phase::toEncounter && phase != timed_round::Phase::encounter)
            || collapsePublished_) return false;
        collapsePublished_ = true;return true;
    }

    [[nodiscard]] bool boss_admitted(std::uint64_t selectedIdentity) noexcept {
        if (!selectedIdentity || (selectedBoss_ && selectedIdentity != selectedBoss_)) return false;
        bossIdentity_ = selectedIdentity;bossAdmitted_ = true;
        return deliver_spawn_receipt();
    }

    [[nodiscard]] timed_round::Result boss_dead(std::uint64_t selectedIdentity,
        std::uint64_t clockTicks) noexcept {
        if (!selectedIdentity || selectedIdentity != bossIdentity_ || !bossAdmitted_)
            return timed_round::Result::unsupported;
        if (!round_.clock_valid(clockTicks)) return timed_round::Result::invalidClock;
        if (bossDeathObserved_) return timed_round::Result::duplicate;
        bossDeathObserved_ = true;bossDeathClock_ = clockTicks;
        if (bossDeadToken_.run && round_.snapshot().phase == timed_round::Phase::encounter)
            return enqueue({bossDeadToken_,coo::Milestone::observed})
                ? timed_round::Result::accepted : timed_round::Result::exhausted;
        earlyBossDeath_ = true;
        return timed_round::Result::accepted;
    }

    [[nodiscard]] timed_round::Result boss_dead(const coo::PopulationActor& actor,
        const coo::PopulationOwner& source, std::uint64_t clockTicks) noexcept {
        const auto identity = qualified_identity(actor);
        if (!actor.valid() || actor.owner != source || source != bossSource_)
            return timed_round::Result::unsupported;
        return boss_dead(identity, clockTicks);
    }

    [[nodiscard]] bool retirement_acknowledged(bool allRegisteredActorsRetired,
        bool sourceGenerationAcknowledged) noexcept {
        if (!retirementToken_.run || retirementQualified_ || !allRegisteredActorsRetired
            || !sourceGenerationAcknowledged) return false;
        if (!enqueue({retirementToken_,coo::Milestone::observed})) return false;
        retirementQualified_ = true;return true;
    }

    [[nodiscard]] timed_round::Result returned(std::uint64_t clockTicks) noexcept {
        if (!travelArrivalQualified_ || round_.snapshot().phase != timed_round::Phase::returning)
            return timed_round::Result::unsupported;
        const auto result = round_.returned(round_.snapshot().token, clockTicks);
        if (result == timed_round::Result::accepted) reset_round_state();
        return result;
    }

    [[nodiscard]] bool reward_arrived() noexcept {
        if (round_.snapshot().phase != timed_round::Phase::rewards
            || !travelArrivalQualified_) return false;
        rewardArrived_ = true;return true;
    }
    [[nodiscard]] bool accept_reward_placements() noexcept {
        if (!rewardArrived_) return false;
        rewardsAccepted_ = true;return true;
    }

    [[nodiscard]] bool request_completion(std::uint64_t monotonicMilliseconds,
        std::uint64_t elapsedTicks) noexcept {
        if (!rewardArrived_ || !rewardsAccepted_ || completionRequested_
            || round_.snapshot().phase != timed_round::Phase::rewards
            || !valid_clock(elapsedTicks)) return false;
        if (!lifecycle_.complete_timed(lifecycle_.owner(), monotonicMilliseconds)) return false;
        completionRequested_ = true;endEpoch_ = elapsedTicks;
        completion_ = lifecycle_.publication();return true;
    }

    [[nodiscard]] timed_round::Result rewards_finished(std::uint64_t clockTicks) noexcept {
        if (!completionRequested_ || round_.snapshot().phase != timed_round::Phase::rewards)
            return timed_round::Result::unsupported;
        const auto result = round_.rewards_finished(round_.snapshot().token, clockTicks);
        if (result == timed_round::Result::accepted) completion_ = lifecycle_.publication();
        return result;
    }

    [[nodiscard]] bool boss_death_qualified() const noexcept { return bossDeathObserved_; }

    [[nodiscard]] bool advance_lifecycle(std::uint64_t monotonicMilliseconds) noexcept {
        if (!completionRequested_) return false;
        const bool changed = lifecycle_.advance(monotonicMilliseconds);
        completion_ = lifecycle_.publication();
        return changed;
    }

    [[nodiscard]] bool restricted() const noexcept {
        const auto snapshot = round_.snapshot();
        return snapshot.expired && (snapshot.phase == timed_round::Phase::traversal
            || snapshot.phase == timed_round::Phase::toEncounter
            || snapshot.phase == timed_round::Phase::encounter
            || (defeatRewards_ && !rewardArrived_));
    }

    // The caller qualifies the sole admitted participant. Multiplayer needs life
    // receipts for every admitted member; a local death alone is never sufficient.
    bool observe_player_life(std::uint32_t entity,bool alive,std::uint64_t now) noexcept {
        if(!started() || completionRequested_ || defeatRewards_)return false;
        if(alive)defeatPending_=false;
        if(!wipe_.observe(entity,alive,restricted(),now) || defeatPending_)return false;
        defeatPending_=true;return true;
    }
    [[nodiscard]] bool defeat_reward_travel() const noexcept {return defeatRewards_;}

    [[nodiscard]] Snapshot snapshot() const noexcept {
        return {round_.snapshot(), executor_.diagnostics(), completion_, selectedBoss_, endEpoch_,
            restricted(), rewardArrived_, rewardsAccepted_, failed_, unknownScoreEvents_, selectedPlatform_, selectedEncounter_,
            travelCohort_, travelRequested_, travelArrivalQualified_};
    }

private:
    template <class Ports>
    struct Driver final : coo::Services {
        Runtime& owner;
        Ports& ports;
        Driver(Runtime& value, Ports& supplied) noexcept : owner(value), ports(supplied) {}
        bool publish(const coo::Command& command) noexcept {
            const auto* binding = find_binding(*owner.definition_, command.spec);
            if (!binding || !owner.remember(command.token, binding->operation)) return false;
            if (ports.publish(binding->operation, command)) return true;
            owner.forget(command.token);return false;
        }
        void cancel(const coo::Command& command) noexcept {
            const auto* binding = find_binding(*owner.definition_, command.spec);
            if (binding) ports.cancel(binding->operation, command);
        }
    };

    template <class Ports>
    [[nodiscard]] bool ensure_phase(Ports& ports) noexcept {
        const auto phase = round_.snapshot().phase;
        const auto index = phase_index(phase);
        if (!index) return false;
        if (graphPhase_ == phase && executor_.diagnostics().phase != coo::Phase::failed) return true;
        if (graphPhase_ != phase && executor_.diagnostics().phase == coo::Phase::running) return true;
        Driver<Ports> driver(*this, ports);
        if (executor_.diagnostics().phase != coo::Phase::idle) executor_.cancel(driver);
        registered_ = {};registeredCount_ = 0;
        const auto roles = phase_roles(definition_->roles);
        const auto* graph = document_->views().role(roles[*index]);
        if (!graph || !executor_.start(graph->definition, owner_.sessionId)) {
            failed_ = true;
            return false;
        }
        graphPhase_ = phase;
        return true;
    }

    [[nodiscard]] const Score* find_score(std::uint32_t prefab) const noexcept {
        for (const auto& score : definition_->scores)
            if (score.memberPrefabTag == prefab)
                return &score;
        return nullptr;
    }

    struct GeneratedRecord final {
        std::uint64_t identity{};
        coo::PopulationOwner source{};
        std::uint32_t memberPrefabTag{}, completionGroup{}, weight{};
        bool elite{}, scored{}, dead{}, retired{};
    };

    struct EncounterSource final {
        std::uint16_t populationIndex{kMissingIndex};
        coo::PopulationOwner source{};
        std::array<std::uint64_t,64> actorIdentities{};
        std::size_t admitted{},dead{};
        bool intro{};
    };

    struct Registered final { coo::Token token{}; Operation operation{}; };

    [[nodiscard]] const CommandBinding* registered(coo::Token token) const noexcept {
        for (std::size_t i = 0; i < registeredCount_; ++i)
            if (registered_[i].token == token) {
                static CommandBinding result{};
                result.operation = registered_[i].operation;
                return &result;
            }
        return nullptr;
    }

    [[nodiscard]] bool remember(coo::Token token, Operation operation) noexcept {
        if (registeredCount_ == registered_.size()) return false;
        for (std::size_t i = 0; i < registeredCount_; ++i)
            if (registered_[i].token == token) return registered_[i].operation == operation;
        registered_[registeredCount_++] = {token, operation};
        if (operation == Operation::capture) captureToken_ = token;
        if (operation == Operation::travelEncounter) travelToken_ = token;
        if (operation == Operation::travelArrived) travelArrivalToken_ = token;
        if (operation == Operation::bossDead) bossDeadToken_ = token;
        if (operation == Operation::retireEncounter) retirementToken_ = token;
        return true;
    }

    void forget(coo::Token token) noexcept {
        for (std::size_t i = 0; i < registeredCount_; ++i) if (registered_[i].token == token) {
            registered_[i] = registered_[--registeredCount_];registered_[registeredCount_] = {};return;
        }
    }

    [[nodiscard]] static std::uint64_t mix_selection(std::uint32_t seed,
        std::uint64_t round) noexcept {
        std::uint64_t value = static_cast<std::uint64_t>(seed)
            ^ (round * UINT64_C(0x9E3779B97F4A7C15));
        value ^= value >> 30;value *= UINT64_C(0xBF58476D1CE4E5B9);
        value ^= value >> 27;value *= UINT64_C(0x94D049BB133111EB);
        return value ^ (value >> 31);
    }

    [[nodiscard]] static std::uint64_t qualified_identity(const coo::PopulationActor& actor) noexcept {
        return (static_cast<std::uint64_t>(actor.actor) << 32) | actor.entity;
    }

    [[nodiscard]] GeneratedRecord* generated(std::uint64_t identity) noexcept {
        for (std::size_t i = 0; i < generatedCount_; ++i)
            if (generated_[i].identity == identity) return &generated_[i];
        return nullptr;
    }

    [[nodiscard]] const GeneratedRecord* generated(std::uint64_t identity) const noexcept {
        for (std::size_t i = 0; i < generatedCount_; ++i)
            if (generated_[i].identity == identity) return &generated_[i];
        return nullptr;
    }

    [[nodiscard]] static std::optional<std::size_t> phase_index(timed_round::Phase phase) noexcept {
        switch (phase) {
        case timed_round::Phase::entry: return 0;
        case timed_round::Phase::traversal: return 1;
        case timed_round::Phase::toEncounter: return 2;
        case timed_round::Phase::encounter: return 3;
        case timed_round::Phase::returning: return 4;
        case timed_round::Phase::rewards: return 5;
        default: return {};
        }
    }

    [[nodiscard]] static const CommandBinding* find_binding(const Definition& definition,
        const coo::CommandSpec& spec) noexcept {
        const CommandBinding* found{};
        for (const auto& binding : definition.commands) {
            if (binding.command.operation == spec.operation && binding.command.asset == spec.asset
                && binding.command.argument == spec.argument && binding.command.wait == spec.wait) {
                if (found) return nullptr;
                found = &binding;
            }
        }
        return found;
    }

    [[nodiscard]] static bool valid_clock(std::uint64_t clock) noexcept {
        return clock != (std::numeric_limits<std::uint64_t>::max)();
    }

    [[nodiscard]] std::uint64_t effective_clock(std::uint64_t clock) const noexcept {
        const auto phase=round_.snapshot().phase;
        if (bossDeathObserved_ && (phase==timed_round::Phase::toEncounter
                || phase==timed_round::Phase::encounter)
            && valid_clock(bossDeathClock_) && valid_clock(clock) && bossDeathClock_<clock)
            return bossDeathClock_;
        return clock;
    }

public:
    [[nodiscard]] bool prepare_return_travel(coo::Token token,
        std::uint32_t destination) noexcept {
        return prepare_non_encounter_travel(Operation::travelEntry, token, destination);
    }

    [[nodiscard]] bool prepare_reward_travel(coo::Token token,
        std::uint32_t destination) noexcept {
        return prepare_non_encounter_travel(Operation::travelRewards, token, destination);
    }

private:
    [[nodiscard]] bool prepare_non_encounter_travel(Operation operation,
        coo::Token token, std::uint32_t destination) noexcept {
        const auto phase = round_.snapshot().phase;
        if (!started() || !destination || !token.run || token.run != owner_.sessionId
            || !token.incarnation || (operation == Operation::travelEntry
                ? phase != timed_round::Phase::returning : phase != timed_round::Phase::rewards))
            return false;
        const auto* binding = registered(token);
        if (!binding || binding->operation != operation || travelIntent_) return false;
        if (requestCounter_ == (std::numeric_limits<std::uint64_t>::max)()) return false;
        travelToken_ = token;travelRound_ = round_.snapshot().token.round;
        travelCohort_ = ++requestCounter_;travelDestination_ = destination;
        travelOperation_ = operation;travelIntent_ = true;travelRequested_ = false;
        travelArrivalArmed_ = travelSawArrival_ = travelArrivalQualified_ = false;
        return true;
    }

    [[nodiscard]] bool deliver_spawn_receipt() noexcept {
        if (!spawnArmed_ || spawnCompleted_ || !bossIdentity_) return true;
        if (!enqueue({spawnToken_,coo::Milestone::nativeReady})
            || !enqueue({spawnToken_,coo::Milestone::completed})) return false;
        spawnCompleted_ = true;return true;
    }

    void deliver_early_boss_death() noexcept {
        if (!earlyBossDeath_ || !bossDeadToken_.run || !bossDeathObserved_
            || round_.snapshot().phase != timed_round::Phase::encounter) return;
        if (enqueue({bossDeadToken_,coo::Milestone::observed})) earlyBossDeath_ = false;
    }

    void update_mid_readiness() noexcept {
        if (!definition_ || selectedEncounter_ >= definition_->encounters.size()) return;
        const auto& encounter = definition_->encounters[selectedEncounter_];
        for (const auto index : encounter.introPopulationIndices) {
            bool found{};
            for (std::size_t i = 0; i < encounterSourceCount_; ++i)
                if (encounterSources_[i].populationIndex == index) {
                    found = encounterSources_[i].admitted && encounterSources_[i].dead
                        >= encounterSources_[i].admitted;
                    break;
                }
            if (!found) return;
        }
        midReady_ = !encounter.midPopulationIndices.empty();
    }

    [[nodiscard]] std::optional<std::size_t> encounter_source(
        std::uint16_t populationIndex) noexcept {
        for (std::size_t i = 0; i < encounterSourceCount_; ++i)
            if (encounterSources_[i].populationIndex == populationIndex) return i;
        if (!definition_ || selectedEncounter_ >= definition_->encounters.size()) return {};
        const auto& encounter = definition_->encounters[selectedEncounter_];
        bool selected{};
        if (populationIndex == encounter.bossPopulationIndex) selected = true;
        for (const auto index : encounter.introPopulationIndices) selected |= index == populationIndex;
        for (const auto index : encounter.midPopulationIndices) selected |= index == populationIndex;
        if (!selected || encounterSourceCount_ == encounterSources_.size()) return {};
        auto& result = encounterSources_[encounterSourceCount_];
        result = {};result.populationIndex = populationIndex;
        result.intro = false;
        for (const auto index : encounter.introPopulationIndices) result.intro |= index == populationIndex;
        ++encounterSourceCount_;
        return encounterSourceCount_ - 1;
    }

    void reset_travel() noexcept {
        travelToken_ = {};travelArrivalToken_ = {};travelIntent_ = travelRequested_ = false;
        travelArrivalArmed_ = travelSawArrival_ = travelArrivalQualified_ = false;
        travelDestination_ = 0;travelOperation_ = Operation::travelEncounter;
    }

    void reset_round_state() noexcept {
        selectedRound_ = 0;selectedBoss_ = 0;selectionSeed_ = 0;
        selectedEncounter_ = (std::numeric_limits<std::uint32_t>::max)();
        selectedBossPopulation_ = kMissingIndex;bossIdentity_ = 0;bossSource_ = {};
        bossAdmitted_ = spawnArmed_ = spawnCompleted_ = earlyBossDeath_ = false;
        bossDeathObserved_ = false;bossDeathClock_ = (std::numeric_limits<std::uint64_t>::max)();
        spawnToken_ = {};bossDeadToken_ = {};retirementToken_ = {};
        progressToken_ = {};progressArmed_ = progressDelivered_ = false;
        retirementQualified_ = false;collapsePublished_ = false;
        encounterRequested_ = {};encounterRequestedCount_ = 0;
        encounterSources_ = {};encounterSourceCount_ = 0;midReady_ = midRequested_ = false;
        reset_travel();
    }

    template <class Ports>
    [[nodiscard]] bool commit_pending_transition(Ports& ports,
        std::uint64_t clockTicks) noexcept {
        if (executor_.diagnostics().phase != coo::Phase::complete) return true;
        const auto phase = round_.snapshot().phase;
        if (pendingCapture_) {
            const auto result = round_.capture_complete(round_.snapshot().token,
                pendingCaptureClock_ == (std::numeric_limits<std::uint64_t>::max)()
                    ? clockTicks : pendingCaptureClock_);
            if (result != timed_round::Result::accepted) return result == timed_round::Result::duplicate;
            pendingCapture_ = false;captureReceipt_ = false;
            pendingCaptureClock_ = (std::numeric_limits<std::uint64_t>::max)();
            return true;
        }
        if (phase == timed_round::Phase::toEncounter && travelArrivalQualified_) {
            if constexpr (requires(Ports& value) { value.encounter_arrived(); })
                if (!ports.encounter_arrived()) return false;
            const auto result = encounter_arrived(clockTicks);
            return result == timed_round::Result::accepted || result == timed_round::Result::duplicate;
        }
        if (phase == timed_round::Phase::encounter && bossDeathObserved_) {
            const auto result = round_.encounter_defeated(round_.snapshot().token,
                bossDeathClock_ == (std::numeric_limits<std::uint64_t>::max)()
                    ? clockTicks : bossDeathClock_, bossIdentity_);
            if (result == timed_round::Result::accepted) return true;
            return result == timed_round::Result::duplicate;
        }
        if (phase == timed_round::Phase::returning && retirementQualified_
            && travelArrivalQualified_) {
            const auto result = returned(clockTicks);
            return result == timed_round::Result::accepted || result == timed_round::Result::duplicate;
        }
        if (phase == timed_round::Phase::rewards && completionRequested_) {
            const auto result = rewards_finished(clockTicks);
            return result == timed_round::Result::accepted || result == timed_round::Result::duplicate;
        }
        return true;
    }

    void reset_value() noexcept {
        wipe_={};defeatPending_=false;defeatRewards_=false;
        owner_ = {};
        boot_ = 0;
        definition_ = nullptr;
        document_ = nullptr;
        round_ = {};
        executor_ = {};
        lifecycle_.reset();
        graphPhase_ = timed_round::Phase::faulted;
        selectedRound_ = selectedBoss_ = selectionSeed_ = endEpoch_ = 0;
        selectedEncounter_ = 0;
        selectedPlatform_ = kMissingIndex;selectedPlatformRound_ = 0;
        progressToken_ = {};captureToken_ = {};travelToken_ = {};travelArrivalToken_ = {};
        pendingCaptureClock_ = (std::numeric_limits<std::uint64_t>::max)();
        registered_ = {};registeredCount_ = 0;
        completion_ = {};
        progressArmed_ = progressDelivered_ = bossAdmitted_ = retirementQualified_ = false;
        rewardArrived_ = rewardsAccepted_ = completionRequested_ = failed_ = false;
        captureReceipt_ = pendingCapture_ = travelIntent_ = travelRequested_ = false;
        travelArrivalArmed_ = travelSawArrival_ = travelArrivalQualified_ = collapsePublished_ = false;
        travelRound_ = travelCohort_ = requestCounter_ = 0;
        travelDestination_ = 0;
        unknownScoreEvents_ = 0;
        generated_ = {};
        generatedCount_ = 0;
        selectedBossPopulation_ = kMissingIndex;
        spawnToken_ = {};bossDeadToken_ = {};retirementToken_ = {};
        travelOperation_ = Operation::travelEncounter;
        bossIdentity_ = 0;bossSource_ = {};
        bossDeathClock_ = (std::numeric_limits<std::uint64_t>::max)();
        selectedEncounter_ = (std::numeric_limits<std::uint32_t>::max)();
        encounterRequested_ = {};encounterRequestedCount_ = 0;
        encounterSources_ = {};encounterSourceCount_ = 0;
    }

    Owner owner_{};
    std::uint64_t boot_{};
    const Definition* definition_{};
    const Document* document_{};
    timed_round::Service round_{};
    coo::Executor executor_{};
    coo::LifecycleService lifecycle_{};
    timed_round::Phase graphPhase_{timed_round::Phase::faulted};
    coo::Token progressToken_{};
    coo::Token captureToken_{}, travelToken_{}, travelArrivalToken_{};
    coo::Token spawnToken_{}, bossDeadToken_{}, retirementToken_{};
    coo::CompletionPublication completion_{};
    round_wipe::Service wipe_{};bool defeatPending_{},defeatRewards_{};
    std::uint64_t selectedRound_{}, selectedBoss_{}, selectionSeed_{}, endEpoch_{};
    std::uint64_t bossIdentity_{}, bossDeathClock_{(std::numeric_limits<std::uint64_t>::max)()};
    coo::PopulationOwner bossSource_{};
    std::uint16_t selectedPlatform_{kMissingIndex};
    std::uint64_t selectedPlatformRound_{};
    std::uint32_t selectedEncounter_{(std::numeric_limits<std::uint32_t>::max)()};
    std::uint16_t selectedBossPopulation_{kMissingIndex};
    std::uint64_t travelRound_{}, travelCohort_{}, requestCounter_{};
    std::uint32_t travelDestination_{};
    Operation travelOperation_{Operation::travelEncounter};
    std::array<std::uint16_t,kMaximumEncounterPopulations> encounterRequested_{};
    std::size_t encounterRequestedCount_{};
    std::array<EncounterSource,kMaximumEncounterPopulations> encounterSources_{};
    std::size_t encounterSourceCount_{};
    std::array<Registered, kMaximumScoreEntries> registered_{};
    std::size_t registeredCount_{};
    std::array<GeneratedRecord, kMaximumGeneratedActors> generated_{};
    std::size_t generatedCount_{};
    std::uint32_t unknownScoreEvents_{};
    std::uint64_t pendingCaptureClock_{(std::numeric_limits<std::uint64_t>::max)()};
    bool progressArmed_{}, progressDelivered_{}, bossAdmitted_{}, retirementQualified_{};
    bool captureReceipt_{}, pendingCapture_{}, travelIntent_{}, travelRequested_{};
    bool travelArrivalArmed_{}, travelSawArrival_{}, travelArrivalQualified_{}, collapsePublished_{};
    bool rewardArrived_{}, rewardsAccepted_{}, completionRequested_{}, failed_{};
    bool spawnArmed_{}, spawnCompleted_{}, earlyBossDeath_{}, bossDeathObserved_{};
    bool midReady_{}, midRequested_{};
};

static_assert(std::is_trivially_copyable_v<Runtime>);

} // namespace sunrise::server::runtime::activity::round_activity
