#define DAWN_SENSOR_HEAP_FULL_COHORT_TEST 1

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#include "client/hooks/activity_lifecycle/native_activation_global_drop_fanout.h"
#include "client/hooks/network/lifecycle/sensor_state_heap_full_cohort_core.h"

namespace {

using namespace dawn::client::hooks::network::lifecycle::sensor_state_heap_full_cohort;
namespace fanout = dawn::client::hooks::activity_lifecycle;

int g_failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

static_assert(sizeof(EventV1) == 256U);
static_assert(alignof(EventV1) >= 8U);
static_assert(sizeof(Shl1Header) == 4096U);
static_assert(offsetof(EventV1, payload) == 128U);
static_assert(offsetof(Shl1Header, failureCounters) == 320U);
static_assert(offsetof(Shl1Header, siteManifest) == 1024U);
static_assert(validActivityGeneration == 1U << 0U);
static_assert(validCriticalPersistence == 1U << 31U);
static_assert(flagTargetAllocationTracked == 1U << 0U);
static_assert(flagWriteWatch == 1U << 15U);
static_assert(kCoreHookCount == 11U);

[[nodiscard]] ImageView exact_image(std::vector<std::byte>& mapped) {
    mapped.assign(0x08A5EA00U, std::byte{});
    for (const SiteContract& contract : kSiteManifest) {
        std::memcpy(mapped.data() + contract.rva,
                    contract.expectedMapped.data(),
                    contract.prefixLength);
    }
    ImageView output{};
    output.mapped = mapped;
    output.packedSha256 = kPinnedPackedImageSha256;
    output.packedFileSize = 122'984'224ULL;
    output.machine = 0x8664U;
    output.sectionCount = 11U;
    output.timestamp = 0x5F43138BU;
    output.imageSize = 0x08A5EA00U;
    output.entryRva = 0x0187CDD8U;
    output.checksum = 0x0755867CU;
    output.codeViewGuid = {
        std::byte{0xDFU}, std::byte{0xFBU}, std::byte{0xDCU}, std::byte{0x0DU},
        std::byte{0x68U}, std::byte{0xEBU}, std::byte{0x48U}, std::byte{0x41U},
        std::byte{0x8BU}, std::byte{0xFBU}, std::byte{0x7CU}, std::byte{0x76U},
        std::byte{0x18U}, std::byte{0xFFU}, std::byte{0xABU}, std::byte{0x03U},
    };
    output.codeViewAge = 1U;
    return output;
}

void manifest_is_exact_all_or_none_and_has_unique_owners() {
    CHECK(kSiteManifest.size() == 18U);
    std::set<std::uint32_t> rvas{};
    std::size_t coreCount = 0U;
    std::size_t externalCount = 0U;
    const SiteContract* drop = nullptr;
    const SiteContract* receive = nullptr;
    for (const SiteContract& contract : kSiteManifest) {
        CHECK(rvas.insert(contract.rva).second);
        coreCount += contract.action == SiteAction::coreDetour ? 1U : 0U;
        externalCount += contract.action == SiteAction::externalOwner ? 1U : 0U;
        if (contract.rva == 0x3C8EB0U) {
            drop = &contract;
        }
        if (contract.rva == 0x4D7470U) {
            receive = &contract;
        }
    }
    CHECK(coreCount == 11U);
    CHECK(externalCount == 2U);
    CHECK(drop != nullptr && drop->action == SiteAction::externalOwner);
    CHECK(drop != nullptr && drop->prefixLength == 17U);
    CHECK(drop != nullptr && drop->expectedMapped[15] == std::byte{0x4CU});
    CHECK(drop != nullptr && drop->expectedMapped[16] == std::byte{0x89U});
    CHECK(receive != nullptr && receive->prefixLength == 16U);
    CHECK(receive != nullptr && receive->expectedMapped[0] == std::byte{0x40U});

    std::vector<std::byte> mapped{};
    ImageView image = exact_image(mapped);
    std::array<std::uintptr_t, kCoreHookCount> targets{};
    std::uintptr_t resolver{};
    std::array<std::array<std::byte, 17U>, kSiteCount> observed{};
    CHECK(validate_image(image, targets, resolver, observed) == ImageValidationResult::valid);
    CHECK(std::all_of(targets.begin(), targets.end(), [](std::uintptr_t value) {
        return value != 0U;
    }));
    CHECK(resolver == reinterpret_cast<std::uintptr_t>(mapped.data() + 0x323D40U));

    mapped[0x4D7470U] ^= std::byte{1U};
    targets.fill(99U);
    resolver = 99U;
    CHECK(validate_image(image, targets, resolver, observed)
          == ImageValidationResult::prefixMismatch);
    CHECK(std::all_of(targets.begin(), targets.end(), [](std::uintptr_t value) {
        return value == 0U;
    }));
    CHECK(resolver == 0U);
    mapped[0x4D7470U] ^= std::byte{1U};

    std::memcpy(mapped.data() + kSiteManifest[0].rva,
                kSiteManifest[0].packedRaw.data(),
                kSiteManifest[0].prefixLength);
    CHECK(validate_image(image, targets, resolver, observed)
          == ImageValidationResult::prefixMismatch);
    (void)exact_image(mapped);
    image.mapped = mapped;

    mapped[0x3C8EB0U + 16U] ^= std::byte{1U};
    CHECK(validate_image(image, targets, resolver, observed)
          == ImageValidationResult::prefixMismatch);
    mapped[0x3C8EB0U + 16U] ^= std::byte{1U};

    image.packedSha256[0] ^= std::byte{1U};
    CHECK(validate_image(image, targets, resolver, observed)
          == ImageValidationResult::hashMismatch);
}

void identity_arithmetic_assert_and_extent_contracts() {
    constexpr std::uint64_t packed = pack_sensor_identity(0x44332211U, 0x55U, 0x8877U);
    static_assert(packed == 0x8877005544332211ULL);
    CHECK(((packed >> 40U) & 0xFFU) == 0U);
    std::uintptr_t address{};
    CHECK(checked_add(0x1000U, 0x20U, address) && address == 0x1020U);
    CHECK(!checked_add((std::numeric_limits<std::uintptr_t>::max)(), 1U, address));
    std::uint64_t relative{};
    CHECK(checked_sub(0x40U, 0x20U, relative) && relative == 0x20U);
    CHECK(!checked_sub(0x20U, 0x40U, relative));
    std::uint32_t extent{};
    CHECK(align16_extent(17U, 0x20U, extent) && extent == 0x40U);
    CHECK(align16_extent(17U, 0x40U, extent) && extent == 0x60U);
    CHECK(!align16_extent(0x0FFFFFF0U, 0x40U, extent));
    CHECK(exact_heap_assert("index heap double-free? previous does not point back."));
    CHECK(!exact_heap_assert("index heap double-free? previous does not point back"));
    CHECK(!exact_heap_assert("index heap double-free? previous does not point back.!"));
    CHECK(!exact_heap_assert("Index heap double-free? previous does not point back."));
}

class BufferReader final {
public:
    explicit BufferReader(std::span<std::byte> bytes) noexcept
        : bytes_(bytes), begin_(reinterpret_cast<std::uintptr_t>(bytes.data())) {}

