#pragma once

#include "definition.h"
#include "destination/activity_destination_validation.h"
#include "transactions/internal.h"

namespace sunrise::state::activity::adventure_destination {

struct Snapshot final {
    ActivityInstanceKey owner{};
    std::uint64_t recordRevision{};
    destination::DestinationSelection selection{};
};
struct Pending final {
    Snapshot before{};
    destination::DestinationSelection after{};
    bool prepared{};
};
enum class Result : std::uint8_t { prepared,invalidSelection,unjoinedOwner,staleRevision,exhausted };

// Same-host activity selection is a descriptor change. It does not allocate a
// session, reset its membership, grant a new region, or choose a spawn point.
// Server policy has already authenticated the request and selected its route.
[[nodiscard]] inline Result prepare_from(const ActivityState& state,ActivityInstanceKey owner,
                                        std::uint64_t expectedRecordRevision,
                                        const destination::DestinationSelection& selection,
                                        Pending& output) noexcept {
    output={};
    if(!destination::valid(selection) || selection.packageNameLength==0
       || selection.packageNameLength>=selection.packageName.size()
       || selection.descriptorBitLength==0
       || selection.descriptorBitLength>selection.descriptorBits.size()*8
       || static_cast<std::size_t>(selection.descriptorNameBit)+selection.packageName.size()*8>selection.descriptorBitLength
       || !selection.hasDescriptorName || selection.reason<destination::kMinimumReason
       || selection.reason>destination::kMaximumReason
       || selection.activityIndex<0 || selection.activityIndex>destination::kMaximumActivityIndex
       || selection.previousActivityIndex<destination::kAbsentActivityIndex
       || selection.previousActivityIndex>destination::kMaximumActivityIndex) return Result::invalidSelection;
    const auto slot=activity::transactions::find_session(state,owner);
    if(slot>=state.sessions.size() || !state.sessions[slot].joined) return Result::unjoinedOwner;
    const auto& record=state.sessions[slot];
    if(record.recordRevision!=expectedRecordRevision || expectedRecordRevision==0) return Result::staleRevision;
    if(record.destination.packageNameLength!=selection.packageNameLength
       || record.destination.packageName!=selection.packageName) return Result::invalidSelection;
    if(state.stateRevision==kMaximumRevision) return Result::exhausted;
    auto after=record.destination;
    after.reason=selection.reason;
    after.previousActivityIndex=selection.previousActivityIndex;
    after.activityIndex=selection.activityIndex;
    after.hasElementIndex=selection.hasElementIndex;after.elementIndex=selection.elementIndex;
    // Mode1 changes the selected activity inside the existing world. Its native
    // descriptor carries 811C9DC5 arrival/spawn sentinels, not a new landing.
    // Retain the original arrival/spawn policy and all explicit overrides;
    // replacing these scalars makes roster resolution fall back to region0.
    // The exact incoming descriptor below still reaches the original client.
    after.descriptorBits=selection.descriptorBits;after.descriptorBitLength=selection.descriptorBitLength;
    after.descriptorNameBit=selection.descriptorNameBit;after.hasDescriptorName=selection.hasDescriptorName;
    output={{owner,record.recordRevision,record.destination},after,true};
    return Result::prepared;
}
[[nodiscard]] inline bool commit_to(ActivityState& state,Pending& pending) noexcept {
    const auto plan=pending;pending={};
    if(!plan.prepared) return false;
    Pending checked{};
    if(prepare_from(state,plan.before.owner,plan.before.recordRevision,plan.after,checked)!=Result::prepared) return false;
    const auto slot=activity::transactions::find_session(state,plan.before.owner);
    auto& record=state.sessions[slot];
    record.destination=checked.after;
    record.recordRevision=++state.stateRevision;
    return true;
}

// State-lock wrappers for production integration. The pure functions above are
// shared with the standalone transaction tests.
[[nodiscard]] bool snapshot(ActivityInstanceKey owner,Snapshot& output) noexcept;
[[nodiscard]] Result prepare(ActivityInstanceKey owner,std::uint64_t expectedRecordRevision,
                             const destination::DestinationSelection& selection,Pending& output) noexcept;
[[nodiscard]] bool commit(Pending& pending) noexcept;
} // namespace sunrise::state::activity::adventure_destination
