#pragma once
#include "definition.h"
#include "../../../runtime/activity/native_activity_runtime.h"
#include "../../../../core/logging/log.h"
#include "../../../../middleware/bap/activity_message/definition.h"
#include "../../../../state/runtime/runtime.h"
#include <atomic>
#include <cstdio>

namespace sunrise::server::bap::encrypted::activity_message::adventure_start {
namespace wire=middleware::bap::activity_message::adventure_start;
namespace policy=server::runtime::activity::adventure_start;
namespace change=state::activity::adventure_destination;

// Every captured body is complete or explicitly omitted. A process-wide budget
// bounds diagnostics; this function only receives a current, owned svc8 binding.
inline void report(state::activity::ActivityInstanceKey owner,std::span<const std::byte> payload,
                   const char* result,int target=-1) noexcept {
    static std::atomic<unsigned> captureCount{};
    unsigned count=captureCount.load(std::memory_order_relaxed);
    bool captured=false;
    while(count<32) {
        if(captureCount.compare_exchange_weak(count,count+1,std::memory_order_relaxed)) {captured=true;break;}
    }
    std::array<char,core::log::kLineCapacity> line{};
    const bool full=captured && payload.size()<=state::activity::destination::kDescriptorCapacity;
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=adventure_start stage=request result=%s owner=%016llX incarnation=%llu target=%d type=11 bytes=%zu captured=%zu body=",
        result,owner.sessionId,owner.incarnation.value,target,payload.size(),full?payload.size():0);
    if(size<=0 || static_cast<std::size_t>(size)>=line.size()) return;
    auto length=static_cast<std::size_t>(size);
    if(full && !core::log::append_hex(line,length,payload)) return;
    core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),length});
}

[[nodiscard]] inline bool prepare(state::activity::ActivityInstanceKey owner,
    const middleware::bap::activity_message::Request& envelope,ActivityPlan& output) noexcept {
    output={};
    wire::Request request{};
    if(!wire::parse(envelope.payload,request)) {report(owner,envelope.payload,"malformed");return false;}
    const int target=request.selection.activityIndex;
    const auto account=state::account_snapshot();
    if(!request.hasAccount || !account.primarySoid || request.account!=account.primarySoid) {
        report(owner,envelope.payload,"account",target);return false;
    }
    change::Snapshot before{};
    if(!change::snapshot(owner,before)) {report(owner,envelope.payload,"owner",target);return false;}
    const auto& stored=before.selection;
    policy::Context context{};context.owner=owner;context.expectedRecordRevision=before.recordRevision;
    if(stored.descriptorBitLength==0 || stored.descriptorBitLength>stored.descriptorBits.size()*8
       || !wire::parse(std::span(stored.descriptorBits).first((stored.descriptorBitLength+7U)/8U),context.current)) {
        report(owner,envelope.payload,"current_descriptor",target);return false;
    }
    const server::runtime::activity::NativeActivityDefinition* definition{};
    if(!server::runtime::activity::native_activity::snapshot_placements(owner,definition,context.published)) {
        report(owner,envelope.payload,"capabilities",target);return false;
    }
    context.scenario=definition->persistentModule.asset.registry;
    policy::Plan selected{};
    const auto status=policy::prepare(context,request,definition->startRoutes,selected);
    if(status!=policy::Result::accepted) {
        constexpr const char* names[]{"accepted","invalid_owner","invalid_selection","identity","revision","route","placement"};
        report(owner,envelope.payload,names[static_cast<unsigned>(status)],target);return false;
    }
    auto destination=stored;
    const auto& incoming=selected.request.selection;
    destination.reason=incoming.reason;destination.previousActivityIndex=incoming.sourceActivityIndex;
    destination.activityIndex=incoming.activityIndex;
    destination.elementIndex=incoming.elementIndex;destination.hasElementIndex=incoming.hasElementIndex;
    // Keep this owner's existing arrival/spawn policy. Native in-world mode1
    // supplies neutral landing hashes; its raw descriptor is still echoed below.
    destination.descriptorBits=incoming.descriptorBits;
    destination.descriptorBitLength=static_cast<std::uint16_t>(incoming.descriptorBitLength);
    destination.descriptorNameBit=static_cast<std::uint16_t>(incoming.packageNameBitOffset);
    destination.hasDescriptorName=incoming.hasPackageName;
    if(change::prepare(owner,selected.expectedRecordRevision,destination,output.destinationMutation)!=change::Result::prepared) {
        report(owner,envelope.payload,"stale",target);return false;
    }
    output.instanceKey=owner;output.sessionId=owner.sessionId;
    output.mutationDomain=MutationDomain::destination;output.delivery=Delivery::globalStateNotification;
    report(owner,envelope.payload,"prepared",target);return true;
}
} // namespace sunrise::server::bap::encrypted::activity_message::adventure_start
