#pragma once

namespace dawn::core::path {

/**
 * Removes bounded, old writer-owned temporary siblings whose process has stopped.
 * A sibling is named final.process.thread.sequence.tmp, so a crashed writer leaves one behind.
 * @param finalPath Null-terminated final file path that owns the sibling names.
 */
void remove_stale_siblings(const wchar_t* finalPath) noexcept;

} // namespace dawn::core::path
