#pragma once

#include <cstddef>
#include <cstdint>

#include "../../../core/logging/repetition.h"

namespace dawn::server::gameplay::peer {

struct TrafficLogKey {
    std::uint32_t address{};
    std::uint16_t port{};
    std::uint32_t localConnection{};
    std::uint32_t remoteConnection{};
    unsigned stage{};
    bool applicationReady{};
    bool outbound{};
    bool operator==(const TrafficLogKey&) const = default;
};

/** Empty packets that acknowledge queued data, carry records or drops stay individually visible. */
[[nodiscard]] inline bool routine_inbound(bool connected,
                                          bool headPresent,
                                          unsigned cursor,
                                          unsigned entries,
                                          std::size_t large,
                                          std::size_t small,
                                          std::size_t dropped,
                                          std::size_t delivered,
                                          std::size_t retired,
                                          std::size_t queued) noexcept {
    return connected && headPresent && cursor == 1 && entries == 0 && large == 0 && small == 0
           && dropped == 0 && delivered == 0 && retired == 0 && queued == 0;
}

/** The observed successful six-byte empty reply; other sizes and pending sends remain visible. */
[[nodiscard]] inline bool routine_outbound(bool sent,
                                           std::size_t fragments,
                                           std::size_t queued,
                                           std::size_t bytes) noexcept {
    return sent && fragments == 0 && queued == 0 && bytes == 6;
}

/** Caller owns synchronization. Sequence gaps, duplicates, and state changes always pass. */
class TrafficLogRepetition {
public:
    [[nodiscard]] core::log::RepetitionReport observe(const TrafficLogKey& key,
                                                     std::uint16_t sequence,
                                                     bool routine,
                                                     std::uint64_t now) noexcept {
        State* state = observations_.find_or_insert(key);
        if (state == nullptr) {
            return {};
        }
        const bool continuous = state->seen
                                && sequence == (state->sequence + 1U) % 1024U;
        const bool repeatedRoutine = routine && state->routine;
        state->sequence = sequence;
        state->seen = true;
        state->routine = routine;
        return state->counter.observe(now, repeatedRoutine && continuous);
    }

    void clear() noexcept { observations_.clear(); }

private:
    struct State {
        core::log::RepetitionCounter counter{};
        std::uint16_t sequence{};
        bool seen{};
        bool routine{};
    };
    core::log::ObservationTable<TrafficLogKey, State, 32> observations_{};
};

} // namespace dawn::server::gameplay::peer
