#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::client::hooks::bootflow::omega_reveal_native {
[[nodiscard]] bool install() noexcept;
void uninstall() noexcept;
void update_directive(std::byte* component) noexcept;
// Called after the existing original 9F19F0 authority handler returns.
void observe_mission_device(std::byte* component) noexcept;
void observe_mission_position(std::uint32_t player,const float* xyz) noexcept;
// Native active-to-inactive receipt for the exact A74B2200 cinematic; timestamp
// uses GetTickCount64 and is never inferred from its authored duration.
[[nodiscard]] bool cinematic_completion(std::uint64_t run, std::uint64_t& completedAt) noexcept;
// Read-only diagnostics for the two exact roster slots. The caller resolves each
// binding afresh; this owner never retains or invokes a sampled runtime pointer.
[[nodiscard]] bool begin_binding_sample() noexcept;
void observe_binding(bool boss, bool found, std::uint32_t handle,
                     std::uint32_t schema, std::uint32_t componentKind,
                     std::uint32_t bubble, std::uint8_t pending, std::uint8_t applied,
                     const std::byte* runtime, std::span<const std::byte> copiedHeader) noexcept;
}
