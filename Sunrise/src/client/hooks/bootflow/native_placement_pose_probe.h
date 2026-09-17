#pragma once
#include "native_local_placement_probe.h"
#include "coo_native_components.h"

namespace sunrise::client::hooks::bootflow::native_placement_pose_probe {
struct Trace {unsigned stage{1};float actual{},target{};std::int32_t revision{-1};};
// DF6C70 writes the target at +37C; native interpolation advances actual at +370.
// A received target/revision alone is not animation completion.
[[nodiscard]] constexpr bool settled(float actual,float target,std::int32_t revision,
    float desired,std::int32_t minimumRevision) noexcept {
    return actual==desired && target==desired && revision>=minimumRevision;
}

/** Read-only completion of an identity-checked type-4 generic device's position channel. */
[[nodiscard]] inline bool complete(gateway_native::Read& read,
    const native_local_placement_probe::Request& request,float desired,std::int32_t minimumRevision,
    Trace* trace=nullptr) noexcept {
    namespace placement=native_local_placement_probe;
    placement::Identity before{},after{};
    std::uintptr_t row{},device{};std::uint32_t bundle{};
    if(trace)*trace={};
    if(!request.active || !placement::probe(read,request,&before))return false;
    if(trace)trace->stage=2;
    if(!read.entity_row(before.child,row) || !read.value(row+0x4C,bundle)
        // 8080390E is the definition/base interface; the reflected live device is 80803910.
        || !coo_native::component<gateway_native::Read,1024>(read,bundle,before.child.handle,0x80803910U,device))return false;
    float actual{},target{};std::int32_t revision{};
    if(trace)trace->stage=3;
    if(!read.value(device+0x370,actual) || !read.value(device+0x37C,target)
        || !read.value(device+0x960,revision))return false;
    if(trace)*trace={4,actual,target,revision};
    if(!settled(actual,target,revision,desired,minimumRevision))return false;
    if(trace)trace->stage=5;
    if(!placement::probe(read,request,&after) || before!=after)return false;
    if(trace)trace->stage=0;
    return true;
}
}
