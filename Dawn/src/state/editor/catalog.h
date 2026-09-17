#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "../account/account_state.h"
#include "../build_data/items/item_catalog.h"
#include "../build_data/items/details/definition.h"

namespace dawn::state::editor {
enum class GearKind { other, weapon, armor, cosmetic, subclass };
enum class PlugScope { compatible, socketAndGear, socket, gear, all };
struct AbilityChoice { std::uint8_t entry{}; std::string name; };
struct SubclassPath { std::string name; std::uint8_t super{}, melee{}; std::vector<std::string> perks; };
struct CatalogItem {
    build_data::items::Definition definition{};
    build_data::items::details::Definition detail{};
    std::string name, type, description, search;
    std::size_t slot{account::inventory::kEquipmentSlotCount};
    std::uint8_t characterClass{3};
    GearKind kind{GearKind::other};
    bool plug{}, internal{};
    std::uint32_t iconTag{};
    std::array<std::vector<std::uint16_t>, account::inventory::kPlugCapacity> compatible;
    // Jump, grenade, super, melee, class ability; indices address the native subclass entry list.
    std::array<std::vector<AbilityChoice>, 5> abilities;
    std::vector<SubclassPath> paths;
};
struct Catalog {
    std::vector<CatalogItem> items;
    std::unordered_map<std::uint32_t, std::size_t> hashes;
    std::unordered_map<std::uint16_t, std::size_t> indices;
    std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> socketPools;
    std::array<std::vector<std::uint16_t>, 5> gearPools;
    std::vector<std::uint16_t> plugs;
    std::array<std::uint8_t, 6> statRows{};
    const CatalogItem* find(std::uint32_t hash) const noexcept;
    const CatalogItem* index(std::uint16_t id) const noexcept;
    std::vector<std::uint16_t> candidates(const CatalogItem& item, std::size_t lane, PlugScope scope) const;
    void finish();
};
std::string searchable(std::string value);
bool matches(const CatalogItem& item, const std::string& query);
bool fits_class(const CatalogItem& item, CharacterClass characterClass) noexcept;
// Runs on the editor's worker. No game assets or online manifest are bundled.
bool load_catalog(Catalog& output, std::atomic_bool& cancel, std::atomic_uint& progress, std::string& error);
} // namespace dawn::state::editor
