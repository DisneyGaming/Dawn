#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::forest_switches {

inline constexpr std::size_t kCapacity = 16;
inline constexpr std::size_t kSwitchBits = 32 + 1 + 32 + 32;
inline constexpr std::uint32_t kHashClass = 0x80800070U;
inline constexpr std::uint32_t kAbsentHash = 0x811C9DC5U;

struct Entry final {
    std::uint32_t key{};
    std::uint32_t value{};
    friend constexpr bool operator==(const Entry&, const Entry&) = default;
};

struct Batch final {
    std::array<Entry, kCapacity> entries{};
    std::size_t count{};
};

[[nodiscard]] constexpr bool valid_hash(std::uint32_t value) noexcept {
    return value != 0 && value != UINT32_MAX && value != kAbsentHash;
}

[[nodiscard]] constexpr bool valid_key(std::uint32_t value) noexcept {
    return value != 0 && value != UINT32_MAX;
}

[[nodiscard]] constexpr bool valid(const Batch& batch) noexcept {
    if (batch.count > kCapacity) return false;
    for (std::size_t i = 0; i < batch.count; ++i) {
        if (!valid_key(batch.entries[i].key) || !valid_hash(batch.entries[i].value)) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (batch.entries[j].key == batch.entries[i].key) return false;
    }
    return true;
}

template <class Writer>
[[nodiscard]] bool write(Writer& writer, const Batch& batch) noexcept {
    if (!valid(batch)) return false;
    for (std::size_t i = 0; i < batch.count; ++i) {
        const auto& entry = batch.entries[i];
        if (!writer.write(entry.key, 32) || !writer.write(1, 1)
            || !writer.write(kHashClass, 32) || !writer.write(entry.value, 32)) {
            return false;
        }
    }
    return true;
}

} // namespace dawn::middleware::bap::activity_message::native::forest_switches
