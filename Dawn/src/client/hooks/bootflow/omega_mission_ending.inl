// Lookup C4C1A0 returns the registered cinematic directory row. Its +18
// component handle is resolved with the full serial; DD1680 registered that
// component's +2C world owner. Polling the directory does not load a resource.
bool mission_ending_resource(std::uint32_t& owner) noexcept {
    constexpr std::uint32_t selector=0x2FECC6FD;
    auto* row=native<std::byte*(__fastcall*)(const std::uint32_t*)>(0xC4C1A0)(&selector);
    std::uint32_t handle{},self{};std::array<std::byte,0x30> data{};
    if(!row || !copy_value(row+0x18,handle) || handle==UINT32_MAX) return false;
    auto* component=resolve_handle(handle);
    if(!component || !copy_native(component,data.data(),data.size())
        || !omega_reveal_source::matches(data,{0x80C177DD,0x80806647,0x10578})) return false;
    self=read<std::uint32_t>(data.data(),0x24);owner=read<std::uint32_t>(data.data(),0x2C);
    std::uint32_t after{};
    return self==handle && owner && owner!=UINT32_MAX && copy_value(row+0x18,after) && after==handle
        && resolve_handle(handle)==component;
}
void observe_mission_ending(std::byte* component) noexcept {
    namespace ending=omega::ending;
    const auto run=state::activity::mission_run_generation();
    auto view=ending::runtime::snapshot(run);
    if(!view.arrived || view.failed || view.finished) return;
    std::array<std::byte,0x262> data{};
    if(!copy_native(component,data.data(),data.size())
        || !omega_reveal_source::matches(data,{0x80F47BCB,0x80804F07,0x2E8})) return;
    const auto self=read<std::uint32_t>(data.data(),0x24);
    if(resolve_handle(self)!=component) return;
    std::uint32_t owner{};
    if(!mission_ending_resource(owner) || !ending::runtime::resource(view.token,owner)) return;
    if(!view.play) log("ev=omega_ending stage=resource_registered run=%llu epoch=%u owner=%08X revision=%u",run,view.token.epoch,owner,view.revision);
    const auto revision=read<std::uint32_t>(data.data(),0x190);
    const bool activeMovie=read<std::uint8_t>(data.data(),0x260)!=0;
    if(ending::runtime::native(view.token,owner,self,revision,true,activeMovie,GetTickCount64()))
        log("ev=omega_ending stage=native run=%llu epoch=%u owner=%08X revision=%u active=%u",
            run,view.token.epoch,owner,revision,activeMovie?1U:0U);
}
