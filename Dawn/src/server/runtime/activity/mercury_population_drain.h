#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace dawn::server::runtime::activity::mercury_population_drain {

enum class Phase : std::uint8_t { idle, cooling, draining, ready, failed };
enum class Action : std::uint8_t { none, projectDrain, commit, fail };
enum class Sense : std::uint8_t { current, expectedDrain, staleOrForeign };

struct Counts final {
    std::uint32_t admitted{},alive{},dead{},resident{};
    bool failed{};
};

struct Fence final {
    bool consumedKnown{},sourcePendingKnown{},mailboxQuiescent{};
    std::uint8_t categories{1};
    std::array<std::uint32_t,2> target{},consumed{},sourcePending{};
};

struct Projection final {
    std::uint32_t generation{};
    std::array<std::uint32_t,2> requested{};
    bool retireOwned{};
};

// Pure policy for a two-phase Mercury-only full-clear drain. This does not
// publish authority, renew a receipt, or infer retirement from death/time.
class Controller final {
public:
    [[nodiscard]] constexpr bool begin(std::uint32_t generation,
        std::array<std::uint32_t,2> target,std::uint8_t categories) noexcept {
        if(phase_!=Phase::idle || !generation || generation==0x7FFFFFFFU
            || (categories!=1 && categories!=2) || !target[0]
            || (categories==1 && target[1]))return false;
        generation_=generation;target_=target;categories_=categories;return true;
    }

    [[nodiscard]] constexpr Action update(std::uint64_t now,std::uint64_t cooldown,
        Counts counts,Fence fence) noexcept {
        if(phase_==Phase::failed)return Action::fail;
        if(!valid(counts,fence) || now<lastNow_)return fail();
        lastNow_=now;
        const bool fullyDead=counts.admitted && counts.dead==counts.admitted && !counts.alive;
        if(phase_==Phase::idle) {
            if(!fullyDead)return Action::none;
            phase_=Phase::cooling;clearAt_=now;return Action::none;
        }
        if(phase_==Phase::cooling) {
            if(!fullyDead) {phase_=Phase::idle;clearAt_=0;return Action::none;}
            if(now-clearAt_<cooldown || !drain_fence(fence))return Action::none;
            baselineAdmitted_=counts.admitted;phase_=Phase::draining;return Action::projectDrain;
        }
        if(phase_==Phase::draining) {
            // A new birth after the old quota was drained disproves the fence.
            if(counts.admitted!=baselineAdmitted_ || !fullyDead)return fail();
            if(counts.resident || !drain_fence(fence))return Action::none;
            phase_=Phase::ready;return Action::commit;
        }
        return Action::none;
    }

    [[nodiscard]] constexpr Projection projection() const noexcept {
        return phase_==Phase::draining || phase_==Phase::ready
            ?Projection{generation_+1,{},true}:Projection{};
    }

    [[nodiscard]] constexpr Sense sense(std::uint32_t observedGeneration) const noexcept {
        if(observedGeneration==generation_)return Sense::current;
        if((phase_==Phase::draining || phase_==Phase::ready)
            && observedGeneration==generation_+1)return Sense::expectedDrain;
        return Sense::staleOrForeign;
    }

    [[nodiscard]] constexpr bool committed(std::uint32_t generation) noexcept {
        if(phase_!=Phase::ready || generation!=generation_+1)return false;
        generation_=generation;phase_=Phase::idle;clearAt_=0;baselineAdmitted_=0;return true;
    }

    [[nodiscard]] constexpr Phase phase() const noexcept {return phase_;}
    [[nodiscard]] constexpr std::uint32_t authoritative_generation() const noexcept {return generation_;}
    [[nodiscard]] constexpr std::array<std::uint32_t,2> target() const noexcept {return target_;}

private:
    [[nodiscard]] constexpr bool valid(Counts counts,const Fence& fence) const noexcept {
        if(!generation_ || counts.failed || counts.dead>counts.admitted || counts.alive>counts.resident
            || counts.resident>counts.admitted || fence.categories!=categories_
            || fence.target!=target_)return false;
        if(categories_==1 && (fence.consumed[1] || fence.sourcePending[1]))return false;
        return true;
    }
    [[nodiscard]] constexpr bool drain_fence(const Fence& fence) const noexcept {
        if(!fence.consumedKnown || !fence.sourcePendingKnown || !fence.mailboxQuiescent)return false;
        for(std::size_t lane=0;lane<categories_;++lane)
            if(fence.consumed[lane]<target_[lane] || fence.sourcePending[lane])return false;
        return true;
    }
    [[nodiscard]] constexpr Action fail() noexcept {phase_=Phase::failed;return Action::fail;}

    Phase phase_{Phase::idle};
    std::uint8_t categories_{};
    std::uint32_t generation_{},baselineAdmitted_{};
    std::array<std::uint32_t,2> target_{};
    std::uint64_t clearAt_{},lastNow_{};
};

} // namespace dawn::server::runtime::activity::mercury_population_drain
