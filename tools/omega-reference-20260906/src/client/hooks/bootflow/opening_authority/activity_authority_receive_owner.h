#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../hooking/detour.h"
#include "../../network/lifecycle/sensor_state_heap_full_cohort.h"

namespace sunrise::client::hooks::bootflow::opening_authority::activity_authority_receive_owner {

namespace heap_cohort = sunrise::client::hooks::network::lifecycle::sensor_state_heap_full_cohort;

inline constexpr std::uint32_t kTargetRva = 0x4D7470U;
inline constexpr std::uint32_t kPackedRawOffset = 0x4D6A70U;
inline constexpr std::size_t kPrefixBytes = 16U;
inline constexpr std::uint32_t kFanoutAbiVersion = 1U;
inline constexpr std::size_t kParticipantSpecCount = 1U;

using ImageSha256 = std::array<std::byte, 32U>;
using ReceiveOriginal = std::uint64_t(__fastcall*)(void* sensorTable, void* bitStream);

inline constexpr ImageSha256 kPinnedPackedImageSha256{
    std::byte{0x81U}, std::byte{0x96U}, std::byte{0x43U}, std::byte{0x80U}, std::byte{0x66U},
    std::byte{0x4EU}, std::byte{0x7FU}, std::byte{0xCEU}, std::byte{0xE3U}, std::byte{0xC6U},
    std::byte{0x20U}, std::byte{0x08U}, std::byte{0x5AU}, std::byte{0x15U}, std::byte{0x7FU},
    std::byte{0xDEU}, std::byte{0xAFU}, std::byte{0x91U}, std::byte{0xFEU}, std::byte{0xFAU},
    std::byte{0xCFU}, std::byte{0x72U}, std::byte{0x14U}, std::byte{0x90U}, std::byte{0x78U},
    std::byte{0x20U}, std::byte{0xF1U}, std::byte{0x88U}, std::byte{0xBBU}, std::byte{0xEBU},
    std::byte{0x4CU}, std::byte{0xEDU},
};

inline constexpr std::array<std::byte, kPrefixBytes> kExpectedMappedPrefix{
    std::byte{0x40U},
    std::byte{0x57U},
    std::byte{0x41U},
    std::byte{0x55U},
    std::byte{0x41U},
    std::byte{0x56U},
    std::byte{0x41U},
    std::byte{0x57U},
    std::byte{0xB8U},
    std::byte{0x98U},
    std::byte{0x78U},
    std::byte{0x00U},
    std::byte{0x00U},
    std::byte{0xE8U},
    std::byte{0xAEU},
    std::byte{0x56U},
};

inline constexpr std::array<std::byte, kPrefixBytes> kExpectedPackedPrefix{
    std::byte{0xBCU},
    std::byte{0xA8U},
    std::byte{0x84U},
    std::byte{0x83U},
    std::byte{0xAFU},
    std::byte{0x96U},
    std::byte{0x2EU},
    std::byte{0xC3U},
    std::byte{0xA9U},
    std::byte{0x9AU},
    std::byte{0x1CU},
    std::byte{0x62U},
    std::byte{0x5BU},
    std::byte{0x26U},
    std::byte{0x93U},
    std::byte{0x80U},
};

inline constexpr std::array<std::byte, 16U> kPinnedCodeViewGuid{
    std::byte{0xDFU},
    std::byte{0xFBU},
    std::byte{0xDCU},
    std::byte{0x0DU},
    std::byte{0x68U},
    std::byte{0xEBU},
    std::byte{0x48U},
    std::byte{0x41U},
    std::byte{0x8BU},
    std::byte{0xFBU},
    std::byte{0x7CU},
    std::byte{0x76U},
    std::byte{0x18U},
    std::byte{0xFFU},
    std::byte{0xABU},
    std::byte{0x03U},
};

struct ImageView final {
    std::span<const std::byte> mapped{};
    std::span<const std::byte> packed{};
    ImageSha256 packedSha256{};
    std::uint64_t packedFileSize{};
    std::uint16_t machine{};
    std::uint16_t sectionCount{};
    std::uint32_t timestamp{};
    std::uint32_t imageSize{};
    std::uint32_t entryRva{};
    std::uint32_t checksum{};
    std::array<std::byte, 16U> codeViewGuid{};
    std::uint32_t codeViewAge{};
};

struct ValidatedTarget final {
    void* address{};
    std::array<std::byte, kPrefixBytes> observedMapped{};
    std::array<std::byte, kPrefixBytes> observedPacked{};
    bool valid{};
};

enum class ValidationResult : std::uint8_t {
    valid,
    packed_hash_mismatch,
    packed_size_mismatch,
    pe_mismatch,
    codeview_mismatch,
    target_bounds,
    mapped_prefix_mismatch,
    packed_prefix_mismatch,
};

/** Validates only the pinned image identity and this owner's exact mapped/packed site evidence. */
[[nodiscard]] ValidationResult validate(const ImageView& image, ValidatedTarget& output) noexcept;

enum class BoundaryPhase : std::uint8_t {
    pre,
    post,
};

/** Fixed scalar view; no borrowed game storage is retained by this record. */
struct BoundaryObservation final {
    std::uint64_t ownerGeneration{};
    std::uint64_t aggregateEpoch{};
    std::uint64_t callId{};
    std::uintptr_t sensorTable{};
    std::uintptr_t activityWrapper{};
    std::uintptr_t bitStream{};
    std::uint64_t nativeResult{};
    std::uint32_t producerThreadId{};
    BoundaryPhase phase{BoundaryPhase::pre};
    bool wrapperValid{};
    bool entryGenerationCurrent{};
    bool exitGenerationCurrent{};
    bool nativeResultValid{};
};

using HeapEnter = void (*)(void* context,
                           heap_cohort::ReceiveToken& token,
                           void* sensorTable,
                           void* bitStream) noexcept;
using HeapExit = void (*)(void* context,
                          heap_cohort::ReceiveToken& token,
                          std::uint64_t nativeResult) noexcept;
using OwnerSignal = void (*)(void* context) noexcept;
using OwnerDetachedSignal = void (*)(void* context, bool removed) noexcept;
using SceneBoundaryObserver = void (*)(void* context,
                                       const BoundaryObservation& observation) noexcept;

/**
 * Immutable generation-specific fanout retained until clear_after_removed(). Heap callbacks own
 * their typed receive token; Scene callbacks receive only fixed scalar outer-call metadata. Every
 * callback is a discrete call that must be bounded/nonblocking and must not log, allocate, perform
 * I/O, or acquire a contended lock. No callback invocation spans the native original.
 */
struct FanoutV1 final {
    std::uint32_t abiVersion{kFanoutAbiVersion};
    std::uint32_t structBytes{sizeof(FanoutV1)};
    std::uint64_t publicationGeneration{};
    void* heapContext{};
    HeapEnter heapEnter{};
    HeapExit heapExit{};
    OwnerSignal heapOwnerAttached{};
    OwnerSignal heapOwnerQuiescing{};
    OwnerDetachedSignal heapOwnerDetached{};
    void* sceneContext{};
    SceneBoundaryObserver scenePre{};
    SceneBoundaryObserver scenePost{};
};

/** All pointer storage belongs to the aggregate and must outlive confirmed detour removal. */
struct Publication final {
    ValidatedTarget target{};
    const FanoutV1* fanout{};
    const std::atomic<std::uint64_t>* aggregateEpochSource{};
    std::uint64_t ownerGeneration{};
    std::uint64_t aggregateEpoch{};
};

enum class PrepareResult : std::uint8_t {
    prepared,
    retained_predecessor,
    invalid_target,
    invalid_generation,
    aggregate_already_admitting,
    invalid_fanout,
};

/** Performs every fallible participant publication check before the aggregate transaction. */
[[nodiscard]] PrepareResult prepare_publication(const Publication& publication) noexcept;

/** Appends this participant's one spec; it never begins or commits a Detours transaction. */
[[nodiscard]] bool append_specs(std::span<hooking::detour::Spec> output,
                                std::size_t& used) noexcept;

/** Publishes the aggregate-owned committed trampoline and wakes commit-window callers. */
void publish_original(ReceiveOriginal original) noexcept;

/** Opens only the local gate. The aggregate epoch remains the final all-or-none admission store. */
[[nodiscard]] bool accept() noexcept;

/** Closes new observations while every replacement continues to forward natively exactly once. */
void quiesce() noexcept;

/** Reports a deferred/failed aggregate removal without releasing any generation-owned storage. */
void retain_after_failed_remove() noexcept;

/** @return True only when the complete replacement/fanout call scope is empty. */
[[nodiscard]] bool idle() noexcept;

[[nodiscard]] bool accepting() noexcept;
[[nodiscard]] bool has_ownership() noexcept;

/** Clears a preparation only when no aggregate detour was committed. */
[[nodiscard]] bool cancel_before_attach() noexcept;

/**
 * Called only after the aggregate has confirmed this spec removed. Refuses to clear a live gate or
 * in-flight call; deferred/failed detach must retain every pointer and trampoline for retry.
 */
[[nodiscard]] bool clear_after_removed() noexcept;

/** Address the aggregate must list in its protected-code set for this participant. */
[[nodiscard]] void* replacement_entry() noexcept;

struct Counters final {
    std::uint64_t entered{};
    std::uint64_t forwarded{};
    std::uint64_t admitted{};
    std::uint64_t pairedPost{};
    std::uint64_t staleAtExit{};
    std::uint64_t invalidWrapper{};
    std::uint64_t callIdExhausted{};
};

[[nodiscard]] Counters counters() noexcept;

namespace testing {

/** Unit-only call seam; production ownership is still represented by replacement_entry(). */
[[nodiscard]] std::uint64_t invoke(void* sensorTable, void* bitStream) noexcept;

} // namespace testing

} // namespace sunrise::client::hooks::bootflow::opening_authority::activity_authority_receive_owner
