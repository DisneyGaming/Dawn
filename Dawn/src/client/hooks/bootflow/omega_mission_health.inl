bool mission_boss_health(const graph::Owner& owner,std::uint32_t& handle,bool& dead) noexcept {
    std::array<std::byte,0x300> character{};
    if(!copy_native(resolve_handle(owner.character),character.data(),character.size())
        || !omega_reveal_source::matches(character,{0x80F6690BU,0x80806832U,0x738})
        || read<std::uint32_t>(character.data(),0x24)!=owner.character
        || read<std::uint32_t>(character.data(),0x2C)!=owner.entity
        || read<std::uint32_t>(character.data(),0xC0)!=owner.actor
        || read<std::uint32_t>(character.data(),0x2EC)!=0x80804BEEU
        || read<std::int64_t>(character.data(),0x2F0)!=0) return false;
    handle=read<std::uint32_t>(character.data(),0x2E8);
    std::array<std::byte,0x33C> health{};
    if(!copy_native(resolve_handle(handle),health.data(),health.size())
        || !omega_reveal_source::matches(health,{0x815B5A40U,0x80804B8AU,0xF98})
        || read<std::uint32_t>(health.data(),0x24)!=handle || read<std::uint32_t>(health.data(),0x2C)!=owner.entity) return false;
    dead=(read<std::uint32_t>(health.data(),0x338)&1)!=0;return true;
}
void observe_mission_health(const hooking::CallGate::Scope& call) noexcept {
    namespace mission=omega::mission;
    if(!call.accepts_side_effects()) return;
    const auto snapshot=mission::runtime::snapshot(state::activity::mission_run_generation());
    if(!snapshot.command.claimed || (snapshot.phase!=mission::Phase::shield && snapshot.phase!=mission::Phase::eye
        && snapshot.phase!=mission::Phase::recovery)) return;
    graph::Owner owner{};
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=snapshot.command.token.boss.run) return;
        owner=runState.graphOwner;
    }
    MemberView member{};graph::Owner current{};std::uint32_t health{},after{};bool dead{};
    if(!current_owner(owner,member,current) || owner!=current || !member.enabled || !mission_boss_health(owner,health,dead) || dead) return;
    using Get=float(__fastcall*)(std::byte*,std::int32_t) noexcept;
    const float body=native<Get>(0xCD6C20)(resolve_handle(health),0);
    const float eye=native<Get>(0xCD6C20)(resolve_handle(health),1);
    if(!current_owner(owner,member,current) || current!=owner || !mission_boss_health(owner,after,dead)
        || after!=health || dead || !std::isfinite(body)||!std::isfinite(eye)||body<0||body>1||eye<0||eye>1) return;
    bool crossed{},diagnostic{};
    if(snapshot.phase==mission::Phase::shield || snapshot.phase==mission::Phase::eye) {
        const std::lock_guard lock(mutex);
        auto& track=runState.health;
        if(track.token!=snapshot.command.token || track.handle!=health) {track={};track.token=snapshot.command.token;track.handle=health;}
        crossed=track.sampled && !track.crossed && track.previous>.90F && eye<=.90F;
        diagnostic=track.diagnosticSamples<8 && (!track.sampled || std::abs(track.previous-eye)>.001F || crossed);
        if(diagnostic) ++track.diagnosticSamples;
        track.sampled=true;track.previous=eye;if(crossed) track.crossed=true;
    }
    if(diagnostic) log("ev=omega_mission stage=eye_health_sample run=%llu epoch=%u cycle=%u health=%08X body=%.6f eye=%.6f crossed=%u receipt=native_getter_only",
        owner.run,snapshot.command.token.epoch,snapshot.command.cycle,health,static_cast<double>(body),static_cast<double>(eye),crossed?1U:0U);
    if(crossed) mission::runtime::receipt([&](auto& s){return s.health(snapshot.command.token,mission::Health::eyeCrossed);});
    if(snapshot.phase==mission::Phase::recovery && snapshot.command.cycle<3) {
        const float checkpoint=snapshot.command.cycle==1?.55F:.10F;
        if(std::abs(body-checkpoint)<=0.000001F)
            mission::runtime::receipt([&](auto& s){return s.health(snapshot.command.token,mission::Health::bodyCheckpoint);});
    }
}
void observe_mission_boss_death(std::byte* character,std::uint32_t event) noexcept {
    // 9ECC70 resolves the native event arena; +34 is its payload class.
    // C72390's second argument is an arena handle, not an event enum.
    const std::byte* arena{};const std::byte* data{};std::uint32_t kind{};
    if(!copy_value(image+0x274F778,arena) || !arena || !copy_value(arena+0x10,data) || !data
        || !copy_value(data+(event&0xFFFFF)+0x34,kind) || kind!=0x80804C54U) return;
    namespace mission=omega::mission;
    const auto snapshot=mission::runtime::snapshot(state::activity::mission_run_generation());
    if(snapshot.phase!=mission::Phase::death || snapshot.command.cycle!=3 || !snapshot.command.claimed) return;
    graph::Owner owner{};
    {
        const std::unique_lock lock(mutex,std::try_to_lock);
        if(!lock.owns_lock() || runState.run!=snapshot.command.token.boss.run) return;
        owner=runState.graphOwner;
    }
    MemberView member{};graph::Owner current{};std::uint32_t health{};bool dead{};
    if(resolve_handle(owner.character)!=character || !current_owner(owner,member,current) || current!=owner
        || !member.enabled || !mission_boss_health(owner,health,dead) || !dead) return;
    if(mission::runtime::receipt([&](auto& s){return s.health(snapshot.command.token,mission::Health::dead);}))
        log("ev=omega_mission stage=boss_native_death run=%llu epoch=%u character=%08X health=%08X receipt=pre_original_C72390_and_typed_dead_bit",
            owner.run,snapshot.command.token.epoch,owner.character,health);
}
