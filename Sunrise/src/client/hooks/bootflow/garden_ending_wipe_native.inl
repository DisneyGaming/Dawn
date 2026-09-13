// Observe only, at the existing post-F4E660 animation boundary.
void observe_garden_wipe(std::span<const std::byte> bytes,char nativeResult) noexcept {
    namespace garden=state::activity::strike_bond;
    const auto r=garden::request();const auto& flow=r.frame.endingFlow;
    if(!r.frame.campaign || !r.frame.enabled || r.frame.region!=136 || !flow.claimed || !flow.animated
        || flow.retire || flow.wipeFinished || bytes.size()<0xB8)return;
    const auto actor=flow.actor;const auto biped=at<std::uint32_t>(bytes.data()+0x14);
    const auto phase=garden::ending_wipe::phase(bytes,actor,biped,nativeResult);
    if(phase!=garden::EndingAnimationPhase::wipe && phase!=garden::EndingAnimationPhase::finished)return;
    gateway_native::Read read{g_image};std::uintptr_t controller{},selector{},body{},animation{},rows{};
    std::uint32_t stride{},value{},flags{},animationHandle{};
    if(!read.resolve(actor.controller,controller) || !read.resolve(actor.selector,selector) || !read.resolve(biped,body))return;
    const auto header=[&](std::uintptr_t address,std::uint32_t tag,std::uint32_t kind,std::uint64_t offset,std::uint32_t self) noexcept {
        gateway_native::Ref ref{};std::uint32_t v{};
        return read.value(address,ref) && ref.handle==tag && ref.kind==kind && ref.offset==static_cast<std::int64_t>(offset)
            && read.value(address+0x24,v) && v==self && read.value(address+0x2C,v) && v==actor.entity;
    };
    if(!header(controller,0x80F6690BU,0x80806832U,0x738,actor.controller)
        || !header(selector,0x80F45174U,0x8080686CU,0x308,actor.selector)
        || !header(body,0x80F66907U,0x808036CFU,0x1B48,biped)
        || !read.value(selector+0x30,value) || value!=actor.controller
        || !read.value(controller+0x5C0,animationHandle) || !read.resolve(animationHandle,animation)
        || !read.value(animation+0x30,value) || value!=actor.controller
        || !read.value(animation+0x1260,value) || value!=biped
        || !read.value(g_image+0x1F93428,rows) || !read.value(g_image+0x1F93430,stride) || stride<0xE0 || stride>0x1000)return;
    const auto row=rows+static_cast<std::uintptr_t>(actor.entity&8191)*stride;
    if(!read.value(row,value) || value!=actor.serial || !read.value(row+12,value) || value!=actor.entity
        || !read.value(row+4,flags) || (flags&5U))return;
    garden::observe_ending_playback(r.owner,actor,biped,phase);
}
