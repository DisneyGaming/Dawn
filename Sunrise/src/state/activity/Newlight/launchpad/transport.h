#pragma once
#include "native_catalog.h"
#include "../../coo/actor_program_service.h"
#include "../../../../middleware/bap/activity_message/combatant_sense.h"

namespace sunrise::state::activity::newlight::launchpad::transport {
namespace atom=coo::native_atom;
inline constexpr coo::Asset kCarrier=asset(kDivide,1,3),kPilot=asset(kDivide,2,4),kTank=asset(kDivide,1,1),
    kTankMember=asset(kDivide,2,2),kPassenger=asset(kDivide,39,14);
inline constexpr atom::Ref kEntrance{kDivide,58,25},kExit{kDivide,58,26};
// The byte in move_to selects an authored marker, not the entire spline.
// Entrance markers are 0, 4.2121, 9.3295, 13 seconds; exit markers 0, 8.0317, 15.
inline constexpr std::uint8_t kEntranceLast=3,kExitLast=2;
enum class Phase : std::uint8_t {idle,board,enter,deliver,leave,retired};
struct State {
    coo::ActorPublication publication{};coo::CombatantState receipt{};Phase phase{};
    std::uint64_t earliestDeparture{};std::uint32_t passengerRevision{};bool passengerAccepted{};
    void begin(std::uint32_t generation) noexcept {
        if(phase!=Phase::idle) {return;}
        phase=Phase::board;passengerRevision=generation;
        auto& p=publication.program;p.generation=generation;p.revision=1;p.spawn=true;
        p.atoms[0]=atom::snap_to(kEntrance);p.count=1;
    }
    bool attached() const noexcept {return phase==Phase::board || phase==Phase::enter;}
    template<class W> bool passenger(W& w) const noexcept {
        // Authored 80804EE5: revision31 + passenger ClientRef55. Its definition
        // already names this Skiff and socket 4B3BD988. F9DE80 acknowledges only
        // after native attach/detach completes; no parallel cargo spawn is needed.
        return passengerRevision && w.write(passengerRevision,31)
            && (attached()?(w.write(kDivide,32) && w.write(3,7) && w.write(32770,16))
                :(w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767,16)));
    }
    bool passenger(std::uint32_t revision,std::uint32_t registry,std::int8_t type,std::int16_t slot) noexcept {
        if(phase==Phase::idle || passengerAccepted || revision!=passengerRevision
            || (attached()?(registry!=kDivide || type!=2 || slot!=2)
                :(registry!=0x811C9DC5U || type!=-1 || slot!=-1))) {return false;}
        passengerAccepted=true;return true;
    }
    void observe(const middleware::bap::activity_message::combatant_sense::Output& delta) noexcept {
        if(phase==Phase::idle || phase==Phase::retired || (delta.hasSpawnRevision && delta.spawnRevision!=publication.program.generation)) {return;}
        auto final=receipt;auto levels=delta;levels.detached=false;final.merge(levels);
        if(delta.detached && phase==Phase::leave
            && (final.program(publication.program.generation,4,kExitLast) || final.program(publication.program.generation,4,kExitLast+1))) {retire();return;}
        receipt.merge(delta);
    }
    void advance(bool tankAdmitted,std::uint64_t now) noexcept {
        auto& p=publication.program;const auto generation=p.generation;
        if(phase==Phase::board && tankAdmitted && passengerAccepted && receipt.program(generation,1,1)) {
            phase=Phase::enter;p.revision=2;p.count=kEntranceLast;
            for(std::uint8_t i=0;i<kEntranceLast;++i) {p.atoms[i]=atom::move_to(kEntrance,i+1,true);}
        } else if(phase==Phase::enter && receipt.program(generation,2,kEntranceLast)) {
            phase=Phase::deliver;earliestDeparture=now+1000;++passengerRevision;passengerAccepted=false;
            p.revision=3;p.atoms[0]=atom::set_channel(0x80296344U,1.F);p.count=1;
        } else if(phase==Phase::deliver && passengerAccepted && now>=earliestDeparture && receipt.program(generation,3,1)) {
            phase=Phase::leave;p.revision=4;
            for(std::uint8_t i=0;i<kExitLast;++i) {p.atoms[i]=atom::move_to(kExit,i+1,true);}
            p.atoms[kExitLast]=atom::ability(0x07EBF354U,0x7D0D39A9U);p.count=kExitLast+1;
        } else if(phase==Phase::leave && receipt.program(generation,4,kExitLast+1)) {retire();}
    }
    void retire() noexcept {
        if(phase==Phase::idle || phase==Phase::retired) {return;}
        if(attached()) {++passengerRevision;passengerAccepted=false;}
        phase=Phase::retired;publication.retirementGeneration=publication.program.generation+1;
    }
};
}
