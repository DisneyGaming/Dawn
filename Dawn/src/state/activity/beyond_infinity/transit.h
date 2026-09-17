#pragma once
#include "transit_rules.h"
#include "runtime.h"
#include "../runtime.h"
#include <mutex>

namespace dawn::state::activity::beyond_infinity::transit {
inline std::mutex mutex;
inline Transaction transaction;
inline std::uint64_t nextRefresh{};
inline Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,
    bool exactBeyond,Observation observation) noexcept {
    const auto requested=beyond_infinity::request();
    Authority result{};Scope scope{};
    {
        const std::lock_guard lock(mutex);
        if(!exactBeyond || !activity || !run || run!=mission_run_generation()) { return {}; }
        if(transaction.bound() && (transaction.scope().owner.run!=run
            || transaction.scope().activity!=activity || transaction.scope().member!=member)) {
            transaction={};nextRefresh=0;
        }
        const bool valid=requested.owner.valid() && requested.owner.run==run && requested.frame.enabled;
        // A temporary loading state removes mission::request(). Keep an already
        // bound native transaction alive until its matching release receipt.
        if(valid && transaction.bound() && requested.owner!=transaction.scope().owner) {
            transaction={};nextRefresh=0;
        }
        if(valid && destination(requested.frame.transitRoute).valid()) {
            const Scope wanted{requested.owner,activity,member,requested.frame.transitRoute};
            if(transaction.bound() && wanted!=transaction.scope() && !transaction.pending()) { transaction={}; }
            if(!transaction.bound() && observation.hasRegion && (requested.frame.transitContact&(1U<<wanted.route))
                && presentation_ready(requested.frame,wanted.route,observation.currentRegion)) { static_cast<void>(transaction.begin(wanted,observation)); }
        }
        if(!transaction.bound()) { return {}; }
        scope=transaction.scope();static_cast<void>(transaction.observe(scope,observation));
        result=transaction.authority(scope);
    }
    // Native arrival enters the mission executor through its generation-owned
    // receipt port after the transit mutex has been released.
    if(result.arrived) { beyond_infinity::observe_transit(scope.owner,scope.route); }
    return result;
}
inline bool membership_due(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept {
    const auto requested=beyond_infinity::request();
    const std::lock_guard lock(mutex);
    const bool pending=transaction.bound() && transaction.scope().owner.run==run
        && transaction.scope().activity==activity && transaction.pending();
    const bool requestedTransit=requested.owner.valid() && requested.owner.run==run
        && requested.frame.enabled && destination(requested.frame.transitRoute).valid()
        && (!transaction.bound() || transaction.scope().owner!=requested.owner
            || transaction.scope().route!=requested.frame.transitRoute);
    if((!pending && !requestedTransit) || now<nextRefresh) { return false; }
    nextRefresh=now+100;return true;
}
}
