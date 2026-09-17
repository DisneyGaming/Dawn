unsigned executionTicks{},executionActions{},executionBehaviors{},executionDamage{},executionSummaries{};
std::byte* executionComponent{};
const void* executionWorld{};
std::array<std::byte,24> executionHealthContext{};
std::array<std::byte,0x80> executionPacket{};
std::array<std::byte,16> executionArgument{};
bool executionBehaviorResult=true;
bool executionRecycle{};
bool __fastcall execution_behavior(const void* definition,std::byte* component,std::int32_t action,float time,void* output) noexcept {
    ++executionBehaviors;
    check(definition==executionArgument.data() && component==executionComponent && action==2 && time==.125F && output==executionPacket.data(),
        "behavior preserves five native arguments including stack output");
    return executionBehaviorResult;
}
bool __fastcall execution_action(std::byte* component,std::int32_t action,float delta,float* elapsed) noexcept {
    ++executionActions;
    check(component==executionComponent && action==2 && delta==.25F && elapsed,"action ABI preserves index, SIMD delta and output");
    check(trace_eye_behavior(executionArgument.data(),component,action,.125F,executionPacket.data())==executionBehaviorResult,
        "native behavior creation result preserved");
    if(executionRecycle) put(component,0x24,0x21F9E702U);
    *elapsed=.0625F;return false;
}
void __fastcall execution_tick(std::byte* component,const void* world,float delta) noexcept {
    ++executionTicks;
    check(component==executionComponent && world==executionWorld && delta==.5F,"script tick preserves all native arguments");
    float elapsed{};
    check(!trace_eye_script_action(component,2,.25F,&elapsed) && elapsed==.0625F,"native action result and output preserved");
}
void __fastcall execution_summary(const void* context,std::uint32_t attacker,std::uint32_t target,bool killed,bool mode,const void* regions,float amount) noexcept {
    ++executionSummaries;
    check(context==executionArgument.data() && attacker==0x1234 && target==runState.graphOwner.entity && !killed && mode
        && regions==executionPacket.data()+0x64 && amount==0,"damage summary preserves seven native arguments");
}
void __fastcall execution_damage(const void* healthContext,const void* damage,std::byte* packet,bool mode,bool secondary,const void* extra,std::int32_t index) noexcept {
    ++executionDamage;
    check(healthContext==executionHealthContext.data() && damage==executionArgument.data() && packet==executionPacket.data()
        && mode && !secondary && extra==executionArgument.data()+8 && index==-17,"damage ABI preserves seven native arguments");
    (void)trace_eye_damage_gate(healthContext);
    trace_eye_damage_summary(damage,0x1234,runState.graphOwner.entity,false,true,packet+0x64,0);
    put(packet,0x60,0x12345678U);
}
void eye_execution_trace_test() {
    namespace m=omega::mission;
    reach_mission(m::Phase::shield,1);
    const auto before=m::runtime::state.snapshot();
    executionComponent=bind(0x21F9E701,0x300);
    source(executionComponent,0x80F4547DU,0x808084E9U,0x928);
    put(executionComponent,0x24,0x21F9E701U);put(executionComponent,0x2C,before.command.token.boss.entity);
    executionWorld=executionArgument.data();
    eyeScriptTickOriginal.store(&execution_tick);eyeScriptActionOriginal.store(&execution_action);
    eyeBehaviorOriginal.store(&execution_behavior);eyeDamageOriginal.store(&execution_damage);
    eyeDamageSummaryOriginal.store(&execution_summary);
    EyeExecutionScope owner{};check(eye_execution_owner(owner),"trace requires complete live mission owner");
    std::uint32_t health{};bool dead{};
    check(mission_boss_health(owner.owner,health,dead),"trace fixture has typed exact health owner");
    auto* h=resolve_handle(health);put(executionHealthContext.data(),8,h);
    // Execute the unchanged 18-byte native gate for every input bit pattern.
    const auto gate=reinterpret_cast<EyeDamageGate>(image+0xCDCB60);
    const auto flags=read<std::uint8_t>(h,0x338);
    for(unsigned bits=0;bits<256;++bits) {
        put(h,0x338,static_cast<std::uint8_t>(bits));
        check(gate(executionHealthContext.data())==((bits&2)==0),"original CDCB60 is inverse health flag bit1, not a complete immunity verdict");
    }
    put(h,0x338,flags);eyeDamageGateOriginal.store(gate);
    eyeExecutionToken={};eyeExecutionCounts={};
    const auto start=receipts.size();
    trace_eye_script_tick(executionComponent,executionWorld,.5F);
    check(receipts.size()==start+5 && eyeScriptScope==nullptr,"script tick/action/behavior causal trace restores nested scope");
    check(receipts[start+2].find("receipt=behavior_creation_only")!=std::string::npos,"creation trace cannot claim command effect");
    executionRecycle=true;const auto recycled=receipts.size();
    trace_eye_script_tick(executionComponent,executionWorld,.5F);
    executionRecycle=false;put(executionComponent,0x24,0x21F9E701U);
    check(receipts[recycled+3].find("current=0")!=std::string::npos && receipts[recycled+4].find("current=0")!=std::string::npos,
        "recycled component during native call invalidates both return observations");
    auto invokeDamage=[] {trace_eye_damage(executionHealthContext.data(),executionArgument.data(),executionPacket.data(),true,false,executionArgument.data()+8,-17);};
    invokeDamage();
    check(receipts.back().find("gate_calls=1 gate_true=1 summaries=1")!=std::string::npos
        && receipts.back().find("rejection_reason=unmapped")!=std::string::npos,"zero native amount and limited gate evidence kept explicit");
    check(read<std::uint32_t>(executionPacket.data(),0x60)==0x12345678U && eyeDamageScope==nullptr,"native packet change preserved, damage scope restored");
    const auto filtered=receipts.size();
    put(executionComponent,0x2C,before.command.token.boss.entity^0x10000000U);
    trace_eye_script_tick(executionComponent,executionWorld,.5F);
    put(executionComponent,0x2C,before.command.token.boss.entity);
    put(executionHealthContext.data(),8,h+8);invokeDamage();put(executionHealthContext.data(),8,h);
    check(receipts.size()==filtered,"foreign script entity and nonmatching health context forward without tracing");
    callGate.quiesce();trace_eye_script_tick(executionComponent,executionWorld,.5F);invokeDamage();callGate.accept();
    check(receipts.size()==filtered && callGate.idle(),"all nested hooks forward during quiescence");
    eyeExecutionCounts={24,64,64};
    const auto calls=executionDamage;invokeDamage();trace_eye_script_tick(executionComponent,executionWorld,.5F);
    check(receipts.size()==filtered && executionDamage==calls+1,"exhausted trace budgets cannot suppress native calls");
    const auto after=m::runtime::state.snapshot();
    check(after.command.token==before.command.token && after.phase==before.phase && after.command.claimed==before.command.claimed,
        "discovery diagnostics do not advance mission state");
}
