#pragma once

#include <array>
#include <cstddef>

#include "../../../state/activity/lifecycle_generation.h"

namespace sunrise::server::gameplay::group {

/** At most every bounded State record can be a source awaiting retirement. */
inline constexpr std::size_t kSourceRetirementFenceCapacity = 16;

/** Exact source tombstones held from group-row detach through State retirement. */
struct SourceRetirementFences final {
    std::array<state::activity::ActivityInstanceKey, kSourceRetirementFenceCapacity> sources{};
    std::size_t count{};
};

[[nodiscard]] inline bool source_retirement_fenced(
    const SourceRetirementFences& fences,
    state::activity::ActivityInstanceKey source) noexcept {
    if (!static_cast<bool>(source)) {
        return false;
    }
    for (std::size_t index = 0; index < fences.count; ++index) {
        if (fences.sources[index] == source) {
            return true;
        }
    }
    return false;
}

/** A claimant must pass both the group fence and a post-claim exact State validation. */
[[nodiscard]] inline bool source_claim_is_current(
    const SourceRetirementFences& fences,
    state::activity::ActivityInstanceKey source,
    bool exactRecordLive) noexcept {
    return !static_cast<bool>(source)
           || (!source_retirement_fenced(fences, source) && exactRecordLive);
}

/** Installs an idempotent exact source tombstone without wrapping or eviction. */
[[nodiscard]] inline bool begin_source_retirement(
    SourceRetirementFences& fences,
    state::activity::ActivityInstanceKey source) noexcept {
    if (!static_cast<bool>(source)) {
        return false;
    }
    if (source_retirement_fenced(fences, source)) {
        return true;
    }
    if (fences.count >= fences.sources.size()) {
        return false;
    }
    fences.sources[fences.count] = source;
    ++fences.count;
    return true;
}

/** Removes only the exact tombstone whose State retirement has completed. */
inline void finish_source_retirement(
    SourceRetirementFences& fences,
    state::activity::ActivityInstanceKey source) noexcept {
    for (std::size_t index = 0; index < fences.count; ++index) {
        if (fences.sources[index] != source) {
            continue;
        }
        --fences.count;
        fences.sources[index] = fences.sources[fences.count];
        fences.sources[fences.count] = {};
        return;
    }
}

} // namespace sunrise::server::gameplay::group
