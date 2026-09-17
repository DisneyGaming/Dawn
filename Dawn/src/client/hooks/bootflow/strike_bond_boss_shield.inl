// Executed on the admitted Dendron character's existing native update thread.
// Own only pass 7's model base bit. Native reference-owner bits and all other
// passes remain untouched. The authored blue-shell input uses its original
// animation-provider setter; no actor, health, transform or material writes.
namespace garden_shield {
namespace policy=strike_bond_boss_shield;
SRWLOCK lock=SRWLOCK_INIT;
struct Ledger {
    policy::BaseLease base{};policy::Binding binding{};
    int count{-1};bool busy{},pending{};unsigned reports{},phaseReports{};std::uint16_t conflict{};
};
Ledger ledger{};
void reset() noexcept {ledger={};}
void report(const garden::BossRequest& r,const char* event,int count,float fraction,std::uint16_t flags=0,int authoredBase=-1) noexcept {
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),"ev=garden_shield event=%s run=%llu actor=%08X stage=%u mode=%u fighting=%u wanted=%u pass=7 count=%d fraction=%.6f flags=%04X authored_base=%d",
        event,r.owner.run,r.enemy.actor,r.frame.bossStage,static_cast<unsigned>(r.frame.bossCycle.mode),r.frame.bossFighting?1U:0U,
        policy::visible(r.frame)?1U:0U,count,fraction,static_cast<unsigned>(flags),authoredBase);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
