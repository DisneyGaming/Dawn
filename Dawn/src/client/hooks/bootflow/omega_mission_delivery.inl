void deliver_mission_source(std::byte* component,const hooking::CallGate::Scope& call) noexcept {
    namespace delivery=omega_mission_delivery;
    std::uint32_t tag{};
    if(!copy_value(component,tag)) return;
    bool known=false;
    for(const auto& source:omega::mission::kSources) known|=source.definition==tag;
    if(!known) return;
    const auto run=state::activity::mission_run_generation();
    const auto snapshot=omega::mission::runtime::snapshot(run);
    if(!snapshot.generation) return;
    std::array<std::byte,0x690> before{};
    if(!copy_native(component,before.data(),before.size())) return;
    const auto* source=delivery::source(before);
    if(!source) return;
    const auto index=omega::mission::source_index(source->registry,source->slot);
    if(snapshot.requested[index][0]+snapshot.requested[index][1]==0) return;
    {
        const std::lock_guard lock(mutex); reset(run);
        if(runState.missionDeliveryClaimed[index]) return;
    }
    const auto handle=read<std::uint32_t>(before.data(),0x170);
    auto* object=resolve_handle(handle);
    std::array<std::byte,0x70> objectBody{};
    if(!copy_native(object,objectBody.data(),objectBody.size())
        || read<std::uint32_t>(objectBody.data(),0xC)!=0x80807EC9U) return;
    using Resolve=const std::byte*(__fastcall*)(std::byte*,std::uint32_t) noexcept;
    const auto* authority=native<Resolve>(0x9FEC30)(object,0xC4);
    std::array<std::byte,0xC4> body{},fresh{};
    if(!copy_native(authority,body.data(),body.size()) || !delivery::pending(*source,snapshot,objectBody,body)
        || omega_lair_delivery::adopted(before,body)) return;
    std::array<std::byte,0x690> current{};
    if(!call.accepts_side_effects() || state::activity::mission_run_generation()!=run
        || resolve_handle(handle)!=object || !copy_native(component,current.data(),current.size())
        || delivery::source(current)!=source || read<std::uint32_t>(current.data(),0x170)!=handle
        || !copy_native(authority,fresh.data(),fresh.size()) || fresh!=body
        || omega_lair_delivery::adopted(current,body)) return;
    {
        const std::lock_guard lock(mutex);
        if(runState.run!=run || runState.missionDeliveryClaimed[index] || !call.accepts_side_effects()) return;
        runState.missionDeliveryClaimed[index]=true;
    }
    struct Message {std::uint32_t schema,padding;const std::byte* body;};
    const Message message{0x80807EC9U,0,body.data()};
    native<void(__fastcall*)(std::byte*,const Message*) noexcept>(0x4E8FB0)(component,&message);
    const bool adopted=copy_native(component,current.data(),current.size())
        && delivery::source(current)==source && omega_lair_delivery::adopted(current,body);
    log("ev=omega_mission stage=source_delivery run=%llu registry=%08X slot=%u generation=%u requested=%u,%u member=%u adopted=%u",
        run,source->registry,source->slot,snapshot.generation,snapshot.requested[index][0],snapshot.requested[index][1],source->member,adopted?1U:0U);
}
