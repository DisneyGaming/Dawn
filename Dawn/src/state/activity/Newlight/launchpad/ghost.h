#pragma once
#include "native_catalog.h"
#include "../../coo/actor_program_service.h"
#include "../../../../middleware/bap/activity_message/combatant_sense.h"
#include "../../../../middleware/bap/activity_message/native/native_npc_animation_authority.h"

namespace dawn::state::activity::newlight::launchpad::ghost {
namespace animation=middleware::bap::activity_message::native::npc_animation;
inline constexpr auto kSource=asset(kBreach,1,27),kActor=asset(kBreach,2,28),kControl=asset(kBreach,42,104);
// Bank 80FE20C8 / graph 80C0E38D record 0. Native Type-42 owns this whole
// performance, including root motion and audio. Raw clip keys are NOT Type-2
// sequence names: that interface looks up entity sequences, and skips them.
inline constexpr std::uint32_t kSequence=0x9B1D6A71U;
inline constexpr animation::Event kEvents[]{
    {kSequence,0x100E2B1DU}, // first hover -> flight to the lights
    {kSequence,0x7A3E66D9U}, // switch hover -> return flight
    {kSequence,0x5429FB79U}, // rifle approach -> point out the weapon
    {kSequence,0x72F6A4A4U}}; // weapon granted -> authored delete-self
inline constexpr std::uint32_t kRows[]{40,43,41,48,45,46,44,42,46};
inline constexpr float kSeconds[]{6.533334F,3.3F,10.833334F,3.3F,3.333334F,3.3F,1.F,5.533334F,3.3F};
enum class Phase : std::uint8_t { dormant,lights,waitingForRifle,rifle,dismissing,retired };
struct Sample {std::uint8_t node{UINT8_MAX};bool finished{};};
struct State {
    coo::ActorPublication publication{};coo::CombatantState receipt{};
    Phase phase{};std::uint8_t node{UINT8_MAX};bool atLights{},returnReleased{},lightsComplete{};
    void begin(std::uint32_t generation) noexcept {
        if(phase!=Phase::dormant) {return;}
        phase=Phase::lights;auto& p=publication.program;
        p.generation=generation;p.revision=1;p.spawn=true;p.count=1;
        // Admission only. The existing Type-42 controller drives the animation.
        p.atoms[0]=coo::native_atom::trivial();
    }
    void rifle() noexcept {if(phase==Phase::waitingForRifle) {phase=Phase::rifle;}}
    void dismiss() noexcept {if(phase==Phase::waitingForRifle || phase==Phase::rifle) {phase=Phase::dismissing;}}
    void observe(Sample sample) noexcept {
        if(phase==Phase::dormant || phase==Phase::retired || sample.node>=std::size(kRows)) {return;}
        node=sample.node;
        // Pause at the authored switch out of sight. The lights/scurry cue
        // releases the return flight; node 8 then releases the rifle shutter.
        if(phase==Phase::lights && node==3) {atLights=true;}
        if(phase==Phase::lights && returnReleased && node==8) {lightsComplete=true;phase=Phase::waitingForRifle;}
        if(phase==Phase::dismissing && node==6 && sample.finished) {retire();}
    }
    void observe(const middleware::bap::activity_message::combatant_sense::Output& d) noexcept {
        const auto& p=publication.program;
        if(phase==Phase::dormant || phase==Phase::retired || !d.snapshotValid
            || (d.hasSpawnRevision && d.spawnRevision!=p.generation)
            || (d.hasProgramRevision && d.programRevision!=p.revision)) {return;}
        // A program cursor proves neither playback nor completion. Only an
        // already observed delete-self node may use detachment as its end.
        if(phase==Phase::dismissing && node==6 && d.detached && receipt.identified
            && receipt.spawnRevision==p.generation) {retire();}
        receipt.merge(d);
    }
    bool ready() const noexcept {return lightsComplete;}
    void release_return() noexcept {if(atLights) {returnReleased=true;}}
    std::span<const animation::Event> events() const noexcept {
        if(phase==Phase::dormant || phase==Phase::retired) {return {};}
        return std::span(kEvents).first(phase==Phase::dismissing?4:phase==Phase::rifle?3:returnReleased?2:1);
    }
    animation::Control control() const noexcept {
        return phase==Phase::dormant || phase==Phase::retired?animation::Control{}
            :animation::Control{kSequence,animation::kEmptyHash,publication.program.generation};
    }
private:
    void retire() noexcept {phase=Phase::retired;publication.retirementGeneration=publication.program.generation+1U;}
};
}
