#pragma once
#include "adventure_cue_authority.h"
#include "retained_authority_scope.h"

namespace dawn::middleware::bap::activity_message::native::music {
inline constexpr std::uint32_t kSchema = 0x80804F58;
inline constexpr std::size_t kBits = 7223, kDecodedBytes = 0x418, kCapacity = 128;
using Reference = cue::Reference;
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{}, candidateCount{};
    std::uint32_t scope{UINT32_MAX};
    std::array<std::uint32_t, 4> active{};
    std::array<Reference, kCapacity> guards{};
    Reference readiness{};
    friend bool operator==(const Request &, const Request &) = default;
};
[[nodiscard]] inline bool valid(const Request &r) noexcept {
    if (!r.registry || r.registry == UINT32_MAX || r.registry == cue::kAbsent || r.slot > 32767 || !r.candidateCount ||
        r.candidateCount > kCapacity || (r.scope != UINT32_MAX && r.scope > 63) || !cue::valid(r.readiness))
        return false;
    for (std::size_t i = 0; i < kCapacity; ++i) {
        const auto &g = r.guards[i];
        if (i >= r.candidateCount && ((r.active[i / 32] >> (i % 32) & 1U) || g != Reference{}))
            return false;
        if (!cue::valid(g) ||
            (g != Reference{} && (g.registry != r.registry ||
                                  (g.type != 5 && g.type != 33 && g.type != 34 && g.type != 57 && g.type != 60))))
            return false;
    }
    return r.readiness == Reference{} || r.readiness.registry == r.registry;
}
[[nodiscard]] inline bool select(Request &r, std::size_t ordinal) noexcept {
    if (!valid(r) || ordinal >= r.candidateCount)
        return false;
    r.active = {};
    r.active[ordinal / 32] = std::uint32_t{1} << (ordinal % 32);
    return true;
}
template <class Writer> [[nodiscard]] bool write(Writer &w, const Request &r) noexcept {
    if (!valid(r))
        return false;
    for (auto mask : r.active)
        if (!w.write(mask, 32))
            return false;
    const auto reference = [&](const Reference &ref) {
        return w.write(ref.registry, 32) && w.write(ref.type == UINT8_MAX ? 0U : unsigned(ref.type) + 1U, 7) &&
               w.write(ref.slot == UINT16_MAX ? 32767U : unsigned(ref.slot) + 32768U, 16);
    };
    for (const auto &ref : r.guards)
        if (!reference(ref))
            return false;
    return reference(r.readiness);
}
using Decoded = std::array<std::byte, kDecodedBytes>;
[[nodiscard]] inline Decoded decoded(const Request &r) noexcept {
    Decoded out{};
    if (!valid(r))
        return out;
    std::memcpy(out.data(), r.active.data(), 16);
    const auto put = [&](std::size_t at, const Reference &ref) {
        std::memcpy(out.data() + at, &ref.registry, 4);
        out[at + 4] = std::byte{ref.type};
        std::memcpy(out.data() + at + 6, &ref.slot, 2);
    };
    for (std::size_t i = 0; i < kCapacity; ++i)
        put(16 + i * 8, r.guards[i]);
    put(0x410, r.readiness);
    return out;
}
struct Batch final {
    std::array<Request, 4> entries{};
    std::size_t count{};
};
[[nodiscard]] inline const Request *find(const Batch &b, std::uint32_t key, std::uint8_t type,
                                         std::uint16_t slot) noexcept {
    if (type != 11 || b.count > b.entries.size())
        return nullptr;
    for (std::size_t i = 0; i < b.count; ++i)
        if (b.entries[i].registry == key && b.entries[i].slot == slot)
            return &b.entries[i];
    return nullptr;
}
template <class Roster> [[nodiscard]] bool valid(const Batch &b, const Roster &roster, std::uint32_t region) noexcept {
    if (b.count > b.entries.size() || roster.groupCount > roster.groups.size() ||
        roster.topLevelGroupCount > roster.groupCount)
        return false;
    for (std::size_t g = 0; g < roster.groupCount; ++g) {
        const auto &row = roster.groups[g];
        if (row.slotTypes.size() != row.slotIndices.size() || row.slotFlags.size() != row.slotIndices.size())
            return false;
    }
    for (std::size_t i = 0; i < b.count; ++i) {
        const auto &r = b.entries[i];
        if (!valid(r))
            return false;
        if (r.scope == UINT32_MAX) {
            if (roster.bubbleSubBlocks.size() > 64)
                return false;
            for (const auto &block : roster.bubbleSubBlocks) {
                if (block.bubble > 63 || block.keys.size() > 96
                    || (!block.presence.empty() && block.presence.size() != block.keys.size()))
                    return false;
                for (const auto present : block.presence)
                    if (present > 1) return false;
                if constexpr (requires { block.states; }) {
                    if (!block.states.empty() && block.states.size() != block.keys.size())
                        return false;
                    for (const auto state : block.states)
                        if (state < 0x80U) return false;
                }
                for (const auto key : block.keys)
                    if (key == r.registry) return false;
            }
            if (roster.topLevelKeys.empty()) {
                if (!roster.topLevelPresence.empty() || !roster.topLevelStates.empty())
                    return false;
            } else {
                if ((!roster.topLevelPresence.empty()
                        && roster.topLevelPresence.size() != roster.topLevelKeys.size())
                    || (!roster.topLevelStates.empty()
                        && roster.topLevelStates.size() != roster.topLevelKeys.size()))
                    return false;
                for (const auto present : roster.topLevelPresence)
                    if (present > 1) return false;
                for (const auto state : roster.topLevelStates)
                    if (state < 0x80U) return false;
                unsigned keyMatches{};
                std::size_t keyOrdinal{};
                for (std::size_t k = 0; k < roster.topLevelKeys.size(); ++k) {
                    if (roster.topLevelKeys[k] != r.registry) continue;
                    keyOrdinal = k;
                    ++keyMatches;
                }
                if (keyMatches != 1 || (!roster.topLevelPresence.empty()
                    && roster.topLevelPresence[keyOrdinal] != 1))
                    return false;
            }
        } else if (!authority_scope::valid(roster, r.registry, r.scope, region)) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j)
            if (b.entries[j].registry == r.registry && b.entries[j].slot == r.slot)
                return false;
        unsigned matches{};
        for (std::size_t g = 0; g < roster.groupCount; ++g) {
            const auto &row = roster.groups[g];
            if (row.key != r.registry)
                continue;
            if ((r.scope == UINT32_MAX ? g >= roster.topLevelGroupCount : g < roster.topLevelGroupCount)
                || row.slotTypes.size() != row.slotIndices.size() ||
                row.slotFlags.size() != row.slotIndices.size())
                return false;
            for (std::size_t s = 0; s < row.slotIndices.size(); ++s)
                if (row.slotIndices[s] == r.slot) {
                    if (row.slotTypes[s] != 11 || !(row.slotFlags[s] & 2))
                        return false;
                    ++matches;
                }
        }
        if (matches != 1)
            return false;
        const auto readable = [&](const Reference &ref) {
            if (ref == Reference{})
                return true;
            unsigned n{};
            for (std::size_t g = 0; g < roster.groupCount; ++g) {
                const auto &row = roster.groups[g];
                if (row.key != ref.registry)
                    continue;
                for (std::size_t s = 0; s < row.slotIndices.size(); ++s)
                    if (row.slotIndices[s] == ref.slot) {
                        if (row.slotTypes[s] != ref.type || !(row.slotFlags[s] & 1))
                            return false;
                        ++n;
                    }
            }
            return n == 1;
        };
        if (!readable(r.readiness))
            return false;
        for (const auto &guard : r.guards)
            if (!readable(guard))
                return false;
    }
    return true;
}
} // namespace dawn::middleware::bap::activity_message::native::music
