#pragma once
#include "mechanisms.h"
#include "../coo/object_service.h"
#include <bitset>
#include <limits>

namespace sunrise::state::activity::eater_of_worlds {
// Contacts come from the authenticated local physics body and the exact live
// platform object. A position near an authored origin is not a contact.
struct PlatformContact {
    coo::ObjectReceipt object{};
    std::uint64_t observedAt{};
    std::uint32_t player{UINT32_MAX};
    bool supported{};
};
struct PlatformPoseReceipt {
    coo::ObjectReceipt object{};
    std::uint32_t revision{};
    float position{};
};
inline constexpr std::array<std::uint8_t,4> kPathLengths{13,11,13,19};
// All points in each recovered spawn set are inside that path's authored goal
// volume (686321C8/60/235..238). These are native spawn identities, not teleports.
inline constexpr std::array<std::uint32_t,4> kReactorGoalSpawns{
    0x1E8DBF89U,0xC4FDE8CDU,0xFD39D01CU,0x10BB2205U};
inline constexpr std::uint32_t kReactorApproachSpawn=0xAD98065DU;
inline constexpr std::size_t platform_index(coo::Asset source) noexcept {
    for(std::size_t i=0;i<std::size(kReactorPlatforms);++i)
        if(kReactorPlatforms[i].source==source) return i;
    return std::size(kReactorPlatforms);
}
struct ReactorState {
    std::bitset<56> raised{},activated{},poseAcknowledged{};
    std::array<std::uint32_t,56> poseRevision{};
    std::bitset<4> completed{};
    std::uint8_t path{},next{};
    bool aliveKnown{},dead{};
    std::uint32_t healthPlayer{UINT32_MAX};
    std::uint32_t attempt{},player{UINT32_MAX},commandRevision{};
    std::uint64_t dwellStart{},lastContact{},lastSample{};
    std::size_t dwelling{56};

    bool command(std::size_t index,bool up) noexcept {
        if(index>=56 || (poseRevision[index] && raised[index]==up)) return false;
        if(commandRevision==static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return false;
        poseRevision[index]=++commandRevision;raised[index]=up;poseAcknowledged.reset(index);
        return true;
    }
    bool begin(std::uint8_t value,std::uint32_t generation) noexcept {
        if(value<1 || value>4 || completed[value-1] || (value>1 && !completed[value-2])) return false;
        if(path==value) return true;
        path=value;next=0;++attempt;dwelling=56;dwellStart=lastContact=0;
        if(commandRevision<generation) commandRevision=generation;
        for(std::size_t i=0;i<56;++i) if(kReactorPlatforms[i].path+1==path) {
            activated.reset(i);static_cast<void>(command(i,kReactorPlatforms[i].index==0));
        }
        return true;
    }
    void interrupt() noexcept {dwelling=56;dwellStart=lastContact=0;}
    void release(std::uint64_t at) noexcept {
        if(at<=lastSample) return;
        lastSample=at;interrupt();
    }
    bool reset_attempt() noexcept {
        if(!path || completed[path-1]) {interrupt();return false;}
        ++attempt;next=0;interrupt();
        for(std::size_t i=0;i<56;++i) if(kReactorPlatforms[i].path+1==path) {
            activated.reset(i);static_cast<void>(command(i,kReactorPlatforms[i].index==0));
        }
        return true;
    }
    bool identity(std::uint32_t value) noexcept {
        if(value==UINT32_MAX || value==player) return false;
        const bool replaced=player!=UINT32_MAX;player=value;
        return replaced && reset_attempt();
    }
    // A discontinuity cancels dwell. It does not award time between samples.
    bool contact(std::size_t index,std::uint64_t at,std::uint32_t holdMs) noexcept {
        if(at<=lastSample) return false;
        lastSample=at;
        if(!path || completed[path-1] || index>=56 || !raised[index] || !poseAcknowledged[index]
            || kReactorPlatforms[index].path+1!=path || kReactorPlatforms[index].index!=next) {
            interrupt();return false;
        }
        if(dwelling!=index || !lastContact || at-lastContact>250) {
            dwelling=index;dwellStart=at;
        }
        lastContact=at;
        if(at-dwellStart<holdMs) return false;
        activated.set(index);++next;interrupt();
        for(std::size_t i=0;i<56;++i) if(kReactorPlatforms[i].path+1==path && kReactorPlatforms[i].index==next)
            static_cast<void>(command(i,true));
        return true;
    }
    bool goal(std::uint8_t value) noexcept {
        if(!value || value>4 || path!=value || completed[value-1] || next!=kPathLengths[value-1]) return false;
        completed.set(value-1);interrupt();return true;
    }
};
}
