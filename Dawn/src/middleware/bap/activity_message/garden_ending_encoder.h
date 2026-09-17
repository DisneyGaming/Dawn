#pragma once
#include "sensor_auth_update.h"
#include "../../../state/activity/omega/omega_ending_authority.h"
namespace dawn::middleware::bap::activity_message::sensor_auth_update::garden_ending {
namespace garden=state::activity::strike_bond;
struct RosterProjection {
    Roster roster{};
    std::array<BubbleSubBlock,64> blocks{};
    std::array<std::array<std::uint32_t,96>,64> keys{};
    std::array<std::array<std::uint8_t,96>,64> presence{},states{};
    bool prepare(const Snapshot& s) noexcept {
        roster=s.roster;const auto count=roster.bubbleSubBlocks.size();
        if(count>=blocks.size()) return false;
        std::size_t movie=count;bool arena=false;
        for(std::size_t i=0;i<count;++i) {
            const auto& b=roster.bubbleSubBlocks[i];const auto n=b.keys.size();
            if(!n || n>=keys[i].size() || (!b.states.empty() && b.states.size()!=n)) return false;
            if(b.bubble==17) {
                constexpr std::array<std::uint32_t,4> expected{0x2CB86C0FU,0xA1F8CD1CU,0xB9395B1BU,0xC80A735BU};
                if(n!=expected.size()) return false;
                for(std::size_t k=0;k<n;++k) if(b.keys[k]!=expected[k]) return false;
                arena=true;
            }
            if(b.bubble==3) movie=i;
            for(std::size_t k=0;k<n;++k) {
                keys[i][k]=b.keys[k];states[i][k]=b.states.empty()?static_cast<std::uint8_t>(128+s.stateSequence):b.states[k];
            }
            blocks[i]={b.bubble,{keys[i].data(),n},{presence[i].data(),n},{states[i].data(),n}};
        }
        if(!arena) return false;
        auto n=movie<count?blocks[movie].keys.size():0;
        // Never insert before a published block or key: native cleanup uses
        // these ordinals. Append the authored cinematic group once.
        for(std::size_t k=0;k<n;++k) if(keys[movie][k]==garden::kEndingMovieRegistry) {
            presence[movie][k]=1;roster.bubbleSubBlocks={blocks.data(),count};return true;
        }
        keys[movie][n]=garden::kEndingMovieRegistry;presence[movie][n]=1;states[movie][n]=128+s.stateSequence;
        blocks[movie]={3,{keys[movie].data(),n+1},{presence[movie].data(),n+1},{states[movie].data(),n+1}};
        roster.bubbleSubBlocks={blocks.data(),count+(movie==count?1:0)};return true;
    }
};
inline bool write(encoding::bits::Writer& w,const Snapshot& s) noexcept {
    RosterProjection projection;if(!projection.prepare(s))return false;
    const auto& f=s.strike_bond.endingFlow;
    bool ok=w.write(0,kHardwipeWidth)&&w.write(s.patchEpoch.first,kEpochWidth)&&w.write(s.patchEpoch.second,kEpochWidth)
        &&w.write(s.hasGrant?1:0,1);
    if(ok&&s.hasGrant)ok=legacy_write_bubble_block(w,s.grant);
    ok=ok&&native::activity_clock::write_elapsed(w,s.activityClock?s.activityElapsedTicks:s.gameplayClockTicks)
        &&w.write(1,1)&&legacy_write_roster_delta(w,projection.roster,s.stateSequence);
    // Arrival creates a fresh native global pool. Seed all 21 real records;
    // the script and lifetime retain the existing verified authority bodies.
    // Keep lifetime publication after playback so state8 requests native orbit.
    if(ok && f.arrived && (!f.started || f.finished)) {
        ok=w.write(1,1)&&w.write(0x4786C0E0,32)&&w.write(0,32)
            &&legacy_write_object_block(w,s,0x4786C0E0,17,3,2,false)
            &&legacy_write_object_block(w,s,0x4786C0E0,35,1,2,false);
        if(!f.started) {
            constexpr std::array<std::uint8_t,21> types{16,35,18,17,41,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13};
            for(unsigned i=0;ok && i<types.size();++i) {
                if(i==1||i==3)continue;
                const bool sense=i>=4;
                ok=w.write(1,1)&&w.write(0x4786C0E0,32)&&w.write(types[i]+kSlotTypeBias,kSlotTypeWidth)
                    &&w.write(i+kSlotIndexBias,kSlotIndexWidth)&&w.write(sense?3:2,32)
                    &&w.write(1,1)&&w.write(0,1)&&(!sense||w.write(0,1));
            }
        }
        ok=ok&&w.write(0,1);
    }
    return ok&&w.write(1,1)&&w.write(garden::kEndingMovieRegistry,32)&&w.write(0,32)
        &&w.write(1,1)&&w.write(garden::kEndingMovieRegistry,32)
        &&w.write(6+kSlotTypeBias,kSlotTypeWidth)&&w.write(kSlotIndexBias,kSlotIndexWidth)
        &&w.write(265,32)&&w.write(1,1)&&w.write(1,1)
        &&state::activity::omega::ending::write(w,f.revision,f.play)
        &&w.write(0,1)&&w.write(0,1)&&w.write(0,1);
}
}
