#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace dawn::client::hooks::bootflow::omega_boss_graph {

inline constexpr std::uint32_t kGroup = 0xAFB11A12;
inline constexpr std::uint32_t kSequence = 0x65D2379F;
inline constexpr std::uint32_t kSummonEvent = 0xC0F9C866;
inline constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFF;
inline constexpr std::int32_t kQueueCount = 1;
inline constexpr std::int32_t kQueueHead = 0;
using Queue = std::array<std::byte, 0x808>;
using ConditionRequest = std::array<std::byte, 0x80>;

// A889E0 follows relative configuration arrays without bounds checks. Validate
// the exact extracted config's named table before entering that native lookup.
inline bool named_config_layout(std::span<const std::byte> bytes) noexcept {
    if (bytes.size() < 0x1C4) return false;
    const auto u32 = [&](std::size_t at) { std::uint32_t v{}; std::memcpy(&v, bytes.data() + at, 4); return v; };
    const auto u64 = [&](std::size_t at) { std::uint64_t v{}; std::memcpy(&v, bytes.data() + at, 8); return v; };
    if (u64(0x20) != 2 || u64(0x28) > bytes.size() - 0x78) return false;
    const auto group = std::size_t{0x38} + static_cast<std::size_t>(u64(0x28));
    if (group > bytes.size() - 0x48 || u32(group + 0xC) != kGroup || u64(group + 0x10) != 8
        || u64(group + 0x18) > bytes.size() - group - 0x48) return false;
    const auto names = group + 0x28 + static_cast<std::size_t>(u64(group + 0x18));
    return u32(names + 4) == kSequence;
}

