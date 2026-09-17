#pragma once

#include "../../../state/activity/native_population_events.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace dawn::client::hooks::bootflow::native_generated_roster {

inline constexpr std::uint32_t kInvalidHandle=std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kInvalidNativeTag=0x811C9DC5U;
inline constexpr std::size_t kEntryStride=0x38U;
inline constexpr std::size_t kRosterRowStride=12U;
inline constexpr std::size_t kActorRowStride=0xC0U;
inline constexpr std::size_t kMaximumRosterSlots=255U;
inline constexpr std::size_t kMaximumAuthoredRows=512U;
inline constexpr std::size_t kMaximumWorkerEntries=4096U;
inline constexpr std::size_t kMaximumWorkers=4U;
inline constexpr std::size_t kMaximumCachedActors=128U;

/** One native generated-encounter roster row. Zero is a valid authored row index. */
struct RosterRow final {
    std::uint32_t nativeSpawnId{kInvalidHandle};
    std::uint32_t actorHandle{kInvalidHandle};
    std::int32_t authoredActorRow{-1};
    friend constexpr bool operator==(const RosterRow&,const RosterRow&)=default;
};

[[nodiscard]] constexpr bool invalid_row(const RosterRow& row) noexcept {
    return row.nativeSpawnId==kInvalidHandle && row.actorHandle==kInvalidHandle
        && row.authoredActorRow==0;
}

[[nodiscard]] constexpr bool valid_row(const RosterRow& row,std::size_t actorRowCount) noexcept {
    return !invalid_row(row) && row.nativeSpawnId!=0 && row.nativeSpawnId!=kInvalidHandle
        && row.actorHandle!=kInvalidHandle && row.authoredActorRow>=0
        && static_cast<std::size_t>(row.authoredActorRow)<actorRowCount;
}

/** Round-robin cursor used by the live observer.  The caller supplies the
 * current native used-entry count; capacity is never treated as initialized. */
[[nodiscard]] constexpr std::uint32_t next_entry(std::uint32_t& cursor,
    std::uint32_t usedCount) noexcept {
    if(usedCount==0) return kInvalidHandle;
    const auto index=cursor%usedCount;
    cursor=(index+1U)%usedCount;
    return index;
}

/** Contract of the existing 4F0290 getter in roster context. */
template<class Reference>
[[nodiscard]] constexpr bool valid_getter_reference(const Reference& reference) noexcept {
    return reference.handle!=kInvalidHandle
        && reference.kind==0x8080501CU && reference.offset==0;
}

[[nodiscard]] constexpr bool valid_runtime_source_header(std::uint32_t kind) noexcept {
    return kind==0x8080501DU;
}

/** The four fields used to identify one registered native generator. */
struct WorkerTuple final {
    std::uint32_t resourceTag{};
    std::uint32_t seed{};
    std::uint32_t workerDefinitionTag{};
    std::uint32_t workerDefinitionOffset{};
    friend constexpr bool operator==(const WorkerTuple&,const WorkerTuple&)=default;
};

/** Bounded fair cursors for the live generated-roster observer.  Entry order
 * is round-robin, while every entry retains its own row position.  A changed
 * worker salt/definition identity starts a fresh schedule. */
struct ScheduleCursor final {
    WorkerTuple worker{};
    std::uint32_t workerSelf{kInvalidHandle};
    std::uint32_t nextEntry{};
    bool bound{};
    std::array<std::uint16_t,kMaximumWorkerEntries> nextRows{};

    void synchronize(std::uint32_t self,const WorkerTuple& tuple) noexcept {
        if(bound && workerSelf==self && worker==tuple) return;
        worker=tuple;workerSelf=self;nextEntry=0;nextRows.fill(0);bound=true;
    }
    [[nodiscard]] std::uint32_t select_entry(std::uint32_t usedCount) noexcept {
        return native_generated_roster::next_entry(nextEntry,usedCount);
    }
    [[nodiscard]] std::uint32_t row_cursor(std::uint32_t entryIndex,
        std::uint32_t rowCount) const noexcept {
        if(entryIndex>=nextRows.size() || rowCount==0) return kInvalidHandle;
        return static_cast<std::uint32_t>(nextRows[entryIndex])%rowCount;
    }
    void advance_row(std::uint32_t entryIndex,std::uint32_t rowCount) noexcept {
        if(entryIndex>=nextRows.size() || rowCount==0) return;
        const auto row=row_cursor(entryIndex,rowCount);
        nextRows[entryIndex]=static_cast<std::uint16_t>((row+1U)%rowCount);
    }
};

