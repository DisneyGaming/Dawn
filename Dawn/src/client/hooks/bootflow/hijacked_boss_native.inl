// Included in omega_lair_cinematic.cpp: extends its existing native full-body tick.
// No new detour; all pointers are freshly resolved and used only in this callback.
namespace hijacked_boss_native {
namespace mission=state::activity::hijacked;
namespace native=state::activity::hijacked::boss_motion;
struct Lease {
    state::activity::coo::Generation owner{};mission::EnemyReceipt enemy{};
    std::uint32_t revision{},character{UINT32_MAX},selector{UINT32_MAX};
    std::uint8_t stage{};bool dispatching{},issued{};native::LandingEvidence landing{};
};
SRWLOCK lock=SRWLOCK_INIT;
Lease lease{};
bool same_lease(const mission::BossRequest& request,std::uint32_t character,std::uint32_t selector) noexcept {
    return lease.owner==request.owner && lease.enemy==request.enemy && lease.revision==request.revision
        && lease.character==character && lease.selector==selector && lease.stage==request.stage;
}
// A failed pre-call identity check releases only this unissued reservation.
// An actual native dispatch is never retried on a timer or synthetic receipt.
struct DispatchReservation {
    const mission::BossRequest& request;std::uint32_t character,selector;bool held;
    ~DispatchReservation() {
        if(!held) {return;}
        AcquireSRWLockExclusive(&lock);
        if(same_lease(request,character,selector)) {lease.dispatching=false;}
        ReleaseSRWLockExclusive(&lock);
    }
};
void report(const char* event,const mission::BossRequest& request,std::uint32_t detail=0) noexcept {
    std::array<char,240> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=hijacked_boss stage=%s run=%llu actor=%08X phase=%u revision=%u detail=%08X",
        event,static_cast<unsigned long long>(request.enemy.run),request.enemy.actor,
        static_cast<unsigned>(request.stage),request.revision,detail));
}
bool current(const mission::BossRequest& request) noexcept {
    const auto now=mission::boss_request();
    return now.owner==request.owner && now.enemy==request.enemy && now.stage==request.stage
        && now.revision==request.revision && now.requested==request.requested;
}
void observe(void* fullBody) noexcept {
    auto request=mission::boss_request();
    if(!request.owner.valid() || !request.enemy.valid() || request.enemy.registry!=0x153E22CDU
        || request.enemy.source!=21 || request.stage>=native::kDestinations.size()) {return;}
    const auto resolver=g_resolve.load(std::memory_order_acquire);
    const auto issue=g_addEvent.load(std::memory_order_acquire);
    if(!resolver || !issue) {return;}
    std::array<std::byte,0x30> full{};
    if(!copy(fullBody,full) || !identity(full.data(),0x8162C3A2U,0x80803640U,0x2008)) {return;}
    const auto entity=at<std::uint32_t>(full.data()+0x2C);
    const auto fullHandle=at<std::uint32_t>(full.data()+0x24);
    if(entity==UINT32_MAX || fullHandle==UINT32_MAX || resolver(fullHandle)!=fullBody) {return;}
    gateway_native::Read read{g_image};
    std::uintptr_t actors{},entities{},characterAddress{};std::uint32_t stride{},actorSelf{},actorEntity{},bundle{},worldSelf{},flags{};
    if(!read.value(g_image+0x1F9D7F8,actors) || !read.value(g_image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) {return;}
    const auto actor=actors+static_cast<std::uintptr_t>(request.enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x48,actorSelf) || actorSelf!=request.enemy.actor
        || !read.value(actor+0x4C,actorEntity) || actorEntity!=entity) {return;}
    const auto ready=coo_native::enemy(read,g_image,request.enemy);
    if(!ready.created || !ready.health) {return;}
    if(!read.value(g_image+0x1F93428,entities) || !read.value(g_image+0x1F93430,stride)
        || stride<0x50 || stride>0x100000) {return;}
    const auto world=entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
    if(!read.value(world+0xC,worldSelf) || worldSelf!=entity || !read.value(world+4,flags) || (flags&4U)
        || !read.value(world+0x4C,bundle)
        || !coo_native::component<gateway_native::Read,1024>(read,bundle,entity,0x80806832U,characterAddress)) {return;}
    auto* character=reinterpret_cast<std::byte*>(characterAddress);
    std::array<std::byte,0xC4> bytes{};
    if(!copy(character,bytes) || !identity(bytes.data(),0x81578D59U,0x80806832U,0x7D8)
        || at<std::uint32_t>(bytes.data()+0xC0)!=request.enemy.actor
        || at<std::uint32_t>(bytes.data()+0x2C)!=entity) {return;}
    const auto characterHandle=at<std::uint32_t>(bytes.data()+0x24);
    if(characterHandle==UINT32_MAX || resolver(characterHandle)!=character) {return;}
    // Shared CD6C20 decrypts the requested health region; never read encrypted fields as floats.
    std::array<std::byte,16> getterPrefix{};
    std::array<std::byte,0x30> healthBytes{};
    auto* health=resolve_part(ready.healthHandle,0);
    auto* bodyRegion=resolve_part(0x815B5AA2U,0x1A30);std::array<std::byte,0xD4> regionBytes{};
    if(bodyRegion && copy(bodyRegion,regionBytes)
        && identity(regionBytes.data(),0x815B5AA2U,0x80804BABU,0x10F0)
        && at<std::uint32_t>(regionBytes.data()+0x10)==0x6DFE676DU
        && at<std::uint32_t>(regionBytes.data()+0xD0)==0
        && health && copy(health,healthBytes)
        && identity(healthBytes.data(),0x815B5AA2U,0x80804B8AU,0x1338)
        && at<std::uint32_t>(healthBytes.data()+0x24)==ready.healthHandle
        && at<std::uint32_t>(healthBytes.data()+0x2C)==entity
        && copy(reinterpret_cast<void*>(g_image+0xCD6C20),getterPrefix)
        && omega_boss_health::fraction_getter_prefix(getterPrefix)) {
        using Fraction=float(__fastcall*)(void*,std::int32_t) noexcept;
        const auto fraction=reinterpret_cast<Fraction>(g_image+0xCD6C20)(health,0);
        gateway_native::Read after{g_image};
        const auto healthAgain=coo_native::enemy(after,g_image,request.enemy);
        std::array<std::byte,0x30> checkedHealth{};std::array<std::byte,0x300> checkedCharacter{};
        if(healthAgain.created && healthAgain.health && healthAgain.healthHandle==ready.healthHandle
            && resolver(ready.healthHandle)==health && copy(health,checkedHealth)
            && identity(checkedHealth.data(),0x815B5AA2U,0x80804B8AU,0x1338)
            && at<std::uint32_t>(checkedHealth.data()+0x24)==ready.healthHandle
            && at<std::uint32_t>(checkedHealth.data()+0x2C)==entity
            && resolver(characterHandle)==character && copy(character,checkedCharacter)
            && identity(checkedCharacter.data(),0x81578D59U,0x80806832U,0x7D8)
            && at<std::uint32_t>(checkedCharacter.data()+0x24)==characterHandle
            && at<std::uint32_t>(checkedCharacter.data()+0x2C)==entity
            && at<std::uint32_t>(checkedCharacter.data()+0xC0)==request.enemy.actor
            && at<std::uint32_t>(checkedCharacter.data()+0x2E8)==ready.healthHandle
            && current(request)) {mission::observe_health(request.enemy,fraction);}
    }
    const auto refreshed=mission::boss_request();
    if(refreshed.owner!=request.owner || refreshed.enemy!=request.enemy) {return;}
    request=refreshed;
    if(!request.requested || !current(request)) {return;}
    // Biped and named-selector lookup prove the authored sequence ABI before dispatch.
    std::array<std::byte,4> animationRef{},bipedRef{};
    if(!copy(character+0x5C0,animationRef)) {return;}
    auto* animation=resolve_part(at<std::uint32_t>(animationRef.data()),0);
    if(!animation || !copy(animation+0x1260,bipedRef)) {return;}
    const auto bipedHandle=at<std::uint32_t>(bipedRef.data());
    auto* biped=resolve_part(bipedHandle,0);std::array<std::byte,0x848> bipedBytes{};
    if(!biped || !copy(biped,bipedBytes)
        || !identity(bipedBytes.data(),0x81578D5CU,0x808036CFU,0x2828)
        || at<std::uint32_t>(bipedBytes.data()+0x24)!=bipedHandle
        || at<std::uint32_t>(bipedBytes.data()+0x2C)!=entity
        || at<std::uint32_t>(bipedBytes.data()+0x798)!=fullHandle
        || at<std::uint32_t>(bipedBytes.data()+0x83C)!=0x80803466U
        || at<std::uint64_t>(bipedBytes.data()+0x840)!=0) {return;}
    const auto lookupHandle=at<std::uint32_t>(bipedBytes.data()+0x838);
    auto* lookup=resolve_part(lookupHandle,0);std::array<std::byte,0x30> lookupBytes{};
    auto* lookupDefinition=resolve_part(0x80F2FD42U,0x788);std::array<std::byte,16> names{};
    auto* authoredSequences=resolve_part(0x80F2FD3FU,0);std::array<std::byte,0x170> sequenceBytes{};
    auto* interface=resolve_part(0x80FEE862U,0);std::array<std::byte,16> method{};
    if(!lookup || !copy(lookup,lookupBytes) || !identity(lookupBytes.data(),0x80F2FD42U,0x8080344BU,0x788)
        || at<std::uint32_t>(lookupBytes.data()+0x24)!=lookupHandle
        || at<std::uint32_t>(lookupBytes.data()+0x2C)!=entity
        || !lookupDefinition || !copy(lookupDefinition+0x90,names)
        || at<std::uint32_t>(names.data())!=0x80F2FD13U || at<std::uint32_t>(names.data()+0xC)!=0x80F2FD2FU
        || !authoredSequences || !copy(authoredSequences,sequenceBytes)
        || at<std::uint32_t>(sequenceBytes.data()+0xBC)!=0x1F992208U
        || at<std::uint32_t>(sequenceBytes.data()+0x168)!=0xCBFDCA32U
        || !interface || !copy(interface+0x40,method) || at<std::uint32_t>(method.data())!=0x80806750U
        || at<std::uint32_t>(method.data()+4)!=4 || at<std::uintptr_t>(method.data()+8)!=g_image+0x10C6AF0) {return;}
    // Native C61660 uses character+588-relative groups of50 bytes. Group0's
    // interface and typed selector are scoped to the Entangled Mind override.
    std::array<std::byte,16> groups{};std::array<std::byte,0x18> group{};
    if(!copy(character+0x580,groups)) {return;}
    const auto count=at<std::uint64_t>(groups.data());const auto relative=at<std::int64_t>(groups.data()+8);
    if(count<1 || count>32 || relative<=0 || relative>0x100000
        || !copy(character+0x588+relative+0x40,group)
        || at<std::uint32_t>(group.data())!=0x80FEE862U
        || at<std::uint32_t>(group.data()+0xC)!=0x80806750U
        || at<std::uint64_t>(group.data()+0x10)!=0) {return;}
    const auto selectorHandle=at<std::uint32_t>(group.data()+8);
    auto* selector=resolve_part(selectorHandle,0);std::array<std::byte,0xD0> selectorBytes{};
    if(!selector || !copy(selector,selectorBytes) || !identity(selectorBytes.data(),0x80F58FEEU,0x80806751U,0x188)
        || at<std::uint32_t>(selectorBytes.data()+0x24)!=selectorHandle
        || at<std::uint32_t>(selectorBytes.data()+0x2C)!=entity
        || at<std::uint32_t>(selectorBytes.data()+0x30)!=characterHandle
        || at<std::uint32_t>(selectorBytes.data()+0x7C)!=0x808069EFU
        || at<std::uint64_t>(selectorBytes.data()+0x80)!=0) {return;}
    const auto motionHandle=at<std::uint32_t>(selectorBytes.data()+0x78);
    auto* motionComponent=resolve_part(motionHandle,0);std::array<std::byte,0x30> motionBytes{};
    if(!motionComponent || !copy(motionComponent,motionBytes)
        || !identity(motionBytes.data(),0x8162C3A3U,0x808069EEU,0xE40)
        || at<std::uint32_t>(motionBytes.data()+0x24)!=motionHandle
        || at<std::uint32_t>(motionBytes.data()+0x2C)!=entity) {return;}
    const auto landing=native::landing(selectorBytes);
    const bool inactive=!landing.active;
    std::array<float,3> position{};const bool positioned=inactive && actual_boss_position(entity,position);
    bool dispatch=false,complete=false,initialPosition=false,issued=false;
    native::LandingEvidence arrivalEvidence{};
    AcquireSRWLockExclusive(&lock);
    if(!same_lease(request,characterHandle,selectorHandle)) {
        lease={request.owner,request.enemy,request.revision,characterHandle,selectorHandle,request.stage};
    }
    if(lease.issued) {native::retain_landing(lease.landing,landing);}
    // Preserve the already-correct initial-pose path. After dispatch, compare the
    // real world pose against the engine's resolved destination, not the authored
    // point it was allowed to adjust for navigation and the Hydra's height.
    issued=lease.issued;arrivalEvidence=lease.landing;
    initialPosition=!issued && native::arrived(request.stage,position);
    complete=positioned && !lease.dispatching && (initialPosition
        || native::teleport_arrived(issued,arrivalEvidence,landing,position));
    if(!complete && inactive && !lease.issued && !lease.dispatching) {lease.dispatching=true;dispatch=true;}
    ReleaseSRWLockExclusive(&lock);
    DispatchReservation reservation{request,characterHandle,selectorHandle,dispatch};
    if(!current(request)) {return;}
    if(complete) {
        // Revalidate the component and current request after reading the world pose.
        std::array<std::byte,0xD0> settled{};
        if(resolver(selectorHandle)!=selector || !copy(selector,settled)
            || !identity(settled.data(),0x80F58FEEU,0x80806751U,0x188)
            || at<std::uint32_t>(settled.data()+0x24)!=selectorHandle
            || at<std::uint32_t>(settled.data()+0x2C)!=entity
            || at<std::uint32_t>(settled.data()+0x30)!=characterHandle
            || at<std::uint32_t>(settled.data()+0x78)!=motionHandle
            || settled[0x90]!=std::byte{} || native::landing(settled).sequence!=landing.sequence
            || (!initialPosition && !native::teleport_arrived(issued,arrivalEvidence,native::landing(settled),position))
            || !current(request)) {return;}
        if(mission::observe_boss_position(request.enemy,request.stage,request.revision)) {report("arrived",request);}
    } else if(dispatch) {
        const auto command=native::request(request.stage);
        if(command) {
            gateway_native::Read before{g_image};const auto beforeIssue=coo_native::enemy(before,g_image,request.enemy);
            std::array<std::byte,0xC4> ownerAgain{};std::array<std::byte,0xD0> selectorAgain{};
            std::array<std::byte,0x18> groupAgain{};std::array<std::byte,16> groupsAgain{},methodAgain{};
            std::array<std::byte,0x30> fullAgain{},motionAgain{};
            if(!beforeIssue.created || !beforeIssue.health || beforeIssue.healthHandle!=ready.healthHandle
                || resolver(characterHandle)!=character || !copy(character,ownerAgain)
                || !identity(ownerAgain.data(),0x81578D59U,0x80806832U,0x7D8)
                || at<std::uint32_t>(ownerAgain.data()+0x24)!=characterHandle
                || at<std::uint32_t>(ownerAgain.data()+0x2C)!=entity
                || at<std::uint32_t>(ownerAgain.data()+0xC0)!=request.enemy.actor
                || !copy(character+0x580,groupsAgain) || groupsAgain!=groups
                || !copy(character+0x588+relative+0x40,groupAgain) || groupAgain!=group
                || resolver(selectorHandle)!=selector || !copy(selector,selectorAgain)
                || !identity(selectorAgain.data(),0x80F58FEEU,0x80806751U,0x188)
                || at<std::uint32_t>(selectorAgain.data()+0x24)!=selectorHandle
                || at<std::uint32_t>(selectorAgain.data()+0x2C)!=entity
                || at<std::uint32_t>(selectorAgain.data()+0x30)!=characterHandle
                || at<std::uint32_t>(selectorAgain.data()+0x78)!=motionHandle
                || selectorAgain[0x90]!=std::byte{}
                || resolver(motionHandle)!=motionComponent || !copy(motionComponent,motionAgain) || motionAgain!=motionBytes
                || resolver(fullHandle)!=fullBody || !copy(fullBody,fullAgain) || fullAgain!=full
                || !copy(interface+0x40,methodAgain) || methodAgain!=method || !current(request)) {
                report("dispatch_identity_changed",request);return;
            }
            // Use the endpoint from the immediately revalidated selector, not
            // an earlier sample taken before another native call.
            const auto previous=native::landing(selectorAgain).destination;
            bool claimed{};
            AcquireSRWLockExclusive(&lock);
            if(same_lease(request,characterHandle,selectorHandle) && lease.dispatching && !lease.issued) {
                lease.landing={previous};lease.issued=true;lease.dispatching=false;claimed=true;
            }
            ReleaseSRWLockExclusive(&lock);
            if(!claimed) {return;}
            issue(character,command->data(),nullptr);report("teleport_requested",request,selectorHandle);
        }
    }
}
// Damage callbacks can request a retreat before the next full-body update.
// Resolve only a previously authenticated character handle, then let observe()
// repeat the complete actor/component checks before dispatching any command.
void dispatch_pending() noexcept {
    const auto request=mission::boss_request();
    if(!request.requested || request.stage==0 || !request.enemy.valid()) {return;}
    std::uint32_t characterHandle=UINT32_MAX;
    AcquireSRWLockShared(&lock);
    if(lease.owner==request.owner && lease.enemy==request.enemy) {characterHandle=lease.character;}
    ReleaseSRWLockShared(&lock);
    const auto resolver=g_resolve.load(std::memory_order_acquire);
    if(!resolver || characterHandle==UINT32_MAX) {return;}
    auto* character=static_cast<std::byte*>(resolver(characterHandle));
    std::array<std::byte,0xC4> bytes{};std::array<std::byte,4> animationRef{},bipedRef{};
    if(!character || !copy(character,bytes) || !identity(bytes.data(),0x81578D59U,0x80806832U,0x7D8)
        || at<std::uint32_t>(bytes.data()+0x24)!=characterHandle
        || at<std::uint32_t>(bytes.data()+0xC0)!=request.enemy.actor || !copy(character+0x5C0,animationRef)) {return;}
    const auto entity=at<std::uint32_t>(bytes.data()+0x2C);
    auto* animation=resolve_part(at<std::uint32_t>(animationRef.data()),0);
    if(!animation || !copy(animation+0x1260,bipedRef)) {return;}
    const auto bipedHandle=at<std::uint32_t>(bipedRef.data());
    auto* biped=resolve_part(bipedHandle,0);std::array<std::byte,0x79C> bipedBytes{};
    if(!biped || !copy(biped,bipedBytes) || !identity(bipedBytes.data(),0x81578D5CU,0x808036CFU,0x2828)
        || at<std::uint32_t>(bipedBytes.data()+0x24)!=bipedHandle
        || at<std::uint32_t>(bipedBytes.data()+0x2C)!=entity || !current(request)) {return;}
    const auto fullHandle=at<std::uint32_t>(bipedBytes.data()+0x798);
    auto* fullBody=fullHandle==UINT32_MAX?nullptr:resolver(fullHandle);
    if(fullBody) {observe(fullBody);}
}
} // namespace hijacked_boss_native
