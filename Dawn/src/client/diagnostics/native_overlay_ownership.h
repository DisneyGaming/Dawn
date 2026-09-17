#pragma once

#include <cstdint>

namespace dawn::client::diagnostics::native_overlays::detail {

/** Tracks restoration only while the native byte still holds this control's last edit. */
struct Ownership final {
    struct Edit final {
        std::uint8_t before{};
        std::uint8_t after{};
    };

    std::uint8_t original{};
    std::uint8_t last{};
    bool owned{};

    /** An observed native/debugger change ends our restoration claim. */
    void observe(std::uint8_t current) noexcept {
        if (owned && current != last) {
            owned = false;
        }
    }

    /** Called only after an exact compare/exchange succeeds. */
    void written(Edit edit) noexcept {
        observe(edit.before);
        if (!owned) {
            original = edit.before;
        }
        last = edit.after;
        owned = original != last;
    }
};

} // namespace dawn::client::diagnostics::native_overlays::detail
