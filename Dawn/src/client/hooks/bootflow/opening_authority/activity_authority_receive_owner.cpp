#include "activity_authority_receive_owner.h"

#include <cstring>
#include <limits>

#include "../../../hooking/call_gate.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace dawn::client::hooks::bootflow::opening_authority::activity_authority_receive_owner {
namespace {

[[nodiscard]] consteval bool matches_heap_cohort_contract() noexcept {
    for (const heap_cohort::SiteContract& contract : heap_cohort::kSiteManifest) {
        if (contract.rva != kTargetRva) {
            continue;
        }
        if (contract.packedRawOffset != kPackedRawOffset || contract.prefixLength != kPrefixBytes
            || contract.action != heap_cohort::SiteAction::externalOwner) {
            return false;
        }
        for (std::size_t index = 0U; index < kPrefixBytes; ++index) {
            if (contract.expectedMapped[index] != kExpectedMappedPrefix[index]
                || contract.packedRaw[index] != kExpectedPackedPrefix[index]) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static_assert(matches_heap_cohort_contract(),
              "the sole receive owner must share the full-cohort external-owner contract");

enum class ParticipantPhase : std::uint8_t {
    detached,
    prepared,
    published,
    accepting,
    quiescing,
};

hooking::CallGate g_callGate{};
std::atomic<ReceiveOriginal> g_original{};
std::atomic<void*> g_target{};
std::atomic<const FanoutV1*> g_fanout{};
std::atomic<const std::atomic<std::uint64_t>*> g_aggregateEpochSource{};
std::atomic<std::uint64_t> g_ownerGeneration{};
std::atomic<std::uint64_t> g_aggregateEpoch{};
std::atomic<std::uint64_t> g_nextCallId{1U};
std::atomic<ParticipantPhase> g_phase{ParticipantPhase::detached};
std::atomic_bool g_heapAttachPublished{};

std::atomic<std::uint64_t> g_entered{};
std::atomic<std::uint64_t> g_forwarded{};
std::atomic<std::uint64_t> g_admitted{};
std::atomic<std::uint64_t> g_pairedPost{};
std::atomic<std::uint64_t> g_staleAtExit{};
std::atomic<std::uint64_t> g_invalidWrapper{};
std::atomic<std::uint64_t> g_callIdExhausted{};

struct GenerationSnapshot final {
    const FanoutV1* fanout{};
    const std::atomic<std::uint64_t>* aggregateEpochSource{};
    std::uint64_t ownerGeneration{};
    std::uint64_t aggregateEpoch{};
};

[[nodiscard]] bool valid_fanout(const FanoutV1& fanout, std::uint64_t ownerGeneration) noexcept {
    return fanout.abiVersion == kFanoutAbiVersion && fanout.structBytes == sizeof(FanoutV1)
           && fanout.publicationGeneration == ownerGeneration && fanout.heapEnter != nullptr
           && fanout.heapExit != nullptr && fanout.heapOwnerAttached != nullptr
           && fanout.heapOwnerQuiescing != nullptr && fanout.heapOwnerDetached != nullptr
           && fanout.scenePre != nullptr && fanout.scenePost != nullptr;
}

[[nodiscard]] GenerationSnapshot generation_snapshot() noexcept {
    GenerationSnapshot output{};
    output.fanout = g_fanout.load(std::memory_order_acquire);
    output.aggregateEpochSource = g_aggregateEpochSource.load(std::memory_order_acquire);
    output.ownerGeneration = g_ownerGeneration.load(std::memory_order_acquire);
    output.aggregateEpoch = g_aggregateEpoch.load(std::memory_order_acquire);
    return output;
}

[[nodiscard]] bool generation_current(const GenerationSnapshot& snapshot) noexcept {
    return snapshot.fanout != nullptr && snapshot.aggregateEpochSource != nullptr
           && snapshot.ownerGeneration != 0U && snapshot.aggregateEpoch != 0U
           && snapshot.fanout->publicationGeneration == snapshot.ownerGeneration
           && g_fanout.load(std::memory_order_acquire) == snapshot.fanout
           && g_aggregateEpochSource.load(std::memory_order_acquire)
                  == snapshot.aggregateEpochSource
           && g_ownerGeneration.load(std::memory_order_acquire) == snapshot.ownerGeneration
           && g_aggregateEpoch.load(std::memory_order_acquire) == snapshot.aggregateEpoch
           && snapshot.aggregateEpochSource->load(std::memory_order_acquire)
                  == snapshot.aggregateEpoch
           && g_callGate.accepting();
}

[[nodiscard]] std::uint32_t producer_thread_id() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint32_t>(GetCurrentThreadId());
#else
    static std::atomic<std::uint32_t> next{1U};
    thread_local const std::uint32_t current = next.fetch_add(1U, std::memory_order_relaxed);
    return current;
#endif
}

[[nodiscard]] std::uint64_t claim_call_id() noexcept {
    std::uint64_t current = g_nextCallId.load(std::memory_order_relaxed);
    for (;;) {
        if (current == 0U) {
            g_callIdExhausted.fetch_add(1U, std::memory_order_relaxed);
            return 0U;
        }
        const std::uint64_t next =
            current == std::numeric_limits<std::uint64_t>::max() ? 0U : current + 1U;
        if (g_nextCallId.compare_exchange_weak(
                current, next, std::memory_order_relaxed, std::memory_order_relaxed)) {
            if (next == 0U) {
                g_callIdExhausted.fetch_add(1U, std::memory_order_relaxed);
                return 0U;
            }
            return current;
        }
    }
}

[[nodiscard]] std::uint64_t __fastcall receive_replacement(void* sensorTable,
                                                           void* bitStream) noexcept {
    hooking::CallGate::Scope call{g_callGate};
    g_entered.fetch_add(1U, std::memory_order_relaxed);

    const ReceiveOriginal original = hooking::await_original(g_original);
    const GenerationSnapshot generation = generation_snapshot();
    const bool entryCurrent = call.accepts_side_effects() && generation_current(generation);
    const std::uint64_t callId = entryCurrent ? claim_call_id() : 0U;

    const std::uintptr_t tableValue = reinterpret_cast<std::uintptr_t>(sensorTable);
    const bool wrapperValid = tableValue >= 0x28U;
    const std::uintptr_t wrapperValue = wrapperValid ? tableValue - 0x28U : 0U;
    if (entryCurrent && !wrapperValid) {
        g_invalidWrapper.fetch_add(1U, std::memory_order_relaxed);
    }

    heap_cohort::ReceiveToken heapToken{};
    bool preDelivered = false;
    BoundaryObservation observation{};
    if (entryCurrent && callId != 0U) {
        observation.ownerGeneration = generation.ownerGeneration;
        observation.aggregateEpoch = generation.aggregateEpoch;
        observation.callId = callId;
        observation.sensorTable = tableValue;
        observation.activityWrapper = wrapperValue;
        observation.bitStream = reinterpret_cast<std::uintptr_t>(bitStream);
        observation.producerThreadId = producer_thread_id();
        observation.phase = BoundaryPhase::pre;
        observation.wrapperValid = wrapperValid;
        observation.entryGenerationCurrent = true;

        generation.fanout->heapEnter(
            generation.fanout->heapContext, heapToken, sensorTable, bitStream);
        generation.fanout->scenePre(generation.fanout->sceneContext, observation);
        preDelivered = true;
        g_admitted.fetch_add(1U, std::memory_order_relaxed);
    }

    const std::uint64_t result = original(sensorTable, bitStream);
    g_forwarded.fetch_add(1U, std::memory_order_relaxed);

    if (preDelivered) {
        // The heap bridge's exit is deliberately the first instruction-owned callback after the
        // original. It restores the receive TLS opened immediately before the native call.
        generation.fanout->heapExit(generation.fanout->heapContext, heapToken, result);

        observation.phase = BoundaryPhase::post;
        observation.nativeResult = result;
        observation.nativeResultValid = true;
        observation.exitGenerationCurrent = generation_current(generation);
        if (!observation.exitGenerationCurrent) {
            g_staleAtExit.fetch_add(1U, std::memory_order_relaxed);
        }
        generation.fanout->scenePost(generation.fanout->sceneContext, observation);
        g_pairedPost.fetch_add(1U, std::memory_order_relaxed);
    }
    return result;
}

void clear_counters() noexcept {
    g_entered.store(0U, std::memory_order_relaxed);
    g_forwarded.store(0U, std::memory_order_relaxed);
    g_admitted.store(0U, std::memory_order_relaxed);
    g_pairedPost.store(0U, std::memory_order_relaxed);
    g_staleAtExit.store(0U, std::memory_order_relaxed);
    g_invalidWrapper.store(0U, std::memory_order_relaxed);
    g_callIdExhausted.store(0U, std::memory_order_relaxed);
}

void clear_publication() noexcept {
    g_original.store(nullptr, std::memory_order_release);
    g_target.store(nullptr, std::memory_order_release);
    g_fanout.store(nullptr, std::memory_order_release);
    g_aggregateEpochSource.store(nullptr, std::memory_order_release);
    g_ownerGeneration.store(0U, std::memory_order_release);
    g_aggregateEpoch.store(0U, std::memory_order_release);
    g_nextCallId.store(1U, std::memory_order_release);
    g_heapAttachPublished.store(false, std::memory_order_release);
    clear_counters();
}

} // namespace

ValidationResult validate(const ImageView& image, ValidatedTarget& output) noexcept {
    output = {};
    if (image.packedSha256 != kPinnedPackedImageSha256) {
        return ValidationResult::packed_hash_mismatch;
    }
    if (image.packedFileSize != 122'984'224ULL || image.packed.size() != image.packedFileSize) {
        return ValidationResult::packed_size_mismatch;
    }
    if (image.machine != 0x8664U || image.sectionCount != 11U || image.timestamp != 0x5F43138BU
        || image.imageSize != 0x08A5EA00U || image.entryRva != 0x0187CDD8U
        || image.checksum != 0x0755867CU) {
        return ValidationResult::pe_mismatch;
    }
    if (image.codeViewGuid != kPinnedCodeViewGuid || image.codeViewAge != 1U) {
        return ValidationResult::codeview_mismatch;
    }
    if (kTargetRva > image.mapped.size() || kPrefixBytes > image.mapped.size() - kTargetRva
        || kPackedRawOffset > image.packed.size()
        || kPrefixBytes > image.packed.size() - kPackedRawOffset) {
        return ValidationResult::target_bounds;
    }
    if (image.mapped.size() < image.imageSize) {
        return ValidationResult::pe_mismatch;
    }

    std::memcpy(output.observedMapped.data(), image.mapped.data() + kTargetRva, kPrefixBytes);
    std::memcpy(output.observedPacked.data(), image.packed.data() + kPackedRawOffset, kPrefixBytes);
    if (output.observedMapped != kExpectedMappedPrefix) {
        output = {};
        return ValidationResult::mapped_prefix_mismatch;
    }
    if (output.observedPacked != kExpectedPackedPrefix) {
        output = {};
        return ValidationResult::packed_prefix_mismatch;
    }
    output.address = const_cast<std::byte*>(image.mapped.data() + kTargetRva);
    output.valid = true;
    return ValidationResult::valid;
}

PrepareResult prepare_publication(const Publication& publication) noexcept {
    if (g_phase.load(std::memory_order_acquire) != ParticipantPhase::detached
        || g_target.load(std::memory_order_acquire) != nullptr
        || g_original.load(std::memory_order_acquire) != nullptr || !g_callGate.idle()) {
        return PrepareResult::retained_predecessor;
    }
    if (!publication.target.valid || publication.target.address == nullptr
        || publication.target.observedMapped != kExpectedMappedPrefix
        || publication.target.observedPacked != kExpectedPackedPrefix) {
        return PrepareResult::invalid_target;
    }
    if (publication.fanout == nullptr || publication.aggregateEpochSource == nullptr
        || publication.ownerGeneration == 0U || publication.aggregateEpoch == 0U) {
        return PrepareResult::invalid_generation;
    }
    if (publication.aggregateEpochSource->load(std::memory_order_acquire) != 0U) {
        return PrepareResult::aggregate_already_admitting;
    }
    if (!valid_fanout(*publication.fanout, publication.ownerGeneration)) {
        return PrepareResult::invalid_fanout;
    }

    g_callGate.quiesce();
    g_target.store(publication.target.address, std::memory_order_release);
    g_fanout.store(publication.fanout, std::memory_order_release);
    g_aggregateEpochSource.store(publication.aggregateEpochSource, std::memory_order_release);
    g_ownerGeneration.store(publication.ownerGeneration, std::memory_order_release);
    g_aggregateEpoch.store(publication.aggregateEpoch, std::memory_order_release);
    g_phase.store(ParticipantPhase::prepared, std::memory_order_release);
    return PrepareResult::prepared;
}

bool append_specs(std::span<hooking::detour::Spec> output, std::size_t& used) noexcept {
    if (g_phase.load(std::memory_order_acquire) != ParticipantPhase::prepared
        || used >= output.size()) {
        return false;
    }
    void* const target = g_target.load(std::memory_order_acquire);
    if (target == nullptr) {
        return false;
    }
    output[used++] = {target, replacement_entry()};
    return true;
}

void publish_original(ReceiveOriginal original) noexcept {
    if (original == nullptr
        || g_phase.load(std::memory_order_acquire) != ParticipantPhase::prepared) {
        return;
    }
    g_phase.store(ParticipantPhase::published, std::memory_order_release);
    hooking::publish_original(g_original, original);
}

bool accept() noexcept {
    ParticipantPhase expected = ParticipantPhase::published;
    if (!g_phase.compare_exchange_strong(expected,
                                         ParticipantPhase::accepting,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return expected == ParticipantPhase::accepting;
    }
    const FanoutV1* const fanout = g_fanout.load(std::memory_order_acquire);
    if (fanout == nullptr || g_original.load(std::memory_order_acquire) == nullptr) {
        g_phase.store(ParticipantPhase::quiescing, std::memory_order_release);
        return false;
    }
    fanout->heapOwnerAttached(fanout->heapContext);
    g_heapAttachPublished.store(true, std::memory_order_release);
    g_callGate.accept();
    return true;
}

void quiesce() noexcept {
    g_callGate.quiesce();
    const ParticipantPhase previous =
        g_phase.exchange(ParticipantPhase::quiescing, std::memory_order_acq_rel);
    if ((previous == ParticipantPhase::accepting || previous == ParticipantPhase::published)
        && g_heapAttachPublished.load(std::memory_order_acquire)) {
        const FanoutV1* const fanout = g_fanout.load(std::memory_order_acquire);
        if (fanout != nullptr) {
            fanout->heapOwnerQuiescing(fanout->heapContext);
        }
    }
}

void retain_after_failed_remove() noexcept {
    if (g_phase.load(std::memory_order_acquire) != ParticipantPhase::quiescing
        || !g_heapAttachPublished.load(std::memory_order_acquire)) {
        return;
    }
    const FanoutV1* const fanout = g_fanout.load(std::memory_order_acquire);
    if (fanout != nullptr) {
        fanout->heapOwnerDetached(fanout->heapContext, false);
    }
}

bool idle() noexcept {
    return g_callGate.idle();
}

bool accepting() noexcept {
    return g_callGate.accepting();
}

bool has_ownership() noexcept {
    return g_phase.load(std::memory_order_acquire) != ParticipantPhase::detached
           || g_target.load(std::memory_order_acquire) != nullptr
           || g_original.load(std::memory_order_acquire) != nullptr || !g_callGate.idle();
}

bool cancel_before_attach() noexcept {
    if (g_phase.load(std::memory_order_acquire) != ParticipantPhase::prepared
        || g_original.load(std::memory_order_acquire) != nullptr || !g_callGate.idle()) {
        return false;
    }
    clear_publication();
    g_phase.store(ParticipantPhase::detached, std::memory_order_release);
    return true;
}

bool clear_after_removed() noexcept {
    if (g_callGate.accepting() || !g_callGate.idle()
        || g_phase.load(std::memory_order_acquire) != ParticipantPhase::quiescing) {
        return false;
    }
    const FanoutV1* const fanout = g_fanout.load(std::memory_order_acquire);
    if (fanout != nullptr && g_heapAttachPublished.load(std::memory_order_acquire)) {
        fanout->heapOwnerDetached(fanout->heapContext, true);
    }
    clear_publication();
    g_phase.store(ParticipantPhase::detached, std::memory_order_release);
    return true;
}

void* replacement_entry() noexcept {
    return reinterpret_cast<void*>(&receive_replacement);
}

Counters counters() noexcept {
    Counters output{};
    output.entered = g_entered.load(std::memory_order_relaxed);
    output.forwarded = g_forwarded.load(std::memory_order_relaxed);
    output.admitted = g_admitted.load(std::memory_order_relaxed);
    output.pairedPost = g_pairedPost.load(std::memory_order_relaxed);
    output.staleAtExit = g_staleAtExit.load(std::memory_order_relaxed);
    output.invalidWrapper = g_invalidWrapper.load(std::memory_order_relaxed);
    output.callIdExhausted = g_callIdExhausted.load(std::memory_order_relaxed);
    return output;
}

namespace testing {

std::uint64_t invoke(void* sensorTable, void* bitStream) noexcept {
    return receive_replacement(sensorTable, bitStream);
}

} // namespace testing

} // namespace dawn::client::hooks::bootflow::opening_authority::activity_authority_receive_owner
