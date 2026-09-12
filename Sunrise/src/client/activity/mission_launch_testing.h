#pragma once
// Only the isolated lifecycle test executable substitutes native entry points and time.
#if defined(SUNRISE_MISSION_LAUNCH_TESTS)
#include <cstdint>
namespace sunrise::client::activity::mission_launch::testing {
[[nodiscard]] std::uintptr_t native_entry(std::uintptr_t rva) noexcept;
[[nodiscard]] std::uint64_t now() noexcept;
}
#endif
