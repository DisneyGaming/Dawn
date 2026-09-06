#pragma once

#include <cstdint>

namespace sunrise::state::activity::omega_scene_lifecycle {

/** Clears native scene-retirement observations from an earlier launch or pre-handoff scene. */
void reset() noexcept;

/**
 * Records one terminal retirement of an authored Omega scene cast.
 * This function is lock-free because it is called from inside Destiny's native teardown path.
 * @param sceneHandle Authored scene handle read before the native function retires it.
 * @return True only when this handle added a new pending retirement bit.
 */
[[nodiscard]] bool schedule(std::uint32_t sceneHandle) noexcept;

/** @return Bit mask of authored Omega scene casts awaiting the server's normal update pump. */
[[nodiscard]] std::uint32_t pending_mask() noexcept;

/** Atomically takes all pending retirement bits for deferred host-side processing. */
[[nodiscard]] std::uint32_t consume() noexcept;

} // namespace sunrise::state::activity::omega_scene_lifecycle
