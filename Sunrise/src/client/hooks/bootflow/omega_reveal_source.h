#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::client::hooks::bootflow::omega_reveal_source {

struct Identity {
    std::uint32_t resource;
    std::uint32_t runtimeClass;
    std::int64_t offset;
};

// Live callback headers from the 2026-09-05 reveal capture, correlated with the
// exact roster entry pointers. Header +4 is the runtime component class. The
// asset-definition classes 80809A3B / 80804F06 instead appear at component +98.
inline constexpr Identity kBoss{0x80F4756AU, 0x8080948FU, 0x728};
inline constexpr Identity kIntro{0x80F478D3U, 0x80804F07U, 0x2E8};

[[nodiscard]] inline bool matches(std::span<const std::byte> component,
                                  Identity expected) noexcept {
    if (component.size() < 16) return false;
    Identity actual{};
    std::memcpy(&actual.resource, component.data(), sizeof actual.resource);
    std::memcpy(&actual.runtimeClass, component.data() + 4, sizeof actual.runtimeClass);
    std::memcpy(&actual.offset, component.data() + 8, sizeof actual.offset);
    return actual.resource == expected.resource && actual.runtimeClass == expected.runtimeClass
        && actual.offset == expected.offset;
}

} // namespace sunrise::client::hooks::bootflow::omega_reveal_source
