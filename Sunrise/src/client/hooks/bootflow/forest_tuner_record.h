#pragma once

#include <array>
#include <cstddef>
#include <cstring>

#include "forest_tuner_state.h"

namespace sunrise::client::hooks::bootflow::forest_tuner {

/** Writes one edge anchor to a decoded 0x80805007 record, not its packed wire form.
 * The 0x8080500F block aligns each float separately; the four groups do not have a
 * common stride. Offsets come from the executable's reflection descriptors. */
inline void write_group(std::byte* record, std::size_t index, const Group& source) noexcept {
    struct Offsets {
        std::size_t a;
        std::size_t b;
        std::size_t weight;
        std::size_t active;
    };
    constexpr std::array<Offsets, 4> kOffsets{{
        {0x08U, 0x09U, 0x0CU, 0x10U},
        {0x11U, 0x12U, 0x14U, 0x18U},
        {0x19U, 0x1AU, 0x1CU, 0x20U},
        {0x21U, 0x22U, 0x24U, 0x28U},
    }};
    const Offsets& offsets = kOffsets[index];
    record[offsets.a] = static_cast<std::byte>(source.a.load(std::memory_order_relaxed) & 0xFF);
    record[offsets.b] = static_cast<std::byte>(source.b.load(std::memory_order_relaxed) & 0xFF);
    const float weight = source.weight.load(std::memory_order_relaxed);
    std::memcpy(record + offsets.weight, &weight, sizeof(weight));
    record[offsets.active] =
        static_cast<std::byte>(source.active.load(std::memory_order_relaxed) ? 1 : 0);
}

} // namespace sunrise::client::hooks::bootflow::forest_tuner
