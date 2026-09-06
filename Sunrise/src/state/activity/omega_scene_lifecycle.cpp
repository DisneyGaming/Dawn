#include "omega_scene_lifecycle.h"

#include <atomic>

namespace sunrise::state::activity::omega_scene_lifecycle {
namespace {

constexpr std::uint32_t kSceneOneHandle = 0x80EC0F0EU;
constexpr std::uint32_t kSceneTwoHandle = 0x80EC0FA8U;
constexpr std::uint32_t kSceneThreeHandle = 0x80EC0FA6U;

std::atomic_uint32_t g_pendingMask{};

[[nodiscard]] constexpr std::uint32_t scene_bit(std::uint32_t handle) noexcept {
    return handle == kSceneOneHandle     ? 1U
           : handle == kSceneTwoHandle ? 2U
           : handle == kSceneThreeHandle ? 4U
                                         : 0U;
}

} // namespace

void reset() noexcept {
    g_pendingMask.store(0U, std::memory_order_release);
}

bool schedule(std::uint32_t sceneHandle) noexcept {
    const std::uint32_t bit = scene_bit(sceneHandle);
    if (bit == 0U) {
        return false;
    }
    return (g_pendingMask.fetch_or(bit, std::memory_order_acq_rel) & bit) == 0U;
}

std::uint32_t pending_mask() noexcept {
    return g_pendingMask.load(std::memory_order_acquire);
}

std::uint32_t consume() noexcept {
    return g_pendingMask.exchange(0U, std::memory_order_acq_rel);
}

} // namespace sunrise::state::activity::omega_scene_lifecycle
