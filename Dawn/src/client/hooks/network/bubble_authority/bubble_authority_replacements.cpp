#include "bubble_authority_replacements.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../../state/activity/forced/activity_forced_destination.h"
#include "../coordinator/network_call_coordinator.h"
#include "../platform.h"
#include "scope/bubble_authority_scope.h"

namespace dawn::client::hooks::network::bubble_authority {
namespace {

/** Log the decoder and the forced arm once each. Both run on every roster message. */
std::atomic_bool g_decoderSeen{false};
std::atomic_bool g_forcedSeen{false};
std::atomic_bool g_authorityReadyForcedSeen{false};
std::atomic_uint32_t g_epochObservations{};

/** Pinned Season of Arrivals accessor used by the roster-prefix epoch gate. */
constexpr std::uintptr_t kActivityEpochAccessorRva = 0x41B620U;
constexpr std::array<std::byte, 6> kActivityEpochAccessorPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}};
/** The decoder retains the echoed 128-bit epoch in its roster container. */
constexpr std::size_t kRosterEpochOffset = 0x111F8;
/** Native authority-container readiness byte checked immediately after the roster schema. */
constexpr std::size_t kRosterAuthorityReadyOffset = 0x10E7F;
/** Native bit-reader count used to prove where a rejected message stopped. */
constexpr std::size_t kConsumedBitsOffset = 0x24;
constexpr std::uint32_t kMaximumEpochObservations = 12;

/**
 * Logs the first roster decode and the first forced authority read. The forced read is what makes
 * message 5's per-bubble authority arm apply at all. Without it the client keeps its own build
 * state and only the unusable bubble applies.
 * @param stage Which event this is.
 */
void report_once(const char* stage) noexcept {
    std::array<char, 96> line{};
    const int written =
        std::snprintf(line.data(), line.size(), "ev=bubbleauth stage=%s result=ok", stage);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

using Decoder = bool(__fastcall*)(void*, void*, void*);
using ContentUntracked = bool(__fastcall*)();
using ActivityEpochAccessor = const std::byte*(__fastcall*)();

/** Resolves the exact native accessor only when the pinned runtime prefix still matches. */
[[nodiscard]] ActivityEpochAccessor activity_epoch_accessor() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kActivityEpochAccessorRva;
    for (std::size_t index = 0; index < kActivityEpochAccessorPrefix.size(); ++index) {
        if (target[index] != kActivityEpochAccessorPrefix[index]) {
            return nullptr;
        }
    }
    return reinterpret_cast<ActivityEpochAccessor>(target);
}

/** @return True while a forced private authored mission uses Dawn's authority roster. */
[[nodiscard]] bool opening_is_forced(std::string_view& package) noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall"
               || package == "mission_scot");
}

/** Logs both operands of the native roster-prefix epoch comparison. */
void report_epoch_gate(std::uint32_t observation,
                       bool result,
                       const void* roster,
                       const void* bitStream,
                       std::string_view package) noexcept {
    std::uint64_t echoedFirst = 0;
    std::uint64_t echoedSecond = 0;
    std::uint64_t expectedFirst = 0;
    std::uint64_t expectedSecond = 0;
    std::uint32_t consumedBits = 0;
    bool accessorOk = false;
    __try {
        const auto* rosterBytes = static_cast<const std::byte*>(roster);
        std::memcpy(&echoedFirst, rosterBytes + kRosterEpochOffset, sizeof echoedFirst);
        std::memcpy(&echoedSecond,
                    rosterBytes + kRosterEpochOffset + sizeof echoedFirst,
                    sizeof echoedSecond);
        const auto* streamBytes = static_cast<const std::byte*>(bitStream);
        std::memcpy(&consumedBits, streamBytes + kConsumedBitsOffset, sizeof consumedBits);
        const ActivityEpochAccessor accessor = activity_epoch_accessor();
        const std::byte* expected = accessor != nullptr ? accessor() : nullptr;
        if (expected != nullptr) {
            std::memcpy(&expectedFirst, expected, sizeof expectedFirst);
            std::memcpy(&expectedSecond, expected + sizeof expectedFirst, sizeof expectedSecond);
            accessorOk = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        accessorOk = false;
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bubbleauth stage=epoch_gate_probe n=%u result=%u bits=%u echoed_first=0x%016llX echoed_second=0x%016llX expected_first=0x%016llX expected_second=0x%016llX expected_ok=%u forced=%.*s",
        observation,
        result ? 1U : 0U,
        consumedBits,
        static_cast<unsigned long long>(echoedFirst),
        static_cast<unsigned long long>(echoedSecond),
        static_cast<unsigned long long>(expectedFirst),
        static_cast<unsigned long long>(expectedSecond),
        accessorOk ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (written > 0) {
        const std::size_t length =
            static_cast<std::size_t>(written) < line.size()
                ? static_cast<std::size_t>(written)
                : line.size() - 1;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), length});
    }
}

/**
 * Restores the retired activity-authority container's completion latch for archived missions.
 * The pinned client checks this byte after the roster delta and otherwise skips every phase-two
 * object block without treating the packet as an error. The normal container constructor writes
 * the same value after it finishes; the archived activity no longer reaches that constructor.
 */
