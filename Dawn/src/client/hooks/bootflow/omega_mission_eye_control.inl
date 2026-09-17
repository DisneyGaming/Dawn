#include "omega_boss_vfx_start.h"
// CF9087D3 is the authored parent input, not a writable health fraction.
// Original 80F6690A: sorted provider 41, definition4370, runtime2F10.
// A0FE60 resolves that definition and publishes through the native dirty path.
bool read_mission_eye_control(const graph::Owner& owner,float& value) noexcept {
    namespace eye=omega_boss_eye_diagnostics;
    auto* parent=resolve_handle(owner.parent);
    std::array<std::byte,0x1474> before{},after{};
    if(!copy_native(parent,before.data(),before.size()) || !eye::animation_parent(before,owner)) return false;
    const auto relative=read<std::uintptr_t>(before.data(),0x1330);
    const auto address=reinterpret_cast<std::uintptr_t>(parent)+0x1340+relative+41*0x30;
    std::array<std::byte,0x30> provider{};
    if(!copy_native(reinterpret_cast<const void*>(address),provider.data(),provider.size())
        || !omega_reveal_source::matches(provider,{0x80F6690AU,0x80807EEBU,0x4370})
        || address+0x10+read<std::uintptr_t>(provider.data(),0x10)!=reinterpret_cast<std::uintptr_t>(parent)) return false;
    value=read<float>(provider.data(),0x20);
    return std::isfinite(value) && value>=0.F && value<=1.F && resolve_handle(owner.parent)==parent
        && copy_native(parent,after.data(),after.size()) && eye::animation_parent(after,owner)
        && read<std::uintptr_t>(after.data(),0x1330)==relative;
}
bool hold_mission_eye(const graph::Owner& owner,const omega::mission::Token& token,bool enable,
                      const hooking::CallGate::Scope& call) noexcept {
    constexpr std::uint32_t property=0xCF9087D3U;
    crown::Track retained{};
    {
        const std::lock_guard lock(mutex);
        if(runState.graphOwner!=owner || runState.crown.cycle<1 || runState.crown.cycle>2 || runState.crown.eyeBusy) return false;
        retained=runState.crown;
        if(enable && retained.eyeReleased) return false;
        if(retained.eyeOwned && retained.eyeToken.boss!=token.boss) return false;
        if(!enable && !retained.eyeOwned) return true;
        runState.crown.eyeBusy=true;
    }
    const auto fresh=[&]() {
        MemberView member{};graph::Owner current{};
        return call.accepts_side_effects() && state::activity::mission_run_generation()==owner.run
            && omega::mission::runtime::snapshot(owner.run).command.token==token
            && current_owner(owner,member,current) && current==owner && member.enabled;
    };
    float before{},after{};const bool initial=enable && !retained.eyeOwned;
    bool confirmed=fresh() && read_mission_eye_control(owner,before);
    const float target=enable?1.F:0.F;
    if(confirmed && before!=target) {
        std::array<std::byte,17600> source{};
        auto* asset=resolve_handle(0x80F6690AU);auto* parent=resolve_handle(owner.parent);
        confirmed=copy_native(asset,source.data(),source.size()) && omega_boss_vfx_start::source_layout(source)
            && read<std::uint32_t>(source.data(),0x4398)==property && fresh();
        if(confirmed) {
            native<void(__fastcall*)(std::byte*,const std::uint32_t*,float) noexcept>(0xA0FE60)(parent,&property,target);
            confirmed=fresh() && resolve_handle(0x80F6690AU)==asset && resolve_handle(owner.parent)==parent;
        }
    }
    confirmed=confirmed && read_mission_eye_control(owner,after) && after==target && fresh();
    {
        const std::lock_guard lock(mutex);
        if(runState.graphOwner!=owner || runState.crown.start!=retained.start) return false;
        runState.crown.eyeBusy=false;
        if(!confirmed) {runState.crown.uncertain=true;return false;}
        if(initial) {runState.crown.eyeToken=token;runState.crown.eyePrevious=before;}
        runState.crown.eyeOwned=enable;
        if(!enable) runState.crown.eyeReleased=true;
    }
    if(initial || !enable) log("ev=omega_mission stage=eye_control run=%llu epoch=%u cycle=%u property=CF9087D3 value=%g owned=%u receipt=native_parent_setter",
        owner.run,token.epoch,retained.cycle,static_cast<double>(after),enable?1U:0U);
    return true;
}
