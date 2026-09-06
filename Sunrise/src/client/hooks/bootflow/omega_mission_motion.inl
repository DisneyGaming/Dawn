// The native selector owns placement, animation, VFX and movement. This bridge
// submits one authored selector action and observes its original lifecycle.
struct MissionMotionView {
    std::array<std::byte,0x1800> bytes{};
    motion::Arena arena{};
    std::byte* component{};
    std::byte* selector{};
    std::uint32_t selectorHandle{UINT32_MAX};
    bool selectorIdle{};
};
bool mission_motion_view(const graph::Owner& owner,MissionMotionView& out) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::motion_view);
    std::array<std::byte,0x590> character{};
    if(!copy_native(resolve_handle(owner.character),character.data(),character.size())
        || read<std::uint32_t>(character.data(),0x24)!=owner.character
        || read<std::uint32_t>(character.data(),0x2C)!=owner.entity) return false;
    const auto handle=read<std::uint32_t>(character.data(),0x428);
    out.component=resolve_handle(handle);
    if(!copy_native(out.component,out.bytes.data(),out.bytes.size()) || !motion::arena(out.bytes,out.arena)
        || out.arena.self!=handle || out.arena.entity!=owner.entity) return false;
    const auto displacement=read<std::int64_t>(character.data(),0x588);
    if(displacement<0 || displacement>0x10000) return false;
    auto* row=resolve_handle(owner.character)+0x588+displacement+0x90;
    std::array<std::byte,24> tuple{};
    if(!copy_native(row,tuple.data(),tuple.size()) || read<std::uint32_t>(tuple.data(),0)!=0x80FEE862U
        || read<std::uint32_t>(tuple.data(),12)!=0x80806750U || read<std::int64_t>(tuple.data(),16)!=0) return false;
    out.selectorHandle=read<std::uint32_t>(tuple.data(),8); out.selector=resolve_handle(out.selectorHandle);
    std::array<std::byte,0x98> selector{},movement{};
    if(!copy_native(out.selector,selector.data(),selector.size())
        || !omega_reveal_source::matches(selector,{0x80F45176U,0x80806751U,0x188})
        || read<std::uint32_t>(selector.data(),0x24)!=out.selectorHandle
        || read<std::uint32_t>(selector.data(),0x2C)!=owner.entity
        || read<std::uint32_t>(selector.data(),0x30)!=owner.character
        || read<std::uint32_t>(selector.data(),0x78)!=handle
        || !copy_native(resolve_handle(out.arena.movement),movement.data(),movement.size())
        || !omega_reveal_source::matches(movement,{0x80F4516EU,0x80803A00U,0xE78})
        || read<std::uint32_t>(movement.data(),0x24)!=out.arena.movement
        || read<std::uint32_t>(movement.data(),0x2C)!=owner.entity) return false;
    out.selectorIdle=read<std::uint8_t>(selector.data(),0x90)==0;
    return true;
}
bool mission_motion_owner(graph::Owner& owner,omega::mission::Snapshot& snapshot) noexcept {
    const auto run=state::activity::mission_run_generation();
    snapshot=omega::mission::runtime::snapshot(run);
    if(snapshot.command.action!=omega::mission::Action::depart || snapshot.phase!=omega::mission::Phase::departure) return false;
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=run || !runState.queueAccepted) return false;
        owner=runState.graphOwner;
    }
    MemberView member{};graph::Owner current{};
    return current_owner(owner,member,current) && current==owner && member.enabled
        && snapshot.command.token.boss==omega::mission::Boss{owner.run,owner.generation,owner.actor,
            owner.character,owner.biped,owner.entity,owner.member,owner.revision};
}
bool mission_motion_raw(const graph::Owner& owner,const omega::mission::Token& token,std::byte* raw,
    motion::Identity& identity,std::array<std::byte,0xF0>& bytes) noexcept {
    MissionMotionView view{};
    if(!mission_motion_view(owner,view) || !copy_native(raw,bytes.data(),bytes.size())
        || read<std::uint32_t>(bytes.data(),0x30)!=owner.entity
        || read<std::uint32_t>(bytes.data(),0x38)!=view.arena.movement
        || read<std::uint32_t>(bytes.data(),0x6C)!=0x3D
        || read<std::uint32_t>(bytes.data(),0x58)!=view.selectorHandle
        || read<std::int64_t>(bytes.data(),0x60)!=0) return false;
    for(unsigned i=0;i<view.arena.count;++i) {
        const auto& slot=view.arena.slots[i];
        if(slot.opcode==0x3D && slot.size==0xF0 && slot.flags==0
            && view.component+view.arena.data+slot.offset==raw) {
            identity={token,view.arena.self,view.arena.movement,view.selectorHandle,slot.slot}; return true;
        }
    }
    return false;
}
void mission_motion_sample(const graph::Owner& owner,const omega::mission::Token& token,std::byte* raw) noexcept {
    motion::Identity identity{};std::array<std::byte,0xF0> bytes{};
    if(!mission_motion_raw(owner,token,raw,identity,bytes)) return;
    motion::Point target{};std::memcpy(target.data(),bytes.data()+0x10,16);
    const auto stage=read<std::uint8_t>(bytes.data(),0xA4);
    const std::lock_guard lock(mutex);
    if(runState.run!=owner.run || runState.graphOwner!=owner) return;
    const auto before=runState.departure.stage;
    if(runState.departure.observe(identity,stage,target,read<std::uint32_t>(bytes.data(),0x34)) && before!=stage)
        log("ev=omega_mission stage=departure_motion run=%llu epoch=%u motion=%08X slot=%u native_stage=%u movement=%08X target=%.5f,%.5f,%.5f",
            owner.run,token.epoch,identity.component,identity.slot,stage,identity.movement,
            static_cast<double>(target[0]),static_cast<double>(target[1]),static_cast<double>(target[2]));
}
bool __fastcall mission_motion_update(void* descriptor,const void* context,std::byte* raw) noexcept {
    const hooking::CallGate::Scope call(callGate);
    graph::Owner owner{};omega::mission::Snapshot snapshot{};
    const bool observe=call.accepts_side_effects() && active() && mission_motion_owner(owner,snapshot);
    if(observe) mission_motion_sample(owner,snapshot.command.token,raw);
    const bool result=hooking::await_original(motionUpdateOriginal)(descriptor,context,raw);
    if(observe && call.accepts_side_effects()) mission_motion_sample(owner,snapshot.command.token,raw);
    return result;
}
void __fastcall mission_motion_cleanup(void* descriptor,std::byte* raw,void* context) noexcept {
    const hooking::CallGate::Scope call(callGate);
    graph::Owner owner{};omega::mission::Snapshot snapshot{};motion::Identity identity{};
    std::array<std::byte,0xF0> bytes{};
    const bool observe=call.accepts_side_effects() && active() && mission_motion_owner(owner,snapshot)
        && mission_motion_raw(owner,snapshot.command.token,raw,identity,bytes);
    hooking::await_original(motionCleanupOriginal)(descriptor,raw,context);
    if(!observe || !call.accepts_side_effects()) return;
    const std::lock_guard lock(mutex);
    if(runState.run==owner.run && runState.graphOwner==owner && runState.departure.cleanup(identity))
        log("ev=omega_mission stage=departure_cleanup run=%llu epoch=%u motion=%08X slot=%u",owner.run,
            identity.token.epoch,identity.component,identity.slot);
}
bool mission_physics_position(std::uint32_t handle,std::uint32_t entity,motion::Point& position) noexcept {
    std::array<std::byte,0x240> bytes{};
    auto* component=resolve_handle(handle);
    if(!copy_native(component,bytes.data(),bytes.size()) || read<std::uint32_t>(bytes.data(),0x24)!=handle
        || read<std::uint32_t>(bytes.data(),0x2C)!=entity) return false;
    // 458A80 and component+150 are velocity, saved by the teleport constructor
    // at raw+20 and restored by 464890. Read the published entity translation
    // through original 558330, which decodes world-row+D0 and restores W=1.
    const std::byte* rows{};std::uint32_t stride{};
    if(!copy_value(image+0x1F93428,rows) || !copy_value(image+0x1F93430,stride)
        || !rows || stride<0xE0 || stride>0x1000) return false;
    auto* row=rows+static_cast<std::size_t>(entity&0x1FFF)*stride;
    std::array<std::byte,0xE0> world{};
    if(!copy_native(row,world.data(),world.size()) || read<std::uint32_t>(world.data(),0xC)!=entity
        || (read<std::uint8_t>(world.data(),4)&1)!=0) return false;
    using Get=void(__fastcall*)(const std::byte*,motion::Point*) noexcept;
    // Entry adjusts its embedded interface pointer by signed -0x60.
    native<Get>(0x558330)(row+0x60,&position);
    std::uint32_t after{};
    return copy_value(row+0xC,after) && after==entity && motion::finite(position);
}
void pump_mission_motion(const hooking::CallGate::Scope& call) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::motion_pump);
    graph::Owner owner{};omega::mission::Snapshot snapshot{};
    if(!call.accepts_side_effects() || !mission_motion_owner(owner,snapshot) || snapshot.command.island>=5) return;
    const auto token=snapshot.command.token;
    MissionMotionView view{};
    if(!mission_motion_view(owner,view)) return;
    motion::Track track{};
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=owner.run || runState.graphOwner!=owner) return;
        if(runState.departure.token!=token) { runState.departure={};runState.departure.token=token; }
        track=runState.departure;
    }
    if(track.uncertain || track.completed) return;
    if(!track.stopClaimed) {
        MemberView member{};graph::Owner fresh{};
        if(!current_owner(owner,member,fresh) || fresh!=owner || !member.enabled) return;
        const bool intro=snapshot.command.island==0;
        const bool final=snapshot.command.island==4;
        if(final) {
            if(!mission_crown_queue(owner,member,2) || view.arena.count!=1 || view.arena.slots[0].opcode!=0x39) return;
            const std::lock_guard lock(mutex);
            if(!runState.crown.accepted || runState.crown.cycle!=2 || runState.crown.stopClaimed
                || (runState.crown.lease.state()!=graph::LeaseState::idle && runState.crown.lease.state()!=graph::LeaseState::released)) return;
        } else if(intro ? !member.exactQueue || member.head!=0 || view.arena.count!=1 || view.arena.slots[0].opcode!=0x39
            : member.head!=member.count || !motion::departure_ready(view.bytes,view.arena)) return;
        if(!omega::mission::runtime::receipt([&](auto& s){return s.claim(token,omega::mission::Action::depart);})) return;
        {
            const std::lock_guard lock(mutex);
            if(runState.run!=owner.run || runState.departure.token!=token) return;
            runState.departure.stopClaimed=true;
            if(final) runState.crown.stopClaimed=true;
        }
        if(intro || final) {
            const auto sequence=final?crown::cycles[1].sequence:graph::kSequence;
            auto request=graph::make_condition_request(sequence,0);
            graph::write(request,8,std::uint32_t{});request[0x60]=std::byte{0x5D};
            native<void(__fastcall*)(std::byte*,const void*,const void*) noexcept>(0xC693F0)
                (resolve_handle(owner.character),request.data(),nullptr);
            log("ev=omega_mission stage=departure_stop_requested run=%llu epoch=%u actor=%08X group=%08X sequence=%08X",
                owner.run,token.epoch,owner.actor,graph::kGroup,sequence);
        }
        return;
    }
    MemberView member{};graph::Owner fresh{};
    if(!current_owner(owner,member,fresh) || fresh!=owner || !member.enabled || member.head!=member.count) return;
    retire_queue(owner,fresh);
    if(snapshot.command.island==4) {
        const std::lock_guard lock(mutex);
        if(runState.departure.token!=token || !runState.crown.stopClaimed) return;
        runState.crown.retired=true;
    }
    if(!track.requestClaimed) {
        if(!view.selectorIdle || !motion::departure_ready(view.bytes,view.arena)) return;
        {
            const std::lock_guard lock(mutex);
            if(runState.run!=owner.run || runState.departure.token!=token || runState.departure.requestClaimed) return;
            runState.departure.requestClaimed=true;
            // Mark before native dispatch so any synchronous native callback
            // is qualified by this command, never by a later mission epoch.
            runState.departure.submitted=true;
        }
        const auto target=motion::destinations[snapshot.command.island];
        using Start=bool(__fastcall*)(std::byte*,std::uint8_t,std::uint32_t,const motion::Point*,std::int32_t) noexcept;
        // Captured group1 interface +30 resolves to this original method.
        // 80F45176 has one mode4 selector: 3s disappearance, .4s reappearance,
        // and its authored departure/arrival event resources.
        const bool accepted=native<Start>(0x10C6AF0)(view.selector,0,UINT32_MAX,&target,0);
        const std::lock_guard lock(mutex);
        if(runState.run!=owner.run || runState.departure.token!=token) return;
        if(!accepted) runState.departure.uncertain=true;
        log("ev=omega_mission stage=departure_requested run=%llu epoch=%u island=%u selector=%08X accepted=%u readiness=%s",
            owner.run,token.epoch,snapshot.command.island,view.selectorHandle,accepted?1U:0U,
            view.arena.count?"native_default_locomotion":"empty_arena");
        return;
    }
    if(!track.cleaned) return;
    bool retired=true;
    for(unsigned i=0;i<view.arena.count;++i) if(view.arena.slots[i].opcode==0x3D) retired=false;
    motion::Point actual{};
    const bool positionValid=mission_physics_position(track.physics,owner.entity,actual);
    const auto waitMask=static_cast<std::uint8_t>((retired?0:1)|(view.selectorIdle?0:2)
        |(positionValid?0:4)|(positionValid&&motion::position_matches(track.placed,actual,0.1F)?0:8));
    {
        const std::lock_guard lock(mutex);
        if(runState.run!=owner.run || runState.departure.token!=token) return;
        if(waitMask && runState.departure.completionWaitMask!=waitMask) {
            runState.departure.completionWaitMask=waitMask;
            log("ev=omega_mission stage=departure_wait run=%llu epoch=%u island=%u retired=%u selector_idle=%u position_valid=%u actual=%.5f,%.5f,%.5f target=%.5f,%.5f,%.5f",
                owner.run,token.epoch,snapshot.command.island,retired?1U:0U,view.selectorIdle?1U:0U,positionValid?1U:0U,
                static_cast<double>(actual[0]),static_cast<double>(actual[1]),static_cast<double>(actual[2]),
                static_cast<double>(track.placed[0]),static_cast<double>(track.placed[1]),static_cast<double>(track.placed[2]));
        }
    }
    if(!positionValid) return;
    bool finished{};
    {
        const std::lock_guard lock(mutex);
        if(runState.run==owner.run && runState.graphOwner==owner)
            finished=runState.departure.finish(token,retired,view.selectorIdle,actual);
    }
    if(finished && omega::mission::runtime::receipt([&](auto& s){return s.animation(token,omega::mission::Animation::departureFinished);}))
        log("ev=omega_mission stage=departure_complete run=%llu epoch=%u island=%u receipt=native_stages_cleanup_and_position",
            owner.run,token.epoch,snapshot.command.island);
}
