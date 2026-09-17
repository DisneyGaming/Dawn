#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <span>

#include "../../../core/logging/log.h"
#include "../../../state/activity/events/activity_event_selection.h"
#include "../../../state/build_data/vendors/vendor_catalog.h"
#include "../../memory/current_process_memory.h"
#include "../../targets/game/content.h"
#include "../handles/handle_resolver.h"

namespace sunrise::client::content::vendors {

/** Eva's duplicate offers share event flag 950 with her bounties. Keep that flag intact
 * and adapt only the duplicate offers' presentation predicates in the loaded catalogue.
 * No purchase rows, package files, bounty predicates or executable code are changed. */
inline void service_festival_catalogue(std::uint64_t now) noexcept {
    static std::uint64_t next{};
    if (now < next || !targets::game::content::is_resolved()) return;
    next = now + 1000;
    namespace catalog = state::build_data::vendors;
    catalog::Definition definition{};
    if (!catalog::find(0x36D32C3CU, definition) || definition.definitionTag != 0x81319082U
        || definition.saleCount != 389) return;
    const handles::Source source{
        reinterpret_cast<std::uintptr_t>(targets::game::content::get().contentHandleTablesSlot),
        nullptr, memory::read_current_process};
    std::uintptr_t root{};
    if (!handles::resolve(source, definition.definitionTag, root)) return;
    const auto read = [&]<class T>(std::uintptr_t address, T& out) noexcept {
        return memory::read_current_process(nullptr, address,
            std::as_writable_bytes(std::span(&out, 1)));
    };
    const auto bounded = [&](std::uintptr_t address, std::size_t size) noexcept {
        return address >= root && address - root <= definition.definitionSize
            && size <= definition.definitionSize - (address - root);
    };
    std::uint64_t size{}, count{};
    std::uint32_t hash{};
    std::int64_t relative{};
    if (!read(root, size) || size != definition.definitionSize
        || !read(root + 8, hash) || hash != definition.definitionHash
        || !read(root + 48, count) || count != definition.saleCount
        || !read(root + 56, relative) || relative < 0
        || static_cast<std::uint64_t>(relative) > size) return;
    const auto rows = root + 72 + static_cast<std::uintptr_t>(relative);
    if (!bounded(rows, count * catalog::kSaleRowStride)) return;
    using Program = std::array<std::uint32_t, 6>;
    constexpr Program authored{1, 950, 12, 133, 4, UINT32_MAX};
    // The native expression VM's CONST(0), CONST(0), LESS_THAN is always false.
    constexpr Program hidden{11, 0, 11, 0, 15, UINT32_MAX};
    const bool active = !state::activity::events::withheld(0x7C6DE64FU);
    const auto& desired = active ? hidden : authored;
    std::array<std::uintptr_t, 20> addresses{};
    std::array<Program, 20> previous{};
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        catalog::SaleRow duplicate{}, original{};
        if (!catalog::sale_row(definition, 62 + i, duplicate)
            || !catalog::sale_row(definition, 11 + i, original)
            || duplicate.itemIndex != original.itemIndex
            || duplicate.categoryIndex != original.categoryIndex) return;
        const auto row = rows + (62 + i) * catalog::kSaleRowStride;
        std::uint16_t item{};
        if (!read(row + 70, item) || item != duplicate.itemIndex
            || !read(row + 120, count) || count < 1 || count > 4
            || !read(row + 128, relative) || relative < 0
            || static_cast<std::uint64_t>(relative) > size) return;
        const auto expressions = row + 144 + static_cast<std::uintptr_t>(relative);
        if (!bounded(expressions, count * 16) || !read(expressions, count)
            || count < 3 || count > 8 || !read(expressions + 8, relative)
            || relative < 0 || static_cast<std::uint64_t>(relative) > size) return;
        addresses[i] = expressions + 24 + static_cast<std::uintptr_t>(relative);
        if (!bounded(addresses[i], count * 8) || !read(addresses[i], previous[i])
            || (previous[i] != authored && previous[i] != hidden)) return;
    }
    const auto replace = [&](std::size_t i, const Program& expected, const Program& value) noexcept {
        Program current{};
        SIZE_T written{};
        return read(addresses[i], current) && current == expected
            && WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(addresses[i]),
                value.data(), sizeof(value), &written) && written == sizeof(value);
    };
    bool changed{};
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        if (previous[i] == desired) continue;
        if (!replace(i, previous[i], desired)) {
            bool restored = replace(i, desired, previous[i]);
            for (std::size_t j = 0; j < i; ++j)
                if (previous[j] != desired) restored = replace(j, desired, previous[j]) && restored;
            core::log::write(core::log::Channel::client, core::log::Level::warn,
                restored ? "ev=festival_catalogue result=write_failed rollback=ok"
                         : "ev=festival_catalogue result=write_failed rollback=incomplete");
            return;
        }
        changed = true;
    }
    if (changed) core::log::write(core::log::Channel::client, core::log::Level::info,
        active ? "ev=festival_catalogue result=ok duplicate_offers=20 bounties=preserved"
               : "ev=festival_catalogue result=restored");
}

} // namespace sunrise::client::content::vendors
