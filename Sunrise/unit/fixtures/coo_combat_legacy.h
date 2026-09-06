// Frozen accepted Forest candidate; only includes/namespace adapted.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "state/activity/omega_enemy_chase_catalog.h"
#include "state/activity/omega_enemy_crown_catalog.h"
#include "state/activity/omega_enemy_crown_waves.h"
#include "state/activity/omega_rescue_scene_authority.h"

namespace sunrise::state::activity::frozen_combat {

enum class Action : std::uint8_t {
    none, summonLeft, summonRight, depart, summonBoth,
    beginDeletion,breakShield,endEyePhase,relocateFinal,finishEncounter
};
enum class Phase : std::uint8_t {
    initial, leftReady, leftRequested, leftPlaying, rightReady, rightRequested,
    rightPlaying, clearing, departureReady, departing, complete,
    bothReady, bothRequested, bothPlaying,mechanicReady,mechanicRequested,mechanicPlaying,waiting
};
struct Boss final {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},character{UINT32_MAX},entity{UINT32_MAX};
    std::uint32_t generation{},revision{};
    std::uint8_t island{};
    std::uint32_t actionEpoch{};
    bool operator==(const Boss&) const = default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return run!=0 && actor!=UINT32_MAX && character!=UINT32_MAX
            && entity!=UINT32_MAX && generation!=0 && island<=4;
    }
};
/** Crown mechanics use a command epoch independently of the physical island.
 * The caller captures this token when the command is requested and keeps it for
 * every native receipt from that request. */
struct CrownToken final {
    Boss boss{};
    std::uint8_t cycle{};
    bool operator==(const CrownToken&) const = default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return boss.valid() && boss.island==4 && cycle>=1 && cycle<=3;
    }
};
enum class CrownStage : std::uint8_t {
    none,waves,deletion,rescue,route,carrying,eyeOpening,eyeDps,recovery,
    relocationAdds,relocation,finalArrival,death,ending,finished
};
enum class AnimationMilestone : std::uint8_t {
    deletionStarted,deletionHold,eyeExposing,eyeVulnerable,recoveryStarted,
    recovered,deathStarted,deathFinished
};
enum class SceneMilestone : std::uint8_t { started,rescueReady,eyeReady,completed };
enum class HealthMilestone : std::uint8_t {
    eyeThresholdReached,eyeDepleted=eyeThresholdReached,checkpointReached,bossDead
};
enum class ChargeMilestone : std::uint8_t { pickedUp,dropped,dunked };
enum class GateMilestone : std::uint8_t { firstRing,chargePlatform,returnPortal,eyePlatform,finalPlatform,finalCannon };
struct ChargeReceipt final {
    CrownToken token{};
    std::uint32_t sourceHandle{UINT32_MAX},generation{},itemEntity{UINT32_MAX},player{UINT32_MAX};
    std::uint32_t sinkHandle{UINT32_MAX},registry{};
    std::uint16_t sourceSlot{},sinkSlot{};
    bool operator==(const ChargeReceipt&) const = default;
};
struct SceneRequest final {
    CrownToken token{};
    omega_rescue_npc::SceneCommand command{};
    bool enabled{};
};
struct Group final {
    std::uint16_t source{};
    std::uint8_t count{};
    Action summon{};
    bool member{};
    std::uint8_t island{};
    std::uint32_t registry{0xF4D0E0B2U};
    std::array<std::uint8_t,2> requested{};
    std::uint8_t cycle{},wave{};
    bool required{true};
};
/** Identity sampled at native admission and again before the native death
 * subscriber retires the actor. Source handles include their allocation salt. */
struct ActorReceipt final {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},sourceHandle{UINT32_MAX},generation{};
    std::uint16_t source{};
    std::uint32_t registry{0xF4D0E0B2U};
    bool operator==(const ActorReceipt&) const = default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return run!=0 && actor!=UINT32_MAX && sourceHandle!=UINT32_MAX && generation!=0
            && registry!=0 && registry!=UINT32_MAX;
    }
};
/** Reconstructed first-test policy, not the recovered original host script.
 * Exact catalog slots, species and spawn rules are authored. Retail frames
 * 246-248 s show the six central Goblins materialising at the gate while the
 * authored intro two-arm summon (graph 80F45178 record 0 node 4, clip
 * 80F45188) is still playing: sq_front_1/2 (sources 3/4, the gate placements)
 * are that summonBoth cohort. Side/sniper and wave_b/guards/Achronos remain the
 * later left/right arm batches; their 6+9 split still needs visual comparison.
 * Batches overlap; there is no kill barrier between the three summons. */
inline constexpr std::array<Group,11> kGroups{{
    {3,3,Action::summonBoth,false},{4,3,Action::summonBoth,false},
    {7,2,Action::summonLeft,false},{8,2,Action::summonLeft,false},
    {9,1,Action::summonLeft,false},{10,1,Action::summonLeft,false},
    {11,3,Action::summonRight,false},{12,3,Action::summonRight,false},
    {13,1,Action::summonRight,false},{14,1,Action::summonRight,false},
    {1,1,Action::summonRight,true},
}};

