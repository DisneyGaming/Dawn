// C613E0 establishes the weapon owner; its nested C5FEF0 call decodes the
// target slot. This scope never survives the original native dispatch.
namespace garden_target {
namespace policy=strike_bond_target_binding;
using Dispatch=void(__fastcall*)(void*,const void*,const void*) noexcept;
using Decode=void(__fastcall*)(const void*,const void*,std::uint8_t*) noexcept;
std::atomic<Dispatch> dispatch{};std::atomic<Decode> decode{};
struct Active {
    state::activity::coo::Generation owner{};garden::EnemyReceipt enemy{};
    policy::Binding binding{};const void* request{};const void* context{};bool valid{};
};
thread_local const Active* active{};
struct Scope {const Active* previous;explicit Scope(const Active& value) noexcept:previous(active){active=&value;}~Scope(){active=previous;}};
SRWLOCK lock=SRWLOCK_INIT;garden::EnemyReceipt reported{};unsigned lines{};std::uint64_t last{},count{};
void reset() noexcept {reported={};lines=0;last=0;count=0;}
void report(const Active& a,const char* stage,std::uintptr_t context) noexcept {
    const auto now=GetTickCount64();bool emit{};std::uint64_t n{};
    AcquireSRWLockExclusive(&lock);
    if(reported!=a.enemy){reset();reported=a.enemy;}
    n=++count;if(lines<32 && (lines<4 || now-last>=5000)){++lines;last=now;emit=true;}
    ReleaseSRWLockExclusive(&lock);if(!emit)return;
    gateway_native::Read read{g_image};std::uintptr_t environment{},program{};
    (void)read.value(context+8,environment);(void)read.value(context+0x10,program);
    std::array<char,448> text{};
    std::snprintf(text.data(),text.size(),
        "ev=garden_target_binding stage=%s run=%llu generation=%u actor=%08X controller=%08X target=%08X serial=%08X slot=0 requests=%llu environment=%llX program=%llX",
        stage,a.owner.run,a.owner.value,a.enemy.actor,a.binding.controllerSelf,a.binding.target.handle,a.binding.target.serial,n,
        static_cast<unsigned long long>(environment),static_cast<unsigned long long>(program));
    core::log::write(core::log::Channel::client,core::log::Level::info,text.data());
}
__declspec(noinline) void __fastcall dispatch_hook(void* weapon,const void* request,const void* context) noexcept {
    const hooking::CallGate::Scope gate{g_gate};Active current{};
    if(gate.accepts_side_effects() && garden::native_run()) {
        gateway_native::Read read{g_image};std::uint8_t opcode{};
        if(read.value(reinterpret_cast<std::uintptr_t>(request)+0x60,opcode) && opcode==0x24) {
            const auto mission=garden::boss_request();
            if(policy::sample(read,g_image,reinterpret_cast<std::uintptr_t>(weapon),reinterpret_cast<std::uintptr_t>(request),mission,current.binding)) {
                current.owner=mission.owner;current.enemy=mission.enemy;current.request=request;current.context=context;current.valid=true;
            }
        }
    }
    const Scope scope{current};
    hooking::await_original(dispatch)(weapon,request,context);
}
__declspec(noinline) void __fastcall decode_hook(const void* request,const void* context,std::uint8_t* output) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    hooking::await_original(decode)(request,context,output);
    if(!gate.accepts_side_effects() || caller!=g_image+0xC6157B || !active || !active->valid
        || active->request!=request || active->context!=context || !output) return;
    gateway_native::Read read{g_image};std::uint8_t nativeSlot{};
    if(!read.value(reinterpret_cast<std::uintptr_t>(output),nativeSlot) || nativeSlot!=UINT8_MAX) return;
    const auto mission=garden::boss_request();policy::Binding fresh{};
    if(mission.owner!=active->owner || mission.enemy!=active->enemy
        || !policy::sample(read,g_image,active->binding.weapon,reinterpret_cast<std::uintptr_t>(request),mission,fresh)
        || fresh!=active->binding) return;
    if(!policy::decoder_context(read,reinterpret_cast<std::uintptr_t>(context))) {
        report(*active,"context_rejected",reinterpret_cast<std::uintptr_t>(context));return;
    }
    // Only the local decoded target changes. The engine consumes its own slot,
    // computes aim/visibility/range, and controls charge, cooldown and firing.
    *output=0;
    report(*active,"primary_target_selected",reinterpret_cast<std::uintptr_t>(context));
}
using Context=void(__fastcall*)(const void*,void*) noexcept;
using Apply=void(__fastcall*)(void*,const void*,const void*) noexcept;
SRWLOCK replayLock=SRWLOCK_INIT;garden::EnemyReceipt replayEnemy{};
unsigned replayAttempts{},replayGuards{};std::uint64_t replayLast{};bool replayBusy{};
void reset_replay() noexcept {replayEnemy={};replayAttempts=0;replayGuards=0;replayLast=0;replayBusy=false;}
void report_replay(const garden::BossRequest& request,const char* stage,unsigned reason,
    std::uint16_t state=0xFFFF) noexcept {
    std::array<char,384> text{};
    std::snprintf(text.data(),text.size(),
        "ev=garden_target_replay stage=%s run=%llu generation=%u actor=%08X reason=%u requested=%04X",
        stage,request.owner.run,request.owner.value,request.enemy.actor,reason,static_cast<unsigned>(state));
    core::log::write(core::log::Channel::client,core::log::Level::info,text.data());
}
// C31200 has just passed, and C306A0 has not consumed the target yet. Reissue
// only the native cached no-target action; no firing result or flags are forced.
void before_commit(void* controller,void* weapon) noexcept {
    const auto mission=garden::boss_request();if(!policy::wanted(mission)) return;
    const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&replayLock);
    if(replayEnemy!=mission.enemy){reset_replay();replayEnemy=mission.enemy;}
    const bool allowed=!replayBusy && replayAttempts<4 && (!replayLast || now-replayLast>=1000);
    if(allowed){replayBusy=true;replayLast=now;}
    ReleaseSRWLockExclusive(&replayLock);if(!allowed)return;
    struct Release {garden::EnemyReceipt enemy;~Release(){AcquireSRWLockExclusive(&replayLock);if(replayEnemy==enemy) replayBusy=false;ReleaseSRWLockExclusive(&replayLock);}} release{mission.enemy};
    gateway_native::Read read{g_image};policy::Replay binding{};unsigned reason{};
    const auto w=reinterpret_cast<std::uintptr_t>(weapon);
    if(!policy::replay_sample(read,g_image,w,mission,binding,reason)
        || binding.binding.controller!=reinterpret_cast<std::uintptr_t>(controller)) {
        if(reason>2){AcquireSRWLockExclusive(&replayLock);const bool emit=replayEnemy==mission.enemy && replayGuards++<4;ReleaseSRWLockExclusive(&replayLock);if(emit) report_replay(mission,"guard",reason);}
        return;
    }
    constexpr std::array<std::uint8_t,16> contextPrefix{0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x56,0x57,0x41,0x56,0x48,0x83};
    constexpr std::array<std::uint8_t,16> applyPrefix{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
    std::array<std::byte,16> prefix{};
    if(!read.copy(g_image+0xA91FC0,prefix) || std::memcmp(prefix.data(),contextPrefix.data(),16)
        || !read.copy(g_image+0xC620F0,prefix) || std::memcmp(prefix.data(),applyPrefix.data(),16)) {
        AcquireSRWLockExclusive(&replayLock);replayAttempts=4;ReleaseSRWLockExclusive(&replayLock);
        report_replay(mission,"native_boundary_rejected",0);return;
    }
    alignas(16) std::array<std::byte,0x30> context{};
    reinterpret_cast<Context>(g_image+0xA91FC0)(reinterpret_cast<void*>(binding.binding.mainState+0xC0),context.data());
    gateway_native::Read check{g_image};policy::Replay again{};const auto current=garden::boss_request();
    if(current.owner!=mission.owner || current.enemy!=mission.enemy
        || !policy::replay_sample(check,g_image,w,current,again,reason) || again!=binding) return;
    AcquireSRWLockExclusive(&replayLock);++replayAttempts;ReleaseSRWLockExclusive(&replayLock);
    if(!policy::decoder_context(check,reinterpret_cast<std::uintptr_t>(context.data()))) {
        report_replay(mission,"context_rejected",0);return;
    }
    context[0x28]=std::byte{4}; // A7AFE0's native add-action phase.
    reinterpret_cast<Apply>(g_image+0xC620F0)(controller,reinterpret_cast<void*>(binding.action),context.data());
    gateway_native::Read after{g_image};std::uint32_t self{};std::uint16_t requested{};
    const bool observed=after.value(w+0x1D8,self) && self==binding.binding.controllerSelf
        && after.value(w+0xB2,requested);
    report_replay(mission,observed && requested==1?"target_committed":"unconfirmed",0,observed?requested:0xFFFF);
}
} // namespace garden_target
