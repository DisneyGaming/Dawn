#pragma once

#include <array>
#include <cstdint>

namespace dawn::state::activity::omega_arc_charge {

inline constexpr std::uint32_t kItemEntity = 0x80F44F83U;
inline constexpr std::uint32_t kCarryController = 0x80F66667U;
inline constexpr std::uint32_t kRequiredChargeProperty = 0x9C99BE55U;

struct Cycle final {
    std::uint8_t index;
    std::uint32_t registry;
    std::uint16_t carrySlot, sinkSlot, effectSlot, effectGateSlot;
    std::uint32_t carryDefinition, sinkDefinition, effectDefinition, effectGateDefinition;
    std::uint32_t sinkEntity, sinkController;
};

// Exact package slots. The pickup entity is shared, but its activity source and
// each destination's native interaction controller are distinct in every cycle.
inline constexpr std::array<Cycle, 3> kCycles{{
    {0, 0x0040BF06U, 18, 20, 19, 21,
     0x80F47672U, 0x80F47678U, 0x80F47675U, 0x80F4767BU, 0x80F44F89U, 0x80F6666EU},
    {1, 0x0040BF05U, 1, 3, 2, 4,
     0x80F47718U, 0x80F4771EU, 0x80F4771BU, 0x80F47721U, 0x80F44F8DU, 0x80F66671U},
    {2, 0x0040BF03U, 0, 2, 1, 3,
     0x80F47848U, 0x80F4784EU, 0x80F4784BU, 0x80F47851U, 0x80F44F91U, 0x80F66673U},
}};

enum class Object : std::uint8_t { none, carry, sink, effect };
struct Source final { const Cycle* cycle{}; Object object{}; };

[[nodiscard]] constexpr Source source(std::uint32_t registry, std::uint8_t type,
                                      std::uint16_t slot) noexcept {
    if (type != 4) { return {}; }
    for (const auto& cycle : kCycles) {
        if (cycle.registry != registry) { continue; }
        if (slot == cycle.carrySlot) { return {&cycle, Object::carry}; }
        if (slot == cycle.sinkSlot) { return {&cycle, Object::sink}; }
        if (slot == cycle.effectSlot) { return {&cycle, Object::effect}; }
    }
    return {};
}

[[nodiscard]] constexpr Source definition(std::uint32_t resource) noexcept {
    for (const auto& cycle : kCycles) {
        if (resource == cycle.carryDefinition) { return {&cycle, Object::carry}; }
        if (resource == cycle.sinkDefinition) { return {&cycle, Object::sink}; }
        if (resource == cycle.effectDefinition) { return {&cycle, Object::effect}; }
    }
    return {};
}

} // namespace dawn::state::activity::omega_arc_charge