    void reject(std::uintptr_t begin, std::size_t bytes) noexcept {
        rejectBegin_ = begin;
        rejectEnd_ = begin + bytes;
    }

    [[nodiscard]] bool copy(std::uintptr_t source, void* destination, std::size_t bytes) noexcept {
        if (destination == nullptr || source < begin_ || bytes > bytes_.size()
            || source - begin_ > bytes_.size() - bytes
            || (rejectBegin_ != 0U && source < rejectEnd_ && source + bytes > rejectBegin_)) {
            return false;
        }
        std::memcpy(destination, reinterpret_cast<const void*>(source), bytes);
        return true;
    }

    [[nodiscard]] bool hash(std::uintptr_t source,
                            std::size_t bytes,
                            std::uint64_t& output) noexcept {
        output = 0U;
        if (source < begin_ || bytes > bytes_.size() || source - begin_ > bytes_.size() - bytes
            || (rejectBegin_ != 0U && source < rejectEnd_ && source + bytes > rejectBegin_)) {
            return false;
        }
        std::uint64_t hash = 14695981039346656037ULL;
        for (std::size_t index = 0U; index < bytes; ++index) {
            hash ^= std::to_integer<std::uint8_t>(
                bytes_[static_cast<std::size_t>(source - begin_) + index]);
            hash *= 1099511628211ULL;
        }
        output = hash;
        return true;
    }

private:
    std::span<std::byte> bytes_{};
    std::uintptr_t begin_{};
    std::uintptr_t rejectBegin_{};
    std::uintptr_t rejectEnd_{};
};

template <typename Value>
void store(std::vector<std::byte>& memory, std::size_t offset, Value value) {
    std::memcpy(memory.data() + offset, &value, sizeof value);
}

void heap_layout_hash_and_all_backlink_predicates() {
    std::vector<std::byte> memory(0x3000U);
    const std::uintptr_t root = reinterpret_cast<std::uintptr_t>(memory.data());
    const std::uintptr_t heap = root + 0x100U;
    const std::uintptr_t base = root + 0x1000U;
    const std::uint64_t relativeHeader = 0x100U;
    const std::uint64_t relativePayload20 = 0x120U;
    const std::uint32_t count = 17U;
    store(memory, 0x100U, base);
    store<std::uint8_t>(memory, 0x100U + 0xA4U, 0U);
    store<std::uint32_t>(memory, 0x1000U + 0x100U, 0x40U);
    store<std::uint64_t>(memory, 0x1000U + 0x100U + 0x10U, 0U);
    store<std::uint64_t>(memory, 0x1000U + 0x100U + 0x18U, 0U);
    store<std::uint64_t>(memory, 0x100U + 0x60U, relativeHeader);
    store<std::uint64_t>(memory, 0x100U + 0x68U, relativeHeader);
    for (std::uint32_t index = 0U; index < count; ++index) {
        memory[0x1000U + 0x120U + index] = std::byte{static_cast<std::uint8_t>(index + 1U)};
    }
    BufferReader reader{memory};
    HeapCapture capture = capture_heap(reader, heap, relativePayload20, count, 7U);
    CHECK((capture.valid & (validHeap | validHeapBase | validHeader | validCurrentLinks
                            | validPreviousRelation | validNextRelation | validPayloadHash))
          == (validHeap | validHeapBase | validHeader | validCurrentLinks
              | validPreviousRelation | validNextRelation | validPayloadHash));
    CHECK(capture.payload.headerWidth == 0x20U);
    CHECK(capture.payload.relativeHeader == relativeHeader);
    CHECK(capture.payload.invariant
          == (invariantPreviousGood | invariantNextGood | invariantHeadGood
              | invariantTailGood));
    CHECK((capture.flags & (flagPreviousPredicateBad | flagNextPredicateBad
                            | flagCountOrExtentMismatch))
          == 0U);

    store<std::uint8_t>(memory, 0x100U + 0xA4U, 1U);
    store<std::uint32_t>(memory, 0x1000U + 0x100U, 0x60U);
    constexpr std::uint64_t previous = 0x300U;
    constexpr std::uint64_t next = 0x400U;
    store<std::uint64_t>(memory, 0x1000U + 0x100U + 0x10U, next);
    store<std::uint64_t>(memory, 0x1000U + 0x100U + 0x18U, previous);
    store<std::uint64_t>(memory, 0x1000U + 0x300U + 0x10U, relativeHeader);
    store<std::uint64_t>(memory, 0x1000U + 0x400U + 0x18U, relativeHeader);
    capture = capture_heap(reader, heap, 0x140U, count, 9U);
    CHECK(capture.payload.headerWidth == 0x40U);
    CHECK((capture.payload.invariant & (invariantPreviousGood | invariantNextGood))
          == (invariantPreviousGood | invariantNextGood));

    store<std::uint64_t>(memory, 0x1000U + 0x400U + 0x18U, 0x999U);
    capture = capture_heap(reader, heap, 0x140U, count, 9U);
    CHECK((capture.valid & validNextRelation) != 0U);
    CHECK((capture.flags & flagNextPredicateBad) != 0U);
    CHECK((capture.payload.invariant & invariantNextGood) == 0U);

    store<std::uint64_t>(memory, 0x1000U + 0x400U + 0x18U, relativeHeader);
    reader.reject(base + next + 0x18U, sizeof(std::uint64_t));
    capture = capture_heap(reader, heap, 0x140U, count, 9U);
    CHECK((capture.valid & validNextRelation) == 0U);
    CHECK((capture.flags & flagGuardedReadFault) != 0U);
    CHECK((capture.flags & flagNextPredicateBad) == 0U);
}

[[nodiscard]] RecordState state(std::uintptr_t record, std::uint32_t marker = 1U) {
    RecordState output{};
    output.record = record;
    output.sensorTable = 0x8000U + marker * 0x100U;
    output.tableValid = true;
    output.sensorKey = marker;
    output.active = true;
    return output;
}

struct ReuseCapture final {
    bool called{};
    std::uint32_t generation{};
    bool active{};
};

void capture_reuse(void* context, const RecordState& previous) noexcept {
    auto& output = *static_cast<ReuseCapture*>(context);
    output.called = true;
    output.generation = previous.generation;
    output.active = previous.active;
}

void record_registry_collision_reuse_race_stale_and_concurrency() {
    Shl1Header header{};
    auto registry = std::make_unique<RecordRegistry<2U>>();
    registry->bind(&header);
    registry->reset();
    RecordToken first{};
    RecordToken second{};
    RecordToken third{};
    CHECK(registry->begin(0x10U, state(0x10U), first) == RegistryResult::success);
    CHECK(registry->begin(0x30U, state(0x30U), second) == RegistryResult::success);
    CHECK(registry->begin(0x50U, state(0x50U), third) == RegistryResult::full);
    CHECK(!third);

    ReuseCapture reuse{};
    RecordToken reused{};
    CHECK(registry->begin(0x10U,
                          state(0x10U, 2U),
                          reused,
                          &capture_reuse,
                          &reuse)
          == RegistryResult::success);
    CHECK(reuse.called && reuse.active && reuse.generation == 1U);
    CHECK(reused.generation == 2U);
    CHECK(registry->retire(reused) == RegistryResult::success);
    RecordToken inactiveReuse{};
    reuse = {};
    CHECK(registry->begin(0x10U,
                          state(0x10U, 3U),
                          inactiveReuse,
                          &capture_reuse,
                          &reuse)
          == RegistryResult::success);
    CHECK(!reuse.called);

    registry->testing_set_version(0x10U, 5U);
    RecordState view{};
    CHECK(registry->read(0x10U, view, nullptr) == RegistryResult::busy);
    registry->testing_set_version(0x10U, 6U);
    RecordToken stale = inactiveReuse;
    ++stale.generation;
    CHECK(registry->commit(stale) == RegistryResult::stale);
    CHECK(registry->retire(stale) == RegistryResult::stale);
    registry->testing_set_generation(
        0x10U, (std::numeric_limits<std::uint32_t>::max)());
    RecordToken exhausted{};
    CHECK(registry->begin(0x10U, state(0x10U), exhausted) == RegistryResult::exhausted);

    auto concurrent = std::make_unique<RecordRegistry<8U>>();
    concurrent->reset();
    constexpr std::size_t threadCount = 4U;
    constexpr std::size_t cycles = 1000U;
    std::array<std::thread, threadCount> threads{};
    std::atomic_bool startThreads{};
    std::atomic<std::uint32_t> failures{};
    for (std::size_t thread = 0U; thread < threadCount; ++thread) {
        threads[thread] = std::thread([&, thread] {
            const std::uintptr_t record = 0x1000U + thread * 0x10U;
            while (!startThreads.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t cycle = 0U; cycle < cycles; ++cycle) {
                RecordToken token{};
                RecordState snapshot{};
                if (concurrent->begin(record, state(record), token) != RegistryResult::success
                    || concurrent->commit(token) != RegistryResult::success
                    || concurrent->read(record, snapshot, nullptr) != RegistryResult::success
                    || concurrent->retire(token) != RegistryResult::success) {
                    failures.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        });
    }
    startThreads.store(true, std::memory_order_release);
    for (std::thread& thread : threads) {
        thread.join();
    }
    CHECK(failures.load(std::memory_order_relaxed) == 0U);
    CHECK(concurrent->high_water() == threadCount);
}

void allocation_index_reuse_retired_busy_full_and_free_tickets() {
    Shl1Header header{};
    auto allocations = std::make_unique<AllocationIndex<2U>>();
    allocations->bind(&header);
    allocations->reset();
    const AllocationKey payload{0x1000U, 0x80U, AllocationKeyKind::payload};
    const AllocationKey headerKey{0x1000U, 0x60U, AllocationKeyKind::header};
    const AllocationBinding auth{1U, 1U, Member::receivedAuth, false};
    const AllocationBinding sense{2U, 1U, Member::receivedSense, false};
    CHECK(allocations->publish(payload, auth) == RegistryResult::success);
    CHECK(allocations->publish(headerKey, sense) == RegistryResult::success);
    AllocationBinding view{};
    CHECK(allocations->lookup(payload, view) == RegistryResult::success);
    CHECK(view.member == Member::receivedAuth);
    CHECK(allocations->publish({0x2000U, 0x40U, AllocationKeyKind::payload}, auth)
          == RegistryResult::full);

    const AllocationBinding newer{3U, 2U, Member::extractedSense, false};
    CHECK(allocations->publish(payload, newer) == RegistryResult::success);
    CHECK(allocations->publish(payload, auth) == RegistryResult::stale);
    CHECK(allocations->mark_retired(payload, 2U) == RegistryResult::success);
    CHECK(allocations->lookup(payload, view) == RegistryResult::success && view.retired);
    allocations->testing_set_version(payload, 3U);
    CHECK(allocations->lookup(payload, view) == RegistryResult::busy);

    auto records = std::make_unique<RecordRegistry<4U>>();
    records->bind(&header);
    records->reset();
    RecordToken token{};
    CHECK(records->begin(0x4000U, state(0x4000U), token) == RegistryResult::success);
    std::uint32_t ordinal{};
    CHECK(records->next_free_ordinal(token, Member::receivedSense, ordinal)
          == RegistryResult::success);
    CHECK(ordinal == 1U);
    CHECK(records->next_free_ordinal(token, Member::receivedSense, ordinal)
          == RegistryResult::success);
    CHECK(ordinal == 2U);
    records->testing_set_free_ticket(
        token,
        Member::receivedSense,
        (static_cast<std::uint64_t>(token.generation) << 32U)
            | (std::numeric_limits<std::uint32_t>::max)());
    CHECK(records->next_free_ordinal(token, Member::receivedSense, ordinal)
          == RegistryResult::exhausted);
    records->testing_set_free_ticket(
        token,
        Member::receivedSense,
        static_cast<std::uint64_t>(token.generation + 1U) << 32U);
    CHECK(records->next_free_ordinal(token, Member::receivedSense, ordinal)
          == RegistryResult::stale);
}

void event_store_pair_freeze_critical_and_no_overwrite() {
    Shl1Header header{};
    std::array<EventV1, 5U> events{};
    EventLedger ledger{&header, events.data(), 3U, 2U};
    EventClaim pair = ledger.claim_pair();
    CHECK(pair && pair.second != nullptr);
    pair.first->kind = static_cast<std::uint8_t>(EventKind::decode);
    pair.second->kind = static_cast<std::uint8_t>(EventKind::decode);
    pair.first->payload.writerMeta.companionSequence = pair.secondSequence;
    ledger.commit(*pair.second, pair.secondSequence);
    ledger.commit(*pair.first, pair.firstSequence);
    CHECK(pair.first->commitSequence.load(std::memory_order_acquire) == 1U);
    CHECK(pair.second->commitSequence.load(std::memory_order_acquire) == 2U);
    CHECK(pair.first->payload.writerMeta.companionSequence == 2U);

    EventClaim failedPair = ledger.claim_pair();
    CHECK(!failedPair);
    CHECK(header.failureCounters[static_cast<std::size_t>(FailureCounter::writerPairPartial)]
              .load(std::memory_order_relaxed)
          == 1U);
    CHECK(ledger.freeze(FreezeReason::duplicateFree, 77U));
    CHECK(!ledger.freeze(FreezeReason::lifecycleStop, 88U));
    CHECK(!ledger.claim_normal());
    CHECK(header.freezeQpc.load(std::memory_order_relaxed) == 77U);

    EventClaim critical1 = ledger.claim_critical();
    EventClaim critical2 = ledger.claim_critical();
    CHECK(critical1 && critical2);
    ledger.commit(*critical1.first, critical1.firstSequence);
    ledger.commit(*critical2.first, critical2.firstSequence);
    CHECK(!ledger.claim_critical());
    CHECK(critical1.firstSequence == 4U && critical2.firstSequence == 5U);
    CHECK(header.failureCounters[static_cast<std::size_t>(FailureCounter::criticalEventFull)]
              .load(std::memory_order_relaxed)
          == 1U);
    CHECK(events[0].kind == static_cast<std::uint8_t>(EventKind::decode));
}

void writer_free_unlink_verdict_classification_is_exact() {
    CHECK(classify_failure(0U, flagCountOrExtentMismatch)
          == FreezeReason::writerGoodToBad);
    CHECK(classify_failure(flagCountOrExtentMismatch, flagCountOrExtentMismatch)
          == FreezeReason::none);
    CHECK(classify_failure(0U, 0U, 2U) == FreezeReason::duplicateFree);
    CHECK(classify_failure(0U, flagPreviousPredicateBad)
          == FreezeReason::badPreviousPredicate);
    CHECK(classify_failure(0U, flagNextPredicateBad) == FreezeReason::badNextPredicate);
    CHECK(classify_failure(0U, flagGuardedReadFault)
          == FreezeReason::trackedReadFailure);
    CHECK(classify_failure(0U, 0U, 1U) == FreezeReason::none);
}

struct TlsA { TlsA* previous{}; };
struct TlsB { TlsB* previous{}; };
struct TlsC { TlsC* previous{}; };
struct TlsD { TlsD* previous{}; };
struct TlsE { TlsE* previous{}; };

void five_tls_scopes_restore_nested_and_account_misnest() {
    Shl1Header header{};
    TlsA a1{}, a2{};
    TlsB b{};
    TlsC c{};
    TlsD d{};
    TlsE e{};
    TlsA* aslot{};
    TlsB* bslot{};
    TlsC* cslot{};
    TlsD* dslot{};
    TlsE* eslot{};
    bool aa = false, ab = false, ba = false, ca = false, da = false, ea = false;
    {
        TlsScope outer{aslot, a1, &header, aa};
        CHECK(aslot == &a1 && a1.previous == nullptr);
        {
            TlsScope inner{aslot, a2, &header, ab};
            TlsScope bs{bslot, b, &header, ba};
            TlsScope cs{cslot, c, &header, ca};
            TlsScope ds{dslot, d, &header, da};
            TlsScope es{eslot, e, &header, ea};
            CHECK(aslot == &a2 && a2.previous == &a1);
            CHECK(bslot == &b && cslot == &c && dslot == &d && eslot == &e);
        }
        CHECK(aslot == &a1);
    }
    CHECK(aslot == nullptr && bslot == nullptr && cslot == nullptr && dslot == nullptr
          && eslot == nullptr);

    aa = false;
    ab = false;
    {
        TlsScope outer{aslot, a1, &header, aa};
        {
            TlsScope inner{aslot, a2, &header, ab};
            aslot = &a1;
        }
    }
    CHECK(header.failureCounters[static_cast<std::size_t>(FailureCounter::tlsMisnest)]
              .load(std::memory_order_relaxed)
          >= 1U);
}

using Decode = std::uint64_t (*)(void*,
                                 void*,
                                 std::int32_t*,
                                 std::uint32_t,
                                 std::uint32_t) noexcept;

struct AbiCapture final {
    void* stream{};
    void* destination{};
    std::int32_t* key{};
    std::uint32_t schema{};
    std::uint32_t flags{};
    std::uint32_t calls{};
};

AbiCapture g_abi{};

std::uint64_t fake_decode(void* stream,
                          void* destination,
                          std::int32_t* key,
                          std::uint32_t schema,
                          std::uint32_t flags) noexcept {
    g_abi = AbiCapture{stream, destination, key, schema, flags, g_abi.calls + 1U};
    return 0xFEDCBA9876543201ULL;
}

std::uint64_t forward_decode(Decode original,
                             void* stream,
                             void* destination,
                             std::int32_t* key,
                             std::uint32_t schema,
                             std::uint32_t flags) noexcept {
    return original(stream, destination, key, schema, flags);
}

void full_decode_abi_forwards_once_and_preserves_high_rax() {
    g_abi = {};
    std::int32_t key = 17;
    void* const stream = reinterpret_cast<void*>(0x11110000U);
    void* const destination = reinterpret_cast<void*>(0x22220000U);
    const std::uint64_t result =
        forward_decode(&fake_decode, stream, destination, &key, 0x80800001U, 0xA5A5U);
    CHECK(result == 0xFEDCBA9876543201ULL);
    CHECK(g_abi.calls == 1U);
    CHECK(g_abi.stream == stream && g_abi.destination == destination && g_abi.key == &key);
    CHECK(g_abi.schema == 0x80800001U && g_abi.flags == 0xA5A5U);
}

struct FanoutState final {
    std::atomic_bool entered{};
    std::atomic_bool release{};
    std::atomic<std::uint32_t> pre{};
    std::atomic<std::uint32_t> post{};
};

void fanout_pre(void* context,
                const fanout::NativeActivationGlobalDropCohort&) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    state.pre.fetch_add(1U, std::memory_order_relaxed);
    state.entered.store(true, std::memory_order_release);
    while (!state.release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

void fanout_post(void* context,
                 const fanout::NativeActivationGlobalDropCohort&) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    state.post.fetch_add(1U, std::memory_order_relaxed);
}

void immutable_fanout_retains_table_until_inflight_callback_leaves() {
    FanoutState state{};
    fanout::NativeActivationGlobalDropFanout owner{};
    const fanout::NativeActivationGlobalDropObserverTable table{
        fanout::kNativeActivationGlobalDropObserverAbiVersion,
        &state,
        &fanout_pre,
        &fanout_post,
    };
    fanout::NativeActivationGlobalDropObserverHandle handle{};
    fanout::NativeActivationGlobalDropObserverHandle duplicate{};
    CHECK(owner.register_observer(&table, handle));
    CHECK(!owner.register_observer(&table, duplicate));
    owner.accept();
    const fanout::NativeActivationGlobalDropCohort cohort{3U, 4U, 1U, true};
    std::thread producer([&] {
        auto dispatch = owner.acquire();
        dispatch.notify_pre(cohort);
        dispatch.notify_post(cohort);
        dispatch.finish();
    });
    while (!state.entered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    CHECK(!owner.idle());
    CHECK(!owner.clear_after_removed());
    CHECK(owner.has_registration());
    state.release.store(true, std::memory_order_release);
    producer.join();
    CHECK(owner.clear_after_removed());
    CHECK(!owner.has_registration());
    CHECK(!owner.unregister_observer(handle));
    CHECK(state.post.load(std::memory_order_relaxed) == 1U);
    CHECK(!owner.acquire());
    CHECK(state.pre.load(std::memory_order_relaxed) == 1U);
}

} // namespace

int main() {
    manifest_is_exact_all_or_none_and_has_unique_owners();
    identity_arithmetic_assert_and_extent_contracts();
    heap_layout_hash_and_all_backlink_predicates();
    record_registry_collision_reuse_race_stale_and_concurrency();
    allocation_index_reuse_retired_busy_full_and_free_tickets();
    event_store_pair_freeze_critical_and_no_overwrite();
    writer_free_unlink_verdict_classification_is_exact();
    five_tls_scopes_restore_nested_and_account_misnest();
    full_decode_abi_forwards_once_and_preserves_high_rax();
    immutable_fanout_retains_table_until_inflight_callback_leaves();

    if (g_failures != 0) {
        std::cerr << g_failures << " full-cohort check(s) failed\n";
        return 1;
    }
    std::cout << "all sensor heap full-cohort checks passed\n";
    return 0;
}
