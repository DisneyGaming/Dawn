#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../state/activity/Newlight/launchpad/tower.h"
#include "../../../state/activity/Newlight/launchpad/transit.h"

namespace sunrise::client::hooks::bootflow::launchpad_handoff {
namespace native=omega_activity_handoff;
namespace mission=state::activity::newlight::launchpad;
namespace tower=mission::tower;
struct Exit {std::uint64_t run{},nonce{};std::uintptr_t session{};std::int16_t index{-1};bool pending{};};
inline Exit exit;
inline void depart_divide(const mission::Request& request) noexcept {
    namespace transit=mission::transit;
    static transit::Departure sent{};
    static std::uintptr_t leaving{};static bool complete{};
    const auto wanted=transit::departure();
    if(!wanted.scope.owner.valid() || wanted.scope.owner!=request.owner
        || request.owner.run!=state::activity::mission_run_generation() || !in_world()
        || !transit::departure_allowed(request.frame.cinematic,request.frame.section)) {return;}
    if(wanted==sent && complete) {return;}
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    using Target=std::uintptr_t(__fastcall*)(std::int32_t);
    using Leave=void(__fastcall*)(std::int32_t);
    const auto world=native::resolve<native::World>(base,0xC03430,{0x40,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B});
    const auto target=native::resolve<Target>(base,0xC265C0,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x63});
    const auto leave=native::resolve<Leave>(base,0xC27B90,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x63});
    if(!world || !target || !leave) {return;}
    const auto manager=world();std::uint8_t enabled{};std::int32_t current{},state{};
    if(!manager || !native::read(manager+8,enabled) || !enabled
        || !native::read(manager+0x722A0,current) || current<0 || current>1) {return;}
    const auto session=target(2);
    if(session!=manager+0x722A8+static_cast<std::uintptr_t>(current^1)*0x1C8A0
        || !native::read(session+0x1AEF8,state)) {return;}
    if(transit::departure()!=wanted || mission::native_owner()!=request.owner || target(2)!=session) {return;}
    if(wanted==sent) {
        if(session==leaving && state==0 && transit::departure_complete(wanted)) {
            complete=true;
            core::log::write(core::log::Channel::client,core::log::Level::info,"ev=launchpad stage=divide_departure result=complete");
        }
        return;
    }
    if(state!=4 && state!=6) {return;}
    // Retail's transition cleanup calls C27B90(2): leave the alternate public
    // session through 178CE40, retaining the current session and fireteam.
    sent=wanted;leaving=session;complete=false;leave(2);
    std::array<char,192> line{};
    std::snprintf(line.data(),line.size(),"ev=launchpad stage=divide_departure result=requested run=%llu owner=%u transition=%u session=%p",
        static_cast<unsigned long long>(request.owner.run),request.owner.value,wanted.transition,reinterpret_cast<void*>(session));
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
inline bool named(native::Name lookup,std::int16_t index,std::string_view expected) noexcept {
    const auto address=reinterpret_cast<std::uintptr_t>(lookup(index));
    for(std::size_t i=0;i<=expected.size();++i) {char value{};if(!native::read(address+i,value) || value!=(i<expected.size()?expected[i]:'\0')) {return false;}}
    return true;
}
inline void poll() noexcept {
    const auto now=GetTickCount64();tower::tick(now);
    const auto request=mission::request();
    depart_divide(request);
    if(request.owner.valid() && request.frame.towerRequested) {static_cast<void>(tower::begin(request.owner));}
    auto state=tower::state();
    if(state.phase==tower::Phase::towerArriving && in_world() && state::activity::mission_seed_armed()) {
        static_cast<void>(tower::arrived(state::activity::mission_run_generation(),state::activity::world_phase()));
        state=tower::state();
    }
    if(state.phase==tower::Phase::complete) {mission::finish_handoff(state.origin);return;}
    if(state.phase==tower::Phase::failed || state.phase==tower::Phase::idle) {exit={};return;}
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(exit.pending) {
        const auto target=state.phase==tower::Phase::approachLoading?2:
            state.phase==tower::Phase::towerLoading?84:-1;
        if(target<0 || exit.index!=target || exit.run!=state.run
            || exit.run!=state::activity::mission_run_generation()) {exit={};return;}
        std::uintptr_t session{};std::uint8_t flags{};std::uint16_t index{};std::uint64_t nonce{};native::Effective effective{};
        if(!native::fireteam_session(base,session) || session!=exit.session
            || !native::read(session+native::kTransitionProperty+0x140,flags) || !(flags&1U)
            || !native::read(session+native::kTransitionProperty+native::kRequestValue+4,index) || index!=exit.index
            || !native::read(session+native::kTransitionProperty+native::kRequestValue+0x18,nonce) || nonce!=exit.nonce
            || !native::read_effective(session,effective) || effective.kind==native::kKindUnfilled) {return;}
        const auto step=native::resolve<native::Step>(base,0xE2D510,{0x48,0x83,0xEC,0x28,0xE8,0x07,0x83,0x00});
        const auto leave=native::resolve<native::ReportFailure>(base,0xE2DEB0,{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57});
        if(!step || !leave || step()!=native::kInWorldStep) {return;}
        exit.pending=false;
        // Reuse the loading-only suppression window for these confirmed launches.
        // It releases on return to in_world; Tower's authored movie has its own
        // controller. Preserve the native transition descriptor and world teardown.
        native::arm_suppress(exit.run,now,native::kInWorldStep);
        std::array<char,160> line{};std::snprintf(line.data(),line.size(),
            "ev=launchpad stage=handoff result=loading_cinematics_suppressed run=%llu activity=%d",
            static_cast<unsigned long long>(exit.run),exit.index);
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        leave(native::kCleanupStep,native::kBenignExitReason);return;
    }
    const std::int16_t target=state.phase==tower::Phase::approachRequested?2:
        state.phase==tower::Phase::towerRequested && state.host.state==0?84:-1;
    if(target<0 || state.run!=state::activity::mission_run_generation()) {return;}
    static std::uint64_t next{};if(now<next) {return;}next=now+250;
    const auto sessionReady=native::resolve<native::Ready>(base,0x1788810,{0x83,0xB9,0x6C,0x08,0x00,0x00,0x00,0x0F});
    const auto memberReady=native::resolve<native::Ready>(base,0x178D740,{0x4C,0x8B,0xC1,0x48,0x63,0x89,0x3C,0xE9});
    const auto record=native::resolve<native::Record>(base,0xBFA030,{0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0xD9});
    const auto construct=native::resolve<native::Construct>(base,0xC061D0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto valid=native::resolve<native::Valid>(base,0x4D5460,{0x0F,0xB6,0x11,0xB0,0x01,0x80,0xFA,0xFF});
    const auto name=native::resolve<native::Name>(base,0xDDECA0,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83});
    const auto clear=native::resolve<native::Clear>(base,0xBF95D0,{0x48,0x83,0xEC,0x38,0xE8,0xE7,0xE0,0x7F});
    const auto select=native::resolve<native::Select>(base,0xBFB1F0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto commit=native::resolve<native::Commit>(base,0xBF97D0,{0x89,0x4C,0x24,0x08,0x48,0x83,0xEC,0x38});
    if(!sessionReady || !memberReady || !record || !construct || !valid || !name || !clear || !select || !commit) {return;}
    std::uintptr_t session{};std::int32_t member{};
    if(!native::fireteam_session(base,session) || !sessionReady(session) || !memberReady(session)
        || !native::read(session+0xE93C,member) || member<0 || member>=12
        || !named(name,target,target==2?"cine_110_twr":"city_tower_social_d2")) {return;}
    const auto current=record(static_cast<std::uint32_t>(member));std::uint8_t launch{};
    if(!current || !native::read(current+0xA33,launch) || launch>=3) {return;}
    alignas(16) std::array<std::byte,0x120> selection{};
    if(construct(selection.data(),0,target)!=selection.data()) {return;}
    std::int16_t from{},to{};std::memcpy(&from,selection.data()+2,2);std::memcpy(&to,selection.data()+4,2);
    std::uint64_t nonce{};
    if(from!=target || to!=target || selection[0]!=std::byte{} || !valid(selection.data())
        || !native::fireteam_transition_nonce(session,nonce)) {return;}
    std::memcpy(selection.data()+0x10,&nonce,sizeof nonce);
    if(!valid(selection.data()) || (target==2 && !state::activity::forced::suspend_launchpad_for_completed_run(state.run))
        || !tower::queued(state.run,target,now)) {return;}
    clear();select(0,selection.data());commit(1);exit={state.run,nonce,session,target,true};
    std::array<char,144> line{};std::snprintf(line.data(),line.size(),"ev=launchpad stage=handoff result=queued run=%llu activity=%d",static_cast<unsigned long long>(state.run),target);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
}
