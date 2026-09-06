#pragma once
#include "coo/omega_script_views.h"
#include "coo/native_services.h"
#include "coo/population_service.h"
#include "coo/scene_service.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "coo/omega_combat_definition.h"
#include <type_traits>
#include "omega_enemy_chase_catalog.h"
#include "omega_enemy_crown_catalog.h"
#include "omega_enemy_crown_waves.h"
#include "omega_rescue_scene_authority.h"

namespace sunrise::state::activity::omega_first_lair {

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
    void begin(std::uint64_t run,bool executorOwned=false) noexcept {
        CombatServices services(*this);executor_.cancel(services);
        const auto retained=executor_;
        *this={};executor_=retained;run_=run;executorOwned_=executorOwned;
        if(executorOwned_ && !executor_.start(coo::script::graph(coo::combat::kSectionRoles[0], *coo::combat::kSections[0]),run)) { failed_=true; }
    }
    // Only the serialized publication owner calls this. Native callbacks keep
    // their immediate admission/claim contracts and record qualified facts.
    void update_executor() noexcept {
        if(!executorOwned_ || failed_) { return; }
        CombatServices services(*this);
        for(unsigned pass=0;pass<128;++pass) {
            const auto before=executor_.diagnostics();
            executor_.update(services);
            if(executor_.diagnostics().phase==coo::Phase::failed) { failed_=true;return; }
            const auto& definition=coo::script::graph(coo::combat::kSectionRoles[section_], *coo::combat::kSections[section_]);
            for(std::size_t i=0;i<definition.steps.size();++i) {
                if(executor_.step_state(i).phase!=coo::StepPhase::active) { continue; }
                for(std::size_t n=0;n<definition.steps[i].commands.size();++n) {
                    const auto& spec=definition.steps[i].commands[n];
                    if(spec.operation==coo::Operation::observation && !executor_.step_state(i).commands[n].completed
                        && fact(static_cast<coo::combat::Fact>(spec.argument))) {
                        static_cast<void>(executor_.enqueue({executor_.token(i,n),coo::Milestone::observed}));
                    }
                }
            }
            executor_.update(services);
            const auto after=executor_.diagnostics();
            if(after.phase==coo::Phase::failed) { failed_=true;return; }
            if(after.phase==coo::Phase::complete && section_+1U<coo::combat::kSections.size()) {
                executor_.cancel(services);++section_;
                if(!executor_.start(coo::script::graph(coo::combat::kSectionRoles[section_], *coo::combat::kSections[section_]),run_)) { failed_=true;return; }
                continue;
            }
            if(after.phase==coo::Phase::complete || (before.active==after.active && before.complete==after.complete)) { return; }
        }
        failed_=true; // A malformed definition cannot spin the publication owner.
    }
    [[nodiscard]] bool executor_owned() const noexcept { return executorOwned_; }
    [[nodiscard]] std::uint8_t executor_section() const noexcept { return section_; }
    [[nodiscard]] coo::Diagnostics executor_diagnostics() const noexcept { return executor_.diagnostics(); }
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
        if(executorOwned_) { initialFinished_=true;phase_=Phase::clearing; }
        else { finish_initial_summon(); }
        return true;
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
        if(executorOwned_) {
            const bool playing=(action==Action::summonLeft && phase_==Phase::leftPlaying)
                || (action==Action::summonRight && phase_==Phase::rightPlaying)
                || (action==Action::summonBoth && phase_==Phase::bothPlaying);
            if(!playing) { return false; }
            if(island_==0 && action==Action::summonBoth) { initialFinished_=true; }
            else { summonFinished_=true; }
            phase_=Phase::clearing;return true;
        }
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
        return populations_.source_enabled(kAllGroups,slot,registry);
    }
    [[nodiscard]] bool admitted(const ActorReceipt& receipt) noexcept {
        if(failed_) { return false; }
        const auto result=populations_.admit(kAllGroups,receipt,run_,boss_.generation);
        if(result==coo::Admission::overflow) { failed_=true; }
        return result==coo::Admission::accepted;
    }
    /** Only a verified native death transition may call this. Spawner consumed,
     * missing/streamed-out actors and removal callbacks are not death receipts. */
    [[nodiscard]] bool died(const ActorReceipt& receipt) noexcept {
        if(failed_ || !populations_.died(receipt,run_,boss_.generation)) { return false; }
        advance_clear();return true;
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
        return populations_.enabled(index);
    }
    [[nodiscard]] const omega_rescue_npc::Commands& rescue_scenes() const noexcept { return sceneService_.commands(); }
    [[nodiscard]] SceneRequest scene_request(std::uint16_t slot) const noexcept {
        const auto index=scene_index(slot);
        if(index>=sceneService_.commands().size()) { return {}; }
        return {sceneService_.owner(index),sceneService_.commands()[index],!failed_ && sceneService_.commands()[index].generation!=0};
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
            return executorOwned_ || start_scene(rescue_slot());
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
        if(failed_ || index>=sceneService_.commands().size() || !owner.valid() || sceneService_.owner(index)!=owner
            || sceneService_.commands()[index].generation==0 || static_cast<unsigned>(event)>static_cast<unsigned>(SceneMilestone::completed)) { return false; }
        const auto bit=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(event));
        if(sceneService_.seen(index,bit)) { return false; }
        if(event==SceneMilestone::rescueReady) {
            if(!current(owner) || crownStage_!=CrownStage::rescue || slot!=rescue_slot()) { return false; }
            rescueReady_=true;crownStage_=CrownStage::route;phase_=Phase::waiting;
            if(!executorOwned_ && cycle_==1) {
                if(!start_scene(81) || !start_scene(82)) { return false; }
            } else if(!executorOwned_ && cycle_==2) {
                if(!start_scene(68) || !start_scene(67)) { return false; }
            }
        } else if(event!=SceneMilestone::started && event!=SceneMilestone::completed) { return false; }
        sceneService_.mark(index,bit);
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
            if(cycle_<3) { sceneService_.stop(scene_index(cycle_==1?83:66)); }
            carried_={};chargeDunked_=true;
            if(!executorOwned_) { schedule(Action::breakShield,CrownStage::eyeOpening); }
            return true;
        }
        return false;
    }
    [[nodiscard]] bool health(const CrownToken& owner,HealthMilestone event) noexcept {
        if(!current(owner)) { return false; }
        if(event==HealthMilestone::eyeDepleted && crownStage_==CrownStage::eyeDps && (!executorOwned_ || !eyeThreshold_)) {
            // Scene9 retains its eye hold and reminder until this authored
            // release. Its exit child and terminal remain native-owned.
            if(cycle_==1 && !scene_event(9,omega_rescue_npc::kReleaseFirstEye)) { return false; }
            // The final authored Scene releases its DFA chain into Ghost
            // retrieval; its one-second graph delay remains native-owned.
            if(cycle_==3 && !scene_event(46,0x505750FEU)) { return false; }
            recovered_=checkpointReached_=false;
            if(executorOwned_) { eyeThreshold_=true; }
            else { schedule(Action::endEyePhase,cycle_==3?CrownStage::death:CrownStage::recovery); }
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
            if(scene<sceneService_.commands().size() && sceneService_.commands()[scene].generation!=0) { sceneService_.stop(scene); }
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
        if(!executorOwned_ && !start_scene(84)) { return false; }
        finalDeparted_=true;crownStage_=CrownStage::finalArrival;phase_=Phase::waiting;start_final_if_ready();return true;
    }
private:
    class CombatServices final : public coo::NativeServices<CombatServices> {
    public:
        explicit CombatServices(Encounter& owner) noexcept : owner_(owner) {}
        [[nodiscard]] coo::ServiceContext context() const noexcept { return {coo::script::graph(coo::combat::kSectionRoles[owner_.section_], *coo::combat::kSections[owner_.section_]), owner_.run_, owner_.executor_.diagnostics().incarnation}; }
        bool request(const coo::Command& command) noexcept {
            if(command.schema!=coo::Schema::omegaArchive || command.token.run!=owner_.run_
                || command.spec.asset!=coo::combat::kBinding || owner_.failed_) { return false; }
            if(command.spec.operation==coo::Operation::observation) {
                return command.spec.wait==coo::Wait::observed
                    && command.spec.argument>0 && command.spec.argument<=static_cast<unsigned>(coo::combat::Fact::deathFinished);
            }
            return command.spec.operation==coo::Operation::mechanic && command.spec.wait==coo::Wait::requested
                && owner_.control(static_cast<coo::combat::Control>(command.spec.argument));
        }
        void retire(const coo::Command&) noexcept {} // Native ownership stays with the run, never raw executor tokens.
    private:
        Encounter& owner_;
    };
    [[nodiscard]] bool cohorts_cleared() const noexcept {
        bool required{};
        for(std::size_t i=0;i<kAllGroups.size();++i) {
            if(!group_current(kAllGroups[i]) || !kAllGroups[i].required) { continue; }
            required=true;
            if(!populations_.cleared(i,kAllGroups[i].count)) { return false; }
        }
        return required;
    }
    [[nodiscard]] bool fact(coo::combat::Fact value) const noexcept {
        using F=coo::combat::Fact;
        if(failed_) { return false; }
        switch(value) {
        case F::initialFinished:return initialFinished_;
        case F::summonFinished:return summonFinished_;
        case F::cleared:return phase_==Phase::clearing && cohorts_cleared();
        case F::departed:return phase_==Phase::complete;
        case F::arrived:return pendingArrival_==island_+1U;
        case F::crownReady:return island_==3 && phase_==Phase::complete && crown_restricted();
        case F::deletionStarted:return crownStage_==CrownStage::rescue;
        case F::rescueReady:return rescueReady_;
        case F::dunked:return chargeDunked_;
        case F::vulnerable:return crownStage_==CrownStage::eyeDps;
        case F::eyeThreshold:return eyeThreshold_;
        case F::recovered:return recovered_;
        case F::checkpoint:return checkpointReached_;
        case F::returnReady:return returnReady_;
        case F::finalDeparted:return finalDeparted_;
        case F::finalArrival:return pendingFinalArrival_ && crownStage_==CrownStage::finalArrival && final_traversal();
        case F::dead:return bossDead_;
        case F::deathFinished:return deathFinished_;
        default:return false;
        }
    }
    [[nodiscard]] bool control(coo::combat::Control value) noexcept {
        using C=coo::combat::Control;
        if(failed_ || !boss_.valid()) { return false; }
        switch(value) {
        case C::left:case C::right:case C::both: {
            const auto action=value==C::left?Action::summonLeft:value==C::right?Action::summonRight:Action::summonBoth;
            if(!has_action(action)) { return false; }
            summonFinished_=false;
            phase_=value==C::left?Phase::leftReady:value==C::right?Phase::rightReady:Phase::bothReady;
            break;
        }
        case C::depart:phase_=Phase::departureReady;break;
        case C::island1:case C::island2:case C::island3: {
            const auto next=static_cast<std::uint8_t>(static_cast<unsigned>(value)-static_cast<unsigned>(C::island1)+1U);
            if(pendingArrival_!=next || phase_!=Phase::complete) { return false; }
            island_=next;boss_.island=next;pendingArrival_=0;phase_=Phase::waiting;break;
        }
        case C::crown1:
            if(!fact(coo::combat::Fact::crownReady)) { return false; }
            island_=4;boss_.island=4;cycle_=1;wave_=0;crownStage_=CrownStage::waves;phase_=Phase::waiting;break;
        case C::wave1:case C::wave2:
            if(boss_.actionEpoch==UINT32_MAX) { return false; }
            wave_=value==C::wave1?1:2;++boss_.actionEpoch;break;
        case C::deletion:schedule(Action::beginDeletion,CrownStage::deletion);break;
        case C::rescue:if(!start_scene(rescue_slot())) { return false; }break;
        case C::route:
            if(cycle_==1 && (!start_scene(81) || !start_scene(82))) { return false; }
            if(cycle_==2 && (!start_scene(68) || !start_scene(67))) { return false; }
            break;
        case C::expose:schedule(Action::breakShield,CrownStage::eyeOpening);break;
        case C::recover:schedule(Action::endEyePhase,CrownStage::recovery);break;
        case C::die:schedule(Action::endEyePhase,CrownStage::death);break;
        case C::crown2:start_cycle(2);eyeThreshold_=false;break;
        case C::escapeAdds:
            if(boss_.actionEpoch==UINT32_MAX) { return false; }
            restrictionReleased_=true;wave_=3;++boss_.actionEpoch;crownStage_=CrownStage::relocationAdds;break;
        case C::relocate:schedule(Action::relocateFinal,CrownStage::relocation);break;
        case C::finalScene:if(!start_scene(84)) { return false; }break;
        case C::crown3:
            if(!scene_event(84,omega_rescue_npc::kFinalCannonApproach)
                || !scene_event(84,omega_rescue_npc::kFinalCannonArrived)) { return false; }
            restrictionReleased_=false;start_cycle(3);eyeThreshold_=false;break;
        case C::ending:schedule(Action::finishEncounter,CrownStage::ending);break;
        default:return false;
        }
        return !failed_;
    }
    coo::Executor executor_{};
    std::uint8_t section_{};
    bool executorOwned_{},initialFinished_{},summonFinished_{},eyeThreshold_{};
    [[nodiscard]] bool initial_receipt(const Boss& boss) const noexcept {
        return !failed_ && run_!=0 && boss.run==run_ && boss.valid() && boss.island==0
            && boss.actionEpoch==0 && island_==0;
    }
    void finish_initial_summon() noexcept {
        if(has_action(Action::summonLeft)) { phase_=Phase::leftReady; }
        else if(has_action(Action::summonRight)) { phase_=Phase::rightReady; }
        else { phase_=Phase::clearing;advance_clear(); }
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
        const auto generation=static_cast<std::uint64_t>(boss_.generation)+boss_.actionEpoch;
        if(boss_.actionEpoch==UINT32_MAX || !sceneService_.begin(scene_index(slot),generation,token())) {
            failed_=true;return false;
        }
        return true;
    }
    [[nodiscard]] bool scene_event(std::uint16_t slot,std::uint32_t event) noexcept {
        const auto result=sceneService_.event(scene_index(slot),event);
        if(result==coo::SceneEvent::overflow) { failed_=true; }
        return result==coo::SceneEvent::accepted;
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
        if(executorOwned_) { return; }
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
        if(executorOwned_) { return; }
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
        if(executorOwned_) { return; }
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
            if(group_current(kAllGroups[i]) && kAllGroups[i].summon==action) { populations_.enable(i); }
        }
    }
    void start_arrived_island() noexcept {
        if(executorOwned_) { return; }
        island_=pendingArrival_;pendingArrival_=0;
        boss_.island=island_;
        if(has_action(Action::summonLeft)) { phase_=Phase::leftReady; }
        else if(has_action(Action::summonRight)) { phase_=Phase::rightReady; }
        else { failed_=true; }
    }
    void start_crown_if_ready() noexcept {
        if(executorOwned_) { return; }
        if(!failed_ && island_==3 && phase_==Phase::complete && crown_restricted()) {
            island_=4;boss_.island=4;cycle_=1;wave_=0;crownStage_=CrownStage::waves;phase_=Phase::bothReady;
        }
    }
    void advance_clear() noexcept {
        if(executorOwned_) { return; }
        if(phase_!=Phase::clearing || failed_) { return; }
        // Retail relocation overlaps the Cabal escape cohort. Native summon
        // completion is its barrier; living/late actors stay in the ledger.
        if(crownStage_==CrownStage::relocationAdds) { schedule(Action::relocateFinal,CrownStage::relocation);return; }
        bool required{};
        for(std::size_t i=0;i<kAllGroups.size();++i) {
            if(!group_current(kAllGroups[i]) || !kAllGroups[i].required) { continue; }
            required=true;
            if(!populations_.cleared(i,kAllGroups[i].count)) { return; }
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
    coo::PopulationService<ActorReceipt,kAllGroups.size(),kMaximumGroupPopulation> populations_{};
    std::uint64_t run_{};
    Boss boss_{};
    Phase phase_{Phase::initial};
    std::array<std::uint32_t,4> cannonGenerations_{};
    std::uint8_t island_{},pendingArrival_{},completedIslands_{};
    bool failed_{},crownArrived_{};
    std::uint8_t cycle_{},wave_{},gateArrivals_{};
    CrownStage crownStage_{};
    Action mechanicAction_{};
    coo::SceneService<omega_rescue_npc::SceneCommand,CrownToken,omega_rescue_npc::kScenes.size()> sceneService_{};
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

static_assert(std::is_trivially_copyable_v<Encounter>);

} // namespace sunrise::state::activity::omega_first_lair
