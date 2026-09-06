#pragma once

#include "omega_ikora_authority.h"
#include "../../../middleware/bap/activity_message/scene_sense.h"

namespace sunrise::state::activity::omega::ikora {
/** Exact native entered-state prefix; packet coalescing and later revisions are valid. */
template<class Object>
[[nodiscard]] bool monitor_entered(const Object& object, std::uint16_t slot) noexcept {
    return (slot == 20U || slot == 24U)
        && object.registryKey == kRegistry && object.slotType == 30U
        && object.slotIndex == slot && object.bodyBits == 99U
        && object.bodyFirst == 0xF000000030000000ULL
        && (object.bodySecond >> 32U) == 0U
        && object.bodySecond > 0U && object.bodySecond <= 0x7FFFFFFFU
        && object.bodyThird == 0U && object.bodyFourth == 0U;
}

/** Owned by one authenticated ActivityBindingState. Retained across publication/keepalive. */
struct Lattice final {
    std::uint32_t revision{};
    bool released{};
    bool ready{};

    void begin() noexcept { *this = {}; ready = true; }

    [[nodiscard]] bool observe(const middleware::bap::activity_message::scene_sense::Output& output,
                                bool openingTriggered) noexcept {
        if (!ready || !openingTriggered || !output.delta
            || output.generationWire != kSceneGenerationWire || output.revision == 0U
            || output.revision > 0x7FFFFFFFU || output.revision <= revision
            || output.eventCount > output.events.size()) return false;
        revision = output.revision;
        if (released) return false;
        for (std::size_t index = 0; index < output.eventCount; ++index) {
            if (output.events[index] == kLatticeRelease) { released = true; return true; }
        }
        return false;
    }
};
} // namespace sunrise::state::activity::omega::ikora
