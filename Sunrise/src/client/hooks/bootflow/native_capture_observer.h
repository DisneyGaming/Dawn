#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow {

// The arc-charge receipt owner supplies the sole physical 1006F20 detour.
using NativeCaptureTick = std::uint8_t(__fastcall*)(void*) noexcept;
/** Samples immediately around exactly one call to the supplied native tick. */
[[nodiscard]] std::uint8_t observe_native_capture_tick(void* controller, NativeCaptureTick original) noexcept;

/** Initialize the capture sampler; never install a second timer detour. */
[[nodiscard]] bool install_native_capture_observer() noexcept;
void quiesce_native_capture_observer() noexcept;
/** Retains sampler state while an admitted native update is still in flight. */
[[nodiscard]] bool uninstall_native_capture_observer() noexcept;
[[nodiscard]] bool native_capture_observer_has_ownership() noexcept;

} // namespace sunrise::client::hooks::bootflow
