void deliver_rescue_source(std::byte* component,const hooking::CallGate::Scope& call) noexcept {
    namespace delivery=omega_rescue_delivery;
    std::uint32_t tag{};if(!copy_value(component,tag)) return;
    bool known{};for(const auto& row:omega::rescue::sources) known|=row.definition==tag;
    if(!known) return;
    const auto run=state::activity::mission_run_generation();
    const auto snapshot=omega::mission::runtime::snapshot(run);
    if(!snapshot.generation) return;
    std::array<std::byte,0x690> before{},current{};
    if(!copy_native(component,before.data(),before.size())) return;
    const auto* row=delivery::source(before);
    if(!row || !omega::rescue::requested(snapshot,row->slot)) return;
    const auto index=static_cast<std::size_t>(row-omega::rescue::sources.data());
    // Record the typed owning reference even when the normal native path has
    // already adopted it. Scene start revalidates the live authority below.
    const omega_cannon_delivery::Reference ref{read<std::uint32_t>(before.data(),0x160),read<std::int64_t>(before.data(),0x168)};
    std::uint32_t owner=UINT32_MAX;
    native<std::uint32_t*(__fastcall*)(const std::byte*,std::uint32_t*) noexcept>(0x4E5C60)(component,&owner);
    auto* base=resolve_handle(ref.member);
    if(!base || owner!=ref.member || ref.offset<0 || ref.offset>0x2000000 || base+ref.offset!=component
        || read<std::uint32_t>(before.data(),0x164)!=0x80809A3BU) return;
    {const std::lock_guard lock(mutex);reset(run);runState.rescueReferences[index]=ref;
     if(runState.rescueDeliveryClaimed[index]) return;}
    const auto handle=read<std::uint32_t>(before.data(),0x170);
    auto* object=resolve_handle(handle);
    std::array<std::byte,0x70> objectBytes{},freshObject{};
    if(!copy_native(object,objectBytes.data(),objectBytes.size())
        || read<std::uint32_t>(objectBytes.data(),0xC)!=0x80807EC9U) return;
    using Resolve=const std::byte*(__fastcall*)(std::byte*,std::uint32_t) noexcept;
    const auto* authority=native<Resolve>(0x9FEC30)(object,0xC4);
    std::array<std::byte,0xC4> body{},fresh{};
    if(!copy_native(authority,body.data(),body.size()) || !delivery::pending(*row,snapshot,objectBytes,body)
        || omega_lair_delivery::adopted(before,body)) return;
    const auto latest=omega::mission::runtime::snapshot(run);
    if(!call.accepts_side_effects() || state::activity::mission_run_generation()!=run
        || latest.generation!=snapshot.generation || !omega::rescue::requested(latest,row->slot)
        || resolve_handle(handle)!=object || !copy_native(object,freshObject.data(),freshObject.size()) || freshObject!=objectBytes
        || !copy_native(authority,fresh.data(),fresh.size()) || fresh!=body
        || !copy_native(component,current.data(),current.size()) || delivery::source(current)!=row
        || read<std::uint32_t>(current.data(),0x170)!=handle || omega_lair_delivery::adopted(current,body)) return;
    {const std::lock_guard lock(mutex);
     if(runState.run!=run || runState.rescueDeliveryClaimed[index] || !call.accepts_side_effects()) return;
     runState.rescueDeliveryClaimed[index]=true;}
    struct Message {std::uint32_t schema,padding;const std::byte* body;};
    const Message message{0x80807EC9U,0,body.data()};
    native<void(__fastcall*)(std::byte*,const Message*) noexcept>(0x4E8FB0)(component,&message);
    const bool adopted=copy_native(component,current.data(),current.size())
        && delivery::source(current)==row && omega_lair_delivery::adopted(current,body);
    log("ev=omega_mission stage=rescue_source_delivery run=%llu slot=%u generation=%u adopted=%u receipt=native_apply",
        run,row->slot,snapshot.generation,adopted?1U:0U);
}
