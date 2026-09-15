#pragma once

#include <cstdint>

namespace sunrise::state::build_data::cache::records {

/**
 * Current build-data cache format. Every non-current valid prefix is rebuildable stale state;
 * bump this whenever a stored shape or extraction identity changes.
 */
inline constexpr std::uint32_t kCacheFormatVersion = 56;

} // namespace sunrise::state::build_data::cache::records
