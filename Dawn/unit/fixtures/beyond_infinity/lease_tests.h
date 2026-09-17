#pragma once
#include "../../../src/state/activity/coo/lifecycle_service.h"
#include <limits>
namespace beyond_lease_fixture {
inline void run(void (*check)(bool,const char*)) {
    namespace coo=dawn::state::activity::coo;
    coo::LifecycleService lease;
    check(!lease.reserve_through({},1),"unowned lifecycle cannot reserve revisions");
    check(lease.begin(90,2),"begin small revision lease");
    const auto owner=lease.owner();
    check(!lease.reserve_through({owner.run+1,owner.value},10)
        && !lease.reserve_through({owner.run,owner.value+1},10),"only exact current owner can extend its lease");
    check(!lease.reserve_through(owner,0) && !lease.reserve_through(owner,32767)
        && !lease.reserve_through(owner,(std::numeric_limits<std::uint32_t>::max)()),"zero and out-of-range revision reservations rejected");
    check(lease.complete(owner),"complete lifecycle before extending lease");
    check(lease.reserve_through(owner,80) && lease.reserve_through(owner,80),"lease extends beyond initial reservation and permits repeat reservation");
    check(lease.owner()==owner && lease.publication().valid() && lease.publication().owner==owner,"extension preserves owner and completion publication");
    check(lease.reserve_through(owner,owner.value),"already reserved revision may be requested without lowering high-water mark");
    lease.reset();
    check(!lease.reserve_through(owner,100),"retired owner cannot extend a lease");
    check(lease.begin(90,2) && lease.owner().value==81,"extended high-water mark survives reset and lower reservation");
    const auto next=lease.owner();
    check(!lease.reserve_through(owner,100) && !lease.reserve_through(next,next.value-1),"prior owner and revisions preceding current owner rejected");
    check(!lease.reserve_through(next,101,100) && lease.reserve_through(next,100,100),"custom reservation maximum enforced inclusively");
    check(lease.reserve_through(next,32766),"last native revision can be reserved");
    lease.reset();
    check(!lease.begin(91,1),"future lease cannot reuse the final reserved native revision");
}
}
