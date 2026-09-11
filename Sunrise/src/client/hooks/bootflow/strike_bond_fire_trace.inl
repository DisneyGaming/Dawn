// Included in omega_enemy_lair_receipts.cpp inside its private namespace.
// Diagnostic observers never clear suppression or override eligibility. The
// update boundary also releases the exact retained startup selector natively.
namespace garden_fire {
namespace trace=strike_bond_fire_trace;
using OneTick=void(__fastcall*)(void*) noexcept;
using Duration=void(__fastcall*)(void*,float) noexcept;
using Update=std::uint8_t(__fastcall*)(void*,const void*) noexcept;
using Eligibility=std::uint8_t(__fastcall*)(void*,void*,const void*) noexcept;
std::atomic<OneTick> oneTick{};
std::atomic<Duration> duration{};
std::atomic<Update> update{};
std::atomic<Eligibility> eligibility{};
struct Capture {
    trace::Identity identity{};trace::Suppression before{};
    state::activity::coo::Generation owner{};garden::EnemyReceipt enemy{};
    std::uint8_t stage{};bool fighting{},middleBroken{},valid{};
    std::uint64_t clock{};
};
Capture begin(void* input,bool controller) noexcept {
    Capture out{};
    if(!garden::native_run()) return out;
    gateway_native::Read read{g_image};std::array<std::byte,8> header{};
    const auto address=reinterpret_cast<std::uintptr_t>(input);
    if(!read.copy(address,header)
        || trace::field<std::uint32_t>(header,0)!=(controller?0x80F66F56U:0x80F459A3U)
        || trace::field<std::uint32_t>(header,4)!=(controller?0x80806832U:0x80803A00U)) return out;
    const auto request=garden::boss_request();
    if(!trace::sample(read,g_image,address,controller,request,out.identity)
        || !trace::suppression(read,out.identity,out.before)) return out;
    out.owner=request.owner;out.enemy=request.enemy;out.stage=request.frame.bossStage;
    out.fighting=request.frame.bossFighting;out.middleBroken=request.frame.lensDestroyed[7];
    out.clock=request.frame.gameplayClockTicks;out.valid=true;return out;
}
struct Inputs {
    std::uint32_t argumentBits{UINT32_MAX},weapon{UINT32_MAX},config{UINT32_MAX};
    std::array<std::uint8_t,6> weaponState{UINT8_MAX,UINT8_MAX,UINT8_MAX,UINT8_MAX,UINT8_MAX,UINT8_MAX};
};
struct Key {std::uintptr_t caller{};std::uint64_t last{},calls{};std::uint8_t kind{};bool used{};};
SRWLOCK lock=SRWLOCK_INIT;
state::activity::coo::Generation owner{};
std::uint32_t actorHandle{UINT32_MAX},lines{};
std::array<Key,64> keys{};
std::array<std::uint64_t,4> calls{};
std::uint64_t sequence{},timerPositive{},timerZero{},timerDecreased{},timerCleared{},gatePassed{},gateFailed{};
void reset() noexcept {
    owner={};actorHandle=UINT32_MAX;lines=0;keys={};calls={};sequence=0;
    timerPositive=timerZero=timerDecreased=timerCleared=gatePassed=gateFailed=0;
}
void finish(void* input,bool controller,const Capture& before,std::uint8_t kind,std::uintptr_t caller,
            std::uint8_t result,const Inputs& inputs) noexcept {
    if(!before.valid) return;
    const auto request=garden::boss_request();
    if(request.owner!=before.owner || request.enemy!=before.enemy) return;
    gateway_native::Read read{g_image};trace::Identity identity{};trace::Suppression after{};
    if(!trace::sample(read,g_image,reinterpret_cast<std::uintptr_t>(input),controller,request,identity)
        || identity!=before.identity || !trace::suppression(read,identity,after)) return;
    const auto now=GetTickCount64();
    const auto rva=caller>=g_image && caller-g_image<0x10000000?caller-g_image:0;
    bool emit{},first{};std::uint64_t seq{},total{},positive{},zero{},decreased{},cleared{},passed{},failed{};
    std::array<std::uint64_t,4> totals{};
    AcquireSRWLockExclusive(&lock);
    if(owner!=before.owner || actorHandle!=before.enemy.actor) {reset();owner=before.owner;actorHandle=before.enemy.actor;}
    seq=++sequence;++calls[kind];totals=calls;
    if(kind==2) {
        const auto delta=std::bit_cast<float>(inputs.argumentBits);
        if(delta>0.F && delta<60.F) ++timerPositive;else if(delta==0.F) ++timerZero;
        const auto a=std::bit_cast<float>(before.before.ticks),b=std::bit_cast<float>(after.ticks);
        if(a>b) ++timerDecreased;
        if(before.before.active && !after.active) ++timerCleared;
    }
    if(kind==3) {if(result) ++gatePassed;else ++gateFailed;}
    Key* key{};
    for(auto& row:keys) if(row.used && row.kind==kind && row.caller==rva) {key=&row;break;}
    if(!key) for(auto& row:keys) if(!row.used) {row={rva,0,0,kind,true};key=&row;first=true;break;}
    if(key) {
        total=++key->calls;
        if(lines<2048 && (first || total<=4 || now-key->last>=1000)) {emit=true;key->last=now;++lines;}
    }
    positive=timerPositive;zero=timerZero;decreased=timerDecreased;cleared=timerCleared;passed=gatePassed;failed=gateFailed;
    ReleaseSRWLockExclusive(&lock);
    if(!emit) return;
    // Capture only the first occurrence of a caller, after native execution and
    // outside the ledger lock. Normalize addresses to the pinned image's RVAs.
    std::array<void*,8> stack{};std::array<std::uintptr_t,8> stackRvas{};
    if(first) {
        const auto count=CaptureStackBackTrace(1,static_cast<DWORD>(stack.size()),stack.data(),nullptr);
        for(USHORT i=0;i<count;++i) {
            const auto address=reinterpret_cast<std::uintptr_t>(stack[i]);
            if(address>=g_image && address-g_image<0x10000000) stackRvas[i]=address-g_image;
        }
    }
    std::array<char,1024> line{};
    std::snprintf(line.data(),line.size(),
        "ev=garden_fire_trace kind=%u seq=%llu thread=%lu tick=%llu run=%llu generation=%u actor=%08X entity=%08X character=%08X "
        "fight=%u stage=%u middle=%u game_clock=%llu caller_rva=%llX caller_n=%llu "
        "before=%08X/%u after=%08X/%u argument_bits=%08X result=%u weapon=%08X config=%08X "
        "w48=%02X w49=%02X wb2=%02X wb3=%02X w155=%02X w156=%02X "
        "one_tick_calls=%llu duration_calls=%llu update_calls=%llu gate_calls=%llu "
        "dt_positive=%llu dt_zero=%llu decremented=%llu cleared=%llu gate_pass=%llu gate_fail=%llu",
        static_cast<unsigned>(kind),seq,GetCurrentThreadId(),now,before.owner.run,before.owner.value,before.enemy.actor,
        identity.entity,identity.self,before.fighting?1U:0U,static_cast<unsigned>(before.stage),before.middleBroken?1U:0U,
        before.clock,static_cast<unsigned long long>(rva),total,before.before.ticks,static_cast<unsigned>(before.before.active),
        after.ticks,static_cast<unsigned>(after.active),inputs.argumentBits,static_cast<unsigned>(result),inputs.weapon,inputs.config,
        static_cast<unsigned>(inputs.weaponState[0]),static_cast<unsigned>(inputs.weaponState[1]),static_cast<unsigned>(inputs.weaponState[2]),
        static_cast<unsigned>(inputs.weaponState[3]),static_cast<unsigned>(inputs.weaponState[4]),static_cast<unsigned>(inputs.weaponState[5]),
        totals[0],totals[1],totals[2],totals[3],positive,zero,decreased,cleared,passed,failed);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
    if(first) {
        std::snprintf(line.data(),line.size(),"ev=garden_fire_stack run=%llu seq=%llu kind=%u caller=%llX stack=%llX,%llX,%llX,%llX,%llX,%llX,%llX,%llX",
        before.owner.run,seq,static_cast<unsigned>(kind),static_cast<unsigned long long>(rva),
        static_cast<unsigned long long>(stackRvas[0]),static_cast<unsigned long long>(stackRvas[1]),
        static_cast<unsigned long long>(stackRvas[2]),static_cast<unsigned long long>(stackRvas[3]),
        static_cast<unsigned long long>(stackRvas[4]),static_cast<unsigned long long>(stackRvas[5]),
        static_cast<unsigned long long>(stackRvas[6]),static_cast<unsigned long long>(stackRvas[7]));
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
    }
}
__declspec(noinline) void __fastcall one_tick_hook(void* character) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto before=scope.accepts_side_effects()?begin(character,false):Capture{};
    hooking::await_original(oneTick)(character);
    if(scope.accepts_side_effects()) finish(character,false,before,0,caller,0,{});
}
__declspec(noinline) void __fastcall duration_hook(void* character,float seconds) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto before=scope.accepts_side_effects()?begin(character,false):Capture{};
    hooking::await_original(duration)(character,seconds);
    if(scope.accepts_side_effects()) {Inputs inputs;inputs.argumentBits=std::bit_cast<std::uint32_t>(seconds);finish(character,false,before,1,caller,0,inputs);}
}
__declspec(noinline) std::uint8_t __fastcall update_hook(void* character,const void* frame) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto before=scope.accepts_side_effects()?begin(character,false):Capture{};
    Inputs inputs;if(before.valid) {gateway_native::Read read{g_image};(void)read.value(reinterpret_cast<std::uintptr_t>(frame)+0x10,inputs.argumentBits);}
    const auto result=hooking::await_original(update)(character,frame);
    if(scope.accepts_side_effects() && before.valid) {
        garden_intro::after_update(character);
        garden_carriage::after_update(character);
        garden_cycle::after_update(character);
    }
    if(scope.accepts_side_effects()) finish(character,false,before,2,caller,result,inputs);
    return result;
}
__declspec(noinline) std::uint8_t __fastcall eligibility_hook(void* controller,void* weapon,const void* context) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto before=scope.accepts_side_effects()?begin(controller,true):Capture{};
    Inputs inputs;
    if(before.valid) {
        gateway_native::Read read{g_image};std::uint32_t handle{},self{},binding{};std::uintptr_t state{};
        const auto address=reinterpret_cast<std::uintptr_t>(controller),w=reinterpret_cast<std::uintptr_t>(weapon);
        if(read.value(address+0x24,self) && read.value(address+0x5C0,handle) && read.resolve(handle,state)
            && read.value(state+0x30,binding) && binding==self && w==state+0x8F0) {
            constexpr std::array<std::uintptr_t,6> offsets{0x48,0x49,0xB2,0xB3,0x155,0x156};
            for(std::size_t i=0;i<offsets.size();++i) (void)read.value(w+offsets[i],inputs.weaponState[i]);
            const auto ctx=reinterpret_cast<std::uintptr_t>(context);
            (void)read.value(ctx+0x20,inputs.weapon);
            std::uintptr_t config{};
            if(read.value(ctx+0x28,config)) (void)read.value(config,inputs.config);
        }
    }
    const auto result=hooking::await_original(eligibility)(controller,weapon,context);
    if(scope.accepts_side_effects() && before.valid && result) garden_target::before_commit(controller,weapon);
    if(scope.accepts_side_effects()) finish(controller,true,before,3,caller,result,inputs);
    return result;
}
} // namespace garden_fire
