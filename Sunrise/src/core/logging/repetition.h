#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::core::log {

/** Reporting cadence for routine, explicitly classified observations. */
inline constexpr std::uint64_t kRoutineReportIntervalMs = 30'000;

/** Counts refer to omitted observations since the previous emitted observation. */
struct RepetitionReport {
    bool emit{true};
    std::uint64_t suppressed{};
    std::uint64_t windowMs{};
};

/** Caller owns synchronization. Meaningful observations flush counts and always pass. */
class RepetitionCounter {
public:
    [[nodiscard]] RepetitionReport observe(std::uint64_t now, bool routine = true) noexcept {
        if (!started_) {
            started_ = true;
            lastReport_ = now;
            return {};
        }
        const std::uint64_t elapsed = now >= lastReport_ ? now - lastReport_ : 0;
        if (!routine || now < lastReport_ || elapsed >= kRoutineReportIntervalMs) {
            const RepetitionReport report{true, suppressed_, elapsed};
            suppressed_ = 0;
            lastReport_ = now;
            return report;
        }
        ++suppressed_;
        return {false, 0, 0};
    }

private:
    std::uint64_t lastReport_{};
    std::uint64_t suppressed_{};
    bool started_{};
};

/** Fixed storage; an untracked key is always reported, even when the table is full. */
template<class Key, class Value, std::size_t Capacity>
class ObservationTable {
public:
    [[nodiscard]] Value* find_or_insert(const Key& key) noexcept {
        for (Entry& entry : entries_) {
            if (entry.used && entry.key == key) {
                return &entry.value;
            }
        }
        for (Entry& entry : entries_) {
            if (!entry.used) {
                entry = {key, {}, true};
                return &entry.value;
            }
        }
        return nullptr;
    }

    void clear() noexcept { entries_ = {}; }

private:
    struct Entry {
        Key key{};
        Value value{};
        bool used{};
    };
    std::array<Entry, Capacity> entries_{};
};

} // namespace sunrise::core::log
