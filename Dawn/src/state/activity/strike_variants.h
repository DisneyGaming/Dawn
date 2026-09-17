#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace dawn::state::activity::strikes {
enum class Difficulty : std::uint8_t { standard, adept, master, grandmaster };
struct Variant {
    std::string_view package;
    Difficulty difficulty;
    std::uint16_t activity;
    std::uint32_t hash;
};
// Build 86657 public table 81327CF0, joined to client display table 81327D35.
// Difficulty settings: Adept 813206C1, Master 81327CEB, Grandmaster 81327CEF.
// The direct Grandmaster entries avoid the weekly rotating Ordeal selection.
inline constexpr std::array<Variant, 8> kVariants{{
    {"strike_pact", Difficulty::standard, 230, 0x9FFC7326U},
    {"strike_pact", Difficulty::adept, 830, 0x9E9A9777U},
    {"strike_pact", Difficulty::master, 833, 0x9E9A9772U},
    {"strike_pact", Difficulty::grandmaster, 835, 0x789EB910U},
    {"strike_bond", Difficulty::standard, 229, 0x99BDAB3DU},
    {"strike_bond", Difficulty::adept, 808, 0x96FD9EF8U},
    {"strike_bond", Difficulty::master, 811, 0x96FD9EFDU},
    {"strike_bond", Difficulty::grandmaster, 813, 0x3BC629F7U},
}};
[[nodiscard]] constexpr const Variant* find(std::string_view package, Difficulty difficulty) noexcept {
    for (const auto& row : kVariants) {
        if (row.package == package && row.difficulty == difficulty) { return &row; }
    }
    return nullptr;
}
[[nodiscard]] constexpr const Variant* find(std::int16_t activity) noexcept {
    for (const auto& row : kVariants) {
        if (row.activity == activity) { return &row; }
    }
    return nullptr;
}
[[nodiscard]] constexpr const char* name(Difficulty difficulty) noexcept {
    switch (difficulty) {
    case Difficulty::standard: return "Standard";
    case Difficulty::adept: return "Adept";
    case Difficulty::master: return "Master";
    case Difficulty::grandmaster: return "Grandmaster (GM)";
    }
    return "Unavailable";
}
}
