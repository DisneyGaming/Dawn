// Observational discovery traces. Raw damage flags are not an immunity enum.
// Native ABI evidence: build/omega-full-20260905/eye-*.txt and diagnostic contract.
using EyeScriptTick=void(__fastcall*)(std::byte*,const void*,float) noexcept;
using EyeScriptAction=bool(__fastcall*)(std::byte*,std::int32_t,float,float*) noexcept;
using EyeBehavior=bool(__fastcall*)(const void*,std::byte*,std::int32_t,float,void*) noexcept;
using EyeDamage=void(__fastcall*)(const void*,const void*,std::byte*,bool,bool,const void*,std::int32_t) noexcept;
using EyeDamageGate=bool(__fastcall*)(const void*) noexcept;
using EyeDamageSummary=void(__fastcall*)(const void*,std::uint32_t,std::uint32_t,bool,bool,const void*,float) noexcept;
std::atomic<EyeScriptTick> eyeScriptTickOriginal{};
std::atomic<EyeScriptAction> eyeScriptActionOriginal{};
std::atomic<EyeBehavior> eyeBehaviorOriginal{};
std::atomic<EyeDamage> eyeDamageOriginal{};
std::atomic<EyeDamageGate> eyeDamageGateOriginal{};
std::atomic<EyeDamageSummary> eyeDamageSummaryOriginal{};
std::mutex eyeExecutionMutex;
omega::mission::Token eyeExecutionToken{};
std::array<unsigned,3> eyeExecutionCounts{};
std::atomic<std::uint64_t> eyeExecutionSerial{};
struct EyeExecutionScope {
    omega::mission::Token token{};
    graph::Owner owner{};
    std::uint64_t serial{};
    std::uint32_t component{},resource{},health{},cycle{};
    unsigned gateCalls{},gateTrue{},summaryCalls{};
    float amount{};
};
thread_local EyeExecutionScope* eyeScriptScope{};
thread_local EyeExecutionScope* eyeDamageScope{};
struct EyeScopeLease {
    EyeExecutionScope*& slot;
    EyeExecutionScope* previous;
    EyeScopeLease(EyeExecutionScope*& s,EyeExecutionScope* value) noexcept:slot(s),previous(s) {slot=value;}
    ~EyeScopeLease() {slot=previous;}
};
bool eye_execution_current(const EyeExecutionScope& scope) noexcept {
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    MemberView member{};graph::Owner owner{};
    return snapshot.command.token==scope.token && snapshot.chargeDunked
        && current_owner(scope.owner,member,owner) && owner==scope.owner && member.enabled;
}
bool eye_execution_owner(EyeExecutionScope& scope) noexcept {
    if(!active()) return false;
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    if(!snapshot.chargeDunked || snapshot.command.cycle<1 || snapshot.command.cycle>3) return false;
    scope.token=snapshot.command.token;scope.cycle=snapshot.command.cycle;
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=scope.token.boss.run) return false;
        scope.owner=runState.graphOwner;
    }
    const auto& b=scope.token.boss;const auto& o=scope.owner;
    return o.run==b.run && o.generation==b.generation && o.revision==b.revision
        && o.actor==b.actor && o.character==b.character && o.entity==b.entity
        && o.biped==b.biped && o.member==b.member && eye_execution_current(scope);
}
bool eye_execution_budget(EyeExecutionScope& scope,unsigned lane) noexcept {
    const std::lock_guard lock(eyeExecutionMutex);
    if(eyeExecutionToken!=scope.token) {eyeExecutionToken=scope.token;eyeExecutionCounts={};}
    constexpr std::array<unsigned,3> limits{24,64,64};
    if(eyeExecutionCounts[lane]>=limits[lane]) return false;
    ++eyeExecutionCounts[lane];scope.serial=++eyeExecutionSerial;return true;
}
bool eye_script_identity(std::byte* component,EyeExecutionScope& scope) noexcept {
    std::array<std::byte,0x30> bytes{};
    if(!copy_native(component,bytes.data(),bytes.size())) return false;
    const auto tag=read<std::uint32_t>(bytes.data(),0);
    if(tag!=0x80F4547DU && tag!=0x80F45563U) return false;
    if(!omega_reveal_source::matches(bytes,{tag,0x808084E9U,tag==0x80F4547DU?0x928:0x488})) return false;
    scope.component=read<std::uint32_t>(bytes.data(),0x24);scope.resource=tag;
    return resolve_handle(scope.component)==component
        && read<std::uint32_t>(bytes.data(),0x2C)==scope.token.boss.entity;
}
bool eye_script_candidate(const std::byte* component) noexcept {
    std::uint32_t tag{};
    return copy_value(component,tag) && (tag==0x80F4547DU || tag==0x80F45563U);
}
bool eye_script_current(std::byte* component,const EyeExecutionScope& scope) noexcept {
    auto identity=scope;
    return eye_execution_current(scope) && eye_script_identity(component,identity)
        && identity.component==scope.component && identity.resource==scope.resource;
}
bool eye_damage_current(const std::byte* health,const EyeExecutionScope& scope) noexcept {
    std::uint32_t current{};bool dead{};
    return eye_execution_current(scope) && mission_boss_health(scope.owner,current,dead)
        && current==scope.health && resolve_handle(current)==health;
}
void __fastcall trace_eye_script_tick(std::byte* component,const void* world,float delta) noexcept {
    const hooking::CallGate::Scope call(callGate);EyeExecutionScope scope{};
    const bool observed=call.accepts_side_effects() && eye_script_candidate(component) && eye_execution_owner(scope)
        && eye_script_identity(component,scope) && eye_execution_budget(scope,0);
    // A nested foreign invocation must not inherit an observed script's scope.
    EyeScopeLease lease(eyeScriptScope,observed?&scope:nullptr);
    if(observed) log("ev=omega_mission stage=eye_script_tick_enter run=%llu epoch=%u cycle=%u trace=%llu graph=%08X self=%08X delta=%g receipt=execution_entry_only",
        scope.token.boss.run,scope.token.epoch,scope.cycle,scope.serial,scope.resource,scope.component,static_cast<double>(delta));
    hooking::await_original(eyeScriptTickOriginal)(component,world,delta);
    if(observed) log("ev=omega_mission stage=eye_script_tick_exit run=%llu epoch=%u trace=%llu current=%u receipt=execution_return_only",
        scope.token.boss.run,scope.token.epoch,scope.serial,
        call.accepts_side_effects() && eye_script_current(component,scope)?1U:0U);
}
bool __fastcall trace_eye_script_action(std::byte* component,std::int32_t action,float delta,float* elapsed) noexcept {
    const hooking::CallGate::Scope call(callGate);EyeExecutionScope scope{};
    const bool observed=call.accepts_side_effects() && eye_script_candidate(component) && eye_execution_owner(scope)
        && eye_script_identity(component,scope) && eye_execution_budget(scope,1);
    EyeScopeLease lease(eyeScriptScope,observed?&scope:nullptr);
    if(observed) log("ev=omega_mission stage=eye_script_action_enter run=%llu epoch=%u cycle=%u trace=%llu graph=%08X self=%08X action=%d delta=%g receipt=action_runner_only",
        scope.token.boss.run,scope.token.epoch,scope.cycle,scope.serial,scope.resource,scope.component,action,static_cast<double>(delta));
    const bool result=hooking::await_original(eyeScriptActionOriginal)(component,action,delta,elapsed);
    if(observed) log("ev=omega_mission stage=eye_script_action_exit run=%llu epoch=%u trace=%llu action=%d native_result=%u current=%u receipt=action_runner_only",
        scope.token.boss.run,scope.token.epoch,scope.serial,action,result?1U:0U,
        call.accepts_side_effects() && eye_script_current(component,scope)?1U:0U);
    return result;
}
bool __fastcall trace_eye_behavior(const void* definition,std::byte* component,std::int32_t action,float time,void* output) noexcept {
    const hooking::CallGate::Scope call(callGate);auto* scope=eyeScriptScope;
    const bool observed=call.accepts_side_effects() && scope && eye_script_current(component,*scope);
    const bool result=hooking::await_original(eyeBehaviorOriginal)(definition,component,action,time,output);
    if(observed) log("ev=omega_mission stage=eye_script_behavior run=%llu epoch=%u trace=%llu graph=%08X action=%d native_result=%u current=%u receipt=behavior_creation_only",
        scope->token.boss.run,scope->token.epoch,scope->serial,scope->resource,action,result?1U:0U,
        call.accepts_side_effects() && eye_script_current(component,*scope)?1U:0U);
    return result;
}
bool __fastcall trace_eye_damage_gate(const void* context) noexcept {
    const hooking::CallGate::Scope call(callGate);auto* scope=eyeDamageScope;
    const std::byte* health{};
    const bool observed=call.accepts_side_effects() && scope && context
        && copy_value(static_cast<const std::byte*>(context)+8,health)
        && eye_damage_current(health,*scope);
    const bool result=hooking::await_original(eyeDamageGateOriginal)(context);
    if(observed && call.accepts_side_effects() && eye_damage_current(health,*scope)) {++scope->gateCalls;if(result) ++scope->gateTrue;}
    return result;
}
void __fastcall trace_eye_damage_summary(const void* context,std::uint32_t attacker,std::uint32_t target,
    bool killed,bool mode,const void* regions,float amount) noexcept {
    const hooking::CallGate::Scope call(callGate);auto* scope=eyeDamageScope;
    if(call.accepts_side_effects() && scope && target==scope->token.boss.entity && eye_execution_current(*scope)) {
        ++scope->summaryCalls;scope->amount=amount;
        log("ev=omega_mission stage=eye_damage_summary run=%llu epoch=%u trace=%llu target=%08X attacker=%08X native_killed=%u native_mode=%u native_amount=%g receipt=native_summary_arguments",
            scope->token.boss.run,scope->token.epoch,scope->serial,target,attacker,killed?1U:0U,mode?1U:0U,static_cast<double>(amount));
    }
    hooking::await_original(eyeDamageSummaryOriginal)(context,attacker,target,killed,mode,regions,amount);
}
void __fastcall trace_eye_damage(const void* healthContext,const void* damage,std::byte* packet,
    bool mode,bool secondary,const void* extra,std::int32_t index) noexcept {
    const hooking::CallGate::Scope call(callGate);EyeExecutionScope scope{};
    const std::byte* health{};bool dead{};std::array<std::byte,0x68> before{},after{};
    std::uint32_t healthTag{};
    const bool observed=call.accepts_side_effects() && healthContext
        && copy_value(static_cast<const std::byte*>(healthContext)+8,health)
        && copy_value(health,healthTag) && healthTag==0x815B5A40U && eye_execution_owner(scope)
        && mission_boss_health(scope.owner,scope.health,dead)
        && healthContext && copy_value(static_cast<const std::byte*>(healthContext)+8,health)
        && health==resolve_handle(scope.health) && copy_native(packet,before.data(),before.size())
        && eye_execution_budget(scope,2);
    EyeScopeLease lease(eyeDamageScope,observed?&scope:nullptr);
    if(observed) log("ev=omega_mission stage=eye_damage_enter run=%llu epoch=%u cycle=%u trace=%llu health=%08X flags60=%08X count64=%d receipt=incoming_pipeline_only",
        scope.token.boss.run,scope.token.epoch,scope.cycle,scope.serial,scope.health,
        read<std::uint32_t>(before.data(),0x60),read<std::int32_t>(before.data(),0x64));
    hooking::await_original(eyeDamageOriginal)(healthContext,damage,packet,mode,secondary,extra,index);
    if(observed) {
        const bool current=call.accepts_side_effects() && eye_damage_current(health,scope)
            && copy_native(packet,after.data(),after.size());
        log("ev=omega_mission stage=eye_damage_exit run=%llu epoch=%u trace=%llu current=%u flags60=%08X gate_calls=%u gate_true=%u summaries=%u native_amount=%g rejection_reason=unmapped receipt=pipeline_return_only",
            scope.token.boss.run,scope.token.epoch,scope.serial,current?1U:0U,
            read<std::uint32_t>(after.data(),0x60),scope.gateCalls,scope.gateTrue,scope.summaryCalls,static_cast<double>(scope.amount));
    }
}
