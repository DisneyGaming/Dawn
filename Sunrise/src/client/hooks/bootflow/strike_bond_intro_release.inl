// Existing native character-update hook owns this call. No new global hook,
// remote thread, suppression flag patch or direct motion-arena teardown.
namespace garden_intro {
namespace intro=strike_bond_intro_release;
using Remove=void(__fastcall*)(void*,const void*,void*) noexcept;
SRWLOCK lock=SRWLOCK_INIT;
garden::EnemyReceipt issued{};
garden::EnemyReceipt observed{};
std::uint32_t selectorSelf{};unsigned reports{};
void reset() noexcept {issued={};observed={};selectorSelf=0;reports=0;}
void report(const garden::BossRequest& request,const char* stage,unsigned reason=0,std::uint32_t selector=0) noexcept {
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),
        "ev=garden_intro_release stage=%s run=%llu generation=%u actor=%08X selector=%08X reason=%u group=AFB11A12 sequence=31A03F93",
        stage,request.owner.run,request.owner.value,request.enemy.actor,selector,reason);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
void after_update(void* character) noexcept {
    const auto request=garden::boss_request();
    if(!intro::wanted(request)) return;
    AcquireSRWLockExclusive(&lock);
    if(observed!=request.enemy) {reset();observed=request.enemy;}
    const bool already=issued==request.enemy;
    ReleaseSRWLockExclusive(&lock);
    if(already) return;
    gateway_native::Read read{g_image};intro::Binding binding{};unsigned reason{};
    const auto address=reinterpret_cast<std::uintptr_t>(character);
    if(!intro::sample(read,g_image,address,request,binding,reason)) {
        AcquireSRWLockExclusive(&lock);const bool emit=reports++<4;ReleaseSRWLockExclusive(&lock);
        if(emit) report(request,"guard",reason);return;
    }
    // Never dispatch across a lifecycle transition or component relocation.
    const auto current=garden::boss_request();gateway_native::Read check{g_image};intro::Binding again{};
    if(current.owner!=request.owner || current.enemy!=request.enemy
        || !intro::sample(check,g_image,address,current,again,reason) || binding!=again) return;
    AcquireSRWLockExclusive(&lock);
    const bool claim=issued!=request.enemy;
    if(claim) {issued=request.enemy;selectorSelf=binding.selectorSelf;}
    ReleaseSRWLockExclusive(&lock);
    if(!claim) return;
    // The named native stop cancels only group0/sequence1 on this admitted NPC.
    // Its callback clears the selector and notifies the native motion scheduler.
    // Scheduler retirement and AC4 expiry remain native and occur on later ticks.
    const auto stop=intro::stop_request();
    reinterpret_cast<Remove>(g_image+0xC693F0)(reinterpret_cast<void*>(binding.controller),stop.data(),nullptr);
    gateway_native::Read after{g_image};std::uintptr_t selector{};std::uint32_t ownerEntity{},self{},flags{};
    const bool inactive=after.resolve(binding.selectorSelf,selector) && selector==binding.selector
        && after.value(selector+0x24,self) && self==binding.selectorSelf
        && after.value(selector+0x2C,ownerEntity) && ownerEntity==binding.character.entity
        && after.value(selector+0xF4,flags) && !(flags&1U);
    report(request,inactive?"selector_released":"stop_sent_unconfirmed",0,binding.selectorSelf);
}
} // namespace garden_intro
