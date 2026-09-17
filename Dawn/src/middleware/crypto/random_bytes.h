#pragma once

#include <cstddef>
#include <span>

namespace dawn::middleware::crypto::random {

/**
 * Fills a buffer with Windows system randomness.
 * Every ephemeral secret Dawn owns comes from here, never from an authored default.
 * @param output Storage to overwrite completely.
 * @return True when Windows produced every byte.
 */
[[nodiscard]] bool fill(std::span<std::byte> output) noexcept;

} // namespace dawn::middleware::crypto::random
