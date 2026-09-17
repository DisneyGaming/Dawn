// Included in the existing population observer; source retirement forwards here
// through its call gate only from the authenticated native allocator boundary.
namespace garden_retirement {
namespace policy=strike_bond_boss_retirement;
SRWLOCK lock=SRWLOCK_INIT;
policy::Lease retained{};bool claimed{};
void reset() noexcept {retained={};claimed=false;}
void report(const char* stage,const policy::Lease& lease,const char* reason,
    policy::EntityState result=policy::EntityState::unreadable) noexcept {
    std::array<char,448> line{};
    std::snprintf(line.data(),line.size(),
        "ev=garden_retirement stage=%s run=%llu generation=%u actor=%08X source=%08X entity=%08X bundle=%08X parent=%08X character=%08X reason=%s result=%u",
        stage,static_cast<unsigned long long>(lease.owner.run),lease.enemy.generation,lease.enemy.actor,lease.enemy.owner,
        lease.entity,lease.bundle,lease.parent,lease.characterSelf,reason,static_cast<unsigned>(result));
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
void capture(const garden::BossRequest& r,const strike_bond_fire_trace::Identity& identity) noexcept {
    AcquireSRWLockShared(&lock);
    const bool known=retained.owner==r.owner && retained.enemy==r.enemy
        && retained.entity==identity.entity && retained.characterSelf==identity.self;
    ReleaseSRWLockShared(&lock);if(known) return;
    gateway_native::Read read{g_image};policy::Lease lease{};
    if(!policy::capture(read,g_image,r,identity,lease)) return;
    const auto current=garden::boss_request();
    if(!policy::same_request(current,lease) || current.frame.bossDead) return;
    AcquireSRWLockExclusive(&lock);
    // A capture that raced a newer mission must not replace its lease.
    const auto live=garden::boss_request();
    const bool changed=policy::same_request(live,lease) && !live.frame.bossDead && retained!=lease;
    if(changed) {retained=lease;claimed=false;}
    ReleaseSRWLockExclusive(&lock);
    if(changed) report("retained",lease,"live_identity");
}
__declspec(noinline) void dispatch(std::uintptr_t source,bool allocatorReady) noexcept {
    const auto request=garden::boss_request();
    if(!request.frame.enabled || !request.frame.bossDead || request.frame.finished || !request.enemy.valid()) return;
    gateway_native::Read sourceRead{g_image};
    if(!policy::source_matches(sourceRead,source,request.enemy)) return;
    AcquireSRWLockShared(&lock);const auto lease=retained;const bool done=claimed;ReleaseSRWLockShared(&lock);
    if(done && policy::same_request(request,lease)) return;
    if(!policy::requested(request,lease)) {report("deferred",lease,"lease_or_retirement");return;}
    if(!allocatorReady) {report("deferred",lease,"allocator_unavailable");return;}
    gateway_native::Read read{g_image};
    if(!policy::validate(read,g_image,request,source,lease)) {report("deferred",lease,"identity");return;}
    const auto current=garden::boss_request();gateway_native::Read checked{g_image};
    if(!policy::validate(checked,g_image,current,source,lease)) {report("deferred",lease,"identity_changed");return;}
    AcquireSRWLockExclusive(&lock);
    const bool take=retained==lease && !claimed;
    if(take) claimed=true;
    ReleaseSRWLockExclusive(&lock);if(!take) return;
    // No cache lock crosses the engine call: retirement can emit population
    // callbacks. The surrounding existing source hook guards native reentry.
    if(!policy::requested(garden::boss_request(),lease)) {
        AcquireSRWLockExclusive(&lock);if(retained==lease) claimed=false;ReleaseSRWLockExclusive(&lock);
        report("deferred",lease,"request_changed");return;
    }
    report("requested",lease,"source_boundary");
    reinterpret_cast<void(__fastcall*)(std::uint32_t)>(g_image+0x56A8F0)(lease.entity);
    gateway_native::Read after{g_image};const auto result=policy::entity_state(after,g_image,lease);
    const bool accepted=result==policy::EntityState::marked || result==policy::EntityState::replaced;
    report(accepted?"accepted":"deferred",lease,accepted?"native_mark_or_release":"native_result_unconfirmed",result);
}
}
