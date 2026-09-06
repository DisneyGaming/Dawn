#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "../../../state/activity/omega/omega_progression.h"

namespace sunrise::client::hooks::bootflow::omega_presentation {
namespace omega = state::activity::omega;

inline constexpr std::size_t kDialogueBytes = 0x188 + 34*32;
inline constexpr std::size_t kDirectiveBytes = 0x218;

template<class T> [[nodiscard]] T read(std::span<std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}
template<class T> void write(std::span<std::byte> bytes, std::size_t offset, T value) noexcept {
    std::memcpy(bytes.data() + offset, &value, sizeof value);
}

/** Repair requested dialogue records before native scans them. The native
 * processed-generation mirror is outside this span and remains exclusively native-owned. */
[[nodiscard]] inline std::uint64_t sync_dialogue(std::span<std::byte> bytes,
                                           omega::Progress progress) noexcept {
    if (bytes.size() < kDialogueBytes
        || !omega::dialogue_source(read<std::uint32_t>(bytes, 0), read<std::int64_t>(bytes, 8)))
        return 0;
    std::uint64_t changed = 0;
    for (unsigned index = 0; index < 2; ++index) {
        if (!(index == 0 ? progress.vistaDialogue : progress.exitDialogue)) continue;
        const unsigned row = index == 0 ? 7U : 9U;
        const std::size_t start = 0x188 + row*32;
        if (read<std::int32_t>(bytes, start + 24) == 1
            && read<std::uint8_t>(bytes, start + 28) == 2
            && read<std::uint64_t>(bytes, start + 8) != 0) continue;
        // Same decoded values as the type-53 wire writer. No time-based replay or new generation.
        write<std::uint64_t>(bytes, start, UINT64_MAX);
        write<std::uint64_t>(bytes, start + 8, 1);
        write<std::uint64_t>(bytes, start + 16, 0xFFFF00FF811C9DC5ULL);
        write<std::int32_t>(bytes, start + 24, 1);
        write<std::uint8_t>(bytes, start + 28, 2);
        changed |= UINT64_C(1) << row;
    }
    for (unsigned row=12;row<34;++row) {
        if ((progress.lairDialogueRequestedMask & (UINT64_C(1) << row)) == 0) continue;
        const std::size_t start=0x188+row*32;
        const bool pending=progress.lairDialoguePendingRow==row;
        const auto mode=static_cast<std::uint8_t>(pending ? 2U : 0U);
        const std::uint64_t time=pending ? 1U : 0U;
        if (read<std::int32_t>(bytes,start+24)==1
            && read<std::uint8_t>(bytes,start+28)==mode
            && read<std::uint64_t>(bytes,start+8)==time) continue;
        write<std::uint64_t>(bytes,start,UINT64_MAX);
        write<std::uint64_t>(bytes,start+8,time);
        write<std::uint64_t>(bytes,start+16,0xFFFF00FF811C9DC5ULL);
        write<std::int32_t>(bytes,start+24,1);
        write<std::uint8_t>(bytes,start+28,mode);
        changed |= UINT64_C(1) << row;
    }
    return changed;
}

/** Supply both the direct ActivityPoint and its named cross-area locator. Native updates
 * need the locator to resolve a newly loaded point or route between different bubbles. */
[[nodiscard]] inline bool sync_directive(std::span<std::byte> bytes,
                                        omega::Progress progress) noexcept {
    if (progress.lairObjectiveEvent != 0 || bytes.size() < kDirectiveBytes
        || read<std::uint32_t>(bytes, 0) != 0x80F47BD4U
        || read<std::int64_t>(bytes, 8) != 0xB88
        || read<std::uint32_t>(bytes, 0x190) != 0x1EBF4621U
        || read<std::int8_t>(bytes, 0x198) != 0) return false;
    const auto target = omega::waypoint(progress.route);
    if (target.registry == 0) return false;
    const std::array<std::uint32_t, 4> locator = progress.destination == 0x811C9DC5U
        ? std::array<std::uint32_t, 4>{0x811C9DC5U, 0x811C9DC5U, 0x811C9DC5U, 0x811C9DC5U}
        : std::array<std::uint32_t, 4>{progress.destination, target.bubbleName,
                                      target.registry, target.pointName};
    if (read<std::uint32_t>(bytes, 0x1F8) == target.registry
        && read<std::uint8_t>(bytes, 0x1FC) == 47
        && read<std::uint16_t>(bytes, 0x1FE) == target.index
        && std::memcmp(bytes.data() + 0x208, locator.data(), sizeof locator) == 0) return false;
    write<std::uint32_t>(bytes, 0x1F8, target.registry);
    write<std::uint8_t>(bytes, 0x1FC, 47);
    write<std::uint16_t>(bytes, 0x1FE, target.index);
    std::memcpy(bytes.data() + 0x208, locator.data(), sizeof locator);
    return true;
}
} // namespace sunrise::client::hooks::bootflow::omega_presentation