[[nodiscard]] constexpr bool current_early_death_epoch(std::uint64_t witnessEpoch,
    std::uint64_t requestedEpoch,std::uint64_t currentEpoch) noexcept {
    return witnessEpoch!=0 && witnessEpoch==requestedEpoch && requestedEpoch==currentEpoch;
}

/** Structural evidence required before a roster row may authorize admission. */
struct LinkEvidence final {
    WorkerTuple worker{};
    WorkerTuple expectedWorker{};
    RosterRow row{};
    std::uint32_t workerSelf{kInvalidHandle};
    std::uint32_t expectedWorkerSelf{kInvalidHandle};
    std::uint32_t entryIndex{kInvalidHandle};
    std::uint32_t expectedEntryIndex{kInvalidHandle};
    std::size_t actorRowCount{};
    std::uint32_t actorSelf{kInvalidHandle};
    std::uint32_t parentSelf{kInvalidHandle};
    std::uint32_t parentActor{kInvalidHandle};
    std::uint32_t parentEntity{kInvalidHandle};
    std::uint32_t actorHandle{kInvalidHandle};
    std::uint32_t actorParent{kInvalidHandle};
    std::uint32_t entitySelf{kInvalidHandle};
    std::uint32_t entityHandle{kInvalidHandle};
    std::uint32_t sceneHandle{kInvalidHandle};
    std::uint32_t scenePrefab{kInvalidHandle};
    std::uint32_t authoredPrefab{kInvalidHandle};
};

[[nodiscard]] constexpr bool qualifies(const LinkEvidence& evidence) noexcept {
    return evidence.worker==evidence.expectedWorker
        && evidence.workerSelf==evidence.expectedWorkerSelf
        && evidence.entryIndex==evidence.expectedEntryIndex
        && valid_row(evidence.row,evidence.actorRowCount)
        && evidence.row.actorHandle==evidence.actorHandle
        && evidence.actorSelf==evidence.actorHandle
        && evidence.parentSelf!=kInvalidHandle && evidence.parentActor==evidence.actorHandle
        && evidence.parentEntity==evidence.entityHandle
        && evidence.actorParent==evidence.parentSelf
        && evidence.entitySelf==evidence.entityHandle
        && evidence.sceneHandle!=kInvalidHandle && evidence.scenePrefab==evidence.authoredPrefab
        && evidence.authoredPrefab!=0 && evidence.authoredPrefab!=kInvalidHandle
        && evidence.authoredPrefab!=kInvalidNativeTag;
}

/** Complete observer-side provenance retained after a qualified roster row. */
struct Provenance final {
    state::activity::native_population::Lease lease{};
    std::uint32_t sourceHandle{kInvalidHandle};
    std::uint32_t sourceKind{};
    std::int64_t sourceOffset{};
    std::uint32_t actorHandle{kInvalidHandle};
    std::uint32_t entityHandle{kInvalidHandle};
    std::uint32_t parentHandle{kInvalidHandle};
    std::uint32_t workerSelf{kInvalidHandle};
    std::uint32_t entryIndex{kInvalidHandle};
    std::uint32_t rosterRowIndex{kInvalidHandle};
    std::uint32_t authoredActorRow{kInvalidHandle};
    std::uint32_t nativeSpawnId{kInvalidHandle};
    std::uint32_t memberPrefabTag{kInvalidHandle};
    std::uint32_t completionGroup{kInvalidHandle};
};

enum class CacheIntake : std::uint8_t { accepted, duplicate, conflict, overflow, invalid };

