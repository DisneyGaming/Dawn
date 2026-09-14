#pragma once

#include <cstdint>
#include <string_view>

namespace sunrise::state::activity::progress {

/** Stable Sunrise-internal key used for one exact package name. */
[[nodiscard]] std::uint32_t mission_key(std::string_view packageName) noexcept;

/**
 * Stores the newest durable progress reported by an authenticated activity controller.
 * Package name is hashed only as Sunrise's stable database key; it is not a claimed game hash.
 */
[[nodiscard]] bool observe(std::uint64_t characterSoid,
                           std::string_view packageName,
                           std::int32_t activityIndex,
                           std::uint32_t checkpointHash,
                           std::int32_t checkpointSliceSet,
                           std::int32_t progress,
                           bool completed) noexcept;

/** Clears process-local write suppression. Durable rows remain in SQLite. */
void reset() noexcept;

} // namespace sunrise::state::activity::progress
