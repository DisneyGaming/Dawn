#pragma once
#include <cstddef>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::interaction {
// A native80804FB8 command within the type4 source's80809AEA override list.
// Original9EF680 dispatches it throughB31960 toF33930. Enabling invalidates
// the prompt cache; original player predicates, spatial tests and use remain.
enum class Mode : std::uint8_t { unchanged, disabled, enabled };
inline constexpr std::size_t kPayloadBits=122;
[[nodiscard]] constexpr bool valid(Mode mode) noexcept {
    return mode==Mode::unchanged || mode==Mode::disabled || mode==Mode::enabled;
}
template<class Writer> [[nodiscard]] bool write_record(Writer& writer,Mode mode) noexcept {
    if(!valid(mode))return false;
    if(mode==Mode::unchanged)return writer.write(0,1);
    // Authored rally setup has an absent scoped override and zero command
    // revision. Preserve that predicate and leave native use counters alone.
    return writer.write(1,1) && writer.write(0x80804FB8U,32)
        && writer.write(mode==Mode::enabled?3:2,2)
        && writer.write(0x811C9DC5U,32) && writer.write(0,7) && writer.write(32767,16)
        && writer.write(0x80000000U,32) && writer.write(0,1);
}
}
