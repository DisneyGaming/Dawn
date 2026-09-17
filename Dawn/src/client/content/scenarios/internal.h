#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "../../../middleware/content/packages/reader/reader.h"
#include "../../../middleware/content/packages/tables/roster_intersection.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../../../state/build_data/scenarios/definition.h"

namespace dawn::client::content::scenarios {

/** Scenario tag for mission_towerfall, used to keep extraction diagnostics narrowly scoped. */
inline constexpr std::uint32_t kTowerfallScenarioTag = 0x80B500BCU;

namespace layouts = state::build_data::scenarios;
namespace reader = middleware::content::packages::reader;

/**
 * Placed-object tags the memo holds. The walk reaches 5,826 distinct objects over the installed
 * packages, and the table needs headroom to stay a cheap open-addressed probe.
 */
inline constexpr std::size_t kObjectMemoCapacity = 16'384;
/** Memo value for an object that declares no roster slot type. */
inline constexpr std::uint16_t kNotARosterGroup = 0xFFFF;
/** One memo row: a placed-object tag and the roster group it produced. */
struct ObjectMemo {
    std::uint32_t tag{};
    std::uint32_t registryKey{};
    /** Explicit, non-global ObjectBubble indices declared by this object. */
    std::uint64_t explicitSliceMask{};
    std::uint16_t group{kNotARosterGroup};
    /** Bit 0/1/2: declared dialogue, music, and directive root slots. */
    std::uint8_t authoredRootCueMask{};
    bool carriesRosterSlot{};
    bool completeLayout{};
};

/** One descriptor-backed slot, including its real (possibly non-contiguous) object index. */
struct SlotRecord {
    std::uint16_t index{};
    std::uint8_t type{};
    std::uint8_t flags{};
    std::uint32_t descriptorTag{};
    std::uint32_t descriptorOffset{};
    std::uint32_t componentClass{};
    std::uint32_t senseSchema{};
    std::uint32_t authSchema{};
};

/** Fixed working storage for one roster pass, kept off the caller stack. */
struct RosterStorage {
    std::vector<std::byte> scenario;
    std::vector<std::byte> entry;
    std::vector<std::byte> registry;
    std::vector<std::byte> object;
    std::vector<std::byte> chain;
    std::array<ObjectMemo, kObjectMemoCapacity> memo{};
    std::array<layouts::RosterGroup, layouts::kRosterGroupCapacity> groups{};
    std::size_t groupCount{};
    /** Descriptors found on the object being resolved. */
    std::array<SlotRecord, layouts::kRosterSlotCapacity> slots{};
    std::size_t slotCount{};
    bool slotsOverflowed{};
    /** Group objects whose descriptor walk produced no usable publish shape. */
    std::size_t unresolvedGroups{};
    /** Selected slices whose authored overlay exceeded a fixed or wire capacity. */
    std::size_t authoredOverflows{};
    /** Authored layouts refused because the global catalog reserve was exhausted. */
    std::size_t catalogOverflows{};
    /** Ordinary destination publications rejected instead of truncating fixed group arrays. */
    std::size_t ordinaryOverflows{};
    /** Authored slices rejected because an object or descriptor chain did not read completely. */
    std::size_t authoredReadFailures{};
    /** Destination/slice publications rejected for one registry key resolving to two layouts. */
    std::size_t keyConflicts{};
    /** Destinations walked so far. The walk resumes here on the next call. */
    std::size_t cursor{};
    /** Tag reads spent in the current call, which is what bounds how long it blocks. */
    std::size_t reads{};
};

/** Tag-read budget bounds one process-freeze interval and keeps worker shutdown responsive. */
inline constexpr std::size_t kRosterReadBudget = 150;

/** Live scenario tags found by the class sweep. The measured live count is 468. */
inline constexpr std::size_t kLiveTagCapacity = 1'024;
/**
 * How long the collection keeps retrying the destinations that have not read yet.
 * Packages register during the boot, so an early attempt reads fewer of them. One run latched at
 * 417 of 466 and the destination it dropped was the Tower.
 */
inline constexpr std::uint64_t kResolveWindowMs = 15'000;
/** Tag reads one collection call may spend, for the same reason the roster walk is bounded. */
inline constexpr std::size_t kResolveReadBudget = 150;

/** One live scenario tag and the map-package stem of the package that carries it. */
struct LiveTag {
    std::uint32_t tag{};
    std::array<char, layouts::kSpawnStemCapacity> stem{};
    std::uint8_t stemLength{};
};

/** One pass of fixed storage, kept off the caller stack. */
struct Storage {
    std::array<LiveTag, kLiveTagCapacity> liveTags{};
    std::size_t liveTagCount{};
    std::array<layouts::Definition, layouts::kDefinitionCapacity> rows{};
    std::size_t rowCount{};
    /** Patch index each row's tag came from, so a later patch replaces an earlier one. */
    std::array<std::uint32_t, layouts::kDefinitionCapacity> rowPatch{};
    /** One byte per row: set once its bubble layout has read. */
    std::array<std::uint8_t, layouts::kDefinitionCapacity> resolved{};
    std::size_t resolvedCount{};
    /** Rows whose tag the class sweep still carries. Only these can ever read. */
    std::size_t liveRowCount{};
    /** Where the next resolve pass starts, so every pending row is retried in turn. */
    std::size_t resolveCursor{};
    /** Tick after which the collection stops waiting for the rows that have not read. */
    std::uint64_t resolveDeadlineTick{};
    std::vector<std::byte> blob{};
    RosterStorage roster{};
    /** Resolved rows, moved to the front. The roster walk runs over exactly these. */
    std::size_t keptCount{};
    /** Retried rounds of the resolve window, so a boot that never reads reports it once. */
    std::uint32_t resolveRounds{};
    /** Set once the sweep and the name match are done, so they run once per boot. */
    bool collected{};
    /** Set once the resolved rows are compacted and the roster walk may start. */
    bool compacted{};
};

/**
 * Sweeps the installed packages for scenario tags and matches them to destination names.
 * @param source Package directory and borrowed block keys.
 * @param storage Pass storage receiving the live tags and the named rows.
 * @param reason Receives the step that refused, or stays null.
 * @return True when both steps finished.
 */
[[nodiscard]] bool
collect_rows(const reader::Source& source, Storage& storage, const char*& reason) noexcept;

/**
 * Reads the bubble layout of rows that have not read yet, within this call's budget.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Pass storage carrying the resolve cursor.
 * @return True when the collection has settled and may be compacted.
 */
[[nodiscard]] bool
resolve_pending(const reader::Source& source, reader::Scratch& scratch, Storage& storage) noexcept;

/**
 * Moves every resolved row to the front of the row array.
 * @param storage Pass storage whose kept count is set here.
 */
void compact_rows(Storage& storage) noexcept;

/**
 * Re-arms the resolve window so a pass that read nothing tries the whole row set again.
 * @param storage Pass storage whose resolve state is cleared.
 */
void rearm_resolve(Storage& storage) noexcept;

/** Reduces one descriptor's schemas to the reset flags encoded for its slot. */
[[nodiscard]] constexpr std::uint8_t slot_flags(std::uint32_t authSchema,
                                                 std::uint32_t senseSchema) noexcept {
    namespace wire = middleware::content::packages::tables;
    std::uint8_t flags = 0;
    if (authSchema != wire::kAbsentSchema) {
        flags |= layouts::kSlotAuthFlag;
    }
    if (senseSchema != wire::kAbsentSchema) {
        flags |= layouts::kSlotSenseFlag;
    }
    return flags;
}

/** Records one descriptor as a real indexed slot of the object being resolved. */
void record_slot(RosterStorage& storage,
                 const middleware::content::packages::tables::SlotDescriptor& descriptor) noexcept;

/** Fills one group from the descriptors already collected for it. */
[[nodiscard]] bool fill_slots(RosterStorage& storage,
                              std::span<const std::byte> objectBlob,
                              const middleware::content::packages::tables::Array& declaredSlots,
                              layouts::RosterGroup& group,
                              bool allowPartial = false) noexcept;

/** One candidate group of one destination, with what its publish order is sorted on. */
struct Candidate {
    std::uint16_t group{};
    std::uint32_t key{};
    bool bindsPlayer{};
    bool reportsLifetime{};
    bool primaryRegistry{};
};

/** One group published only in the bubbles selected by its mask. */
struct BubbleCandidate {
    std::uint16_t group{};
    std::uint64_t mask{};
};

/** Content context that decides whether a partial descriptor layout is authored for this slice. */
struct ResolveContext {
    std::uint32_t scenarioTag{};
    std::uint32_t scenarioHash{};
    std::uint16_t scenarioPackage{};
    std::uint8_t slice{};
    std::uint8_t registry{};
};

/** Stable terminal state of one object-resolution attempt for extraction diagnostics. */
enum class ResolveDisposition : std::uint8_t {
    none,
    resolved,
    notRelevant,
    objectReadFailed,
    objectKeyMissing,
    bubbleLayoutInvalid,
    slotsAbsent,
    slotCapacity,
    descriptorWalkFailed,
    slotFillFailed,
    catalogCapacity,
};

/** One resolved object and the independent roles it proved in its current registry context. */
struct ResolvedObject {
    std::uint16_t group{kNotARosterGroup};
    std::uint32_t objectTag{};
    std::uint32_t registryKey{};
    std::uint64_t explicitSliceMask{};
    std::uint64_t declaredSlotCount{};
    std::size_t collectedSlotCount{};
    ResolveDisposition disposition{ResolveDisposition::none};
    std::uint8_t authoredRootCueMask{};
    bool ordinary{};
    bool scenarioRoot{};
    bool selectedLocal{};
    bool authoredRejected{};
    bool slotsOverflowed{};
};

/** Authored candidates observed in one registry state before cross-state intersection. */
struct AuthoredObservation {
    std::uint16_t root{kNotARosterGroup};
    std::array<std::uint16_t, layouts::kDestinationAuthoredGroupCapacity> locals{};
    std::size_t localCount{};
    bool overflowed{};
    bool keyConflict{};
};

/** Authored candidates safe across every registry state observed for one slice. */
struct AuthoredSlice {
    std::uint16_t root{kNotARosterGroup};
    std::array<std::uint16_t, layouts::kDestinationAuthoredGroupCapacity> locals{};
    std::size_t localCount{};
    bool observed{};
    bool overflowed{};
    bool keyConflict{};
};

/** @return True when both groups carry the same registry key and full wire slot layout. */
[[nodiscard]] constexpr bool same_group_layout(const layouts::RosterGroup& left,
                                               const layouts::RosterGroup& right) noexcept {
    if (left.registryKey != right.registryKey || left.slotCount != right.slotCount) {
        return false;
    }
    for (std::size_t slot = 0; slot < left.slotCount; ++slot) {
        if (left.slotTypes[slot] != right.slotTypes[slot]
            || left.slotFlags[slot] != right.slotFlags[slot]
            || left.slotIndices[slot] != right.slotIndices[slot]
            || left.descriptorTags[slot] != right.descriptorTags[slot]
            || left.descriptorOffsets[slot] != right.descriptorOffsets[slot]
            || left.componentClasses[slot] != right.componentClasses[slot]
            || left.senseSchemas[slot] != right.senseSchemas[slot]
            || left.authSchemas[slot] != right.authSchemas[slot]) {
            return false;
        }
    }
    return true;
}

/** Everything one destination's walk builds up. */
struct Walk {
    middleware::content::packages::tables::RosterIntersection intersection{};
    std::array<Candidate, middleware::content::packages::tables::kRosterKeyCapacity> candidates{};
    std::size_t candidateCount{};
    /** Bit set for each measured Omega authored key observed as an ordinary candidate. */
    std::uint8_t measuredOmegaMask{};
    std::array<AuthoredSlice, layouts::kBubbleCapacity> authored{};
    /** An unread entry has no known slice ordinal, so no authored overlay can be proven. */
    bool authoredUnresolved{};
    /** An ordinary key resolved to multiple layouts in this destination. */
    bool ordinaryKeyConflict{};
    /** A fixed ordinary candidate or publish array could not hold the complete result. */
    bool ordinaryOverflowed{};
};

/** @return One bit identifying an exact measured Omega group, or zero for every other key. */
[[nodiscard]] std::uint8_t measured_omega_key_bit(std::uint32_t key) noexcept;

/**
 * Splits safe-everywhere and bubble-local candidates into the destination row's two lists.
 * @param walk Accumulator for one destination.
 * @param row Destination row receiving its group indices.
 */
void publish_groups(Walk& walk, RosterStorage& storage, layouts::Definition& row) noexcept;

/**
 * Finds the roster group of one placed object, reading it only the first time it is seen.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Working storage for this pass.
 * @param objectTag Tag from an object registry.
 * @param group Receives the roster group index, or the not-a-group sentinel.
 * @return True when the object was read or was already known.
 */
[[nodiscard]] bool resolve_object(const reader::Source& source,
                                  reader::Scratch& scratch,
                                  RosterStorage& storage,
                                  std::uint32_t objectTag,
                                  const ResolveContext& context,
                                  ResolvedObject& output) noexcept;

/**
 * Walks the next batch of destination rows for their roster groups.
 * One call spends at most the read budget and then returns, so the pass resumes across calls.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Working storage carrying the cursor between calls.
 * @param rows Destination rows whose tag is already set, updated in place.
 * @return True when every row has been walked.
 */
[[nodiscard]] bool build_rosters(const reader::Source& source,
                                 reader::Scratch& scratch,
                                 RosterStorage& storage,
                                 std::span<layouts::Definition> rows) noexcept;

} // namespace dawn::client::content::scenarios
