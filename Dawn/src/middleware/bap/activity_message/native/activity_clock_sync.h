#pragma once
#include "activity_clock_authority.h"
#include "../activity_patch_epoch_parser.h"

namespace dawn::middleware::bap::activity_message::native::activity_clock {
// A clock-only type5: original 3C9FC0 consumes the first 201 bits, then the
// enclosing 4D92A0 sees roster latch0, absent roster delta, empty source-group loop and no trailing
// pair. This does not grant a bubble, construct sources or change membership.
inline constexpr std::uint32_t kSynchronizationMessageType=5;
inline constexpr std::size_t kSynchronizationBits=205;
inline constexpr std::size_t kSynchronizationBytes=(kSynchronizationBits+7)/8;
template<class Writer> [[nodiscard]] bool write_synchronization(Writer& writer,
    const patch_epoch::PatchEpoch& epoch,std::uint64_t elapsed) noexcept {
    return elapsed!=UINT64_MAX && writer.write(0,8)
        && writer.write(epoch.first,64) && writer.write(epoch.second,64)
        && writer.write(0,1) && write_elapsed(writer,elapsed)
        && writer.write(0,1) && writer.write(0,1) && writer.write(0,1) && writer.write(0,1);
}
}
