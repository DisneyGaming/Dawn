#pragma once
#include "omega_mission_state.h"
#include <mutex>

namespace sunrise::state::activity::omega::mission_presentation {
struct View {
    std::uint64_t requested{}, submittedAt{};
    std::uint32_t objective{}, timeouts{};
    std::uint8_t pending{255};
    bool endingSubmitted{}, cinematic{};
};
// The event mapping is the documented reconstruction. Authored bank durations
// separate submitted cues; no clock creates an encounter transition.
class Queue final {
public:
    void observe(const mission::Snapshot& s,std::uint64_t now) noexcept {
        const auto run=s.command.token.boss.run;
        if(!s.generation || !run) return;
        if(run_!=run) { *this=Queue{};run_=run; }
        if(s.endingStarted) {v_.cinematic=true;v_.pending=255;return;}
        const auto cycle=s.command.cycle;
        if(cycle==1 && s.scenes[0].generation) request(14);
        if(s.rescueStartedMask&1) request(15);
        if(s.rescueReadyMask&1) request(16);
        if(s.rescueStartedMask&2) request(25);
        if(s.rescueReadyMask&2) request(26);
        if(s.rescueStartedMask&4) request(30);
        if(s.phase==mission::Phase::route || s.phase==mission::Phase::carrying) {
            v_.objective=0x85A8F583;
            if(cycle==1) request(18);
            // Documented reminder input, reconstructed at the actual charge
            // platform receipt while its item has not yet been picked up.
            if(s.chargePlatform && !s.chargePickedUp) request(cycle==3?31:21);
        } else if(s.phase==mission::Phase::eye) {
            v_.objective=0xA41DE99B;
            if(cycle==1) request(22);else if(cycle==3) request(32);
            discard(18);discard(21);discard(31);
        } else if(s.phase==mission::Phase::departure || s.phase==mission::Phase::arrival) {
            v_.objective=0xDF97334D;
            if(cycle==2) request(29);
        } else if(s.phase==mission::Phase::summon || s.phase==mission::Phase::clearance) {
            v_.objective=0x31A51CEB;
        }
        if(s.phase!=mission::Phase::eye && s.phase!=mission::Phase::shield) {discard(22);discard(32);}
        if(cycle>1) discard(18);
        if(s.nativeBossDead) request(33);
        if(v_.pending!=255 && now>=offeredAt_ && now-offeredAt_>=15000) {
            retired_|=bit(v_.pending);v_.pending=255;++v_.timeouts;
        }
        if(v_.pending==255 && now>=next_) {
            for(auto row:kRows) if((v_.requested&bit(row)) && !(retired_&bit(row))) {
                v_.pending=row;offeredAt_=now;break;
            }
        }
    }
    bool dispatch(std::uint64_t run,std::int32_t row,std::uint32_t generation,std::uint64_t now) noexcept {
        if(run_!=run || generation!=1 || row!=v_.pending || row<0 || row>=64 || v_.cinematic) return false;
        const auto index=static_cast<unsigned>(row);
        if(retired_&bit(index)) return false;
        retired_|=bit(index);v_.pending=255;next_=now+duration(index)+250;
        if(row==15) next_=now+6400;
        if(row==25) next_=now+5840;
        if(row==33) {v_.endingSubmitted=true;v_.submittedAt=now;}
        return true;
    }
    View view(std::uint64_t run) const noexcept {return run_==run?v_:View{};}
private:
    static constexpr std::array<std::uint8_t,13> kRows{14,15,16,18,21,22,25,26,29,30,31,32,33};
    static constexpr std::uint64_t bit(unsigned row) noexcept {return UINT64_C(1)<<row;}
    static constexpr unsigned duration(unsigned row) noexcept {
        switch(row) {
        case 14:return 5610;case 15:return 4431;case 16:return 6354;case 18:return 2300;
        case 21:return 3537;case 22:return 3320;case 25:return 4173;case 26:return 1712;case 29:return 3201;
        case 30:return 5488;case 31:return 2780;case 32:return 3529;case 33:return 5000+3852;default:return 0;
        }
    }
    void request(unsigned row) noexcept {v_.requested|=bit(row);}
    void discard(unsigned row) noexcept {if(!(v_.requested&bit(row)))return;retired_|=bit(row);if(v_.pending==row)v_.pending=255;}
    std::uint64_t run_{},retired_{},next_{},offeredAt_{};View v_{};
};
inline std::mutex mutex;
inline Queue queue;
inline View observe(const mission::Snapshot& s,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);queue.observe(s,now);return queue.view(s.command.token.boss.run);
}
inline bool dispatch(std::uint64_t run,std::int32_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);return queue.dispatch(run,row,generation,now);
}
inline View snapshot(std::uint64_t run) noexcept {
    const std::lock_guard lock(mutex);return queue.view(run);
}
} // namespace sunrise::state::activity::omega::mission_presentation
