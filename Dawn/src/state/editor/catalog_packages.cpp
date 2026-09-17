// SPDX-License-Identifier: GPL-3.0-only
// Investment name/icon layouts adapted from Sundial; see vendor/sundial/NOTICE.md.
#include <Windows.h>
#include "catalog.h"
#include "localized_strings.h"
#include "../../client/content/items/packages/internal.h"
#include "../runtime/state_account_transaction_helpers.h"
#include "../../../vendor/sundial/class_items.h"
#include <algorithm>
#include <memory>

namespace dawn::state::editor {
namespace {
namespace packages = client::content::items::packages;
namespace reader = middleware::content::packages::reader;
namespace tables = middleware::content::packages::tables;
struct ReadScope {
    reader::BlockKeys keys{};
    std::unique_ptr<reader::Scratch> scratch{std::make_unique<reader::Scratch>()};
    ~ReadScope() { reader::close_files(*scratch); SecureZeroMemory(&keys, sizeof keys); }
};
bool pool_member(void* context, std::uint16_t id) noexcept {
    static_cast<std::vector<std::uint16_t>*>(context)->push_back(id);
    return true;
}
}
bool load_catalog(Catalog& output, std::atomic_bool& cancel, std::atomic_uint& progress, std::string& error) {
    if (!build_data::configured_item_details_ready() || !build_data::socket_plug_rules_ready()) {
        error = "The installed item catalog is still loading. Try again after character selection appears.";
        return false;
    }
    ReadScope scope;
    core::path::Buffer directory{};
    if (!packages::package_directory(directory) || !packages::collect_keys(scope.keys)) {
        error = "Game packages are not ready to read yet."; return false;
    }
    reader::Source source{directory.chars.data(), &scope.keys};
    std::array<std::uint32_t, packages::kContainerCandidates> globalsTags{};
    std::size_t globalsCount{};
    std::vector<std::byte> globals, stringMap, localizedIndex, iconTable;
    tables::Array stringRows{}, localizedRows{}, iconRows{};
    bool located = false;
    if (packages::investment_globals_tags(globalsTags, globalsCount)) {
        for (std::size_t i = 0; i < globalsCount && !located; ++i) {
            std::uint32_t stringTag{}, localizedTag{}, iconTag{};
            located = reader::read_tag(source, *scope.scratch, globalsTags[i], globals)
                && tables::child_tag(globals, 33, stringTag) && tables::child_tag(globals, 72, localizedTag)
                && tables::child_tag(globals, 75, iconTag)
                && reader::read_tag(source, *scope.scratch, stringTag, stringMap)
                && reader::read_tag(source, *scope.scratch, localizedTag, localizedIndex)
                && reader::read_tag(source, *scope.scratch, iconTag, iconTable)
                && tables::find_array_at(stringMap, 8, stringRows) && stringRows.elementClass == 0x80805CDFU
                && tables::find_array_at(localizedIndex, 8, localizedRows)
                && tables::find_array_at(iconTable, 8, iconRows) && iconRows.elementClass == 0x80802957U;
        }
    }
    if (!located) { error = "Could not read the installed item names and preview index."; return false; }
    std::unordered_map<std::uint32_t, std::uint32_t> tags;
    for (std::size_t i = 0; i < stringRows.count; ++i) {
        tables::IndexRow row{};
        if (tables::index_row(stringMap, stringRows, i, row)) tags[row.definitionHash] = row.targetTag;
    }
    std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, std::string>> cache;
    std::unordered_map<std::uint32_t, bool> previewReferences;
    const auto resolve = [&](std::span<const std::byte> data, std::size_t at) -> std::string {
        std::uint32_t table{}, hash{};
        if (!strings::read(data, at, table) || !strings::read(data, at + 4, hash) || table >= localizedRows.count) return {};
        auto it = cache.find(table);
        if (it == cache.end()) {
            std::unordered_map<std::uint32_t, std::string> values;
            std::uint32_t headerTag{}, dataTag{};
            std::vector<std::byte> header, bytes;
            if (strings::read(std::span<const std::byte>(localizedIndex), localizedRows.dataOffset + table * 8 + 4, headerTag)
                && reader::read_tag(source, *scope.scratch, headerTag, header)
                && strings::read(std::span<const std::byte>(header), 24, dataTag)
                && reader::read_tag(source, *scope.scratch, dataTag, bytes)) (void)strings::decode(header, bytes, values);
            it = cache.emplace(table, std::move(values)).first;
        }
        const auto found = it->second.find(hash);
        return found == it->second.end() ? std::string{} : found->second;
    };
    Catalog result;
    build_data::constants::InvestmentConstants constants{};
    if (!build_data::find_investment_constants(constants)) { error = "Armor stat definitions are not ready."; return false; }
    result.statRows = constants.characterStatRows;
    const auto count = build_data::item_definition_count();
    std::vector<std::byte> itemStrings, iconDefinition;
    for (std::size_t i = 0; i < count && !cancel.load(); ++i) {
        CatalogItem item;
        if (!build_data::find_item_definition_index(static_cast<std::uint16_t>(i), item.definition)
            || !build_data::find_configured_item_detail(item.definition.definitionIndex, item.detail)) continue;
        item.characterClass = classes::find(item.definition.definitionHash);
        // Non-plug records can carry small scalar values at the legacy category fallback offset.
        item.plug = (item.definition.plugCategoryHash > 0xFFFFU && item.definition.plugCategoryHash != 0xFFFFFFFFU)
            || build_data::is_socket_plug_pooled(item.definition.definitionIndex);
        if (item.detail.equipmentSlot && *item.detail.equipmentSlot >= 0) {
            (void)runtime::detail::semantic_equipment_slot(static_cast<std::uint8_t>(*item.detail.equipmentSlot), item.slot);
            item.kind = item.slot <= 2 ? GearKind::weapon : item.slot <= 7 ? GearKind::armor
                : item.slot == 11 ? GearKind::subclass : item.slot < account::inventory::kEquipmentSlotCount ? GearKind::cosmetic : GearKind::other;
        }
        if (item.slot < account::inventory::kEquipmentSlotCount
            && item.detail.instancedDefinitionState == build_data::items::details::InstancedDefinitionState::instanced) item.plug = false;
        if (auto it = tags.find(item.definition.definitionHash); it != tags.end()
            && reader::read_tag(source, *scope.scratch, it->second, itemStrings)) {
            item.name = resolve(itemStrings, 0x84);
            item.type = resolve(itemStrings, 0x90);
            item.description = resolve(itemStrings, 0x98);
            std::uint16_t icon{};
            if (strings::read(std::span<const std::byte>(itemStrings), 0x80, icon) && icon < iconRows.count)
                (void)strings::read(std::span<const std::byte>(iconTable), iconRows.dataOffset + icon * 0x18 + 0x10, item.iconTag);
        }
        // An ornament may reuse a weapon/armor bucket; it belongs in cosmetics and the perk picker.
        const auto type = searchable(item.type);
        if (type.find("ornament") != std::string::npos || type.find("shader") != std::string::npos
            || type.find("transmat") != std::string::npos || type.find("projection") != std::string::npos) {
            item.kind = GearKind::cosmetic; item.slot = account::inventory::kEquipmentSlotCount; item.plug = true;
        }
        if (item.kind == GearKind::weapon || item.kind == GearKind::armor) {
            auto [preview, added] = previewReferences.try_emplace(item.iconTag, false);
            if (added) {
                std::uint32_t primary{};
                preview->second = reader::read_tag(source, *scope.scratch, item.iconTag, iconDefinition)
                    && strings::read(std::span<const std::byte>(iconDefinition), 0x14, primary)
                    && tables::package_of(primary) != tables::kAbsentPackageId;
            }
            // Placeholder/test definitions can name an icon container with only a background.
            if (!preview->second) { item.internal = true; item.iconTag = 0; }
        }
        for (std::size_t lane = 0; lane < item.detail.ordinarySocketCount; ++lane) {
            auto& pool = item.compatible[lane];
            (void)build_data::visit_socket_plug_pool(item.definition.definitionIndex, static_cast<std::uint8_t>(lane), &pool_member, &pool);
            (void)build_data::visit_socket_roll_pool(item.definition.definitionIndex, static_cast<std::uint8_t>(lane), &pool_member, &pool);
            if (item.detail.initialPlugIndices[lane] != build_data::items::details::kUnavailableItemIndex)
                pool.push_back(item.detail.initialPlugIndices[lane]);
        }
        result.items.push_back(std::move(item));
        progress.store(static_cast<unsigned>((i + 1) * 100 / count));
    }
    if (cancel.load()) { error = "Catalog loading cancelled."; return false; }
    // Sundial's parallel subclass displays provide localized ability and path names.
    std::vector<std::byte> root, listIndex, displays, list, displayRecord, abilityDisplay;
    std::uint32_t tag{}; tables::Array listRows{}, displayRows{};
    if (tables::child_tag(globals, 0, tag) && reader::read_tag(source, *scope.scratch, tag, root)
        && tables::slot_tag(root, 97, tag) && reader::read_tag(source, *scope.scratch, tag, listIndex)
        && tables::find_array_at(listIndex, 8, listRows)
        && tables::child_tag(globals, 61, tag) && reader::read_tag(source, *scope.scratch, tag, displays)
        && tables::find_array_at(displays, 8, displayRows)) {
        for (auto& item : result.items) if (item.kind == GearKind::subclass) {
            const auto id = item.detail.socketEntryListIndex;
            if (id >= 1 && id <= 3) item.characterClass = 1;
            else if (id >= 5 && id <= 7) item.characterClass = 0;
            else if (id >= 9 && id <= 11) item.characterClass = 2;
            else continue;
            if (item.name.empty()) {
                switch (item.definition.definitionHash) {
                case 0x4F91DC97U: item.name = "Arcstrider"; break;
                case 0xC99B33E9U: item.name = "Sentinel"; break;
                case 0xB0554739U: item.name = "Striker"; break;
                case 0xB920CE9AU: item.name = "Sunbreaker"; break;
                case 0xD8B8D1FCU: item.name = "Gunslinger"; break;
                case 0xC0483D8BU: item.name = "Nightstalker"; break;
                case 0xCF88FEA5U: item.name = "Dawnblade"; break;
                case 0x686A154AU: item.name = "Stormcaller"; break;
                case 0xE7BC88B0U: item.name = "Voidwalker"; break;
                }
            }
            tables::IndexRow row{}; tables::Array entries{};
            if (!tables::index_row(listIndex, listRows, id, row) || !reader::read_tag(source, *scope.scratch, row.targetTag, list)
                || !tables::find_array_at(list, 16, entries) || entries.count > 36
                || entries.count > (list.size() - entries.dataOffset) / 64
                || !tables::index_row(displays, displayRows, id, row) || !reader::read_tag(source, *scope.scratch, row.targetTag, displayRecord)) continue;
            std::unordered_map<std::uint32_t, std::string> names;
            std::vector<std::uint32_t> localTables;
            for (std::size_t at = 16; at + 4 <= displayRecord.size(); at += 4) {
                std::uint32_t displayTag{}, displayHash{}, cls{}, localTable{};
                if (!strings::read(std::span<const std::byte>(displayRecord), at, displayTag)
                    || tables::package_of(displayTag) == tables::kAbsentPackageId
                    || !reader::read_tag(source, *scope.scratch, displayTag, abilityDisplay, cls) || cls != 0x80805C49U
                    || !strings::read(std::span<const std::byte>(displayRecord), at - 16, displayHash)) continue;
                names[displayHash] = resolve(abilityDisplay, 160);
                if (strings::read(std::span<const std::byte>(abilityDisplay), 160, localTable)) localTables.push_back(localTable);
            }
            struct Entry { AbilityChoice choice; std::uint32_t source{}; std::uint8_t group{}; };
            std::vector<Entry> options;
            for (std::size_t i = 0; i < entries.count; ++i) {
                const auto base = entries.dataOffset + i * 64; std::uint32_t displayHash{};
                Entry entry; entry.choice.entry = static_cast<std::uint8_t>(i);
                (void)strings::read(std::span<const std::byte>(list), base, displayHash);
                (void)strings::read(std::span<const std::byte>(list), base + 8, entry.source);
                (void)strings::read(std::span<const std::byte>(list), base + 12, entry.group);
                entry.choice.name = names[displayHash];
                if (entry.choice.name.empty()) entry.choice.name = "Ability " + std::to_string(i);
                options.push_back(std::move(entry));
            }
            for (auto i : {4U,5U,6U}) if (i < options.size()) item.abilities[0].push_back(options[i].choice);
            for (auto i : {7U,8U,9U}) if (i < options.size()) item.abilities[1].push_back(options[i].choice);
            for (auto i : {2U,3U}) if (i < options.size()) item.abilities[4].push_back(options[i].choice);
            std::vector<std::uint32_t> sources;
            for (const auto& entry : options) if (entry.group == 3 && entry.source != account::inventory::kNoDefinitionHash
                && std::find(sources.begin(), sources.end(), entry.source) == sources.end()) sources.push_back(entry.source);
            constexpr std::uint32_t pathHashes[]{0xDF417340U,0x730873A5U,0x761AF51AU};
            constexpr const char* pathFallback[]{"Top path","Bottom path","Middle path"};
            for (std::size_t p = 0; p < sources.size() && p < 3; ++p) {
                SubclassPath path; path.name = pathFallback[p];
                for (auto local : localTables) if (auto bank = cache.find(local); bank != cache.end())
                    if (auto name = bank->second.find(pathHashes[p]); name != bank->second.end()) { path.name = name->second; break; }
                const bool retainedSuper = item.definition.definitionHash == 0x4F91DC97U || item.definition.definitionHash == 0xC99B33E9U;
                path.super = p == 2 && !retainedSuper ? 20 : 10;
                bool first = true;
                for (const auto& entry : options) if (entry.group == 3 && entry.source == sources[p]) {
                    path.perks.push_back(entry.choice.name);
                    if (first && entry.choice.entry != 20) { path.melee = entry.choice.entry; first = false; }
                }
                if (first || path.super >= options.size()) continue;
                item.abilities[3].push_back(options[path.melee].choice);
                if (std::none_of(item.abilities[2].begin(), item.abilities[2].end(), [&](const auto& choice) { return choice.entry == path.super; }))
                    item.abilities[2].push_back(options[path.super].choice);
                item.paths.push_back(std::move(path));
            }
        }
    }
    result.finish();
    output = std::move(result);
    return true;
}
}
