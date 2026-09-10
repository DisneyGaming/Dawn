#pragma once
#include "executor.h"
#include <type_traits>

namespace sunrise::state::activity::coo {

struct PopulationOwner final {
    std::uint64_t activity{}, run{}, incarnation{};
    Asset source{};
    std::uint32_t generation{};
    [[nodiscard]] bool valid() const noexcept {
        return activity && run && incarnation && generation && generation<=0x7FFFFFFFU
            && source.registry && source.registry!=UINT32_MAX && source.registry!=0x811C9DC5U
            && source.definition && source.definition!=UINT32_MAX && source.definition!=0x811C9DC5U;
    }
    friend bool operator==(const PopulationOwner&,const PopulationOwner&)=default;
};
struct PopulationActor final {
    PopulationOwner owner{};
    // Salted native handles. The native adapter must qualify them before intake.
    std::uint32_t actor{UINT32_MAX}, entity{UINT32_MAX};
    [[nodiscard]] bool valid() const noexcept { return owner.valid() && actor!=UINT32_MAX && entity!=UINT32_MAX; }
    friend bool operator==(const PopulationActor&,const PopulationActor&)=default;
};
enum class PopulationIntake : std::uint8_t { accepted, duplicate, unrelated, unknown, conflict, overflow, closed };
enum class PopulationPhase : std::uint8_t { idle, active, retiring, retired };
struct PopulationCounts final {
    std::size_t admitted{}, alive{}, dead{}, resident{};
    bool sourceRetired{}, failed{};
};

// Value-only accounting owned by the activity server. No AI, movement, spawn
// timer, packet encoding, or client pointer belongs here. Native source requests
// can produce authored groups, so this ledger does NOT assume one request is one
// actor. The capability's verified policy decides when to request replacements.
template<std::size_t Capacity>
class NativePopulationLedger final {
    static_assert(Capacity>0 && Capacity<=256);
public:
    [[nodiscard]] bool begin(PopulationOwner owner) noexcept {
        if(!owner.valid() || failed_ || (phase_!=PopulationPhase::idle && phase_!=PopulationPhase::retired)) { return false; }
        if(phase_!=PopulationPhase::idle && owner.incarnation<=owner_.incarnation) { return false; }
        if(phase_!=PopulationPhase::idle && owner.activity==owner_.activity && owner.run==owner_.run
            && owner.source==owner_.source && owner.generation<=owner_.generation) { return false; }
        owner_=owner;actors_={};used_=0;sourceRetired_=false;phase_=PopulationPhase::active;return true;
    }
    [[nodiscard]] PopulationIntake admitted(PopulationActor actor) noexcept {
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        for(std::size_t i=0;i<used_;++i) {
            if(actors_[i].identity==actor) { return PopulationIntake::duplicate; }
            if(actors_[i].identity.actor==actor.actor || actors_[i].identity.entity==actor.entity) {
                failed_=true;return PopulationIntake::conflict;
            }
        }
        if(sourceRetired_ || phase_==PopulationPhase::retired) {
            // New births after the native source's quiescence barrier disprove
            // its contract; fail visibly rather than silently lose ownership.
            failed_=true;return PopulationIntake::closed;
        }
        if(used_==Capacity) { failed_=true;return PopulationIntake::overflow; }
        actors_[used_++].identity=actor;return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationIntake died(PopulationActor actor) noexcept {
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        auto* item=find(actor);if(!item) { return PopulationIntake::unknown; }
        if(item->dead) { return PopulationIntake::duplicate; }
        if(item->retired) { return PopulationIntake::closed; }
        item->dead=true;return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationIntake actor_retired(PopulationActor actor) noexcept {
        if(!matches(actor)) { return PopulationIntake::unrelated; }
        auto* item=find(actor);if(!item) { return PopulationIntake::unknown; }
        if(item->retired) { return PopulationIntake::duplicate; }
        item->retired=true;settle_retirement();return PopulationIntake::accepted;
    }
    // Call only after the server/native service has accepted a retirement request.
    // In-flight births still belong to this source and must be accounted for.
    [[nodiscard]] bool retiring(PopulationOwner owner) noexcept {
        if(owner!=owner_ || phase_!=PopulationPhase::active) { return false; }
        phase_=PopulationPhase::retiring;return true;
    }
    // Qualified native barrier: source stopped, births quiesced, preceding actor
    // observations drained. The adapter must prove this; elapsed time cannot.
    [[nodiscard]] PopulationIntake source_retired(PopulationOwner owner) noexcept {
        if(owner!=owner_ || phase_==PopulationPhase::idle) { return PopulationIntake::unrelated; }
        if(sourceRetired_) { return PopulationIntake::duplicate; }
        if(phase_!=PopulationPhase::retiring) { return PopulationIntake::unrelated; }
        sourceRetired_=true;settle_retirement();return PopulationIntake::accepted;
    }
    [[nodiscard]] PopulationCounts counts() const noexcept {
        PopulationCounts result{used_,0,0,0,sourceRetired_,failed_};
        for(std::size_t i=0;i<used_;++i) {
            result.dead+=actors_[i].dead?1U:0U;
            result.resident+=actors_[i].retired?0U:1U;
            result.alive+=!actors_[i].dead && !actors_[i].retired?1U:0U;
        }
        return result;
    }
    [[nodiscard]] PopulationPhase phase() const noexcept { return phase_; }
    [[nodiscard]] PopulationOwner owner() const noexcept { return owner_; }
private:
    struct Actor final { PopulationActor identity{}; bool dead{},retired{}; };
    [[nodiscard]] bool matches(const PopulationActor& actor) const noexcept {
        return phase_!=PopulationPhase::idle && actor.valid() && actor.owner==owner_;
    }
    [[nodiscard]] Actor* find(const PopulationActor& actor) noexcept {
        for(std::size_t i=0;i<used_;++i) { if(actors_[i].identity==actor) { return &actors_[i]; } }return nullptr;
    }
    void settle_retirement() noexcept {
        if(failed_ || phase_!=PopulationPhase::retiring || !sourceRetired_) { return; }
        for(std::size_t i=0;i<used_;++i) { if(!actors_[i].retired) { return; } }
        phase_=PopulationPhase::retired;
    }
    PopulationOwner owner_{};
    std::array<Actor,Capacity> actors_{};
    std::size_t used_{};
    PopulationPhase phase_{PopulationPhase::idle};
    bool sourceRetired_{},failed_{};
};
static_assert(std::is_trivially_copyable_v<NativePopulationLedger<64>>);
} // namespace sunrise::state::activity::coo
