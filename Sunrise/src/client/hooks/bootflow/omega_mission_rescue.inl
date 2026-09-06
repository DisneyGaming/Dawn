// B438B0 is the original Scene sense producer. A missing selector sets its
// completion byte, so completion alone cannot release the encounter.
struct MissionSceneView {
    omega::mission::Token token{};
    std::uint32_t group{UINT32_MAX},selector{UINT32_MAX},serial{},definition{},generation{};
    std::uint16_t slot{};
    std::int64_t offset{};
    bool operator==(const MissionSceneView&) const = default;
};
bool mission_weak(std::span<const std::byte> weak,std::uint32_t& handle) noexcept {
    if(weak.size()!=8) return false;
    handle=UINT32_MAX;
    native<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(0x352310)(weak.data(),&handle);
    return handle!=UINT32_MAX && handle==read<std::uint32_t>(weak.data(),4);
}
// Compiled graph assets contain reciprocal runtime/definition headers. Their
// offsets vary by graph; a timeline is not a selector with the same state byte.
bool mission_graph_header(const std::byte* runtime,std::uint32_t asset,
    std::uint32_t runtimeClass,std::uint32_t definitionClass) noexcept {
    if(read<std::uint32_t>(runtime,0)!=asset || read<std::uint32_t>(runtime,4)!=runtimeClass) return false;
    const auto offset=read<std::int64_t>(runtime,8);
    auto* package=resolve_handle(asset);
    std::array<std::byte,16> definition{},compiled{};
    if(!package || offset<0 || offset>0x2000000
        || !copy_native(package+offset,definition.data(),definition.size())
        || read<std::uint32_t>(definition.data(),0)!=asset
        || read<std::uint32_t>(definition.data(),4)!=definitionClass) return false;
    const auto back=read<std::int64_t>(definition.data(),8);
    return back>=0 && back<=0x2000000 && copy_native(package+back,compiled.data(),compiled.size())
        && std::equal(compiled.begin(),compiled.end(),runtime);
}
bool mission_scene_view(std::byte* component,MissionSceneView& view) noexcept {
    namespace rescue=omega::rescue;
    std::array<std::byte,0x2F0> bytes{};
    if(!copy_native(component,bytes.data(),bytes.size()) || read<std::uint32_t>(bytes.data(),4)!=0x80806266U
        || read<std::int64_t>(bytes.data(),8)!=0x368) return false;
    const auto definition=read<std::uint32_t>(bytes.data(),0);
    const rescue::Scene* row{};
    for(const auto& candidate:rescue::scenes) if(candidate.definition==definition) row=&candidate;
    if(!row || (row->slot!=9 && row->slot!=27 && row->slot!=46)) return false;
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    const auto* command=rescue::command(snapshot,row->slot);
    if(!command || command->stop || command->generation!=read<std::uint32_t>(bytes.data(),0x254)
        || command->generation!=read<std::uint32_t>(bytes.data(),0x180)
        || read<std::uint8_t>(bytes.data(),0x184) || read<std::uint8_t>(bytes.data(),0x258)) return false;
    std::array<std::byte,0x40> definitionBytes{};
    if(!copy_native(resolve_handle(definition)+0x368,definitionBytes.data(),definitionBytes.size())
        || read<std::uint32_t>(definitionBytes.data(),0x30)!=rescue::kRegistry
        || read<std::uint8_t>(definitionBytes.data(),0x34)!=43
        || read<std::uint16_t>(definitionBytes.data(),0x36)!=row->slot) return false;
    std::uint32_t group=UINT32_MAX,selector=UINT32_MAX;
    native<std::uint32_t*(__fastcall*)(const std::byte*,std::uint32_t*) noexcept>(0x4E5C60)(component,&group);
    const auto base=reinterpret_cast<std::uintptr_t>(resolve_handle(group));
    const auto address=reinterpret_cast<std::uintptr_t>(component);
    if(group==UINT32_MAX || !base || address<base || address-base>0x2000000
        || !mission_weak(std::span(bytes).subspan(0x2E8,8),selector)) return false;
    std::array<std::byte,0x80> owner{};
    if(!copy_native(resolve_handle(selector),owner.data(),owner.size())
        || !mission_graph_header(owner.data(),row->selector-1U,0x80806384U,0x808063A7U)
        || read<std::uint32_t>(owner.data(),0x24)!=selector
        || read<std::uint8_t>(owner.data(),0x7C)!=2) return false;
    view={command->token,group,selector,read<std::uint32_t>(bytes.data(),0x2E8),definition,
        command->generation,row->slot,static_cast<std::int64_t>(address-base)};
    return true;
}
bool mission_blocking_child(const MissionSceneView& view) noexcept {
    auto* selector=resolve_handle(view.selector);
    std::array<std::byte,0x80> owner{};
    if(!copy_native(selector,owner.data(),owner.size()) || read<std::uint32_t>(owner.data(),0x24)!=view.selector) return false;
    const auto count=read<std::int64_t>(owner.data(),0x38),relative=read<std::int64_t>(owner.data(),0x40);
    if(count<=0 || count>256 || relative<0 || relative>0x2000000) return false;
    // DAB710 traverses 30-byte table rows. Each +20 relative link points to
    // the live node; the authored FD node's metadata carries its child at +58.
    for(std::int64_t i=0;i<count;++i) {
        auto* link=selector+relative+0x70+i*0x30;
        std::int64_t distance{};
        if(!copy_value(link,distance) || !distance || distance < -0x2000000 || distance>0x2000000) return false;
        auto* node=link+distance;std::array<std::byte,0x1C0> bytes{};
        if(!copy_native(node,bytes.data(),bytes.size())) return false;
        if(read<std::uint32_t>(bytes.data(),4)!=0x808062FEU
            || read<std::uint32_t>(bytes.data(),0x20)!=0x808062FDU
            || read<std::uint8_t>(bytes.data(),0x98)!=1
            || read<std::int32_t>(bytes.data(),0x1A0)>0) continue;
        const auto metadataOffset=read<std::int64_t>(bytes.data(),8);
        if(metadataOffset<0 || metadataOffset>0x2000000) return false;
        std::array<std::byte,0x90> metadata{};
        if(!copy_native(resolve_handle(read<std::uint32_t>(bytes.data(),0))+metadataOffset,metadata.data(),metadata.size())
            || read<std::uint32_t>(metadata.data(),0x58)!=0x80EC0E00U) continue;
        // The native root links must lead back to this exact selector.
        auto* parent=node;bool rooted{};
        for(unsigned depth=0;depth<16;++depth) {
            std::int64_t back{};if(!copy_value(parent+0x10,back) || back < -0x2000000 || back>0x2000000) break;
            if(!back) {rooted=parent==selector;break;}
            parent=parent+0x10+back;
        }
        if(!rooted) continue;
        std::uint32_t child{};
        if(!mission_weak(std::span(bytes).subspan(0x1B0,8),child)) continue;
        std::array<std::byte,0x80> live{};
        if(!copy_native(resolve_handle(child),live.data(),live.size())
            || read<std::uint32_t>(live.data(),0x24)!=child
            || !mission_graph_header(live.data(),0x80EC0DFFU,0x808084E9U,0x808084D7U)) continue;
        std::array<std::byte,8> after{};std::uint32_t still{};
        if(copy_native(node+0x1B0,after.data(),after.size()) && mission_weak(after,still)
            && still==child && std::equal(after.begin(),after.end(),bytes.begin()+0x1B0)) return true;
    }
    return false;
}
void __fastcall mission_scene_sense(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    MissionSceneView before{},after{};
    const bool observe=call.accepts_side_effects() && active() && mission_scene_view(component,before);
    hooking::await_original(sceneSenseOriginal)(component);
    if(!observe || !call.accepts_side_effects() || !mission_scene_view(component,after) || before!=after) return;
    if(omega::mission::runtime::receipt([&](auto& s){return s.rescue_started(after.token,after.slot);}))
        log("ev=omega_mission stage=rescue_started run=%llu epoch=%u scene=%u selector=%08X receipt=native_scene",
            after.token.boss.run,after.token.epoch,after.slot,after.selector);
    if(!mission_blocking_child(after)) return;
    std::array<std::byte,0x84> events{};
    if(!copy_native(component+0x264,events.data(),events.size())) return;
    const auto count=read<std::int32_t>(events.data(),0);if(count<0 || count>32) return;
    bool entered=after.slot==27;
    for(int i=0;i<count;++i) entered|=read<std::uint32_t>(events.data(),4+static_cast<std::size_t>(i)*4)==0x63A4F800U;
    MissionSceneView final{};
    if(!entered || !mission_scene_view(component,final) || final!=after) return;
    if(omega::mission::runtime::receipt([&](auto& s){return s.rescue_ready(after.token,after.slot);}))
        log("ev=omega_mission stage=rescue_ready run=%llu epoch=%u scene=%u selector=%08X receipt=native_blocking_child",
            after.token.boss.run,after.token.epoch,after.slot,after.selector);
}
