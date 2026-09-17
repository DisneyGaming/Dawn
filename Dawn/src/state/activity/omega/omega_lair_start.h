#pragma once

#include <cstdint>

namespace dawn::state::activity::omega::lair_start {
inline constexpr std::uint32_t kRegistry = 0xF4D0E0B2U;
inline constexpr std::uint32_t kInvalid = 0xFFFFFFFFU;

struct Owner {
    std::uint64_t run{};
    std::uint32_t generation{}, actor{kInvalid}, character{kInvalid}, biped{kInvalid};
    bool operator==(const Owner&) const = default;
};
struct Snapshot {
    std::uint32_t generation{};
    bool leftStarted{};
};

/** The initial left summon admits the first twelve native combatants. Counts
 * reconstruct host policy; actor definitions and placement remain authored. */
class State final {
public:
    [[nodiscard]] Snapshot prepare(std::uint64_t run, std::uint32_t generation) noexcept {
        if (!run || run == UINT64_MAX || generation > 0x7FFFFFFFU) return {};
        if (run != run_) { *this = State{}; run_ = run; }
        if (generation_ == 0) generation_ = generation;
        if (generation != generation_) return {};
        return {generation_, started_};
    }
    [[nodiscard]] bool left_started(const Owner& owner) noexcept {
        if (owner.run != run_ || owner.generation == 0 || owner.generation != generation_
            || owner.actor == 0 || owner.actor == kInvalid
            || owner.character == 0 || owner.character == kInvalid
            || owner.biped == 0 || owner.biped == kInvalid
            || owner.character == owner.biped || started_) return false;
        owner_ = owner;
        started_ = true;
        return true;
    }
private:
    std::uint64_t run_{UINT64_MAX};
    std::uint32_t generation_{};
    Owner owner_{};
    bool started_{};
};

[[nodiscard]] Snapshot prepare(std::uint64_t run, std::uint32_t generation) noexcept;
void note_left_started(std::uint64_t run, std::uint32_t generation,
                       std::uint32_t actor, std::uint32_t character,
                       std::uint32_t biped) noexcept;
} // namespace dawn::state::activity::omega::lair_start
