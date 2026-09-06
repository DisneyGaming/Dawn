// Included after the combat runtime.
#include "omega_boss_animation_glow.inl"

void observe_eye_inputs(const graph::Owner& owner,
                        const observation::Snapshot& snapshot) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::eye_inputs);
    namespace eye = omega_boss_eye_diagnostics;
    static std::mutex eyeMutex;
    static eye::Claims claims;
    unsigned sample{};
    {
        const std::lock_guard lock(eyeMutex);
        if (!claims.claim(state::activity::mission_run_generation(),owner,snapshot,GetTickCount64(),sample)) return;
    }
    ScalarView scalar{};
    std::array<ScalarValue,eye::kInputs.size()> values{};
    const auto fresh_owner = [&owner]() noexcept {
        MemberView member{};
        graph::Owner current{};
        auto* datum=resolve_handle(owner.member);
        return state::activity::mission_run_generation()==owner.run && datum
            && member_view(datum+owner.memberOffset,member) && member.enabled
            && member.exactQueue && member.head==0 && member.count==1
            && character_owner(member,owner.run,owner.character,current) && current==owner;
    };
    const bool ownerBefore=fresh_owner();
    const bool found = ownerBefore && left_scalar(owner.entity,scalar);
    bool available = found && scalar.bound;
    unsigned failure = !ownerBefore ? 25U : found && !scalar.bound ? 24U : scalar.failure;
    std::array<std::byte,0x60> before{},after{};
    auto* controller = available ? resolve_handle(scalar.self) : nullptr;
    if (available && (!copy_native(controller,before.data(),before.size())
        || !eye::controller(before,scalar.self,owner.entity))) { available=false; failure=20; }
    for (std::size_t i=0; available && i<eye::kInputs.size(); ++i) {
        const auto row = reinterpret_cast<std::uintptr_t>(controller)+0x68
            + eye::field<std::uintptr_t>(before,0x58)
            + static_cast<std::uintptr_t>(eye::kInputs[i].index)*16;
        if ((row&15U) || !readable(reinterpret_cast<const void*>(row),16)) {
            available=false; failure=21; break;
        }
        using Get = void(__fastcall*)(std::byte*,std::int32_t,ScalarValue*) noexcept;
        native<Get>(0x57A770)(controller,eye::kInputs[i].index,&values[i]);
    }
    float animationGlow{};
    const bool animationAvailable=available && read_animation_glow(owner,animationGlow);
    if (available && (resolve_handle(scalar.self)!=controller
        || !copy_native(controller,after.data(),after.size())
        || !eye::controller(after,scalar.self,owner.entity)
        || eye::field<std::uintptr_t>(before,0x58)!=eye::field<std::uintptr_t>(after,0x58))) {
        available=false; failure=22;
    }
    if (available && !fresh_owner()) { available=false; failure=25; }
    const auto currentRun = state::activity::mission_run_generation();
    if (currentRun != owner.run) { available=false; failure=23; }
    unsigned finite=0,replicated=0;
    for (std::size_t i=0;i<values.size();++i) {
        if (std::all_of(values[i].lanes.begin(),values[i].lanes.end(),
                        [](float v){return std::isfinite(v);})) finite|=1U<<i;
        if (scalar_equals(values[i],values[i].lanes[0])) replicated|=1U<<i;
    }
    if (!available) {
        log("ev=omega_reveal stage=eye_inputs run=%llu actor=%08X entity=%08X phase=%u node=%d "
            "available=0 reason=%u controller=%08X bound=%u sample=%u diagnostic_only=1",
            owner.run,owner.actor,owner.entity,static_cast<unsigned>(snapshot.phase),
            snapshot.loadedNode,failure,scalar.self,scalar.bound?1U:0U,sample);
        return;
    }
    log("ev=omega_reveal stage=eye_inputs run=%llu actor=%08X entity=%08X phase=%u node=%d "
        "available=1 controller=%08X source=80F6695E/1768 finite=%02X replicated=%02X "
        "body_health=%g eye_health=%g alive=%g input_28D3F669=%g input_47180E9A=%g "
        "input_F33CEA78=%g illumination=%g sample=%u diagnostic_only=1",
        owner.run,owner.actor,owner.entity,static_cast<unsigned>(snapshot.phase),
        snapshot.loadedNode,scalar.self,finite&0x7FU,replicated&0x7FU,
        static_cast<double>(values[0].lanes[0]),static_cast<double>(values[1].lanes[0]),
        static_cast<double>(values[2].lanes[0]),static_cast<double>(values[3].lanes[0]),
        static_cast<double>(values[4].lanes[0]),static_cast<double>(values[5].lanes[0]),
        static_cast<double>(values[6].lanes[0]),sample);
    // Keep the original input line under512 bytes. The native animation
    // producer and computed downstream value are distinct from illumination.
    log("ev=omega_reveal stage=vfx_values run=%llu actor=%08X entity=%08X phase=%u node=%d "
        "available=1 controller=%08X source=80F6695E/1768 finite=%X replicated=%X "
        "animation_available=%u animation_CE0BA42D=%g input_CE0BA42D=%g computed_20AA7FC1=%g "
        "sample=%u diagnostic_only=1",
        owner.run,owner.actor,owner.entity,static_cast<unsigned>(snapshot.phase),
        snapshot.loadedNode,scalar.self,finite>>7,replicated>>7,
        animationAvailable?1U:0U,static_cast<double>(animationGlow),
        static_cast<double>(values[7].lanes[0]),static_cast<double>(values[8].lanes[0]),sample);
}