/** Keep kGroups as the unchanged initial-area recipe for existing fixtures.
 * Chase populations and tactical joins are independently catalogued. A single
 * arm summons each chase cohort: A left, B right, C left. This arm assignment
 * is reconstructed host policy, not recovered retail script behavior. */
inline constexpr auto kAllGroups=[] {
    std::array<Group,kGroups.size()+omega_enemy_chase::kGroups.size()+omega_enemy_crown::kWaves.size()> groups{};
    for(std::size_t i=0;i<kGroups.size();++i) { groups[i]=kGroups[i]; }
    for(std::size_t i=0;i<omega_enemy_chase::kGroups.size();++i) {
        const auto& source=omega_enemy_chase::kGroups[i];
        groups[kGroups.size()+i]={source.source,source.requested,
            source.island==2?Action::summonRight:Action::summonLeft,false,source.island};
    }
    for(std::size_t i=0;i<omega_enemy_crown::kWaves.size();++i) {
        const auto& source=omega_enemy_crown::kWaves[i];
        const auto action=source.wave==0?Action::summonBoth:source.wave==1?Action::summonLeft:Action::summonRight;
        groups[kGroups.size()+omega_enemy_chase::kGroups.size()+i]={source.source,
            static_cast<std::uint8_t>(source.requested[0]+source.requested[1]),action,
            source.member,4,source.registry,source.requested,source.cycle,source.wave,source.required};
    }
    return groups;
}();
inline constexpr auto kMaximumGroupPopulation=[] {
    std::uint8_t maximum{};
    for(const auto& group:kAllGroups) { if(group.count>maximum) { maximum=group.count; } }
    return maximum;
}();
static_assert(kMaximumGroupPopulation>0);
static_assert([] {
    for(std::size_t i=0;i<kAllGroups.size();++i) {
        const auto& group=kAllGroups[i];
        if(group.count==0 || group.source>=21 || group.island>4
            || (group.island==4 && (group.cycle<1 || group.cycle>3
                || omega_enemy_crown::find_spawner(group.source,group.registry)==nullptr))) { return false; }
        for(std::size_t j=0;j<i;++j) {
            if(kAllGroups[j].source==group.source && kAllGroups[j].registry==group.registry) { return false; }
        }
    }
    return true;
}());

/** Host policy consumes actual native receipts. No elapsed-time kill inference,
 * world-coordinate writes or client toy-spawner calls belong to this state. */