[[nodiscard]] bool force_authority_ready(void* roster, std::string_view package) noexcept {
    if (roster == nullptr) {
        return false;
    }
    bool forced = false;
    __try {
        auto* const bytes = static_cast<std::byte*>(roster);
        if (bytes[kRosterAuthorityReadyOffset] == std::byte{0}) {
            bytes[kRosterAuthorityReadyOffset] = std::byte{1};
            forced = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (forced && !g_authorityReadyForcedSeen.exchange(true, std::memory_order_relaxed)) {
        std::array<char, 144> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bubbleauth stage=authority_ready result=forced offset=0x%zX activity=%.*s",
            kRosterAuthorityReadyOffset,
            static_cast<int>(package.size()),
            package.data());
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return true;
}

/** Bubble masks on the roster container (placed-content doc): granted and pending-seed. */
constexpr std::size_t kRosterMaskAOffset = 0x10EB8;
constexpr std::size_t kRosterMaskBOffset = 0x10EC4;
std::atomic_uint64_t g_lastBubbleMasks{~0ULL};

/**
 * Logs the first 32 bubbles of mask A (host granted authority) and mask B (content not yet
 * seeded) whenever either changes. Mask A bit b set and mask B bit b clear is the state that
 * lets the commit tick sweep bubble b's network-replicated placed content into existence;
 * B stuck set means ClientRosterSync_AllRecordsInBubbleSeeded is vetoing the sweep.
 */
__declspec(noinline) void sample_bubble_masks(const void* roster) noexcept {
    if (roster == nullptr) {
        return;
    }
    std::uint32_t maskA = 0;
    std::uint32_t maskB = 0;
    __try {
        std::memcpy(&maskA, static_cast<const std::byte*>(roster) + kRosterMaskAOffset,
                    sizeof maskA);
        std::memcpy(&maskB, static_cast<const std::byte*>(roster) + kRosterMaskBOffset,
                    sizeof maskB);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    const std::uint64_t packed = (std::uint64_t{maskA} << 32U) | maskB;
    if (g_lastBubbleMasks.exchange(packed, std::memory_order_relaxed) == packed) {
        return;
    }
    std::array<char, 112> line{};
    const int written = std::snprintf(line.data(), line.size(),
                                      "ev=bubbleauth stage=masks mask_a=0x%08X mask_b=0x%08X",
                                      maskA, maskB);
    if (written > 0) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Runs the native roster-prefix decoder inside one thread-local authority scope.
 * @param roster Client roster container borrowed by the native decoder.
 * @param bitStream Native bit reader borrowed for this call.
 * @param event Native activity message storage borrowed for this call.
 * @return The native decoder result, or false when there is no original to call.
 */
__declspec(noinline) bool __fastcall decoder_body(void* roster,
                                                  void* bitStream,
                                                  void* event) noexcept {
    coordinator::CallLease lease{};
    coordinator::g_callIngress(
        lease, HookSlot::bubbleAuthorityDecoder, coordinator::ConsumerKind::none);
    const auto call = reinterpret_cast<Decoder>(lease.original);
    const bool scoped = lease.accepting && call != nullptr;
    bool result{};
    if (scoped) {
        scope::enter();
        if (!g_decoderSeen.exchange(true, std::memory_order_relaxed)) {
            report_once("decode");
        }
    }
    __try {
        if (call != nullptr) {
            result = call(roster, bitStream, event);
        }
    } __finally {
        if (scoped) {
            scope::leave();
        }
        coordinator::g_callEgress();
    }
    std::string_view package{};
    if (opening_is_forced(package)) {
        if (result) {
            (void)force_authority_ready(roster, package);
        }
        sample_bubble_masks(roster);
        const std::uint32_t observation =
            g_epochObservations.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (observation <= kMaximumEpochObservations) {
            report_epoch_gate(observation, result, roster, bitStream, package);
        }
    }
    return result;
}

/**
 * Keeps the native build-state read, and forces it true only on the scoped decoder thread.
 * @return Native state, or true only inside an admitted authority decoder call.
 */
__declspec(noinline) bool __fastcall content_untracked_body() noexcept {
    coordinator::CallLease lease{};
    coordinator::g_callIngress(
        lease, HookSlot::contentUntrackedGetter, coordinator::ConsumerKind::none);
    const auto call = reinterpret_cast<ContentUntracked>(lease.original);
    bool result{};
    __try {
        if (call != nullptr) {
            result = call();
        }
        const bool forced = !result && scope::active();
        result = result || forced;
        if (forced && !g_forcedSeen.exchange(true, std::memory_order_relaxed)) {
            report_once("force");
        }
    } __finally {
        coordinator::g_callEgress();
    }
    return result;
}

} // namespace

/** @return The internal-linkage decoder body, kept safe while the detour is removed. */
void* decoder_entry_point() noexcept {
    return reinterpret_cast<void*>(&decoder_body);
}

/** @return The internal-linkage getter body, kept safe while the detour is removed. */
void* content_untracked_entry_point() noexcept {
    return reinterpret_cast<void*>(&content_untracked_body);
}

} // namespace dawn::client::hooks::network::bubble_authority