template <class T, std::size_t N>
inline void write(std::array<std::byte, N>& bytes, std::size_t offset, T value) noexcept {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

inline Queue make_queue(std::uint32_t sequence = kSequence) noexcept {
    Queue result{};
    write(result, 0x00, kQueueCount);
    // Native AB35B0(command, 9, 0): kind +0, completion +4, zero +8
    // and payload +10..3F. Unused commands and all padding are canonical zero.
    result[0x08] = std::byte{9};
    write(result, 0x18, kGroup);
    write(result, 0x1C, sequence);
    // Native 4E2950's absent sensor reference. This is not an actor handle.
    write(result, 0x24, std::uint32_t{0x811C9DC5});
    result[0x28] = std::byte{0xFF};
    write(result, 0x2A, std::uint16_t{0xFFFF});
    // A97800 reads mode at queue+2C and marker +2D; both native default 0.
    return result;
}

inline bool queue_matches(std::span<const std::byte> bytes, std::uint32_t sequence = kSequence) noexcept {
    const auto expected = make_queue(sequence);
    return bytes.size() == expected.size()
        && std::memcmp(bytes.data(), expected.data(), expected.size()) == 0;
}

inline ConditionRequest make_condition_request(std::uint32_t sequence = kSequence, std::uint32_t event = kSummonEvent) noexcept {
    ConditionRequest result{};
    write(result, 0x00, kGroup);
    write(result, 0x04, sequence);
    write(result, 0x08, event);
    result[0x60] = std::byte{0x5E};
    return result;
}

// Only full identities cross ticks. Resolve every native pointer again before use.
struct Owner {
    std::uint64_t run{};
    std::uint32_t member{kInvalidHandle}, actor{kInvalidHandle}, generation{}, revision{};
    std::uint32_t parent{kInvalidHandle}, character{kInvalidHandle}, animation{kInvalidHandle};
    std::uint32_t biped{kInvalidHandle};
    std::int64_t memberOffset{};
    std::uint32_t entity{kInvalidHandle};
    friend bool operator==(const Owner&, const Owner&) = default;
};

inline bool valid_owner(const Owner& owner) noexcept {
    return owner.run != 0 && owner.generation != 0
        && owner.memberOffset >= 0 && owner.memberOffset <= 0x2000000
        && owner.member != kInvalidHandle && owner.actor != kInvalidHandle
        && owner.parent != kInvalidHandle && owner.character != kInvalidHandle
        && owner.animation != kInvalidHandle && owner.biped != kInvalidHandle
        && owner.entity != kInvalidHandle
        && owner.parent != owner.character && owner.character != owner.animation
        && owner.parent != owner.animation;
}

inline bool same_binding(const Owner& lhs, const Owner& rhs) noexcept {
    auto candidate = rhs;
    candidate.revision = lhs.revision;
    return lhs == candidate;
}

struct EventRow {
    std::uint32_t event{};
    std::int16_t sequence{};
    std::int8_t group{}, references{};
    friend bool operator==(const EventRow&, const EventRow&) = default;
};
static_assert(sizeof(EventRow) == 8 && offsetof(EventRow, references) == 7);

struct EventTable {
    std::int32_t count{};
    std::array<EventRow, 16> rows{};
};
static_assert(sizeof(EventTable) == 0x84 && offsetof(EventTable, rows) == 4);

inline bool same_tuple(const EventRow& lhs, const EventRow& rhs) noexcept {
    return lhs.event == rhs.event && lhs.sequence == rhs.sequence && lhs.group == rhs.group;
}
inline constexpr EventRow kSummonTuple{kSummonEvent, 1, 0, 1};

inline bool valid_table(const EventTable& table) noexcept {
    if (table.count < 0 || table.count > 16) return false;
    for (std::int32_t i = 0; i < table.count; ++i) {
        const auto& row = table.rows[static_cast<std::size_t>(i)];
        // A889E0 accepts the absent sequence hash and leaves sequence=-1;
        // C61660 then passes that wildcard unchanged to C66110. Unrelated
        // native wildcard leases must not block our exact sequence1 tuple.
        if (row.references <= 0 || row.sequence < -1 || row.group < 0) return false;
        for (std::int32_t j = 0; j < i; ++j)
            if (same_tuple(row, table.rows[static_cast<std::size_t>(j)])) return false;
    }
    return true;
}

// Copy raw animation+34 through +B7 under the caller's native memory guard.
// Truncated/unreadable data must never masquerade as an empty table.
inline bool parse_event_table(std::span<const std::byte> bytes, EventTable& result) noexcept {
    if (bytes.size() != sizeof(EventTable)) return false;
    EventTable candidate;
    std::memcpy(&candidate, bytes.data(), sizeof(candidate));
    if (!valid_table(candidate)) return false;
    result = candidate;
    return true;
}

inline std::int32_t tuple_index(const EventTable& table, const EventRow& tuple = kSummonTuple) noexcept {
    if (!valid_table(table)) return -1;
    for (std::int32_t i = 0; i < table.count; ++i)
        if (same_tuple(table.rows[static_cast<std::size_t>(i)], tuple)) return i;
    return -1;
}

inline int reference_count(const EventTable& table, const EventRow& tuple = kSummonTuple) noexcept {
    if (!valid_table(table)) return -1;
    const auto index = tuple_index(table, tuple);
    return index < 0 ? 0 : table.rows[static_cast<std::size_t>(index)].references;
}

inline bool same_table(const EventTable& lhs, const EventTable& rhs) noexcept {
    if (!valid_table(lhs) || !valid_table(rhs) || lhs.count != rhs.count) return false;
    for (std::int32_t i = 0; i < lhs.count; ++i)
        if (lhs.rows[static_cast<std::size_t>(i)] != rhs.rows[static_cast<std::size_t>(i)]) return false;
    return true;
}

inline bool expected_add(const EventTable& before, EventTable& after, const EventRow& tuple = kSummonTuple) noexcept {
    if (!valid_table(before)) return false;
    after = before;
    const auto index = tuple_index(before, tuple);
    if (index < 0) {
        if (before.count == 16) return false;
        after.rows[static_cast<std::size_t>(after.count++)] = tuple;
    } else {
        auto& references = after.rows[static_cast<std::size_t>(index)].references;
        if (references == 127) return false; // C717E0 interprets this byte as signed.
        ++references;
    }
    return true;
}

inline bool expected_remove(const EventTable& before, EventTable& after, const EventRow& tuple = kSummonTuple) noexcept {
    if (!valid_table(before)) return false;
    const auto index = tuple_index(before, tuple);
    if (index < 0) return false;
    after = before;
    auto& row = after.rows[static_cast<std::size_t>(index)];
    if (row.references > 1) --row.references;
    else row = after.rows[static_cast<std::size_t>(--after.count)]; // Native swap-last removal.
    return true;
}

enum class LeaseState { idle, addPending, owned, removePending, released, uncertain, abandoned };

// Claim before entering native code. A call with an uncertain receipt is never
// retried, even if a later tick finds an apparently usable tuple. No timers own
// this lease: the native graph's final idle may remain active indefinitely.
class EventLease {
public:
    bool begin_add(const Owner& owner, const EventTable& before,
                   bool closeReached, bool exactQueueActive, const EventRow& tuple = kSummonTuple) noexcept {
        if (state_ != LeaseState::idle || !closeReached || !exactQueueActive
            || !valid_owner(owner) || !expected_add(before, expected_, tuple)) return false;
        tuple_ = tuple; owner_ = mutationOwner_ = owner;
        state_ = LeaseState::addPending;
        return true;
    }

    bool finish_add(const Owner& owner, const EventTable& after, bool readSucceeded = true) noexcept {
        if (state_ != LeaseState::addPending) return false;
        const bool certain = readSucceeded && owner == mutationOwner_ && valid_owner(owner)
            && same_table(after, expected_);
        state_ = certain ? LeaseState::owned : LeaseState::uncertain;
        return certain;
    }

    bool begin_remove(const Owner& current, const EventTable& before) noexcept {
        if (state_ != LeaseState::owned) return false;
        // Command replacement may change revision while the exact character
        // binding survives. It does not transfer the leased reference to a new actor.
        if (!valid_owner(current) || !same_binding(owner_, current)
            || !expected_remove(before, expected_, tuple_)) {
            state_ = LeaseState::uncertain;
            return false;
        }
        mutationOwner_ = current;
        state_ = LeaseState::removePending;
        return true;
    }

    bool finish_remove(const Owner& current, const EventTable& after,
                       bool readSucceeded = true) noexcept {
        if (state_ != LeaseState::removePending) return false;
        const bool certain = readSucceeded && current == mutationOwner_ && valid_owner(current)
            && same_table(after, expected_);
        state_ = certain ? LeaseState::released : LeaseState::uncertain;
        return certain;
    }

    // Only invoke once actor destruction or the old run's teardown is known.
    // This releases no native reference; its component owns destruction.
    void abandon() noexcept { state_ = LeaseState::abandoned; }
    LeaseState state() const noexcept { return state_; }
    const Owner& owner() const noexcept { return owner_; }

private:
    Owner owner_{}, mutationOwner_{};
    EventTable expected_{};
    EventRow tuple_{kSummonTuple};
    LeaseState state_{LeaseState::idle};
};

} // namespace dawn::client::hooks::bootflow::omega_boss_graph
