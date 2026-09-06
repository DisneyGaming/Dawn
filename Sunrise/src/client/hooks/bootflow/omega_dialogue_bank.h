#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "../../content/handles/handle_resolver.h"

namespace sunrise::client::hooks::bootflow::omega_dialogue_bank {

struct Binding final {
    std::uint32_t definition{0xFFFFFFFFU};
    std::int64_t offset{};
    std::uint32_t bank{0xFFFFFFFFU};
};

/** Resolve the bank exactly as native +0x10097D0, through the shared content reader. */
[[nodiscard]] inline Binding resolve(const content::handles::Source& source,
                                     std::uintptr_t component) noexcept {
    Binding result{};
    constexpr auto limit = (std::numeric_limits<std::uintptr_t>::max)();
    if (source.read == nullptr || component == 0 || component > limit-16) { return result; }
    const auto read = [&source](std::uintptr_t address, auto& value) noexcept {
        return source.read(source.context, address,
                           std::span(reinterpret_cast<std::byte*>(&value), sizeof value));
    };
    if (!read(component, result.definition) || !read(component+8, result.offset)
        || result.offset < 0 || result.offset > 0x20000) { return result; }
    std::uintptr_t definition{};
    if (!content::handles::resolve(source, result.definition, definition)) { return result; }
    const auto bankOffset = static_cast<std::uintptr_t>(result.offset)+0x58U;
    if (definition > limit-bankOffset-sizeof(result.bank)) { return result; }
    std::uint32_t bank{};
    if (read(definition+bankOffset, bank)) { result.bank = bank; }
    return result;
}

} // namespace sunrise::client::hooks::bootflow::omega_dialogue_bank
