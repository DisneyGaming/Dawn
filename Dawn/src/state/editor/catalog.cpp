// SPDX-License-Identifier: GPL-3.0-only
// Selection scopes follow Sundial by KyleThmpsn. See vendor/sundial/NOTICE.md.
#include "catalog.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace dawn::state::editor {
std::string searchable(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}
bool matches(const CatalogItem& item, const std::string& query) {
    std::size_t begin = 0;
    while (begin < query.size()) {
        const auto end = query.find(' ', begin);
        const auto word = query.substr(begin, end == std::string::npos ? end : end - begin);
        if (!word.empty() && item.search.find(word) == std::string::npos) return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}
bool fits_class(const CatalogItem& item, CharacterClass characterClass) noexcept {
    return item.characterClass == 3 || item.characterClass == static_cast<std::uint8_t>(characterClass);
}
const CatalogItem* Catalog::find(std::uint32_t hash) const noexcept {
    const auto it = hashes.find(hash);
    return it == hashes.end() ? nullptr : &items[it->second];
}
const CatalogItem* Catalog::index(std::uint16_t id) const noexcept {
    const auto it = indices.find(id);
    return it == indices.end() ? nullptr : &items[it->second];
}
namespace {
void unique(std::vector<std::uint16_t>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
}
void Catalog::finish() {
    hashes.clear(); indices.clear(); socketPools.clear(); plugs.clear();
    for (auto& pool : gearPools) pool.clear();
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto& item = items[i];
        hashes[item.definition.definitionHash] = i;
        indices[item.definition.definitionIndex] = i;
        char hash[40]{};
        std::snprintf(hash, sizeof hash, "0x%08X %u", item.definition.definitionHash, item.definition.definitionHash);
        item.internal = item.internal || item.name.empty()
            || ((item.kind == GearKind::weapon || item.kind == GearKind::armor) && item.type.empty());
        if (item.name.empty()) item.name = std::string(item.plug ? "Unnamed perk " : "Unnamed item ") + hash;
        item.search = searchable(item.name + " " + item.type + " " + item.description + " " + hash);
        if (item.plug) plugs.push_back(item.definition.definitionIndex);
        for (std::size_t lane = 0; lane < item.detail.ordinarySocketCount; ++lane) {
            auto& pool = item.compatible[lane];
            unique(pool);
            const auto type = item.detail.socketTypes[lane];
            if (type != build_data::items::details::kUnavailableSocketType) {
                auto& socket = socketPools[type];
                socket.insert(socket.end(), pool.begin(), pool.end());
                auto& combined = socketPools[0x10000U + (static_cast<std::uint32_t>(item.kind) << 16U) + type];
                combined.insert(combined.end(), pool.begin(), pool.end());
            }
            auto& gear = gearPools[static_cast<std::size_t>(item.kind)];
            gear.insert(gear.end(), pool.begin(), pool.end());
        }
    }
    for (auto& [key, values] : socketPools) { (void)key; unique(values); }
    for (auto& pool : gearPools) unique(pool);
    // Pools also expose unnamed/internal plugs that declare no category of their own.
    for (const auto& pool : gearPools) plugs.insert(plugs.end(), pool.begin(), pool.end());
    unique(plugs);
    for (auto id : plugs) if (auto it = indices.find(id); it != indices.end()) items[it->second].plug = true;
}
std::vector<std::uint16_t> Catalog::candidates(const CatalogItem& item, std::size_t lane, PlugScope scope) const {
    if (lane >= item.detail.ordinarySocketCount || lane >= item.compatible.size()) return {};
    if (scope == PlugScope::all) return plugs;
    if (scope == PlugScope::compatible) return item.compatible[lane];
    if (scope == PlugScope::gear) return gearPools[static_cast<std::size_t>(item.kind)];
    const auto type = item.detail.socketTypes[lane];
    if (type == build_data::items::details::kUnavailableSocketType) return item.compatible[lane];
    const auto key = scope == PlugScope::socket ? type
        : 0x10000U + (static_cast<std::uint32_t>(item.kind) << 16U) + type;
    const auto it = socketPools.find(key);
    return it == socketPools.end() ? std::vector<std::uint16_t>{} : it->second;
}
} // namespace dawn::state::editor
