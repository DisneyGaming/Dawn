#pragma once
#include "omega_definition.h"
#include "../omega_presentation.h"
#include "../omega_first_lair_runtime.h"
#include "../omega_ending.h"

namespace dawn::state::activity::coo::omega {
using Input = MissionInput;
struct Frame final {
    omega_presentation::Presentation presentation{};
    omega_first_lair::Authority encounter{};
    omega_ending::Authority ending{};
};

// Mission bindings supply typed authority producers. Shared composition owns
// selection, module leases, update ordering, retained facts, and teardown.
struct Controllers : MissionPorts<Frame> {
    virtual omega_presentation::Presentation presentation(const Input&) noexcept = 0;
    virtual omega_first_lair::Authority encounter(std::uint64_t, std::uint32_t, bool) noexcept = 0;
    virtual void request_ending(std::uint64_t, bool) noexcept = 0;
    virtual omega_ending::Authority ending(const Input&) noexcept = 0;
    virtual std::array<bool, 8> facts(std::uint64_t, const Frame&) noexcept = 0;
    void update_module(std::uint32_t id, const Input& input, Frame& frame) noexcept final {
        switch (id) {
        case 0: frame.presentation = presentation(input); break;
        case 1: frame.encounter = encounter(input.run, frame.presentation.bossGeneration, input.executor); break;
        case 2:
            if (frame.encounter.endingRequested) { request_ending(input.run, input.executor); }
            frame.ending = ending(input); break;
        }
    }
    std::uint32_t observations(std::uint64_t run, const Frame& frame) noexcept final {
        const auto values = facts(run, frame);
        std::uint32_t result{};
        for (std::size_t i = 0; i < values.size(); ++i) { if (values[i]) { result |= 1U << i; } }
        return result;
    }
};

// Admitted local mission_scot frames only. Native callbacks retain their accepted
// mechanic contracts; they never call the mission runtime.
// Shared selection is latched before seed/roster admission and used by both publishers.
[[nodiscard]] bool select(std::uint64_t run, bool requested) noexcept;
[[nodiscard]] Frame update(const Input& input) noexcept;
void reset() noexcept;
} // namespace dawn::state::activity::coo::omega
