// Reuses the installed B804E0/CDCB60 owner and its CallGate; no additional hook.
namespace strike_pact_damage {
namespace mission=state::activity::strike_pact;
bool current(const mission::BossRequest& before) noexcept {
    const auto now=mission::boss_request();
    return now.owner==before.owner && now.enemy==before.enemy && now.stage==before.stage
        && now.revision==before.revision && now.requested==before.requested;
}
bool query(const void* context,mission::BossRequest& request,strike_pact_boss_damage::Sample& sample,float& fraction) noexcept {
    request=mission::boss_request();if(!request.enemy.valid()) {return false;}
    gateway_native::Read read{g_image};
    if(!strike_pact_boss_damage::sample(read,g_image,reinterpret_cast<std::uintptr_t>(context),request,sample)) {return false;}
    using Fraction=float(__fastcall*)(void*,std::int32_t) noexcept;
    fraction=reinterpret_cast<Fraction>(g_image+0xCD6C20)(reinterpret_cast<void*>(sample.health),0);
    gateway_native::Read after{g_image};strike_pact_boss_damage::Sample checked{};
    return std::isfinite(fraction) && fraction>=0 && fraction<=1 && current(request)
        && strike_pact_boss_damage::sample(after,g_image,reinterpret_cast<std::uintptr_t>(context),request,checked)
        && checked.health==sample.health && checked.handle==sample.handle && checked.bodyRegion==sample.bodyRegion;
}
bool immune(const void* context) noexcept {
    mission::BossRequest request{};strike_pact_boss_damage::Sample sample{};float fraction{};
    return query(context,request,sample,fraction) && mission::boss_damage::blocked(request,fraction);
}
void after(const void* context) noexcept {
    mission::BossRequest request{};strike_pact_boss_damage::Sample sample{};float fraction{};
    if(!query(context,request,sample,fraction)) {return;}
    // The native damage operation has returned: observe applied health, then
    // latch the phase immunity and next-room assignment in this callback.
    static_cast<void>(mission::observe_health(request.enemy,fraction));
}
bool before(const void* context,std::byte* packet) noexcept {
    mission::BossRequest request{};strike_pact_boss_damage::Sample sample{};float fraction{};
    if(!query(context,request,sample,fraction)) {return true;}
    static_cast<void>(mission::observe_health(request.enemy,fraction));
    const auto next=mission::boss_request();
    if(next.owner!=request.owner || next.enemy!=request.enemy) {return false;}
    request=next;
    if(mission::boss_damage::blocked(request,fraction)) {return false;}
    if(request.stage>=2) {return true;}
    std::array<std::byte,0x68+32*12> bytes{};std::int32_t count{};gateway_native::Read read{g_image};
    const auto address=reinterpret_cast<std::uintptr_t>(packet);
    if(!read.value(address+0x64,count) || count<0 || count>32) {return false;}
    auto span=std::span(bytes).first(0x68+static_cast<std::size_t>(count)*12);
    if(!read.copy(address,span)) {return false;}const auto original=bytes;
    const auto result=mission::boss_damage::clamp(span,sample.bodyRegion,mission::boss_damage::floor(request.stage));
    if(result==mission::boss_damage::Packet::invalid || !current(request)) {return false;}
    if(result==mission::boss_damage::Packet::clamped) {
        // Write only the target fraction of matching body entries. The native
        // callback retains its own packet, hit reactions and real death rules.
        for(int i=0;i<count;++i) {
            const auto offset=0x70+static_cast<std::size_t>(i)*12;
            if(std::memcmp(bytes.data()+offset,original.data()+offset,sizeof(float))==0) {continue;}
            SIZE_T written{};
            if(!WriteProcessMemory(GetCurrentProcess(),packet+offset,bytes.data()+offset,sizeof(float),&written)
                || written!=sizeof(float)) {return false;}
        }
        std::array<char,192> line{};
        const auto n=std::snprintf(line.data(),line.size(),"ev=strike_pact_boss stage=damage_clamped run=%llu actor=%08X phase=%u floor=%.6f",
            static_cast<unsigned long long>(request.enemy.run),request.enemy.actor,request.stage,mission::boss_damage::floor(request.stage));
        if(n>0 && static_cast<std::size_t>(n)<line.size()) {core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});}
    }
    return true;
}
}
