#include "omega_mission_eye_control.inl"
bool mission_crown_queue(const graph::Owner& owner,const MemberView& member,unsigned cycle) noexcept {
    if(cycle<1 || cycle>3 || !member.enabled || member.head!=0 || member.count!=1) return false;
    graph::Queue queue{};
    return copy_native(resolve_handle(owner.member)+owner.memberOffset+0x230,queue.data(),queue.size())
        && graph::queue_matches(queue,crown::cycles[cycle-1].sequence);
}
bool mission_named_sequence(const graph::Owner& owner,std::uint32_t name,std::uint32_t resource) noexcept {
    std::array<std::byte,0x4C9> config{};
    auto* originalConfig=resolve_handle(0x80F4519AU);
    if(!copy_native(originalConfig,config.data(),config.size()) || read<std::uint64_t>(config.data(),0x10)!=7
        || read<std::uint64_t>(config.data(),0x18)!=0x38) return false;
    const std::byte* expected{};
    for(unsigned i=0;i<7;++i) {
        const auto at=0x60+i*24;
        if(read<std::uint32_t>(config.data(),at)==name && read<std::uint8_t>(config.data(),at+4)==0
            && read<std::uint32_t>(config.data(),at+16)==resource) expected=originalConfig+at;
    }
    if(!expected) return false;
    using Find=const std::byte*(__fastcall*)(const std::byte*,const std::uint32_t*) noexcept;
    const auto* entry=native<Find>(0xA89430)(originalConfig,&name);
    if(entry!=expected) return false;
    std::array<std::byte,0x860> before{},after{};
    auto* channel=resolve_handle(owner.animation)+0x1980;
    if(!copy_native(channel,before.data(),before.size()) || read<std::uint32_t>(before.data(),0)!=owner.character
        || read<std::uint32_t>(before.data(),4)!=owner.animation) return false;
    bool capacity{};
    for(unsigned i=0;i<8;++i) {
        const auto hash=read<std::uint32_t>(before.data(),0x20+i*0x108);
        if(hash==name) return false;
        if(hash==0x811C9DC5U) capacity=true;
    }
    if(!capacity) return false;
    native<bool(__fastcall*)(std::byte*,const std::byte*,bool) noexcept>(0xC66590)(channel,entry,true);
    MemberView member{};graph::Owner current{};
    if(!current_owner(owner,member,current) || current!=owner || !copy_native(resolve_handle(owner.animation)+0x1980,after.data(),after.size())) return false;
    unsigned matches{};
    for(unsigned i=0;i<8;++i) if(read<std::uint32_t>(after.data(),0x20+i*0x108)==name
        && read<std::uint8_t>(after.data(),0x120+i*0x108)==1) ++matches;
    const bool accepted=matches==1 && read<std::uint32_t>(after.data(),0)==owner.character
        && read<std::uint32_t>(after.data(),4)==owner.animation;
    if(accepted && name==0xC57C61EBU) {
        const std::lock_guard lock(mutex);
        if(runState.graphOwner!=owner) return false;
        for(unsigned i=0;i<8;++i) if(read<std::uint32_t>(after.data(),0x20+i*0x108)==name) {
            runState.crown.refillSlot=i;
            std::memcpy(runState.crown.refillIdentity.data(),after.data()+0x28+i*0x108,16);
        }
    }
    log("ev=omega_mission stage=named_sequence run=%llu name=%08X resource=%08X confirmed=%u receipt=native_slot",
        owner.run,name,resource,accepted?1U:0U);
    return accepted;
}
bool stop_mission_refill(const graph::Owner& owner,const crown::Track& track) noexcept {
    if(track.refillSlot>=8) return true;
    auto* channel=resolve_handle(owner.animation)+0x1980;
    std::array<std::byte,0x860> before{},after{};
    if(!copy_native(channel,before.data(),before.size()) || read<std::uint32_t>(before.data(),0)!=owner.character
        || read<std::uint32_t>(before.data(),4)!=owner.animation) return false;
    constexpr std::uint32_t name=0xC57C61EBU;
    const auto offset=0x20+track.refillSlot*0x108;
    // A naturally retired/reused slot is no longer ours. Never stop its replacement.
    if(read<std::uint32_t>(before.data(),offset)!=name) return true;
    if(std::memcmp(before.data()+offset+8,track.refillIdentity.data(),16)!=0) return false;
    unsigned matches{};for(unsigned i=0;i<8;++i) matches+=read<std::uint32_t>(before.data(),0x20+i*0x108)==name;
    if(matches!=1) return false;
    MemberView member{};graph::Owner current{};
    if(!current_owner(owner,member,current) || current!=owner || !member.enabled) return false;
    using Stop=bool(__fastcall*)(std::byte*,std::byte*,const std::uint32_t*) noexcept;
    const bool stopped=native<Stop>(0xC6FE70)(channel,resolve_handle(owner.character),&name);
    const bool confirmed=stopped && current_owner(owner,member,current) && current==owner
        && copy_native(resolve_handle(owner.animation)+0x1980,after.data(),after.size())
        && read<std::uint32_t>(after.data(),offset)==0x811C9DC5U
        && read<std::uint8_t>(after.data(),offset+0x100)==0;
    log("ev=omega_mission stage=eye_refill_stop run=%llu cycle=%u slot=%u confirmed=%u receipt=native_named_child_stop",
        owner.run,track.cycle,track.refillSlot,confirmed?1U:0U);
    return confirmed;
}
void pump_mission_crown(const hooking::CallGate::Scope& call) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::crown_pump);
    namespace mission=omega::mission;
    const auto snapshot=mission::runtime::snapshot(state::activity::mission_run_generation());
    if(!snapshot.generation || snapshot.command.cycle<1 || snapshot.command.cycle>3 || !call.accepts_side_effects()) return;
    graph::Owner owner{};crown::Track track{};
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=snapshot.command.token.boss.run || !runState.queueAccepted) return;
        owner=runState.graphOwner;track=runState.crown;
    }
    MemberView member{};graph::Owner current{};
    if(!current_owner(owner,member,current) || current!=owner || !member.enabled || track.uncertain) return;
    const auto token=snapshot.command.token;
    const auto cycle=snapshot.command.cycle;
    const auto& definition=crown::cycles[cycle-1];
    if(snapshot.command.action==mission::Action::startCycle && track.start!=token) {
        if(track.accepted && !track.retired) {
            if(!track.stopClaimed) {
                if(!mission_crown_queue(owner,member,track.cycle)
                    || (track.lease.state()!=graph::LeaseState::idle && track.lease.state()!=graph::LeaseState::released)) return;
                {
                    const std::lock_guard lock(mutex);
                    if(runState.crown.start!=track.start || runState.crown.stopClaimed) return;
                    runState.crown.stopClaimed=true;
                }
                auto request=graph::make_condition_request(crown::cycles[track.cycle-1].sequence,0);
                request[0x60]=std::byte{0x5D};
                native<void(__fastcall*)(std::byte*,const void*,const void*) noexcept>(0xC693F0)
                    (resolve_handle(owner.character),request.data(),nullptr);
                return;
            }
            if(member.head!=member.count) return;
        }
        MissionMotionView motionView{};
        if(member.head!=member.count || !mission_motion_view(owner,motionView)
            || !motion::departure_ready(motionView.bytes,motionView.arena)) return;
        if(!mission::runtime::receipt([&](auto& s){return s.claim(token,mission::Action::startCycle);})) return;
        {
            const std::lock_guard lock(mutex);
            runState.crown={};runState.crown.start=token;runState.crown.cycle=cycle;runState.crown.queueClaimed=true;
        }
        const auto queue=graph::make_queue(definition.sequence);
        native<void(__fastcall*)(std::byte*,const void*,std::int32_t) noexcept>(0xAB6C60)
            (resolve_handle(owner.member)+owner.memberOffset,queue.data(),0);
        const bool accepted=current_owner(owner,member,current) && current==owner && mission_crown_queue(owner,member,cycle);
        const std::lock_guard lock(mutex);
        if(runState.run!=owner.run || runState.crown.start!=token) return;
        runState.crown.accepted=accepted;runState.crown.uncertain=!accepted;
        log("ev=omega_mission stage=cycle_queue run=%llu epoch=%u cycle=%u sequence=%08X confirmed=%u",
            owner.run,token.epoch,cycle,definition.sequence,accepted?1U:0U);
        return;
    }
    if(!track.accepted || track.cycle!=cycle || !mission_crown_queue(owner,member,cycle)) return;
    std::uint32_t event{};
    switch(snapshot.command.action) {
    case mission::Action::deletion:event=definition.deletion;break;
    case mission::Action::shield:event=definition.shield;break;
    case mission::Action::recover:case mission::Action::finalDeath:event=definition.eyeEnd;break;
    default:return;
    }
    if(snapshot.command.claimed || track.command==token) return;
    if(track.lease.state()!=graph::LeaseState::idle && track.lease.state()!=graph::LeaseState::released) return;
    graph::EventTable before{};
    if(!event_table(owner,before)) return;
    if(!mission::runtime::receipt([&](auto& s){return s.claim(token,snapshot.command.action);})) return;
    if(snapshot.command.action==mission::Action::shield)
        log("ev=omega_mission stage=eye_shield_claim run=%llu epoch=%u cycle=%u arrival=%u",
            owner.run,token.epoch,cycle,snapshot.eyePlatform?1U:0U);
    {
        const std::lock_guard lock(mutex);
        if(runState.crown.start!=track.start) return;
        runState.crown.command=token;runState.crown.event=event;runState.crown.lease={};
    }
    if(snapshot.command.action==mission::Action::shield && !mission_named_sequence(owner,0xC57C61EBU,0x80F45564U)) {
        const std::lock_guard lock(mutex);runState.crown.uncertain=true;return;
    }
    if(!event_table(owner,before)) return;
    {
        const std::lock_guard lock(mutex);
        if(!runState.crown.lease.begin_add(owner,before,true,true,{event,definition.ordinal,0,1})) return;
    }
    const auto request=graph::make_condition_request(definition.sequence,event);
    native<void(__fastcall*)(std::byte*,const void*,const void*) noexcept>(0xC620F0)
        (resolve_handle(owner.character),request.data(),nullptr);
    graph::EventTable after{};
    const bool confirmed=current_owner(owner,member,current) && current==owner && event_table(owner,after);
    const std::lock_guard lock(mutex);
    if(runState.crown.command!=token) return;
    const bool added=runState.crown.lease.finish_add(current,after,confirmed);
    log("ev=omega_mission stage=cycle_event run=%llu epoch=%u cycle=%u event=%08X confirmed=%u",owner.run,token.epoch,cycle,event,added?1U:0U);
}
void observe_mission_crown(std::span<const std::byte> bytes,bool result,const hooking::CallGate::Scope& call) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::crown_observe);
    namespace mission=omega::mission;
    const auto snapshot=mission::runtime::snapshot(state::activity::mission_run_generation());
    graph::Owner owner{};crown::Track track{};
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || !runState.crown.accepted || runState.crown.uncertain) return;
        owner=runState.graphOwner;track=runState.crown;
    }
    if(!call.accepts_side_effects() || track.cycle!=snapshot.command.cycle
        || read<std::uint32_t>(bytes.data(),0)!=owner.entity || read<std::uint32_t>(bytes.data(),4)!=owner.character
        || read<std::uint32_t>(bytes.data(),0x14)!=owner.biped || read<std::uint32_t>(bytes.data(),0x18)!=owner.entity
        || read<std::uint32_t>(bytes.data(),0x1C)!=owner.biped) return;
    MemberView member{};graph::Owner current{};std::array<std::byte,0x440> bank{};
    if(!current_owner(owner,member,current) || current!=owner || !mission_crown_queue(owner,member,track.cycle)
        || !copy_native(resolve_handle(observation::kBankAsset),bank.data(),bank.size())) return;
    crown::Observation state{};
    if(!crown::decode(bytes,bank,track.cycle,result,state)) return;
    const auto token=snapshot.command.token;
    bool remove{};
    if(track.cycle<3 && snapshot.command.claimed) {
        const bool expose=(snapshot.phase==mission::Phase::shield || snapshot.phase==mission::Phase::eye)
            && (state.node==crown::Node::eyeOpening || state.node==crown::Node::eye);
        const bool release=track.eyeOwned && state.node==crown::Node::recover;
        if((expose || release) && !hold_mission_eye(owner,token,expose,call)) return;
    }
    if(track.lease.state()==graph::LeaseState::owned) {
        const auto& definition=crown::cycles[track.cycle-1];
        remove=(track.event==definition.deletion && state.node==crown::Node::deletion)
            || (track.event==definition.shield && state.node==crown::Node::eyeOpening)
            || (track.event==definition.eyeEnd && (state.node==crown::Node::recover || state.node==crown::Node::death));
    }
    if(remove) {
        graph::EventTable before{},after{};
        if(!event_table(owner,before)) return;
        {
            const std::lock_guard lock(mutex);
            if(runState.crown.command!=track.command || !runState.crown.lease.begin_remove(owner,before)) return;
        }
        const auto request=graph::make_condition_request(crown::cycles[track.cycle-1].sequence,track.event);
        native<void(__fastcall*)(std::byte*,const void*,const void*) noexcept>(0xC693F0)
            (resolve_handle(owner.character),request.data(),nullptr);
        const bool confirmed=current_owner(owner,member,current) && current==owner && event_table(owner,after);
        const std::lock_guard lock(mutex);
        if(runState.crown.command!=track.command || !runState.crown.lease.finish_remove(current,after,confirmed)) return;
    }
    bool checkpoint{},kill{};
    {
        const std::lock_guard lock(mutex);
        if(runState.crown.start!=track.start) return;
        if(runState.crown.node!=state.node) log("ev=omega_mission stage=cycle_node run=%llu cycle=%u node=%d clip=%08X terminal=%u",
            owner.run,track.cycle,state.index,state.clip,state.terminal?1U:0U);
        runState.crown.node=state.node;
        if(state.node==crown::Node::summon) runState.crown.summoned=true;
        if(state.node==crown::Node::recover && !runState.crown.recoverSeen) {runState.crown.recoverSeen=true;checkpoint=true;}
        if(state.terminal && !runState.crown.deathFinished) {runState.crown.deathFinished=true;kill=true;}
    }
    if(snapshot.command.action==mission::Action::startCycle) {
        if(state.node==crown::Node::summon) mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::started);});
        if(state.node==crown::Node::idle && track.summoned) mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::finished);});
    } else if(snapshot.command.action==mission::Action::deletion) {
        if(state.node==crown::Node::deletion) mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::started);});
        if(state.node==crown::Node::hold) mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::deletionHold);});
    } else if(snapshot.command.action==mission::Action::shield && state.node==crown::Node::eye)
        mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::eyeExposed);});
    else if(snapshot.command.action==mission::Action::recover && state.node==crown::Node::idle && track.recoverSeen)
        mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::finished);});
    else if(kill) mission::runtime::receipt([&](auto& s){return s.animation(token,mission::Animation::finished);});
    if(checkpoint && (!stop_mission_refill(owner,track)
        || !mission_named_sequence(owner,track.cycle==1?0xB52CCA3BU:0xB52CCA38U,track.cycle==1?0x80F4545CU:0x80F4545EU))) {
        const std::lock_guard lock(mutex);runState.crown.uncertain=true;
    }
    if(kill) (void)mission_named_sequence(owner,0x9527E28AU,0x80F45460U);
    // Sample on the validated, ongoing Crown playback callback. The member
    // authority callback is not evidence that damage-phase playback was polled.
    // The sampler revalidates the current command and complete native owner.
    observe_mission_health(call);
}