/** Bounded value-only provenance cache. It never infers death from row absence. */
template<std::size_t Capacity=kMaximumCachedActors>
class Cache final {
public:
    [[nodiscard]] CacheIntake observe(const Provenance& value) noexcept {
        if(!valid(value)) return CacheIntake::invalid;
        for(std::size_t i=0;i<used_;++i) {
            if(same(items_[i],value)) return CacheIntake::duplicate;
            if(items_[i].actorHandle==value.actorHandle
                && items_[i].entityHandle==value.entityHandle
                && items_[i].parentHandle==value.parentHandle) return CacheIntake::conflict;
        }
        if(used_==items_.size()) return CacheIntake::overflow;
        items_[used_++]=value;return CacheIntake::accepted;
    }
    [[nodiscard]] bool find(std::uint32_t actor,std::uint32_t entity,std::uint32_t parent,
        Provenance& output) const noexcept {
        const Provenance* found{};
        for(std::size_t i=0;i<used_;++i) {
            const auto& value=items_[i];
            if(value.actorHandle!=actor || value.entityHandle!=entity || value.parentHandle!=parent) continue;
            if(found) return false;
            found=&value;
        }
        if(!found) return false;
        output=*found;return true;
    }
    [[nodiscard]] bool find_matching(std::uint32_t actor,std::uint32_t nativeSpawnId,
        std::uint32_t authoredActorRow,std::uint32_t sourceHandle,std::uint32_t sourceKind,
        std::int64_t sourceOffset,std::uint32_t workerSelf,std::uint32_t entryIndex,
        Provenance& output) const noexcept {
        const Provenance* found{};
        for(std::size_t i=0;i<used_;++i) {
            const auto& value=items_[i];
            if(value.actorHandle!=actor || value.nativeSpawnId!=nativeSpawnId
                || value.authoredActorRow!=authoredActorRow || value.sourceHandle!=sourceHandle
                || value.sourceKind!=sourceKind || value.sourceOffset!=sourceOffset
                || value.workerSelf!=workerSelf || value.entryIndex!=entryIndex) continue;
            if(found) return false;
            found=&value;
        }
        if(!found) return false;
        output=*found;return true;
    }
    void erase(std::uint32_t actor,std::uint32_t entity,std::uint32_t parent,
        const state::activity::native_population::Lease& lease) noexcept {
        for(std::size_t i=0;i<used_;++i) {
            if(items_[i].actorHandle==actor && items_[i].entityHandle==entity
                && items_[i].parentHandle==parent && items_[i].lease==lease) {
                items_[i]=items_[--used_];return;
            }
        }
    }
    void clear() noexcept { used_=0; }
    [[nodiscard]] std::size_t size() const noexcept { return used_; }
    [[nodiscard]] std::size_t snapshot(std::span<Provenance> output) const noexcept {
        const auto count=used_<output.size()?used_:output.size();
        for(std::size_t i=0;i<count;++i) output[i]=items_[i];
        return count;
    }
private:
    [[nodiscard]] static bool valid(const Provenance& value) noexcept {
        return value.lease.activity && value.lease.source.valid()
            && value.lease.source.source.type==37 && value.sourceHandle!=kInvalidHandle
            && value.sourceKind!=0 && value.actorHandle!=kInvalidHandle
            && value.entityHandle!=kInvalidHandle && value.parentHandle!=kInvalidHandle
            && value.workerSelf!=kInvalidHandle && value.entryIndex!=kInvalidHandle
            && value.rosterRowIndex!=kInvalidHandle
            && value.authoredActorRow!=kInvalidHandle && value.nativeSpawnId!=kInvalidHandle
            && value.memberPrefabTag!=0 && value.memberPrefabTag!=kInvalidHandle
            && value.memberPrefabTag!=kInvalidNativeTag;
    }
    [[nodiscard]] static bool same(const Provenance& left,const Provenance& right) noexcept {
        return left.lease==right.lease && left.sourceHandle==right.sourceHandle
            && left.sourceKind==right.sourceKind && left.sourceOffset==right.sourceOffset
            && left.actorHandle==right.actorHandle && left.entityHandle==right.entityHandle
            && left.parentHandle==right.parentHandle && left.workerSelf==right.workerSelf
            && left.entryIndex==right.entryIndex && left.rosterRowIndex==right.rosterRowIndex
            && left.authoredActorRow==right.authoredActorRow
            && left.nativeSpawnId==right.nativeSpawnId
            && left.memberPrefabTag==right.memberPrefabTag
            && left.completionGroup==right.completionGroup;
    }
    std::array<Provenance,Capacity> items_{};
    std::size_t used_{};
};

} // namespace dawn::client::hooks::bootflow::native_generated_roster
