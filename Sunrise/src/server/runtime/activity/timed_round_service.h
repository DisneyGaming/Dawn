#pragma once

#include "../../../state/activity/lifecycle_generation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace sunrise::server::runtime::activity::timed_round {
using Owner=state::activity::ActivityInstanceKey;

enum class Phase : std::uint8_t {
    entry, traversal, toEncounter, encounter, returning, rewards, complete, faulted
};
inline constexpr std::uint8_t kTraversalPhaseBit=static_cast<std::uint8_t>(1U<<1U);
inline constexpr std::uint8_t kToEncounterPhaseBit=static_cast<std::uint8_t>(1U<<2U);
inline constexpr std::uint8_t kEncounterPhaseBit=static_cast<std::uint8_t>(1U<<3U);
inline constexpr std::uint8_t ActiveCombatPhases=static_cast<std::uint8_t>(
    kTraversalPhaseBit|kToEncounterPhaseBit|kEncounterPhaseBit);

struct Configuration final {
    std::uint64_t totalTraversalTicks{};
    std::uint32_t progressTarget{};
    std::uint8_t activePhaseMask{kTraversalPhaseBit};
};
enum class Result : std::uint8_t { accepted, stale, duplicate, unsupported, invalidClock, exhausted };

struct Token final {
    Owner owner{};
    std::uint64_t boot{},round{},phaseGeneration{};
    friend constexpr bool operator==(const Token&,const Token&) noexcept = default;
};

struct Snapshot final {
    Token token{};
    Phase phase{};
    std::uint32_t progress{};
    std::uint64_t completedRounds{},remainingTicks{};
    bool expired{};
    std::uint64_t revision{};
};

