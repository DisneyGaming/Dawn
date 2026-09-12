// Executed on the admitted Dendron character's existing native update thread.
// Only the proven shield draw pass is queued; other passes and native counters
// remain owned by the renderer. No actor, health, animation or transform writes.
namespace garden_shield {
namespace policy=strike_bond_boss_shield;
SRWLOCK lock=SRWLOCK_INIT;
struct Ledger {
    garden::EnemyReceipt enemy{};policy::Binding binding{};
    std::uint64_t next{},refresh{};int count{-1};bool busy{};unsigned reports{};
};
Ledger ledger{};
void reset() noexcept {ledger={};}
void report(const garden::BossRequest& r,const char* event,int count,float fraction) noexcept {
    std::array<char,320> line{};
    std::snprintf(line.data(),line.size(),"ev=garden_shield event=%s run=%llu actor=%08X stage=%u mode=%u pass=7 count=%d fraction=%.6f",
        event,r.owner.run,r.enemy.actor,r.frame.bossStage,static_cast<unsigned>(r.frame.bossCycle.mode),count,fraction);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
void after_update(void* character) noexcept {
    const auto r=garden::boss_request();if(!policy::wanted(r)) return;
    AcquireSRWLockExclusive(&lock);
    if(ledger.busy) {ReleaseSRWLockExclusive(&lock);return;}
    if(ledger.enemy!=r.enemy) {reset();ledger.enemy=r.enemy;}
    const auto now=GetTickCount64();const bool claim=now>=ledger.next;
    if(claim) {ledger.busy=true;ledger.next=now+50;}
    ReleaseSRWLockExclusive(&lock);if(!claim) return;
    struct Release {~Release(){AcquireSRWLockExclusive(&lock);ledger.busy=false;ReleaseSRWLockExclusive(&lock);}} release;
    gateway_native::Read read{g_image};policy::Binding b{};
    const auto address=reinterpret_cast<std::uintptr_t>(character);
    if(!policy::sample(read,g_image,address,r,b) || !policy::boundaries(read,g_image)) {
        if(ledger.reports++<4) report(r,"binding_guard",-1,-1.F);return;
    }
    using Fraction=float(__fastcall*)(void*,int) noexcept;
    const auto fraction=reinterpret_cast<Fraction>(g_image+0xCD6C20)(reinterpret_cast<void*>(b.health),0);
    if(!std::isfinite(fraction) || fraction<0.F || fraction>1.F) return;
    const auto draw=policy::command(b,policy::visible(r.frame,fraction));
    const bool changed=b!=ledger.binding || ledger.count!=draw.count;
    // Native animation/resource updates can republish pass counts. Reassert at
    // four calls per second, and on a state/resource change, not every frame.
    if(!changed && now<ledger.refresh) return;
    const auto current=garden::boss_request();policy::Binding again{};gateway_native::Read checked{g_image};
    if(current.owner!=r.owner || current.enemy!=r.enemy
        || current.frame.bossStage!=r.frame.bossStage || current.frame.bossFighting!=r.frame.bossFighting
        || current.frame.bossCycle.mode!=r.frame.bossCycle.mode
        || policy::visible(current.frame,fraction)!=policy::visible(r.frame,fraction)
        || !policy::sample(checked,g_image,address,current,again) || again!=b) return;
    using Publish=void(__fastcall*)(void*,std::uint32_t,std::uint8_t,std::uint8_t) noexcept;
    reinterpret_cast<Publish>(g_image+policy::kDrawRva)(reinterpret_cast<void*>(draw.object),draw.renderer,draw.pass,draw.count);
    ledger.binding=b;ledger.count=draw.count;ledger.refresh=now+250;
    if(changed) report(current,draw.count?"shown":"hidden",draw.count,fraction);
}
}
