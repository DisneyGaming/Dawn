#pragma once
#include "omega_transit_catalog.h"
namespace sunrise::state::activity::omega::transit {
struct Status {bool active{},retired{};};
inline Status status(const mission::Snapshot& s,const Source& row) noexcept {
    const auto cycle=s.command.cycle;
    const bool finalTransit=cycle==2 && (s.phase==mission::Phase::departure || s.phase==mission::Phase::arrival);
    const bool eyeVisit=s.chargeDunked && (s.phase==mission::Phase::shield || s.phase==mission::Phase::eye);
    if(row.role==Role::eyeFront) return {cycle>=1 && (cycle>1 || s.chargeDunked),false};
    if(row.role==Role::eyeBack) return {cycle>=1 && cycle<=2 && eyeVisit,cycle>2 || finalTransit || s.phase==mission::Phase::recovery};
    if(row.role==Role::eyeReturn) return {cycle>=1 && cycle<=2 && !finalTransit
        && (s.phase==mission::Phase::recovery || (cycle==2 && !eyeVisit)),cycle>2 || finalTransit || (cycle==2 && eyeVisit)};
    if(row.role==Role::destination) return row.cycle==1
        ? Status{cycle>0 && cycle<=2 && !finalTransit,cycle>=3 || finalTransit}
        : Status{cycle>=3 || finalTransit,false};
    if(row.role==Role::finalCore || row.role==Role::finalFx || row.role==Role::finalDisk)
        return {(s.transitPrepared&0x40)!=0 && (cycle>=3 || (finalTransit && s.phase==mission::Phase::arrival)),false};
    if(cycle!=row.cycle) return {false,cycle>row.cycle};
    switch(row.role) {
    case Role::ringCore:case Role::ringFx:case Role::charge:case Role::sink:return {s.chargeEnabled,s.chargeDunked};
    // Retail materializes the runway during the launch route, while the
    // player still has a gun. Keep that geometry through the subsequent dunk;
    // Deposit and its subsequent scripted transport are separate from geometry.
    case Role::bridge:return {s.chargeEnabled || s.chargePickedUp,false};
    // Stage the authored transport only after the completed deposit, then
    // retire it after qualified receiving-platform arrival. Its nested actor's
    // contact coverage still needs live verification; activation is not arrival.
    case Role::portal:return {s.chargeDunked && !s.eyePlatform && s.phase==mission::Phase::shield
        && (cycle==3 || (s.dpsFrontCreated && s.dpsBackCreated)),
        s.chargeDunked && s.eyePlatform};
    // This is the separate o_dunk_end_fx endpoint presentation. The native
    // interaction controller has no render component. Create the endpoint
    // during the charge route; waiting for dunk leaves its source uncreated
    // while the player is trying to use it. Keep it through the accepted dunk.
    case Role::endFx:return {s.chargeEnabled || s.chargeDunked,false};
    default:return {};
    }
}
inline std::uint32_t generation(const mission::Snapshot& s,const Source& row) noexcept {
    const auto value=status(s,row);
    if(row.role==Role::eyeFront) return s.generation+(value.active?1U:0U);
    if(row.role==Role::eyeBack) {
        const auto visit=s.command.cycle>2?2U:static_cast<unsigned>(s.command.cycle);
        if(!visit) return s.generation;
        if(value.active) return s.generation+2U*visit-1U;
        if(visit==1 && !value.retired) return s.generation;
        if(visit==2 && !s.chargeDunked && !value.retired) return s.generation+2;
        return s.generation+2U*visit;
    }
    if(row.role==Role::eyeReturn) {
        const auto cycle=s.command.cycle;
        if(cycle==0 || (cycle==1 && s.phase!=mission::Phase::recovery)) return s.generation;
        if(cycle==1) return s.generation+1;
        if(cycle>2 || (cycle==2 && (s.phase==mission::Phase::departure || s.phase==mission::Phase::arrival))) return s.generation+4;
        if(s.phase==mission::Phase::recovery || s.command.wave==11) return s.generation+3;
        return s.generation+(s.chargeDunked?2U:1U);
    }
    if(row.preparation>=0) return s.generation;
    return s.generation+(value.retired?2U:value.active?1U:0U);
}
inline float position(const mission::Snapshot& s,const Source& r) noexcept {
    const auto* row=&r;const auto value=status(s,r);
    float position{};
    if(row->role==Role::ringFx) position=value.retired?0.2F:value.active?0.1F:0.F;
    // Original DF1F70 with captured 80C22861 links: increasing starts
    // 80BFD0FD (phase_in), decreasing starts 80BFD0FE (phase_out).
    else if(row->role==Role::bridge) position=value.active?1.F:0.F;
    else if(row->role==Role::eyeFront) position=s.command.cycle<=2 && s.chargeDunked
        && (s.phase==mission::Phase::shield || s.phase==mission::Phase::eye)?1.F:0.F;
    else if(row->role==Role::finalFx) position=value.active?1.F:0.F;
    else position=value.retired?1.F:0.F;
    return position;
}
inline unsigned revision(const mission::Snapshot& s,const Source& row) noexcept {
    const auto value=status(s,row);
    if(row.role!=Role::eyeFront) return value.retired?2U:value.active?1U:0U;
    if(s.command.cycle==0 || (s.command.cycle==1 && !s.chargeDunked)) return 0;
    if(s.command.cycle==2 && !s.chargeDunked) return 2;
    if(s.command.cycle>2) return 4;
    return 2U*s.command.cycle-(position(s,row)==1.F?1U:0U);
}
template<class Writer> bool write(Writer& writer,const mission::Snapshot& s,
    std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    const auto* row=find(registry,type,slot);
    if(!row || !s.generation || s.generation>0x7FFFFFFAU) return false;
    const auto value=status(s,*row);
    if(type==4) return mission_devices::source(writer,generation(s,*row),value.active,row->role==Role::sink);
    const auto target=position(s,*row);const auto serial=revision(s,*row);
    // Complete native position/power/lock triplet. Untouched power and lock
    // keep their absent (-1) revision; the native device owns interpolation.
    return writer.write(std::bit_cast<std::uint32_t>(target),32)
        && writer.write(serial?0x8000U+serial:0x7FFFU,16) && writer.write(0,1)
        && writer.write(std::bit_cast<std::uint32_t>(1.F),32) && writer.write(0x7FFFU,16) && writer.write(0,1)
        && writer.write(0,32) && writer.write(0x7FFFU,16) && writer.write(0,1);
}
}