// Shared host policy for repeated timed horde rounds. Progress, travel arrival,
// encounter admission, deaths and rewards are all qualified receipts supplied
// by the executor; this service never guesses them from a native roster.
class Service final {
public:
    static constexpr std::size_t kDeathCapacity=1024;
    static constexpr std::uint32_t kMaximumProgressTarget=
        (std::numeric_limits<std::int32_t>::max)();

    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,
        const Configuration& configuration) noexcept {
        if(owner_ || !owner || !boot || !configuration.totalTraversalTicks
            || !configuration.progressTarget
            || configuration.progressTarget>kMaximumProgressTarget || !configuration.activePhaseMask
            || (configuration.activePhaseMask&static_cast<std::uint8_t>(~ActiveCombatPhases)))return false;
        owner_=owner;boot_=boot;configuration_=configuration;round_=1;
        phaseGeneration_=1;phase_=Phase::entry;remainingTicks_=configuration.totalTraversalTicks;
        revision_=1;return true;
    }

    [[nodiscard]] Result capture_complete(Token token,std::uint64_t clock) noexcept {
        if(const auto result=guard(token,Phase::entry);result!=Result::accepted)return result;
        if(!valid_clock(clock))return Result::invalidClock;
        if(!can_commit(true))return Result::exhausted;
        account_old_phase(clock);phase_=Phase::traversal;expired_=remainingTicks_==0;
        ++phaseGeneration_;++revision_;return Result::accepted;
    }

    [[nodiscard]] Result add_progress(Token token,std::uint64_t clock,
        std::uint64_t qualifiedDeathIdentity,std::uint32_t weight) noexcept {
        if(const auto result=guard(token,Phase::traversal);result!=Result::accepted)return result;
        if(!qualifiedDeathIdentity || !weight || weight>configuration_.progressTarget)
            return Result::unsupported;
        // Weight is authored or reconstructed profile policy supplied by the caller.
        for(std::size_t i=0;i<deathCount_;++i)
            if(deathIdentities_[i]==qualifiedDeathIdentity)return Result::duplicate;
        if(deathCount_==kDeathCapacity)return Result::exhausted;
        if(!valid_clock(clock))return Result::invalidClock;
        const bool completes=progress_>=configuration_.progressTarget-weight;
        if(!can_commit(completes))return Result::exhausted;
        account_old_phase(clock);deathIdentities_[deathCount_++]=qualifiedDeathIdentity;
        progress_=completes?configuration_.progressTarget:progress_+weight;
        if(completes) { phase_=Phase::toEncounter;++phaseGeneration_; }
        ++revision_;return Result::accepted;
    }

    [[nodiscard]] Result tick(Token token,std::uint64_t clock) noexcept {
        if(const auto result=guard_any(token);result!=Result::accepted)return result;
        if(!active_phase(phase_))return Result::unsupported;
        if(!valid_clock(clock))return Result::invalidClock;
        if(hasClock_ && clock==lastClock_)return Result::duplicate;
        if(!can_commit(false))return Result::exhausted;
        account_old_phase(clock);++revision_;return Result::accepted;
    }

    [[nodiscard]] Result encounter_arrived(Token token,std::uint64_t clock) noexcept {
        if(const auto result=guard(token,Phase::toEncounter);result!=Result::accepted)return result;
        if(!valid_clock(clock))return Result::invalidClock;
        if(!can_commit(true))return Result::exhausted;
        account_old_phase(clock);phase_=Phase::encounter;++phaseGeneration_;++revision_;
        return Result::accepted;
    }

    [[nodiscard]] Result encounter_defeated(Token token,std::uint64_t clock,
        std::uint64_t qualifiedEncounterIdentity) noexcept {
        if(const auto result=guard(token,Phase::encounter);result!=Result::accepted)return result;
        if(!qualifiedEncounterIdentity)return Result::unsupported;
        if(!valid_clock(clock))return Result::invalidClock;
        if(!can_commit(true) || completedRounds_==std::numeric_limits<std::uint64_t>::max())
            return Result::exhausted;
        account_old_phase(clock);lastDefeatedIdentity_=qualifiedEncounterIdentity;
        ++completedRounds_;phase_=remainingTicks_?Phase::returning:Phase::rewards;
        ++phaseGeneration_;++revision_;return Result::accepted;
    }

    [[nodiscard]] Result returned(Token token,std::uint64_t clock) noexcept {
        if(const auto result=guard(token,Phase::returning);result!=Result::accepted)return result;
        if(!valid_clock(clock))return Result::invalidClock;
        if(round_==std::numeric_limits<std::uint64_t>::max() || !can_commit(true))
            return Result::exhausted;
        account_old_phase(clock);++round_;phase_=Phase::entry;progress_=0;deathCount_=0;
        expired_=false;++phaseGeneration_;++revision_;return Result::accepted;
    }

    // The profile may choose this host-policy wipe after qualifying a nonempty
    // admitted participant set; it is not proof supplied by a native trace.
    [[nodiscard]] Result all_players_defeated(Token token,std::uint64_t clock,
        std::uint32_t qualifiedParticipantCount) noexcept {
        const bool eligiblePhase=phase_==Phase::traversal || phase_==Phase::toEncounter
            || phase_==Phase::encounter;
        if(const auto result=guard_any(token);result!=Result::accepted)return result;
        if(!eligiblePhase || !qualifiedParticipantCount)return Result::unsupported;
        if(!valid_clock(clock))return Result::invalidClock;
        if(!expired_ && (!active_phase(phase_) || remainingTicks_>clock_delta(clock)))
            return Result::unsupported;
        if(!can_commit(true))return Result::exhausted;
        account_old_phase(clock);expired_=true;phase_=Phase::rewards;++phaseGeneration_;
        ++revision_;return Result::accepted;
    }

    [[nodiscard]] Result rewards_finished(Token token,std::uint64_t clock) noexcept {
        if(const auto result=guard(token,Phase::rewards);result!=Result::accepted)return result;
        if(!valid_clock(clock))return Result::invalidClock;
        if(!can_commit(true))return Result::exhausted;
        account_old_phase(clock);phase_=Phase::complete;++phaseGeneration_;++revision_;
        return Result::accepted;
    }

    [[nodiscard]] Result cancel(Token token) noexcept { return fault(token); }
    [[nodiscard]] Result observation_lost(Token token) noexcept { return fault(token); }

    [[nodiscard]] Snapshot snapshot() const noexcept {
        return {{owner_,boot_,round_,phaseGeneration_},phase_,progress_,completedRounds_,
            remainingTicks_,expired_,revision_};
    }
    [[nodiscard]] std::uint64_t last_defeated_identity() const noexcept {
        return lastDefeatedIdentity_;
    }
    [[nodiscard]] bool clock_valid(std::uint64_t clock) const noexcept {
        return valid_clock(clock);
    }

