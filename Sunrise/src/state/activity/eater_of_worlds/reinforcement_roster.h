#pragma once

#include <cstdint>

namespace sunrise::state::activity::eater_of_worlds::reinforcements {

enum class Kind : std::uint8_t { invalid, colossus, incendior, psion };

// Keep each authored delivery source and its placement rule. Only its passengers
// change. Each source has one class so partial native allocation retries cannot
// alter the composition of the ten-enemy group.
inline constexpr std::uint8_t count(std::uint16_t slot) noexcept {
    switch (slot) {
    case 18: return 2;
    case 19: return 3;
    case 20: return 5;
    default: return 0;
    }
}

inline constexpr Kind select(std::uint16_t slot, std::uint32_t ordinal) noexcept {
    if (ordinal >= count(slot)) return Kind::invalid;
    switch (slot) {
    case 18: return Kind::colossus;
    case 19: return Kind::incendior;
    case 20: return Kind::psion;
    default: return Kind::invalid;
    }
}

// Class identities come from the retained Eater entity definitions. The native
// cabal_gladiator name is the Colossus model confirmed in the delivery capture.
inline constexpr std::uint32_t entity(Kind kind) noexcept {
    switch (kind) {
    case Kind::colossus: return 0x80C0FA98U;
    case Kind::incendior: return 0x80BFAA20U;
    case Kind::psion: return 0x80C1A8E4U;
    default: return 0;
    }
}

static_assert(count(18) + count(19) + count(20) == 10);
static_assert(select(18, 0) == Kind::colossus && select(18, 1) == Kind::colossus);
static_assert(select(19, 2) == Kind::incendior && select(20, 4) == Kind::psion);
static_assert(select(18, 2) == Kind::invalid && select(0, 0) == Kind::invalid);

} // namespace sunrise::state::activity::eater_of_worlds::reinforcements
