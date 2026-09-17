#pragma once

#include <cstdint>

namespace dawn::client::diagnostics::native_overlays {

/** One session-only switch backed by the game's own diagnostic display. */
enum class Overlay : std::uint8_t {
    cameraPosition,
    activityInstances,
    sessionTransitions,
    transitionDetail,
    bubbleHosts,
    retailLog,
    activity,
    channels,
    sessions,
    members,
    inactivity,
    matchmaking,
    qos,
    voice,
    repairs,
    buildInformation,
    count,
};

/** Fresh native state; unavailable controls must not accept edits. */
struct Snapshot final {
    bool available{};
    std::uint8_t value{};
    const char* reason{};
    const char* lastError{};
};

/** Returns a stable menu label or description for a supported display. */
[[nodiscard]] const char* display_name(Overlay overlay) noexcept;
[[nodiscard]] const char* description(Overlay overlay) noexcept;

/** Qualifies the executable reader and reads the current byte without modifying it. */
[[nodiscard]] Snapshot snapshot(Overlay overlay) noexcept;

/** Changes only the byte value the user saw, after checking its reader again. */
[[nodiscard]] bool set_enabled(Overlay overlay, std::uint8_t expected, bool enabled) noexcept;

/** Restores only our still-owned edits, then forgets this module's session state. */
void shutdown() noexcept;

} // namespace dawn::client::diagnostics::native_overlays
