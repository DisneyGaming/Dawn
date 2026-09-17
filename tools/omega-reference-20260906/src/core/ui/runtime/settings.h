#pragma once

#include <Windows.h>

namespace dawn::core::ui::runtime {

/** UI visibility settings, read at boot. */
struct Settings {
    /** When off, the UI ignores the toggle key. */
    bool enabled{true};
    /** Windows virtual key that shows or hides the UI. */
    UINT toggleVirtualKey{VK_INSERT};
};

} // namespace dawn::core::ui::runtime
