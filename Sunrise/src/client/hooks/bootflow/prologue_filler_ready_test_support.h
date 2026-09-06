#pragma once

#include <cstddef>
#include <cstdint>

namespace sunrise::client::hooks::bootflow::prologue_filler_ready_test_support {

/** Native target slots injected only when the production source is compiled by its focused test. */
enum class TargetSlot : std::uint8_t {
    ready,
    state_update,
    activity_ready,
};

/** Supplied by the focused executable's fake native-image boundary. */
[[nodiscard]] std::byte* resolve_target(TargetSlot slot) noexcept;

/** Production-owner observations exposed only to the source-inclusion test build. */
[[nodiscard]] std::uint32_t active_calls() noexcept;
[[nodiscard]] std::uint32_t ownership_count() noexcept;
[[nodiscard]] std::uint32_t published_original_count() noexcept;
[[nodiscard]] bool accepting() noexcept;
[[nodiscard]] bool armed() noexcept;

/** Invokes the actual production replacement bodies. */
[[nodiscard]] bool invoke_ready() noexcept;
[[nodiscard]] std::int32_t invoke_state_update(std::byte* state) noexcept;
[[nodiscard]] std::int32_t invoke_activity_ready(std::byte* state) noexcept;

} // namespace sunrise::client::hooks::bootflow::prologue_filler_ready_test_support
