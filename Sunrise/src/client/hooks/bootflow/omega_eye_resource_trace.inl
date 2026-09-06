// Observe the original character resource dispatcher. Never starts a resource,
// changes its request, or reports successful execution from allocation alone.
using EyeResourceDispatch=bool(__fastcall*)(std::byte*,const void*,bool,std::int32_t,std::int32_t) noexcept;
std::atomic<EyeResourceDispatch> eyeResourceOriginal{};
std::mutex eyeResourceMutex;
omega::mission::Token eyeResourceToken{};
std::array<unsigned,5> eyeResourceCounts{};
bool eye_resource_channel(std::byte* channel,const omega::mission::Boss& boss) noexcept {
    std::array<std::byte,8> head{};std::array<std::byte,0x5C4> character{};
    if(!copy_native(channel,head.data(),head.size()) || read<std::uint32_t>(head.data(),0)!=boss.character) return false;
    auto* c=resolve_handle(boss.character);
    if(!copy_native(c,character.data(),character.size())
        || !omega_reveal_source::matches(character,{0x80F6690BU,0x80806832U,0x738})
        || read<std::uint32_t>(character.data(),0x24)!=boss.character
        || read<std::uint32_t>(character.data(),0x2C)!=boss.entity
        || read<std::uint32_t>(character.data(),0xC0)!=boss.actor) return false;
    const auto animation=read<std::uint32_t>(head.data(),4);
    auto* a=resolve_handle(animation);
    return a && animation==read<std::uint32_t>(character.data(),0x5C0) && a+0x1980==channel;
}
bool __fastcall trace_eye_resource(std::byte* channel,const void* request,bool retain,
    std::int32_t context,std::int32_t secondary) noexcept {
    const hooking::CallGate::Scope call(callGate);
    constexpr std::array<std::uint32_t,5> resources{0x80F453BF,0x80F45568,0x80F4547E,0x80F45564,0x80F45484};
    std::array<std::byte,12> before{};
    omega::mission::Snapshot snapshot{};std::size_t index=resources.size();unsigned ordinal{};
    if(call.accepts_side_effects() && active() && copy_native(request,before.data(),before.size())) {
        const auto resource=read<std::uint32_t>(before.data(),8);
        const auto it=std::find(resources.begin(),resources.end(),resource);
        if(it!=resources.end()) {
            snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
            if(snapshot.command.cycle>=1 && snapshot.command.cycle<=3 && snapshot.chargeDunked
                && eye_resource_channel(channel,snapshot.command.token.boss)) {
                const std::lock_guard lock(eyeResourceMutex);
                if(eyeResourceToken!=snapshot.command.token) {eyeResourceToken=snapshot.command.token;eyeResourceCounts={};}
                const auto candidate=static_cast<std::size_t>(it-resources.begin());
                if(eyeResourceCounts[candidate]<4) {index=candidate;ordinal=++eyeResourceCounts[index];}
            }
        }
    }
    const bool result=hooking::await_original(eyeResourceOriginal)(channel,request,retain,context,secondary);
    if(index<resources.size()) {
        const auto after=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
        const bool current=call.accepts_side_effects() && after.command.token==snapshot.command.token
            && eye_resource_channel(channel,snapshot.command.token.boss);
        log("ev=omega_mission stage=eye_resource_dispatch run=%llu epoch=%u cycle=%u resource=%08X name=%08X kind=%u retain=%u context=%d secondary=%d native_result=%u current=%u ordinal=%u receipt=dispatch_only",
            snapshot.command.token.boss.run,snapshot.command.token.epoch,snapshot.command.cycle,resources[index],
            read<std::uint32_t>(before.data(),0),read<std::uint8_t>(before.data(),4),retain?1U:0U,
            context,secondary,result?1U:0U,current?1U:0U,ordinal);
    }
    return result;
}
