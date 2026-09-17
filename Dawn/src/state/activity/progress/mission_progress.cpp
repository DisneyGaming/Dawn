#include "mission_progress.h"

#include <array>
#include <chrono>
#include <mutex>

#include "../../persistence/persistence.h"
namespace dawn::state::activity::progress {
namespace {

[[nodiscard]] constexpr std::uint32_t package_key(std::string_view name) noexcept {
    std::uint32_t value = 2166136261U;
    for (const char byte : name) {
        value ^= static_cast<std::uint8_t>(byte);
        value *= 16777619U;
    }
    return value;
}

[[nodiscard]] std::int64_t utc_seconds() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct CacheEntry final {
    persistence::MissionRecord record{};
    std::uint64_t lastUse{};
    bool occupied{};
};

inline constexpr std::size_t kCacheCapacity = 32;
std::array<CacheEntry, kCacheCapacity> g_cache{};
std::uint64_t g_cacheClock{};
std::mutex g_mutex;

[[nodiscard]] CacheEntry* cache_entry(std::uint64_t characterSoid,
                                      std::uint32_t missionHash) noexcept {
    CacheEntry* empty = nullptr;
    CacheEntry* oldest = nullptr;
    for (CacheEntry& entry : g_cache) {
        if (entry.occupied && entry.record.characterSoid == characterSoid
            && entry.record.missionHash == missionHash) {
            return &entry;
        }
        if (!entry.occupied && empty == nullptr) empty = &entry;
        if (entry.occupied && (oldest == nullptr || entry.lastUse < oldest->lastUse)) {
            oldest = &entry;
        }
    }
    // Cache entries only suppress identical writes. Evicting one is safe because SQLite remains
    // authoritative and the row is reloaded before the slot is reused.
    return empty != nullptr ? empty : oldest;
}

} // namespace

std::uint32_t mission_key(std::string_view packageName) noexcept {
    return package_key(packageName);
}

bool observe(std::uint64_t characterSoid,
             std::string_view packageName,
             std::int32_t activityIndex,
             std::uint32_t checkpointHash,
             std::int32_t checkpointSliceSet,
             std::int32_t progressValue,
             bool completed) noexcept {
    if (characterSoid == 0 || packageName.empty() || activityIndex < 0 || progressValue < 0
        || !persistence::enabled()) {
        return false;
    }
    const std::uint32_t missionHash = mission_key(packageName);
    if (missionHash == 0) return false;

    const std::lock_guard guard(g_mutex);
    CacheEntry* cached = cache_entry(characterSoid, missionHash);
    if (cached == nullptr) return false;
    if (!cached->occupied || cached->record.characterSoid != characterSoid
        || cached->record.missionHash != missionHash) {
        bool found = false;
        persistence::MissionRecord stored{};
        if (!persistence::load_mission(characterSoid, missionHash, found, stored)) return false;
        cached->record = found ? stored
                               : persistence::MissionRecord{characterSoid,
                                                            missionHash,
                                                            0,
                                                            0,
                                                            activityIndex,
                                                            0,
                                                            0,
                                                            false};
        cached->occupied = true;
    }
    cached->lastUse = ++g_cacheClock;

    persistence::MissionRecord candidate = cached->record;
    candidate.activityIndex = activityIndex;
    candidate.completed = candidate.completed || completed;
    if (checkpointHash != 0
        && (candidate.checkpointHash != checkpointHash
            || candidate.checkpointSliceSet != checkpointSliceSet
            || candidate.progress != progressValue)) {
        candidate.checkpointHash = checkpointHash;
        candidate.checkpointSliceSet = checkpointSliceSet;
        candidate.progress = progressValue;
    } else if (checkpointHash == 0 && progressValue > candidate.progress) {
        candidate.progress = progressValue;
    }
    if (candidate.activityIndex == cached->record.activityIndex
        && candidate.checkpointHash == cached->record.checkpointHash
        && candidate.checkpointSliceSet == cached->record.checkpointSliceSet
        && candidate.progress == cached->record.progress
        && candidate.completed == cached->record.completed) {
        return true;
    }
    candidate.updatedUtc = utc_seconds();
    if (!persistence::store_mission(candidate)) return false;
    cached->record = candidate;
    return true;
}

void reset() noexcept {
    const std::lock_guard guard(g_mutex);
    g_cache = {};
    g_cacheClock = 0;
}

} // namespace dawn::state::activity::progress