bool set_base(const policy::Binding& b,std::uint16_t flags) noexcept {
    // A native reference change between sampling and this write must retry on
    // the next callback; an unconditional whole-word store would lose its bits.
    if(((b.passFlags^flags)&~1U) || ((b.model+policy::kPassFlagsOffset)&1U)) return false;
    __try {
        return static_cast<std::uint16_t>(InterlockedCompareExchange16(
            reinterpret_cast<volatile SHORT*>(b.model+policy::kPassFlagsOffset),
            static_cast<SHORT>(flags),static_cast<SHORT>(b.passFlags)))==b.passFlags;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool same_request(const garden::BossRequest& a,const garden::BossRequest& b) noexcept {
    return a.owner==b.owner && a.enemy==b.enemy && a.frame.bossStage==b.frame.bossStage
        && a.frame.bossFighting==b.frame.bossFighting && a.frame.bossCycle.mode==b.frame.bossCycle.mode
        && policy::visible(a.frame)==policy::visible(b.frame)
        && policy::terminal(a.frame)==policy::terminal(b.frame);
}
void update_phase(const garden::BossRequest& r,const policy::Binding& b) noexcept {
    if(policy::terminal(r.frame)) return;
    gateway_native::Read read{g_image};policy::PhaseInput input{};
    if(!policy::phase_sample(read,g_image,b,r,input) || !policy::phase_boundaries(read,g_image)) {
        if(ledger.phaseReports++<4) report(r,"phase_input_guard",-1,-1.F);return;
    }
    const auto target=policy::phase_value(r.frame);
    if(std::abs(input.value-target)<.001F) return;
    const auto current=garden::boss_request();policy::Binding bound{};policy::PhaseInput fresh{};
    gateway_native::Read checked{g_image};
    if(!same_request(current,r) || !policy::sample(checked,g_image,b.character.character,current,bound) || bound!=b
        || !policy::phase_sample(checked,g_image,bound,current,fresh) || fresh!=input) return;
    using Set=void(__fastcall*)(void*,const std::uint32_t*,float) noexcept;
    reinterpret_cast<Set>(g_image+policy::kPhaseSetterRva)(reinterpret_cast<void*>(fresh.parent),&policy::kPhaseInput,target);
    const auto latest=garden::boss_request();policy::PhaseInput after{};gateway_native::Read finalRead{g_image};
    const bool confirmed=same_request(latest,current)
        && policy::phase_sample(finalRead,g_image,bound,latest,after)
        && after.parent==input.parent && after.parentSelf==input.parentSelf && after.provider==input.provider
        && std::abs(after.value-target)<.001F;
    std::array<char,320> line{};
    std::snprintf(line.data(),line.size(),"ev=garden_shield event=phase_input run=%llu actor=%08X mode=%u input=0958590C before=%.3f target=%.3f confirmed=%u receipt=native_parent_setter",
        r.owner.run,r.enemy.actor,static_cast<unsigned>(r.frame.bossCycle.mode),input.value,target,confirmed?1U:0U);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
void after_update(void* character) noexcept {
    const auto r=garden::boss_request();if(!policy::wanted(r)) return;
    AcquireSRWLockExclusive(&lock);
    if(ledger.busy) {ReleaseSRWLockExclusive(&lock);return;}
    ledger.busy=true;
    ReleaseSRWLockExclusive(&lock);
    struct Release {~Release(){AcquireSRWLockExclusive(&lock);ledger.busy=false;ReleaseSRWLockExclusive(&lock);}} release;
    gateway_native::Read read{g_image};policy::Binding b{};
    const auto address=reinterpret_cast<std::uintptr_t>(character);
    if(!policy::sample(read,g_image,address,r,b) || !policy::boundaries(read,g_image)) {
        if(ledger.reports++<4) report(r,"binding_guard",-1,-1.F);return;
    }
    using Fraction=float(__fastcall*)(void*,int) noexcept;
    const auto fraction=reinterpret_cast<Fraction>(g_image+0xCD6C20)(reinterpret_cast<void*>(b.health),0);
    if(!std::isfinite(fraction) || fraction<0.F || fraction>1.F) return;
    update_phase(r,b);
    auto base=ledger.base;
    const bool acquired=base.bind(r.owner,r.enemy,b);
    const bool show=policy::visible(r.frame) && !base.retiring;
    auto desired=b;desired.passFlags=base.project(b.passFlags,show,policy::terminal(r.frame));
    const auto draw=policy::command(desired);
    const auto conflict=show?std::uint16_t{0}:policy::native_enables(desired.passFlags);
    const bool changed=acquired || ledger.pending || policy::publication_changed(b,desired,ledger.binding,ledger.count);
    // Stable model input survives native publications and renderer rebuilding.
    // No timed draw-count reassertion is needed. Retain terminal ownership even
    // when the shield was already hidden before the dying update.
    if(b.passFlags==desired.passFlags && !changed && conflict==ledger.conflict) {ledger.base=base;return;}
    const auto current=garden::boss_request();policy::Binding again{};gateway_native::Read checked{g_image};
    if(!same_request(current,r)
        || !policy::sample(checked,g_image,address,current,again) || again!=b) return;
    if(b.passFlags!=desired.passFlags && !set_base(b,desired.passFlags)) {
        if(ledger.reports++<4) report(current,"base_write_guard",-1,fraction,b.passFlags,b.authoredBase?1:0);return;
    }
    // Preserve the original base after a successful write even if subsequent
    // publication loses its lease. Never restore through an old model pointer.
    ledger.base=base;ledger.pending|=changed;
    const auto latest=garden::boss_request();policy::Binding final{};gateway_native::Read finalRead{g_image};
    if(!same_request(latest,current)
        || !policy::sample(finalRead,g_image,address,latest,final) || final!=desired) return;
    if(changed) {
        using Publish=void(__fastcall*)(void*,std::uint32_t,std::uint8_t,std::uint8_t) noexcept;
        reinterpret_cast<Publish>(g_image+policy::kDrawRva)(reinterpret_cast<void*>(draw.object),draw.renderer,draw.pass,draw.count);
        ledger.binding=final;ledger.count=draw.count;ledger.pending=false;
    }
    if(conflict && (changed || conflict!=ledger.conflict))
        report(latest,"native_enable_guard",draw.count,fraction,final.passFlags,final.authoredBase?1:0);
    else if(changed) report(latest,draw.count?"shown":"hidden",draw.count,fraction,final.passFlags,final.authoredBase?1:0);
    ledger.conflict=conflict;
}
}