private:
    [[nodiscard]] Result guard(Token token,Phase expected) const noexcept {
        if(!owner_ || token.owner!=owner_ || token.boot!=boot_ || token.round!=round_
            || token.phaseGeneration!=phaseGeneration_)return Result::stale;
        return phase_==expected?Result::accepted:Result::unsupported;
    }
    [[nodiscard]] Result guard_any(Token token) const noexcept {
        if(!owner_ || token.owner!=owner_ || token.boot!=boot_ || token.round!=round_
            || token.phaseGeneration!=phaseGeneration_)return Result::stale;
        return Result::accepted;
    }
    [[nodiscard]] bool valid_clock(std::uint64_t clock) const noexcept {
        return clock!=std::numeric_limits<std::uint64_t>::max()
            && (!hasClock_ || clock>=lastClock_);
    }
    [[nodiscard]] std::uint64_t clock_delta(std::uint64_t clock) const noexcept {
        return hasClock_?clock-lastClock_:0;
    }
    [[nodiscard]] static constexpr std::uint8_t phase_bit(Phase phase) noexcept {
        switch(phase) {
        case Phase::traversal:return kTraversalPhaseBit;
        case Phase::toEncounter:return kToEncounterPhaseBit;
        case Phase::encounter:return kEncounterPhaseBit;
        default:return 0;
        }
    }
    [[nodiscard]] bool active_phase(Phase phase) const noexcept {
        return (configuration_.activePhaseMask&phase_bit(phase))!=0;
    }
    [[nodiscard]] bool can_commit(bool changesPhase) const noexcept {
        return revision_!=std::numeric_limits<std::uint64_t>::max()
            && (!changesPhase || phaseGeneration_!=std::numeric_limits<std::uint64_t>::max());
    }
    void record_clock(std::uint64_t clock) noexcept {hasClock_=true;lastClock_=clock;}
    void account_active_phase(std::uint64_t clock) noexcept {
        const auto delta=clock_delta(clock);
        if(delta>=remainingTicks_) { remainingTicks_=0;expired_=true; }
        else remainingTicks_-=delta;
        record_clock(clock);
    }
    void account_old_phase(std::uint64_t clock) noexcept {
        if(active_phase(phase_))account_active_phase(clock);else record_clock(clock);
    }
    [[nodiscard]] Result fault(Token token) noexcept {
        if(const auto result=guard_any(token);result!=Result::accepted)return result;
        if(phase_==Phase::complete || phase_==Phase::faulted)return Result::unsupported;
        if(!can_commit(true))return Result::exhausted;
        phase_=Phase::faulted;++phaseGeneration_;++revision_;return Result::accepted;
    }

    Owner owner_{};
    Configuration configuration_{};
    std::uint64_t boot_{},round_{},phaseGeneration_{},completedRounds_{};
    std::uint64_t remainingTicks_{},revision_{},lastClock_{},lastDefeatedIdentity_{};
    std::array<std::uint64_t,kDeathCapacity> deathIdentities_{};
    std::size_t deathCount_{};
    std::uint32_t progress_{};
    Phase phase_{};
    bool expired_{},hasClock_{};
};
} // namespace sunrise::server::runtime::activity::timed_round