class Encounter final {
public:
    void begin(std::uint64_t run) noexcept { *this={};run_=run; }
    /** The authored intro graph plays the two-arm summon (record 0 node 4,
     * clip 80F45188) itself after the fly-in; no host command or claim exists.
     * Its native loaded receipt starts the island-zero summonBoth cohort. */
    [[nodiscard]] bool initial_summon(const Boss& boss) noexcept {
        if(!initial_receipt(boss) || phase_!=Phase::initial) { return false; }
        boss_=boss;phase_=Phase::bothPlaying;enable_groups(Action::summonBoth);return true;
    }
    /** Terminal idle (node 2) is reachable only through node 4, so it is also
     * the summonBoth finish. Without the start receipt it still proves the clip
     * played: the cohort is released late rather than never. Retail overlaps
     * the batches, so the first arm needs no kill barrier. */
    [[nodiscard]] bool initial_idle(const Boss& boss) noexcept {
        if(!initial_receipt(boss)) { return false; }
        if(phase_==Phase::initial) { boss_=boss;enable_groups(Action::summonBoth); }
        else if(phase_!=Phase::bothPlaying || boss!=boss_) { return false; }
        finish_initial_summon();return true;
    }
    [[nodiscard]] Action pending() const noexcept {
        if(failed_) { return Action::none; }
        if(phase_==Phase::leftReady) { return Action::summonLeft; }
        if(phase_==Phase::rightReady) { return Action::summonRight; }
        if(phase_==Phase::departureReady) { return Action::depart; }
        if(phase_==Phase::bothReady) { return Action::summonBoth; }
        if(phase_==Phase::mechanicReady) { return mechanicAction_; }
        return Action::none;
    }
    /** Caller verifies a fresh native owner before claiming. A claim precedes
     * the native call, so an uncertain return cannot issue the action twice. */
    [[nodiscard]] bool claim(const Boss& boss,Action action) noexcept {
        if(boss!=boss_ || action==Action::none || pending()!=action) { return false; }
        if(action==Action::summonLeft) { phase_=Phase::leftRequested; }
        else if(action==Action::summonRight) { phase_=Phase::rightRequested; }
        else if(action==Action::summonBoth) { phase_=Phase::bothRequested; }
        else if(action==Action::depart) { phase_=Phase::departing; }
        else { phase_=Phase::mechanicRequested; }
        return true;
    }
    [[nodiscard]] bool summon_started(const Boss& boss,Action action) noexcept {
        if(failed_ || boss!=boss_) { return false; }
        if(action==Action::summonLeft && phase_==Phase::leftRequested) {
            phase_=Phase::leftPlaying;enable_groups(action);return true;
        }
        if(action==Action::summonRight && phase_==Phase::rightRequested) {
            phase_=Phase::rightPlaying;enable_groups(action);return true;
        }
        if(action==Action::summonBoth && phase_==Phase::bothRequested) {
            phase_=Phase::bothPlaying;enable_groups(action);return true;
        }
        return false;
    }
    [[nodiscard]] bool summon_finished(const Boss& boss,Action action) noexcept {
        if(failed_ || boss!=boss_) { return false; }
        if(action==Action::summonLeft && phase_==Phase::leftPlaying) {
            if(has_action(Action::summonRight)) { phase_=Phase::rightReady; }
            else { phase_=Phase::clearing;advance_clear(); }
            return true;
        }
        if(action==Action::summonRight && phase_==Phase::rightPlaying) {
            phase_=Phase::clearing;advance_clear();return true;
        }
        if(action==Action::summonBoth && phase_==Phase::bothPlaying) {
            if(island_==0) { finish_initial_summon();return true; }
            phase_=Phase::clearing;advance_clear();return true;
        }
        return false;
    }
    [[nodiscard]] bool source_enabled(std::uint16_t slot,std::uint32_t registry=0xF4D0E0B2U) const noexcept {
        for(std::size_t i=0;i<kAllGroups.size();++i) {
            if(kAllGroups[i].source==slot && kAllGroups[i].registry==registry && enabled_[i]) { return true; }
        }
        return false;
    }
    [[nodiscard]] bool admitted(const ActorReceipt& receipt) noexcept {
        const auto index=find_admission_group(receipt.source,receipt.registry);
        if(failed_ || !receipt.valid() || receipt.run!=run_ || receipt.generation!=boss_.generation
            || !source_enabled(receipt.source,receipt.registry)) { return false; }
        // A full native actor identity belongs to only one source in this run.
        for(std::size_t i=0;i<actors_.size();++i) {
            for(std::uint8_t n=0;n<counts_[i];++n) {
                if(actors_[i][n].receipt.actor==receipt.actor) { return false; }
            }
        }
        if(index>=kAllGroups.size()) {
            // Unexpected population of a required cohort fails closed. The nonblocking
            // escape cohort (required=false) is never a barrier, so an extra native
            // actor there is ignored rather than aborting the fight.
            for(std::size_t i=0;i<kAllGroups.size();++i) {
                const auto& group=kAllGroups[i];
                if(group.source==receipt.source && group.registry==receipt.registry && enabled_[i] && !group.required) { return false; }
            }
            failed_=true;return false;
        }
        actors_[index][counts_[index]++].receipt=receipt;
        return true;
    }
    /** Only a verified native death transition may call this. Spawner consumed,
     * missing/streamed-out actors and removal callbacks are not death receipts. */
    [[nodiscard]] bool died(const ActorReceipt& receipt) noexcept {
        if(failed_ || !receipt.valid() || receipt.run!=run_ || receipt.generation!=boss_.generation) { return false; }
        for(std::size_t i=0;i<actors_.size();++i) {
            for(std::uint8_t n=0;n<counts_[i];++n) {
                if(actors_[i][n].receipt==receipt) {
                    if(actors_[i][n].dead) { return false; }
                    actors_[i][n].dead=true;advance_clear();return true;
                }
            }
        }
        return false;
    }
    /** Departure requires both native fold completion and the authored next
     * path milestone receipt, supplied by the actor command owner. */
    [[nodiscard]] bool departed(const Boss& boss,bool folded,bool atMilestone) noexcept {
        if(failed_ || boss!=boss_ || phase_!=Phase::departing || !folded || !atMilestone) { return false; }
        completedIslands_=static_cast<std::uint8_t>(completedIslands_|(1U<<island_));
        phase_=Phase::complete;
        if(pendingArrival_!=0) { start_arrived_island(); }
        start_crown_if_ready();
        return true;
    }
    /** Geometry is validated by the native integration owner. Only the next
     * island may latch, after departure is claimed or completed; early landing
     * during the fold cannot bypass its eventual native completion receipt. */
    [[nodiscard]] bool arrived(std::uint64_t run,std::uint8_t island) noexcept {
        if(failed_ || run==0 || run!=run_ || !boss_.valid() || island<1 || island>3
            || island!=island_+1 || pendingArrival_!=0
            || (phase_!=Phase::departing && phase_!=Phase::complete)) { return false; }
        pendingArrival_=island;
        if(phase_==Phase::complete) { start_arrived_island(); }
        return true;
    }
    [[nodiscard]] bool crown_arrived(std::uint64_t run) noexcept {
        if(failed_ || run==0 || run!=run_ || island_!=3 || crownArrived_
            || (phase_!=Phase::departing && phase_!=Phase::complete)) { return false; }
        crownArrived_=true;start_crown_if_ready();return true;
    }
    void cannon_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index=0) noexcept {
        if(run!=0 && run==run_ && generation!=0 && index<cannonGenerations_.size()
            && (!boss_.valid() || generation==boss_.generation)) {
            cannonGenerations_[index]=generation;
            start_crown_if_ready();
        }
    }
    [[nodiscard]] bool cannon_active() const noexcept {
        return !failed_ && (completedIslands_&1U)!=0 && (cannon_prepared_mask()&7U)==7U;
    }
    [[nodiscard]] bool final_cannon_active() const noexcept {
        return !failed_ && (completedIslands_&8U)!=0 && (cannon_prepared_mask()&8U)!=0;
    }
    [[nodiscard]] bool crown_restricted() const noexcept { return crownArrived_ && final_cannon_active() && !restrictionReleased_; }
    [[nodiscard]] std::uint8_t cannon_prepared_mask() const noexcept {
        std::uint8_t mask{};
        if(!boss_.valid()) { return mask; }
        for(std::uint8_t i=0;i<cannonGenerations_.size();++i) {
            if(cannonGenerations_[i]==boss_.generation) { mask=static_cast<std::uint8_t>(mask|(1U<<i)); }
        }
        return mask;
    }
    void invalidate(std::uint64_t run) noexcept { if(run!=0 && run==run_) { failed_=true; } }
    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] const Boss& boss() const noexcept { return boss_; }
    [[nodiscard]] std::uint64_t run() const noexcept { return run_; }
    [[nodiscard]] std::uint8_t island() const noexcept { return island_; }
    [[nodiscard]] std::uint8_t cycle() const noexcept { return cycle_; }
    [[nodiscard]] std::uint8_t wave() const noexcept { return wave_; }
    [[nodiscard]] CrownStage crown_stage() const noexcept { return crownStage_; }
    [[nodiscard]] CrownToken token() const noexcept { return {boss_,cycle_}; }
    [[nodiscard]] bool group_enabled(std::size_t index) const noexcept {
        return index<enabled_.size() && enabled_[index];
    }
    [[nodiscard]] const omega_rescue_npc::Commands& rescue_scenes() const noexcept { return scenes_; }
    [[nodiscard]] SceneRequest scene_request(std::uint16_t slot) const noexcept {
        const auto index=scene_index(slot);
        if(index>=scenes_.size()) { return {}; }
        return {sceneTokens_[index],scenes_[index],!failed_ && scenes_[index].generation!=0};
    }
    [[nodiscard]] bool route_enabled() const noexcept {
        if(failed_ || !rescueReady_ || cycle_==0) { return false; }
        if(cycle_==3) { return true; }
        const auto mask=static_cast<std::uint8_t>(7U<<((cycle_-1U)*3U));
        return (transit_prepared_mask()&mask)==mask;
    }
    [[nodiscard]] bool charge_dunked() const noexcept { return !failed_ && chargeDunked_; }
    [[nodiscard]] bool eye_status_active() const noexcept {
        return !failed_ && chargeDunked_ && boss_.valid()
            && (crownStage_==CrownStage::eyeOpening || crownStage_==CrownStage::eyeDps);
    }
    [[nodiscard]] bool transit_bridge() const noexcept { return route_enabled() && carriedOnce_; }
    /** A qualified native deposit requests the DPS transit. The runtime also
     * waits for native creation of its receiving platform before publishing it.
     * Depositing consumes the charge, so carrying-only proximity cannot gate
     * this transition. */
    [[nodiscard]] bool transit_target() const noexcept {
        return !failed_ && chargeDunked_;
    }
    [[nodiscard]] bool return_launch() const noexcept {
        return !failed_ && returnLaunch_ && (cycle_==1 || cycle_==2);
    }
    /** Source95FB/4/30 was enabled and its full native entity was created at
     * this run's committed generation. This proves launcher readiness, not
     * player arrival: native collision and its authored spline own movement. */
    [[nodiscard]] bool return_created(std::uint64_t run,std::uint32_t entity) noexcept {
        if(run!=run_ || !return_launch() || crownStage_!=CrownStage::recovery
            || phase_!=Phase::mechanicPlaying || returnReady_ || entity==UINT32_MAX
            || entity==returnEntities_[0] || entity==returnEntities_[1]) { return false; }
        returnEntities_[cycle_-1U]=entity;returnReady_=true;finish_recovery();return true;
    }
    [[nodiscard]] bool charge_enabled() const noexcept {
        const auto platformBit=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(GateMilestone::chargePlatform));
        return route_enabled() && (cycle_==3 || (gateArrivals_&platformBit)!=0)
            && (crownStage_==CrownStage::route || crownStage_==CrownStage::carrying);
    }
    [[nodiscard]] bool final_traversal() const noexcept {
        return !failed_ && finalDeparted_ && (transit_prepared_mask()&0x40U)!=0;
    }
    [[nodiscard]] bool ending_requested() const noexcept { return !failed_ && crownStage_==CrownStage::ending; }
    [[nodiscard]] bool animation(const CrownToken& owner,AnimationMilestone event) noexcept {
        if(!current(owner)) { return false; }
        if(event==AnimationMilestone::deletionStarted && crownStage_==CrownStage::deletion
            && phase_==Phase::mechanicRequested && mechanicAction_==Action::beginDeletion) {
            // Preserve the first return across the next combat wave, but
            // remove its collision before the player revisits the eye platform.
            returnLaunch_=false;
            phase_=Phase::mechanicPlaying;crownStage_=CrownStage::rescue;
            return start_scene(rescue_slot());
        }
        if(event==AnimationMilestone::deletionHold && crownStage_==CrownStage::rescue && !deletionHold_) {
            deletionHold_=true;return true;
        }
        if(event==AnimationMilestone::eyeExposing && crownStage_==CrownStage::eyeOpening
            && phase_==Phase::mechanicRequested && mechanicAction_==Action::breakShield) {
            phase_=Phase::mechanicPlaying;return true;
        }
        if(event==AnimationMilestone::eyeVulnerable && crownStage_==CrownStage::eyeOpening
            && (phase_==Phase::mechanicRequested || phase_==Phase::mechanicPlaying)
            && mechanicAction_==Action::breakShield) {
            crownStage_=CrownStage::eyeDps;phase_=Phase::waiting;return true;
        }
        if(event==AnimationMilestone::recoveryStarted && crownStage_==CrownStage::recovery
            && phase_==Phase::mechanicRequested && mechanicAction_==Action::endEyePhase) {
            returnLaunch_=true;returnReady_=false;phase_=Phase::mechanicPlaying;return true;
        }
        if(event==AnimationMilestone::recovered && crownStage_==CrownStage::recovery
            && (phase_==Phase::mechanicRequested || phase_==Phase::mechanicPlaying) && !recovered_) {
            recovered_=true;finish_recovery();return true;
        }
        if(event==AnimationMilestone::deathStarted && crownStage_==CrownStage::death
            && phase_==Phase::mechanicRequested && mechanicAction_==Action::endEyePhase) {
            phase_=Phase::mechanicPlaying;return true;
        }
        if(event==AnimationMilestone::deathFinished && crownStage_==CrownStage::death
            && (phase_==Phase::mechanicRequested || phase_==Phase::mechanicPlaying) && !deathFinished_) {
            deathFinished_=true;finish_death();return true;
        }
        return false;
    }
    [[nodiscard]] bool scene(const CrownToken& owner,std::uint16_t slot,SceneMilestone event) noexcept {
        const auto index=scene_index(slot);
        if(failed_ || index>=scenes_.size() || !owner.valid() || sceneTokens_[index]!=owner
            || scenes_[index].generation==0 || static_cast<unsigned>(event)>static_cast<unsigned>(SceneMilestone::completed)) { return false; }
        const auto bit=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(event));
        if((sceneMilestones_[index]&bit)!=0) { return false; }
        if(event==SceneMilestone::rescueReady) {
            if(!current(owner) || crownStage_!=CrownStage::rescue || slot!=rescue_slot()) { return false; }
            rescueReady_=true;crownStage_=CrownStage::route;phase_=Phase::waiting;
            if(cycle_==1) {
                if(!start_scene(81) || !start_scene(82)) { return false; }
            } else if(cycle_==2) {
                if(!start_scene(68) || !start_scene(67)) { return false; }
            }
        } else if(event!=SceneMilestone::started && event!=SceneMilestone::completed) { return false; }
        sceneMilestones_[index]=static_cast<std::uint8_t>(sceneMilestones_[index]|bit);
        return true;
    }
    [[nodiscard]] bool charge(const ChargeReceipt& receipt,ChargeMilestone event) noexcept {
        if(!current(receipt.token) || receipt.generation!=boss_.generation || receipt.sourceHandle==UINT32_MAX
            || receipt.itemEntity==UINT32_MAX || receipt.player==UINT32_MAX
            || receipt.registry!=cycle_registry() || receipt.sourceSlot!=charge_slot()) { return false; }
        if(event==ChargeMilestone::pickedUp && crownStage_==CrownStage::route && charge_enabled()) {
            if(cycle_<3 && !scene_event(cycle_==1?83:66,0x0E57C0FAU)) { return false; }
            carried_=receipt;carriedOnce_=true;crownStage_=CrownStage::carrying;return true;
        }
        if(crownStage_!=CrownStage::carrying || !same_charge(receipt,carried_)) { return false; }
        if(event==ChargeMilestone::dropped) {
            carried_={};crownStage_=CrownStage::route;return true;
        }
        if(event==ChargeMilestone::dunked && receipt.sinkHandle!=UINT32_MAX && receipt.sinkSlot==sink_slot()) {
            if(!scene_event(rescue_slot(),cycle_==3?0x14A9A975U:omega_rescue_npc::kReleaseBlocking)) { return false; }
            if(cycle_<3) { scenes_[scene_index(cycle_==1?83:66)].stop=true; }
            carried_={};chargeDunked_=true;schedule(Action::breakShield,CrownStage::eyeOpening);return true;
        }
        return false;
    }
    [[nodiscard]] bool health(const CrownToken& owner,HealthMilestone event) noexcept {
        if(!current(owner)) { return false; }
        if(event==HealthMilestone::eyeDepleted && crownStage_==CrownStage::eyeDps) {
            // Scene9 retains its eye hold and reminder until this authored
            // release. Its exit child and terminal remain native-owned.
            if(cycle_==1 && !scene_event(9,omega_rescue_npc::kReleaseFirstEye)) { return false; }
            // The final authored Scene releases its DFA chain into Ghost
            // retrieval; its one-second graph delay remains native-owned.
            if(cycle_==3 && !scene_event(46,0x505750FEU)) { return false; }
            recovered_=checkpointReached_=false;
            schedule(Action::endEyePhase,cycle_==3?CrownStage::death:CrownStage::recovery);
            return true;
        }
        if(event==HealthMilestone::checkpointReached && crownStage_==CrownStage::recovery
            && cycle_<3 && !checkpointReached_
            && mechanicAction_==Action::endEyePhase
            && (phase_==Phase::mechanicRequested || phase_==Phase::mechanicPlaying)) {
            checkpointReached_=true;finish_recovery();return true;
        }
        if(event==HealthMilestone::bossDead && crownStage_==CrownStage::death && cycle_==3 && !bossDead_
            && mechanicAction_==Action::endEyePhase
            && (phase_==Phase::mechanicRequested || phase_==Phase::mechanicPlaying)) {
            bossDead_=true;restrictionReleased_=true;finish_death();return true;
        }
        return false;
    }
    [[nodiscard]] bool ending(const CrownToken& owner,bool finished) noexcept {
        if(!current(owner) || crownStage_!=CrownStage::ending || mechanicAction_!=Action::finishEncounter) { return false; }
        if(!finished && phase_==Phase::mechanicRequested) {
            const auto scene=scene_index(46);
            if(scene<scenes_.size() && scenes_[scene].generation!=0) { scenes_[scene].stop=true; }
            phase_=Phase::mechanicPlaying;return true;
        }
        if(finished && phase_==Phase::mechanicPlaying) {
            phase_=Phase::complete;crownStage_=CrownStage::finished;return true;
        }
        return false;
    }
    void transit_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t core) noexcept {
        if(run!=0 && run==run_ && generation!=0 && core<transitGenerations_.size()
            && (!boss_.valid() || generation==boss_.generation)) {
            transitGenerations_[core]=generation;start_final_if_ready();
        }
    }
    [[nodiscard]] std::uint8_t transit_prepared_mask() const noexcept {
        std::uint8_t mask{};
        if(!boss_.valid()) { return mask; }
        for(std::uint8_t i=0;i<transitGenerations_.size();++i) {
            if(transitGenerations_[i]==boss_.generation) { mask=static_cast<std::uint8_t>(mask|(1U<<i)); }
        }
        return mask;
    }
    [[nodiscard]] bool gate_arrived(const CrownToken& owner,GateMilestone gate,std::uint32_t player) noexcept {
        if(!current(owner) || player==UINT32_MAX) { return false; }
        if(gate==GateMilestone::finalCannon) {
            if(crownStage_!=CrownStage::finalArrival || !finalDeparted_ || finalCannonApproached_
                || (finalRoutePlayer_!=UINT32_MAX && finalRoutePlayer_!=player)) { return false; }
            if(!scene_event(84,omega_rescue_npc::kFinalCannonApproach)) { return false; }
            finalRoutePlayer_=player;finalCannonApproached_=true;return true;
        }
        if(gate==GateMilestone::finalPlatform) {
            if(pendingFinalArrival_ || (crownStage_!=CrownStage::finalArrival
                && !(crownStage_==CrownStage::relocation && phase_==Phase::mechanicRequested))
                || (finalRoutePlayer_!=UINT32_MAX && finalRoutePlayer_!=player)) { return false; }
            finalRoutePlayer_=player;pendingFinalArrival_=true;start_final_if_ready();return true;
        }
        if((crownStage_!=CrownStage::route && crownStage_!=CrownStage::carrying) || !route_enabled()
            || static_cast<unsigned>(gate)>=static_cast<unsigned>(GateMilestone::finalPlatform)) { return false; }
        const auto bit=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(gate));
        if((gateArrivals_&bit)!=0 || (routePlayer_!=UINT32_MAX && routePlayer_!=player)) { return false; }
        if(gate==GateMilestone::chargePlatform && cycle_<3) {
            if(!scene_event(cycle_==1?82:67,0x97C48FC6U) || !start_scene(cycle_==1?83:66)) { return false; }
        }
        routePlayer_=player;gateArrivals_=static_cast<std::uint8_t>(gateArrivals_|bit);return true;
    }
    [[nodiscard]] bool final_departed(const CrownToken& owner,bool folded,bool atMilestone) noexcept {
        if(!current(owner) || crownStage_!=CrownStage::relocation || phase_!=Phase::mechanicRequested
            || mechanicAction_!=Action::relocateFinal || !folded || !atMilestone) { return false; }
        // Authored Scene84 requests its three cannon Echoes. Keep that Scene's
        // generation through proximity, arrival and the following boss epoch.
        if(!start_scene(84)) { return false; }
        finalDeparted_=true;crownStage_=CrownStage::finalArrival;phase_=Phase::waiting;start_final_if_ready();return true;
    }
