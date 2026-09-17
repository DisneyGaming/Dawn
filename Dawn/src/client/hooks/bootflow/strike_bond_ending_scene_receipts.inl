// Uses the existing Scene tick boundary, after the native callback returns.
bool replace_garden_callback(std::uintptr_t address,std::uint32_t expected,std::uint32_t value) noexcept {
    __try {
        return InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),
            static_cast<LONG>(value),static_cast<LONG>(expected))==static_cast<LONG>(expected);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void dispatch_garden_reaction(std::uintptr_t signal) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*)>(g_image+0xDB4B80)(reinterpret_cast<void*>(signal));}
    __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void delay_garden_reaction(Read& read,std::uintptr_t source,std::uintptr_t root,Weak weak,
    const state::activity::strike_bond::EndingSpeechReceipt& receipt) noexcept {
    namespace garden=state::activity::strike_bond;
    namespace path=garden_ending_scene_path;
    const auto action=path::reaction(read,root,weak.handle);
    if(action==path::Reaction::unavailable)return;
    // DB4B80 consumes the normal start input even while the callback is held.
    // Restore and replay that same input only after the server's six-second wait.
    constexpr std::array<unsigned char,16> prefix{0x4C,0x8B,0xC1,0x33,0xC9,0x41,0x8B,0x40,
        0x20,0xFF,0xC8,0x83,0xF8,0xFD,0x77,0x0D};
    std::array<unsigned char,prefix.size()> actual{};Weak after{};std::uint32_t generation{};
    if(!read.value(g_image+0xDB4B80,actual) || actual!=prefix
        || !garden::sagira_delay(receipt,action==path::Reaction::held)
        || !read.value(source+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial
        || !read.value(source+0x254,generation) || generation!=receipt.generation
        || path::reaction(read,root,weak.handle)!=action)return;
    const auto signal=root+0x1660;
    if(action==path::Reaction::arm) {
        if(replace_garden_callback(signal+0x28,weak.handle,UINT32_MAX))
            core::log::write(core::log::Channel::client,core::log::Level::info,"ev=strike_bond stage=sagira_interruption_armed");
    } else if(replace_garden_callback(signal+0x28,UINT32_MAX,weak.handle)) {
        core::log::write(core::log::Channel::client,core::log::Level::info,"ev=strike_bond stage=sagira_interruption_resumed delay_ms=6000");
        dispatch_garden_reaction(signal);
    }
}

void observe_garden_ending_speech(std::uintptr_t source) noexcept {
    namespace garden=state::activity::strike_bond;
    Read read;Ref ref{};std::uintptr_t definition{},root{};Weak weak{},after{};Diag diag{};
    if(!read.value(source,ref) || ref.handle!=garden::kEndingScene.definition
        || ref.kind!=0x80806266U || ref.offset!=0x368 || !read.resolve(ref,definition)) return;
    std::array<std::byte,8> scope{};
    if(!read.copy(definition+0x30,scope)
        || at<std::uint32_t>(scope.data())!=garden::kEndingScene.registry
        || at<std::uint16_t>(scope.data()+4)!=43 || at<std::uint16_t>(scope.data()+6)!=5) return;
    const auto request=garden::request();const auto i=garden::scene_index(garden::kEndingScene);
    if(!request.owner.valid() || !request.frame.campaign || !request.frame.enabled || !request.frame.ending
        || request.frame.finished || !request.frame.scan.started || request.frame.scenes[i].stop) return;
    std::uint32_t generation{};std::uint8_t complete{};
    if(!read.value(source+0x254,generation) || generation!=request.frame.scenes[i].generation || !generation
        || !read.value(source+0x258,complete) || complete>1
        || !read.value(source+0x2E8,weak) || !read.weak(weak,root,diag)) return;
    const auto cue=garden_ending_scene_path::probe(read,root,weak.handle);
    std::uint32_t afterGeneration{};
    if(!cue.valid || !read.value(source+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial
        || !read.value(source+0x254,afterGeneration) || afterGeneration!=generation)return;
    const garden::EndingSpeechReceipt receipt{request.owner,generation,weak.handle,weak.serial};
    if(cue.gotHim) garden::observe_ending_speech(receipt,1);
    delay_garden_reaction(read,source,root,weak,receipt);
    if(cue.closing) garden::observe_ending_scene_cue(receipt,true);
}
