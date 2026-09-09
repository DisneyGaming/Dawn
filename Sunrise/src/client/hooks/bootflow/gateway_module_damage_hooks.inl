// Included in the installed object-source owner, under the same CallGate.
using ModuleDamage=void(__fastcall*)(const void*,const void*,std::byte*,bool,bool,const void*,std::int32_t) noexcept;
using ModuleDamageGate=bool(__fastcall*)(const void*) noexcept;
using ModuleDamageSummary=void(__fastcall*)(const void*,std::uint32_t,std::uint32_t,bool,bool,const void*,float) noexcept;
std::atomic<ModuleDamage> g_moduleDamage{};
std::atomic<ModuleDamageGate> g_moduleDamageGate{};
std::atomic<ModuleDamageSummary> g_moduleDamageSummary{};
__declspec(noinline) bool gateway_damage_blocked(const void* context) noexcept {
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample)) { return false; }
    if(gateway_module_damage::blocked(state::activity::gateway::ending_request())) { return true; }
    const auto request=state::activity::beyond_infinity::lens_request();
    AcquireSRWLockShared(&g_lock);const auto candidate=g_beyondLensCandidate;ReleaseSRWLockShared(&g_lock);
    const bool current=beyond_infinity_lens_damage::current(read,request,candidate,sample);
    return beyond_infinity_lens_damage::blocked(request,current);
}
__declspec(noinline) void gateway_damage_receipt(const void* context) noexcept {
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample) || !sample.dead) { return; }
    const auto request=state::activity::gateway::ending_request();
    if(gateway_module_damage::current(read,request,sample) && read.weak({request.owner.serial,request.owner.entity})) {
        state::activity::gateway::observe_module(request.owner,true);
    }
    const auto lens=state::activity::beyond_infinity::lens_request();
    // A dead candidate never invents the live receipt required for progression.
    if(lens.lens.valid() && lens.vulnerable && beyond_infinity_lens_damage::current(read,lens,{},sample)) {
        state::activity::beyond_infinity::observe_lens(lens.lens,true);
    }
}
__declspec(noinline) bool __fastcall gateway_damage_gate_hook(const void* context) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    const bool result=hooking::await_original(g_moduleDamageGate)(context);
    if(!gate.accepts_side_effects()) { return result; }
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample)) { return result; }
    const auto request=state::activity::gateway::ending_request();
    const bool current=gateway_module_damage::current(read,request,sample)
        && read.weak({request.owner.serial,request.owner.entity});
    const bool gatewayResult=gateway_module_damage::allowed(request,current,result);
    const auto lens=state::activity::beyond_infinity::lens_request();
    AcquireSRWLockShared(&g_lock);const auto candidate=g_beyondLensCandidate;ReleaseSRWLockShared(&g_lock);
    return beyond_infinity_lens_damage::allowed(lens,
        beyond_infinity_lens_damage::current(read,lens,candidate,sample),gatewayResult);
}
__declspec(noinline) void __fastcall gateway_damage_hook(const void* context,const void* damage,std::byte* packet,
    bool mode,bool secondary,const void* extra,std::int32_t index) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    if(gate.accepts_side_effects() && gateway_damage_blocked(context)) { return; }
    hooking::await_original(g_moduleDamage)(context,damage,packet,mode,secondary,extra,index);
    if(gate.accepts_side_effects()) { gateway_damage_receipt(context); }
}
__declspec(noinline) void __fastcall gateway_damage_summary_hook(const void* context,std::uint32_t attacker,std::uint32_t target,
    bool killed,bool mode,const void* regions,float amount) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    if(gate.accepts_side_effects() && killed) { gateway_damage_receipt(context); }
    hooking::await_original(g_moduleDamageSummary)(context,attacker,target,killed,mode,regions,amount);
}
