#pragma once
#include "transit_rules.h"
#include "runtime.h"
#include "tower.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
namespace sunrise::state::activity::newlight::launchpad::transit {
inline std::mutex mutex;
inline Transaction transaction;
inline Departure departureRequest,departed;
inline std::uint64_t nextRefresh{};
inline Departure departure() noexcept {const std::lock_guard lock(mutex);return departureRequest;}
inline bool departure_complete(Departure expected) noexcept {
    const std::lock_guard lock(mutex);
    if(!expected.scope.owner.valid() || expected!=departureRequest) {return false;}
    departed=expected;return true;
}
inline native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,
    bool exact,native::Observation observation,bool endingReady=false,bool divideRetained=false,std::uint8_t transition=0) noexcept {
    const auto wanted=launchpad::request();native::Authority result{};Scope scope{};
    {
        const std::lock_guard lock(mutex);
        if(!exact || !activity || !run || run!=mission_run_generation()) {return {};}
        if(transaction.bound() && (transaction.scope().owner.run!=run
            || (wanted.owner.valid() && wanted.owner!=transaction.scope().owner))) {transaction={};departureRequest={};departed={};nextRefresh=0;}
        // The public Divide activity shares the destination name and run. Its
        // membership cannot replace the private host's cinematic transaction.
        if(transaction.bound() && (transaction.scope().activity!=activity || transaction.scope().member!=member)) {return {};}
        if(transaction.bound()) {static_cast<void>(transaction.project(transaction.scope(),observation));}
        // Geometry may remain resident in the overlap after the public session
        // leaves. A confirmed native disconnect also makes that outgoing leg safe.
        endingReady|=divideRetained && departed.scope.owner.valid() && departed==departureRequest
            && departed.scope==transaction.scope() && departed.transition==transition;
        if(wanted.owner.valid() && wanted.owner.run==run) {
            Scope requested{wanted.owner,activity,member,wanted.frame.cinematic.route()};
            if(destination(requested).valid() && (requested.route!=2 || endingReady)) {
                static_cast<void>(transaction.next(requested,observation));
            }
        }
        if(!transaction.bound()) {return {};}
        scope=transaction.scope();result=transaction.project(scope,observation);
        // Dock13 overlaps the Divide's retention volume. Retire its outgoing
        // public session explicitly, without inventing an empty native leg.
        const auto departure=scope.owner==wanted.owner && result.complete && divideRetained
            && departure_allowed(wanted.frame.cinematic,wanted.frame.section)?Departure{scope,transition}:Departure{};
        if(departure!=departureRequest) {departureRequest=departure;departed={};}
        static Scope lastScope{};
        static native::Teleport lastHost{},lastClient{};
        static std::uint8_t lastWanted{255};static bool lastEndingReady{};
        if(scope!=lastScope || result.host!=lastHost || observation.local!=lastClient
            || wanted.frame.cinematic.route()!=lastWanted || endingReady!=lastEndingReady) {
            lastScope=scope;lastHost=result.host;lastClient=observation.local;lastWanted=wanted.frame.cinematic.route();lastEndingReady=endingReady;
            std::array<char,320> line{};
            std::snprintf(line.data(),line.size(),"ev=launchpad stage=transit run=%llu owner=%u wanted=%u route=%u host=%d/%u/%d client=%d/%u/%d region=%d receipt=%u complete=%u ending_ready=%u",
                static_cast<unsigned long long>(scope.owner.run),scope.owner.value,lastWanted,scope.route,
                result.host.state,result.host.token,result.host.sliceSetIndex,observation.local.state,
                observation.local.token,observation.local.sliceSetIndex,observation.currentRegion,
                observation.hasTeleport?1U:0U,result.complete?1U:0U,endingReady?1U:0U);
            core::log::write(core::log::Channel::server,core::log::Level::info,line.data());
        }
    }
    if(result.arrived) {launchpad::observe_arrival(scope.owner,scope.route);}
    if(result.complete && observation.hasRegion && observation.hasTeleport && observation.local.state==0
        && wanted.frame.cinematic.phase==cinematics::Phase::gameplay) {
        launchpad::observe_region(scope.owner,observation.currentRegion);
    }
    return result;
}
inline bool membership_due(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept {
    const auto wanted=launchpad::request();const std::lock_guard lock(mutex);
    if(transaction.bound() && transaction.scope().owner.run==run && transaction.scope().activity!=activity) {return false;}
    const bool pending=transaction.bound() && transaction.scope().activity==activity && transaction.scope().owner.run==run && transaction.pending();
    const bool requested=wanted.owner.valid() && wanted.owner.run==run && destination(wanted.frame.cinematic.route()).valid()
        && (!transaction.bound() || transaction.scope().route!=wanted.frame.cinematic.route());
    if((!pending && !requested) || now<nextRefresh) {return false;}nextRefresh=now+100;return true;
}
}
