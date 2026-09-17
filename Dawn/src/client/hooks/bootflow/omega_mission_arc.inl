struct MissionArcSource {
    std::uint64_t run{};std::uint32_t source{UINT32_MAX},entity{UINT32_MAX},generation{},definition{};
    std::int64_t offset{};
    bool operator==(const MissionArcSource&) const = default;
};
std::mutex missionArcMutex;
std::array<std::array<MissionArcSource,2>,3> missionArcSources{};
omega::mission::ChargeReceipt missionArcHeld{};
bool missionArcHolding{};
thread_local bool missionArcUse{},missionArcDeferredDrop{};
bool mission_arc_source(const MissionArcSource& expected) noexcept {
    std::array<std::byte,0x448> bytes{};std::uint32_t entity{};
    auto* base=resolve_handle(expected.source);
    auto* component=base?base+expected.offset:nullptr;
    omega_cannon_delivery::Reference identity;
    return expected.run==state::activity::mission_run_generation()
        && copy_native(component,bytes.data(),bytes.size())
        && omega_reveal_source::matches(bytes,{expected.definition,0x80809928U,0x4C8})
        && mission_source_identity(component,bytes,0x160,0x80809927U,identity)
        && identity.member==expected.source && identity.offset==expected.offset
        && read<std::uint32_t>(bytes.data(),0x180)==expected.generation
        && read<std::uint8_t>(bytes.data(),0x188)!=0
        && mission_weak(std::span(bytes).subspan(0x440,8),entity) && entity==expected.entity;
}
void observe_mission_arc_source(std::byte* component) noexcept {
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    if(!snapshot.generation || snapshot.command.cycle<1 || snapshot.command.cycle>3 || !snapshot.chargeEnabled) return;
    std::array<std::byte,0x448> bytes{};
    if(!copy_native(component,bytes.data(),bytes.size())) return;
    for(const auto& row:omega::transit::sources) {
        if(row.cycle!=snapshot.command.cycle || (row.role!=omega::transit::Role::charge && row.role!=omega::transit::Role::sink)
            || !omega_reveal_source::matches(bytes,{row.definition,0x80809928U,0x4C8})
            || read<std::uint32_t>(bytes.data(),0x180)!=omega::transit::generation(snapshot,row)
            || !read<std::uint8_t>(bytes.data(),0x188)) continue;
        omega_cannon_delivery::Reference identity;
        if(!mission_source_identity(component,bytes,0x160,0x80809927U,identity)) return;
        MissionArcSource binding{snapshot.command.token.boss.run,identity.member,UINT32_MAX,
            read<std::uint32_t>(bytes.data(),0x180),row.definition,identity.offset};
        if(!mission_weak(std::span(bytes).subspan(0x440,8),binding.entity)) return;
        const unsigned index=row.role==omega::transit::Role::charge?0U:1U;
        const std::lock_guard lock(missionArcMutex);auto& cached=missionArcSources[row.cycle-1][index];
        if(cached==binding) return;cached=binding;
        log("ev=omega_mission stage=arc_source run=%llu cycle=%u source=%08X entity=%08X role=%u generation=%u",
            binding.run,row.cycle,binding.source,binding.entity,index,binding.generation);
    }
}
bool mission_arc_component(std::uint32_t entity,std::uint32_t type,std::uint32_t definition,
    std::uint32_t kind,std::int64_t offset,std::uint32_t& handle) noexcept {
    const std::byte* pool{};std::uint32_t stride{};
    if(!copy_value(image+0x1F93428,pool) || !copy_value(image+0x1F93430,stride) || !pool || stride<0xE0 || stride>0x1000) return false;
    auto* world=pool+(entity&0x1FFF)*stride;std::array<std::byte,16> prefix{};
    if(!copy_native(world,prefix.data(),prefix.size()) || read<std::uint32_t>(prefix.data(),0xC)!=entity) return false;
    std::array<std::byte,0x30> reference{};
    using Find=bool(__fastcall*)(const std::byte*,std::uint32_t,void*,std::uint32_t*) noexcept;
    if(!native<Find>(0x557470)(world,type,reference.data(),nullptr)) return false;
    const auto owner=read<std::uint32_t>(reference.data(),0x18);
    const auto displacement=read<std::int64_t>(reference.data(),0x20);
    if(owner==UINT32_MAX || displacement<0 || displacement>0x2000000) return false;
    auto* ownerBase=resolve_handle(owner);
    if(!ownerBase) return false;
    auto* component=ownerBase+displacement;std::array<std::byte,0x30> body{};
    if(!copy_native(component,body.data(),body.size()) || !omega_reveal_source::matches(body,{definition,kind,offset})
        || read<std::uint32_t>(body.data(),0x2C)!=entity) return false;
    handle=read<std::uint32_t>(body.data(),0x24);
    return handle!=UINT32_MAX && resolve_handle(handle)==component;
}
bool mission_arc_pair(unsigned cycle,std::array<MissionArcSource,2>& pair,std::uint32_t& sink) noexcept {
    if(cycle<1 || cycle>3) return false;
    {const std::lock_guard lock(missionArcMutex);pair=missionArcSources[cycle-1];}
    constexpr std::array<std::uint32_t,3> definitions{0x80F6666E,0x80F66671,0x80F66673};
    return mission_arc_source(pair[0]) && mission_arc_source(pair[1])
        // 80804FB0 is the component declaration, not a registered lookup
        // interface. All three sink tables expose 80809658 on that component.
        && mission_arc_component(pair[1].entity,0x80809658,definitions[cycle-1],0x80804FB2,0x388,sink);
}
bool mission_arc_holder(std::byte* component,std::span<const std::byte> bytes,std::uint32_t& player) noexcept {
    // 597B10 reads the item's current world attachment, independent of the
    // holder context. Full player identity must still match the live pool.
    native<std::uint32_t*(__fastcall*)(std::byte*,std::uint32_t*) noexcept>(0x597B10)(component,&player);
    const std::byte* pool{};std::uint32_t stride{},identity{};
    if(player==UINT32_MAX || !copy_value(image+0x1F93428,pool) || !pool
        || !copy_value(image+0x1F93430,stride) || stride<0xE0 || stride>0x1000
        || !copy_value(pool+(player&0x1FFF)*stride+0xC,identity) || identity!=player) return false;
    if(read<std::uint8_t>(bytes.data(),0x470)==3) return true;
    // Confirmed held capture PID48516: this item uses inventory state 1.
    // Require its typed inventory owner AND world attachment to agree; state 1
    // alone (or an item merely lying nearby) is not a pickup receipt.
    if(read<std::uint8_t>(bytes.data(),0x470)!=1
        || read<std::uint32_t>(bytes.data(),0x47C)!=0x80804057U
        || read<std::int64_t>(bytes.data(),0x480)!=0x4C8
        || read<std::uint32_t>(bytes.data(),0x494)!=0x80803E63U) return false;
    const auto owner=read<std::uint32_t>(bytes.data(),0x490);
    const auto offset=read<std::int64_t>(bytes.data(),0x498);
    auto* base=resolve_handle(owner);
    std::uint32_t ownerIdentity{};
    if(!base || offset<0 || offset>0x2000000
        || !copy_value(base+0x24,ownerIdentity) || ownerIdentity!=owner) return false;
    auto* inventory=base+offset;std::array<std::byte,0x30> prefix{};
    if(!copy_native(inventory,prefix.data(),prefix.size())
        || !omega_reveal_source::matches(prefix,{read<std::uint32_t>(bytes.data(),0x478),0x80803E64U,0x468})
        || read<std::uint32_t>(prefix.data(),0x2C)!=player) return false;
    const auto self=read<std::uint32_t>(prefix.data(),0x24);
    return self!=UINT32_MAX && resolve_handle(self)==inventory;
}
void observe_mission_arc_carry(std::byte* component) noexcept {
    std::array<std::byte,0x4A0> bytes{};
    if(!copy_native(component,bytes.data(),bytes.size()) || !omega_reveal_source::matches(bytes,{0x80F66667U,0x80804221U,0x598})) return;
    const auto self=read<std::uint32_t>(bytes.data(),0x24),entity=read<std::uint32_t>(bytes.data(),0x2C);
    if(self==UINT32_MAX || entity==UINT32_MAX || resolve_handle(self)!=component) return;
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    const auto carryState=read<std::uint8_t>(bytes.data(),0x470);
    if(carryState!=1 && carryState!=3) {
        omega::mission::ChargeReceipt held{};
        {
            const std::lock_guard lock(missionArcMutex);
            if(!missionArcHolding || missionArcHeld.token!=snapshot.command.token || missionArcHeld.item!=entity) return;
            if(missionArcUse) {missionArcDeferredDrop=true;return;}
            held=missionArcHeld;missionArcHolding=false;
        }
        omega::mission::runtime::receipt([&](auto& s){return s.drop(held);});return;
    }
    if(snapshot.phase!=omega::mission::Phase::route || !snapshot.chargeEnabled) return;
    std::array<MissionArcSource,2> pair{};std::uint32_t sink{},player=UINT32_MAX;
    if(!mission_arc_pair(snapshot.command.cycle,pair,sink) || pair[0].entity!=entity) return;
    if(!mission_arc_holder(component,bytes,player)) return;
    constexpr std::array<std::uint32_t,3> registries{0x0040BF06,0x0040BF05,0x0040BF03};
    constexpr std::array<std::uint16_t,3> sources{18,1,0},sinks{20,3,2};
    const auto index=snapshot.command.cycle-1;
    omega::mission::ChargeReceipt receipt{snapshot.command.token,pair[0].source,snapshot.generation,entity,player,sink,
        registries[index],sources[index],sinks[index]};
    if(!omega::mission::runtime::receipt([&](auto& s){return s.pickup(receipt);})) return;
    const std::lock_guard lock(missionArcMutex);missionArcHeld=receipt;missionArcHolding=true;
    log("ev=omega_mission stage=arc_pickup run=%llu epoch=%u cycle=%u item=%08X player=%08X sink=%08X state=%u receipt=native_carried",
        receipt.token.boss.run,receipt.token.epoch,snapshot.command.cycle,entity,player,sink,carryState);
}
void __fastcall mission_arc_carry(std::byte* component,std::uint8_t value,const void* holder) noexcept {
    const hooking::CallGate::Scope call(callGate);
    hooking::await_original(arcCarryOriginal)(component,value,holder);
    if(call.accepts_side_effects() && active()) observe_mission_arc_carry(component);
}
void poll_mission_arc_carry() noexcept {
    // The existing bounded 100 ms device pump calls this once after source
    // adoption. Re-read committed native state if its callback preceded the
    // source/holder join; never replay a native pickup or infer one by time.
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    if(!snapshot.chargeEnabled || snapshot.command.cycle<1 || snapshot.command.cycle>3
        || (snapshot.phase!=omega::mission::Phase::route && snapshot.phase!=omega::mission::Phase::carrying)) return;
    MissionArcSource source;
    {const std::lock_guard lock(missionArcMutex);source=missionArcSources[snapshot.command.cycle-1][0];}
    std::uint32_t carry{};
    // Shared carry component's actual registered interface (captured table).
    if(mission_arc_source(source) && mission_arc_component(source.entity,0x80803F6AU,0x80F66667U,0x80804221U,0x598,carry))
        observe_mission_arc_carry(resolve_handle(carry));
}
bool mission_arc_user(std::span<const std::byte> weak,std::uint32_t& entity,const char** failure=nullptr) noexcept {
    // F33A90 stores the interacting player record, not its controlled world
    // entity. Original 4B2260 reads that player's +54 controlled entity.
    const auto reject=[&](const char* reason) {if(failure) *failure=reason;return false;};
    std::uint32_t player{},stride{};const std::byte* pool{};
    if(!mission_weak(weak,player)) return reject("player_weak");
    if(!copy_value(image+0x1F90E18,pool) || !pool
        || !copy_value(image+0x1F90E20,stride) || stride<0x58 || stride>0x4000) return reject("player_pool");
    const auto* record=pool+(player&0x1FFFU)*stride;
    std::array<std::byte,0x58> bytes{};
    // +44 is not an established player identity field. Salt validation comes
    // from original 352310 above; the resolved record must belong to this pool.
    if(resolve_handle(player)!=record || !copy_native(record,bytes.data(),bytes.size())) return reject("player_record");
    entity=read<std::uint32_t>(bytes.data(),0x54);
    const std::byte* worlds{};std::uint32_t worldStride{},identity{},again{};
    if(!(entity!=UINT32_MAX && copy_value(image+0x1F93428,worlds) && worlds
        && copy_value(image+0x1F93430,worldStride) && worldStride>=0xE0 && worldStride<=0x1000
        && copy_value(worlds+(entity&0x1FFFU)*worldStride+0xC,identity) && identity==entity)) return reject("player_world");
    if(!mission_weak(weak,again) || again!=player) return reject("player_weak_changed");
    return true;
}
void __fastcall mission_arc_use(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    omega::mission::ChargeReceipt held{};bool observe{};
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    {
        const std::lock_guard lock(missionArcMutex);
        observe=call.accepts_side_effects() && active() && !missionArcUse && missionArcHolding
            && missionArcHeld.token==snapshot.command.token && snapshot.phase==omega::mission::Phase::carrying;
        if(observe) held=missionArcHeld;
    }
    std::array<std::byte,0x2E8> before{},after{};std::array<MissionArcSource,2> pair{};
    std::uint32_t sink{},player{};
    const bool candidate=observe;
    const char* reason="accepted";const char* actorFailure="none";
    if(!observe) reason="no_current_held_receipt";
    else if(!mission_arc_pair(snapshot.command.cycle,pair,sink) || sink!=held.sink
        || resolve_handle(sink)!=component) reason="sink_binding";
    else if(!copy_native(component,before.data(),before.size())) reason="sink_read";
    else if(!mission_arc_user(std::span(before).subspan(0x2E0,8),player,&actorFailure)) reason="pre_player";
    else if(player!=held.player) reason="pre_holder_mismatch";
    else if(read<std::int32_t>(before.data(),0x2D8)<0
        || read<std::int32_t>(before.data(),0x2DC)<=read<std::int32_t>(before.data(),0x2D8)) reason="no_new_request";
    else observe=true;
    observe=observe && reason==std::string_view("accepted");
    if(observe) {missionArcUse=true;missionArcDeferredDrop=false;}
    hooking::await_original(arcUseOriginal)(component);
    if(!observe) {
        // Only log an attempted use on this encounter's held sink. No polling
        // or per-frame diagnostic scan; the original is always forwarded.
        if(candidate && resolve_handle(held.sink)==component) log("ev=omega_mission stage=arc_use_rejected run=%llu epoch=%u cycle=%u sink=%08X reason=%s actor_check=%s requested=%d consumed=%d actor=%08X expected=%08X",
            held.token.boss.run,held.token.epoch,snapshot.command.cycle,held.sink,reason,actorFailure,
            read<std::int32_t>(before.data(),0x2DC),read<std::int32_t>(before.data(),0x2D8),player,held.player);
        return;
    }
    missionArcUse=false;
    if(!call.accepts_side_effects() || !mission_arc_source(pair[1])) reason="post_source";
    else if(!copy_native(resolve_handle(sink),after.data(),after.size())) reason="post_sink_read";
    else if(!std::equal(before.begin(),before.begin()+16,after.begin())
        || read<std::uint32_t>(after.data(),0x24)!=sink || read<std::uint32_t>(after.data(),0x2C)!=pair[1].entity) reason="post_sink_identity";
    else if(!read<std::uint8_t>(after.data(),0x2D0)) reason="not_used";
    else if(read<std::uint32_t>(after.data(),0x2D8)!=read<std::uint32_t>(before.data(),0x2DC)) reason="request_not_consumed";
    else if(!std::equal(before.begin()+0x2E0,before.end(),after.begin()+0x2E0)) reason="actor_association_changed";
    else if(!mission_arc_user(std::span(after).subspan(0x2E0,8),player,&actorFailure)) reason="post_player";
    else if(player!=held.player) reason="post_holder_mismatch";
    const bool consumed=reason==std::string_view("accepted");
    const bool accepted=consumed && omega::mission::runtime::receipt([&](auto& s){return s.dunk(held);});
    if(consumed && !accepted) reason="mission_state";
    log("ev=omega_mission stage=arc_use_result run=%llu epoch=%u cycle=%u sink=%08X accepted=%u reason=%s actor_check=%s requested=%d consumed_before=%d consumed_after=%d used=%u player_record=%08X actor=%08X expected=%08X",
        held.token.boss.run,held.token.epoch,snapshot.command.cycle,sink,accepted?1U:0U,reason,actorFailure,
        read<std::int32_t>(before.data(),0x2DC),read<std::int32_t>(before.data(),0x2D8),read<std::int32_t>(after.data(),0x2D8),
        read<std::uint8_t>(after.data(),0x2D0),read<std::uint32_t>(before.data(),0x2E4),player,held.player);
    if(accepted || missionArcDeferredDrop) {
        {const std::lock_guard lock(missionArcMutex);if(missionArcHeld==held) missionArcHolding=false;}
        if(!accepted) omega::mission::runtime::receipt([&](auto& s){return s.drop(held);});
    }
    missionArcDeferredDrop=false;
    if(accepted) log("ev=omega_mission stage=arc_dunk run=%llu epoch=%u cycle=%u item=%08X player=%08X sink=%08X receipt=native_consumed_request",
        held.token.boss.run,held.token.epoch,snapshot.command.cycle,held.item,held.player,sink);
}