private:
    struct Actor final { ActorReceipt receipt{};bool dead{}; };
    [[nodiscard]] bool initial_receipt(const Boss& boss) const noexcept {
        return !failed_ && run_!=0 && boss.run==run_ && boss.valid() && boss.island==0
            && boss.actionEpoch==0 && island_==0;
    }
    void finish_initial_summon() noexcept {
        if(has_action(Action::summonLeft)) { phase_=Phase::leftReady; }
        else if(has_action(Action::summonRight)) { phase_=Phase::rightReady; }
        else { phase_=Phase::clearing;advance_clear(); }
    }
    [[nodiscard]] std::size_t find_admission_group(std::uint16_t source,std::uint32_t registry) const noexcept {
        for(std::size_t i=0;i<kAllGroups.size();++i) {
            if(kAllGroups[i].source==source && kAllGroups[i].registry==registry && enabled_[i]
                && counts_[i]<kAllGroups[i].count) { return i; }
        }
        return kAllGroups.size();
    }
    [[nodiscard]] bool current(const CrownToken& owner) const noexcept {
        return !failed_ && owner.valid() && owner.boss==boss_ && owner.cycle==cycle_;
    }
    [[nodiscard]] std::uint32_t cycle_registry() const noexcept {
        return cycle_==1?0x0040BF06U:cycle_==2?0x0040BF05U:0x0040BF03U;
    }
    [[nodiscard]] std::uint16_t rescue_slot() const noexcept { return cycle_==1?9:cycle_==2?27:46; }
    [[nodiscard]] std::uint16_t charge_slot() const noexcept { return cycle_==1?18:cycle_==2?1:0; }
    [[nodiscard]] std::uint16_t sink_slot() const noexcept { return cycle_==1?20:cycle_==2?3:2; }
    [[nodiscard]] static std::size_t scene_index(std::uint16_t slot) noexcept {
        for(std::size_t i=0;i<omega_rescue_npc::kScenes.size();++i) {
            if(omega_rescue_npc::kScenes[i].slot==slot) { return i; }
        }
        return omega_rescue_npc::kScenes.size();
    }
    [[nodiscard]] bool start_scene(std::uint16_t slot) noexcept {
        const auto index=scene_index(slot);
        if(index>=scenes_.size() || boss_.actionEpoch==UINT32_MAX) { failed_=true;return false; }
        // The scene generation is scoped to its authored slot and current run.
        // Preserve positive signed range and fail rather than wrap an identity.
        const auto generation=static_cast<std::uint64_t>(boss_.generation)+boss_.actionEpoch;
        if(generation==0 || generation>0x7FFFFFFFULL) { failed_=true;return false; }
        scenes_[index]={static_cast<std::uint32_t>(generation),false,0,{}};
        sceneTokens_[index]=token();sceneMilestones_[index]=0;return true;
    }
    [[nodiscard]] bool scene_event(std::uint16_t slot,std::uint32_t event) noexcept {
        const auto index=scene_index(slot);
        if(index>=scenes_.size() || scenes_[index].generation==0) { return false; }
        auto& command=scenes_[index];
        for(std::uint8_t i=0;i<command.eventCount;++i) { if(command.events[i]==event) { return true; } }
        if(command.eventCount>=command.events.size()) { failed_=true;return false; }
        command.events[command.eventCount++]=event;return true;
    }
    [[nodiscard]] static bool same_charge(const ChargeReceipt& a,const ChargeReceipt& b) noexcept {
        return a.token==b.token && a.sourceHandle==b.sourceHandle && a.generation==b.generation
            && a.itemEntity==b.itemEntity && a.player==b.player && a.registry==b.registry && a.sourceSlot==b.sourceSlot;
    }
    void schedule(Action action,CrownStage stage) noexcept {
        if(boss_.actionEpoch==UINT32_MAX) { failed_=true;return; }
        ++boss_.actionEpoch;mechanicAction_=action;crownStage_=stage;phase_=Phase::mechanicReady;
    }
    void start_cycle(std::uint8_t cycle) noexcept {
        if(cycle<1 || cycle>3 || boss_.actionEpoch==UINT32_MAX) { failed_=true;return; }
        cycle_=cycle;wave_=0;++boss_.actionEpoch;phase_=Phase::bothReady;crownStage_=CrownStage::waves;
        if(!has_action(Action::summonBoth)) { failed_=true;return; }
        rescueReady_=deletionHold_=recovered_=checkpointReached_=false;
        returnReady_=false;if(cycle==3) { returnLaunch_=false; }
        gateArrivals_=0;routePlayer_=UINT32_MAX;carried_={};carriedOnce_=chargeDunked_=false;
    }
    void finish_recovery() noexcept {
        if(!recovered_ || !checkpointReached_ || !returnReady_) { return; }
        if(cycle_==1) { start_cycle(2); }
        else if(cycle_==2) {
            restrictionReleased_=true;
            bool relocationWave{};
            for(const auto& group:kAllGroups) {
                if(group.island==4 && group.cycle==2 && group.wave==3) { relocationWave=true; }
            }
            if(relocationWave) {
                wave_=3;++boss_.actionEpoch;crownStage_=CrownStage::relocationAdds;
                phase_=has_action(Action::summonLeft)?Phase::leftReady:Phase::rightReady;
            } else { schedule(Action::relocateFinal,CrownStage::relocation); }
        }
    }
    void start_final_if_ready() noexcept {
        if(pendingFinalArrival_ && crownStage_==CrownStage::finalArrival && final_traversal()) {
            // Gate3's disappear path is armed by gate7's approach release. A
            // player already in the final area still needs the ordered cleanup
            // if they bypassed the cannon volume; no movement is synthesized.
            if(!scene_event(84,omega_rescue_npc::kFinalCannonApproach)
                || !scene_event(84,omega_rescue_npc::kFinalCannonArrived)) { return; }
            restrictionReleased_=false;start_cycle(3);
        }
    }
    void finish_death() noexcept {
        if(bossDead_ && deathFinished_) { schedule(Action::finishEncounter,CrownStage::ending); }
    }
    [[nodiscard]] bool group_current(const Group& group) const noexcept {
        return group.island==island_ && (island_!=4 || (group.cycle==cycle_ && group.wave==wave_));
    }
    [[nodiscard]] bool has_action(Action action) const noexcept {
        for(const auto& group:kAllGroups) { if(group_current(group) && group.summon==action) { return true; } }
        return false;
    }
    void enable_groups(Action action) noexcept {
        for(std::size_t i=0;i<kAllGroups.size();++i) {
            if(group_current(kAllGroups[i]) && kAllGroups[i].summon==action) { enabled_[i]=true; }
        }
    }
    void start_arrived_island() noexcept {
        island_=pendingArrival_;pendingArrival_=0;
        boss_.island=island_;
        if(has_action(Action::summonLeft)) { phase_=Phase::leftReady; }
        else if(has_action(Action::summonRight)) { phase_=Phase::rightReady; }
        else { failed_=true; }
    }
    void start_crown_if_ready() noexcept {
        if(!failed_ && island_==3 && phase_==Phase::complete && crown_restricted()) {
            island_=4;boss_.island=4;cycle_=1;wave_=0;crownStage_=CrownStage::waves;phase_=Phase::bothReady;
        }
    }
    void advance_clear() noexcept {
        if(phase_!=Phase::clearing || failed_) { return; }
        // Retail relocation overlaps the Cabal escape cohort. Native summon
        // completion is its barrier; living/late actors stay in the ledger.
        if(crownStage_==CrownStage::relocationAdds) { schedule(Action::relocateFinal,CrownStage::relocation);return; }
        bool required{};
        for(std::size_t i=0;i<actors_.size();++i) {
            if(!group_current(kAllGroups[i]) || !kAllGroups[i].required) { continue; }
            required=true;
            if(!enabled_[i] || counts_[i]!=kAllGroups[i].count) { return; }
            for(std::uint8_t n=0;n<counts_[i];++n) { if(!actors_[i][n].dead) { return; } }
        }
        if(!required) { failed_=true;return; }
        if(island_!=4) { phase_=Phase::departureReady;return; }
        bool next{};
        for(const auto& group:kAllGroups) {
            if(group.island==4 && group.cycle==cycle_ && group.wave==wave_+1U && group.wave<3) { next=true; }
        }
        if(next) {
            if(boss_.actionEpoch==UINT32_MAX) { failed_=true;return; }
            ++wave_;++boss_.actionEpoch;
            phase_=has_action(Action::summonLeft)?Phase::leftReady:Phase::rightReady;
        } else { schedule(Action::beginDeletion,CrownStage::deletion); }
    }
    std::array<std::array<Actor,kMaximumGroupPopulation>,kAllGroups.size()> actors_{};
    std::array<std::uint8_t,kAllGroups.size()> counts_{};
    std::array<bool,kAllGroups.size()> enabled_{};
    std::uint64_t run_{};
    Boss boss_{};
    Phase phase_{Phase::initial};
    std::array<std::uint32_t,4> cannonGenerations_{};
    std::uint8_t island_{},pendingArrival_{},completedIslands_{};
    bool failed_{},crownArrived_{};
    std::uint8_t cycle_{},wave_{},gateArrivals_{};
    CrownStage crownStage_{};
    Action mechanicAction_{};
    omega_rescue_npc::Commands scenes_{};
    std::array<CrownToken,omega_rescue_npc::kScenes.size()> sceneTokens_{};
    std::array<std::uint8_t,omega_rescue_npc::kScenes.size()> sceneMilestones_{};
    std::array<std::uint32_t,7> transitGenerations_{};
    ChargeReceipt carried_{};
    std::uint32_t routePlayer_{UINT32_MAX};
    std::uint32_t finalRoutePlayer_{UINT32_MAX};
    bool finalCannonApproached_{};
    std::array<std::uint32_t,2> returnEntities_{UINT32_MAX,UINT32_MAX};
    bool returnLaunch_{},returnReady_{};
    bool rescueReady_{},deletionHold_{},recovered_{},checkpointReached_{},finalDeparted_{},bossDead_{},deathFinished_{};
    bool restrictionReleased_{},pendingFinalArrival_{},carriedOnce_{},chargeDunked_{};
};

} // namespace sunrise::state::activity::omega_first_lair
