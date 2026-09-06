#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::client::hooks::bootflow::omega_boss_graph_observation {

// F4E660 consumes dt in XMM0, arg2 in RDX, this state in R8 and an optional
// bool* transition output in R9. Observe only after the original returns.
inline constexpr std::uintptr_t kUpdateRva = 0xF4E660;
inline constexpr std::size_t kGraphBytes = 0xB8;
inline constexpr std::uint32_t kGraphAsset = 0x80F45178U;
inline constexpr std::uint32_t kBankAsset = 0x80F45190U;
inline constexpr std::uint32_t kGroup = 0xAFB11A12U;
inline constexpr std::uint32_t kSequence = 0x65D2379FU;
inline constexpr std::uint32_t kInvalid = 0xFFFFFFFFU;

enum class Phase : std::uint8_t { unknown, leadIn, fly, hover, summon, idle };

struct Snapshot {
    std::uint32_t entity{kInvalid}, character{kInvalid}, biped{kInvalid};
    std::uint32_t graphAsset{kInvalid}, clip{kInvalid};
    std::int32_t requestedNode{-1}, requestedGraph{-1};
    std::int32_t loadedNode{-1}, loadedGraph{-1}, bankRow{-1};
    float playbackTime{}, playbackLimit{};
    bool active{}, loop{}, nativeResult{};
    Phase phase{Phase::unknown};
};

template<class T>
inline T field(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    if (offset <= bytes.size() && sizeof value <= bytes.size() - offset)
        std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

// This is the bank-row branch consumed by C886C0 and C858C0/C84EB0. The
// caller supplies a fresh bounded copy of the exact biped's +D0 bank resource.
inline bool bank_clip(std::span<const std::byte> bank, std::int32_t row,
                      std::uint32_t& clip) noexcept {
    if (bank.size() < 0x88 || row < 0
        || field<std::uint64_t>(bank, 0x68) != 18
        || field<std::uint64_t>(bank, 0x08) != 25
        || field<std::uint32_t>(bank, 0x80) != kGraphAsset) return false;
    const auto rowsRelative = field<std::uint64_t>(bank, 0x70);
    const auto clipsRelative = field<std::uint64_t>(bank, 0x10);
    if (row >= 18 || rowsRelative > bank.size() - 0x80
        || clipsRelative > bank.size() - 0x20) return false;
    const auto rowOffset = std::size_t{0x80} + static_cast<std::size_t>(rowsRelative)
        + static_cast<std::size_t>(row) * 0x20;
    if (rowOffset > bank.size() || 0x20 > bank.size() - rowOffset) return false;
    const auto clipIndex = field<std::int16_t>(bank, rowOffset + 0x18);
    if (clipIndex < 0 || clipIndex >= 25) return false;
    const auto clipOffset = std::size_t{0x20} + static_cast<std::size_t>(clipsRelative)
        + static_cast<std::size_t>(clipIndex) * 4;
    if (clipOffset > bank.size() || 4 > bank.size() - clipOffset) return false;
    clip = field<std::uint32_t>(bank, clipOffset);
    return clip != kInvalid;
}

// F4E980..997 commit loaded graph/node only after playback initialization.
// F54BF0 checks playback+9; F50280 reads playback+8 loop and +24 phase.
// Loaded node is intentionally distinct from the next requested node.
inline bool decode(std::span<const std::byte> graph,
                   std::span<const std::byte> bank,
                   bool nativeResult, Snapshot& out) noexcept {
    out = {};
    if (graph.size() < kGraphBytes) return false;
    // F48F70/F4905F stores world entity at +0; F57250 initializes playback
    // with that same world entity. Neither field contains the AI actor handle.
    out.entity = field<std::uint32_t>(graph, 0x00);
    out.character = field<std::uint32_t>(graph, 0x04);
    out.graphAsset = field<std::uint32_t>(graph, 0x10);
    out.biped = field<std::uint32_t>(graph, 0x14);
    out.active = field<std::uint8_t>(graph, 0x21) != 0;
    out.loop = (field<std::uint8_t>(graph, 0x20) & 1) != 0;
    out.bankRow = field<std::int32_t>(graph, 0x30);
    out.playbackLimit = field<float>(graph, 0x38);
    out.playbackTime = field<float>(graph, 0x3C);
    out.requestedNode = field<std::int32_t>(graph, 0xA4);
    out.requestedGraph = field<std::int32_t>(graph, 0xA8);
    out.loadedNode = field<std::int32_t>(graph, 0xB0);
    out.loadedGraph = field<std::int32_t>(graph, 0xB4);
    out.nativeResult = nativeResult;
    if (!nativeResult || !out.active || out.graphAsset != kGraphAsset
        || out.entity == kInvalid || out.character == kInvalid || out.biped == kInvalid
        || out.loadedGraph != 0 || out.requestedGraph != 0
        || out.loadedNode < 0 || out.loadedNode >= 5
        || out.requestedNode < 0 || out.requestedNode >= 5
        || !std::isfinite(out.playbackTime) || out.playbackTime < 0.F
        || !std::isfinite(out.playbackLimit) || out.playbackLimit <= 0.F
        || out.playbackTime > out.playbackLimit + 0.001F
        || field<std::uint32_t>(graph, 0x18) != out.entity
        || field<std::uint32_t>(graph, 0x1C) != out.biped
        || field<std::uint32_t>(graph, 0x28) != kInvalid
        || field<std::uint32_t>(graph, 0x2C) != kInvalid
        || !bank_clip(bank, out.bankRow, out.clip)) return false;
    constexpr std::array rows{2, 5, 1, 1, 13};
    constexpr std::array<std::uint32_t, 5> clips{
        0x80F4517DU, 0x80F1FD8CU, 0x80F4517CU, 0x80F4517CU, 0x80F45188U};
    constexpr std::array phases{Phase::leadIn, Phase::fly, Phase::idle,
                                Phase::hover, Phase::summon};
    constexpr std::array loops{true, false, true, true, false};
    const auto node = static_cast<std::size_t>(out.loadedNode);
    if (out.bankRow != rows[node] || out.clip != clips[node] || out.loop != loops[node])
        return false;
    out.phase = phases[node];
    return true;
}

// Owner must be freshly revalidated against the live member, character,
// animation backlinks, biped self and exact issued queue in the native bridge.
// Equality includes the run, member, full actor, generation and revision.
// Neither a same-index recycled handle nor a graph receipt alone establishes
// command ownership. No pointer from these observations survives its callback.
template<class Owner>
inline bool belongs_to(const Snapshot& graph, const Owner& current,
                       const Owner& issued, std::uint32_t issuedGroup,
                       std::uint32_t issuedSequence) noexcept {
    return current == issued && current.run != 0 && current.generation != 0
        && current.member != kInvalid && current.parent != kInvalid
        && current.character != kInvalid && current.animation != kInvalid
        && current.biped != kInvalid && current.actor != kInvalid && current.entity != kInvalid
        && current.parent != current.character && current.character != current.animation
        && current.parent != current.animation
        && issuedGroup == kGroup && issuedSequence == kSequence
        && graph.nativeResult && graph.active && graph.graphAsset == kGraphAsset
        && graph.loadedGraph == 0 && graph.phase != Phase::unknown
        && graph.entity == current.entity && graph.character == current.character
        && graph.biped == current.biped;
}

} // namespace sunrise::client::hooks::bootflow::omega_boss_graph_observation
