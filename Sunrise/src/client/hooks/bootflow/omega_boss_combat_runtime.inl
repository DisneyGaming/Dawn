// Included beside the graph bridge. The native fullbody callback is the safe
// observation point for issuing the first authored arm action after intro idle.
struct alignas(16) ScalarValue { std::array<float, 4> lanes{}; };
struct ScalarView {
    ScalarValue value{};
    std::uint64_t actual{}, expected{};
    std::uint32_t self{UINT32_MAX}; std::int32_t index{-1}; unsigned failure{}; bool bound{};
    std::array<std::byte, 3> alignment{};
    bool reject(unsigned reason, std::uint64_t valueActual, std::uint64_t valueExpected) noexcept {
        failure = reason; actual = valueActual; expected = valueExpected; return false;
    }
};
constexpr std::array scalarGuards{"left_scalar_ready", "left_scalar_lookup", "left_scalar_read",
    "left_scalar_entity", "left_scalar_bound_self", "left_scalar_self", "left_scalar_bound_source",
    "left_scalar_bound_count", "left_scalar_bound_index", "left_scalar_bound_vector",
    "left_scalar_counts", "left_scalar_index", "left_scalar_vector", "left_scalar_getter"};
bool scalar_equals(const ScalarValue& value, float expected) noexcept {
    return std::all_of(value.lanes.begin(), value.lanes.end(),
        [expected](float lane) { return lane == expected; });
}
bool left_scalar(std::uint32_t entity, ScalarView& out, bool right = false) noexcept {
    const auto property=right?combat::kRightProperty:combat::kLeftProperty;
    const auto variable=right?30:31;
    using Find = bool(__fastcall*)(std::uint32_t, const std::uint32_t*, std::byte**, std::int32_t*) noexcept;
    using Get = ScalarValue*(__fastcall*)(std::byte*, ScalarValue*, std::int32_t) noexcept;
    std::byte* controller{};
    if (!native<Find>(0x583DF0)(entity, &property, &controller, &out.index)) {
        // The authored Panoptes export lives on a bound-variable component.
        // B8BCCA uses this same fallback when the ordinary property lookup fails.
        using BoundFind = bool(__fastcall*)(std::uint32_t, const std::uint32_t*, std::uint32_t*, std::int32_t*) noexcept;
        if (!native<BoundFind>(0x583C70)(entity, &property, &out.self, &out.index))
            return out.reject(1, entity, property);
        controller = resolve_handle(out.self); out.bound = true;
    }
    std::array<std::byte, 0x60> body{};
    if (!copy_native(controller, body.data(), body.size()))
        return out.reject(2, reinterpret_cast<std::uintptr_t>(controller), body.size());
    if (read<std::uint32_t>(body.data(), 0x2C) != entity)
        return out.reject(3, read<std::uint32_t>(body.data(), 0x2C), entity);
    const auto self = read<std::uint32_t>(body.data(), 0x24);
    if (out.bound && self != out.self) return out.reject(4, self, out.self);
    out.self = self;
    if (out.self == UINT32_MAX || resolve_handle(out.self) != controller)
        return out.reject(5, out.self, reinterpret_cast<std::uintptr_t>(controller));
    if (out.bound) {
        // 80F6695E+1768 exports {wildcard scope, A2AE120F, variable31}.
        // +50/+58 is the runtime vector, not the typed 808097C2 reference.
        if (!omega_reveal_source::matches(body, {0x80F6695EU, 0x80809790U, 0x1768}))
            return out.reject(6, read<std::uint64_t>(body.data(), 0), 0x8080979080F6695EULL);
        if (read<std::uint64_t>(body.data(), 0x50) != 67)
            return out.reject(7, read<std::uint64_t>(body.data(), 0x50), 67);
        if (out.index != variable) return out.reject(8, static_cast<std::uint32_t>(out.index), variable);
        const auto row = reinterpret_cast<std::uintptr_t>(controller) + 0x58
            + read<std::uintptr_t>(body.data(), 0x58) + 0x10
            + static_cast<std::uintptr_t>(out.index) * 16;
        if ((row & 15U) || !readable(reinterpret_cast<const void*>(row), 16)) return out.reject(9, row, 16);
        using BoundGet = void(__fastcall*)(std::byte*, std::int32_t, ScalarValue*) noexcept;
        native<BoundGet>(0x57A770)(controller, out.index, &out.value);
        return true;
    }
    const auto first = read<std::int32_t>(body.data(), 0x30);
    const auto second = read<std::int32_t>(body.data(), 0x40);
    if (first < 0 || first > 4096 || second < 0 || second > 4096)
        return out.reject(10, static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(second));
    if (out.index < 0 || out.index >= first + second)
        return out.reject(11, static_cast<std::uint32_t>(out.index), static_cast<std::uint32_t>(first + second));
    const auto table = out.index < first ? 0x38U : 0x48U;
    const auto index = out.index < first ? out.index : out.index - first;
    const auto relative = read<std::uintptr_t>(body.data(), table);
    const auto row = reinterpret_cast<std::uintptr_t>(controller) + table + relative
        + static_cast<std::uintptr_t>(index + 1) * 0x30;
    // 57A0A0 uses aligned SIMD reads and writes. Validate its exact source
    // address, including signed relative-array representation, before entry.
    if ((row & 15U) || !readable(reinterpret_cast<const void*>(row), 16)) return out.reject(12, row, 16);
    return native<Get>(0x57A0A0)(controller, &out.value, out.index) == &out.value
        || out.reject(13, out.self, static_cast<std::uint32_t>(out.index));
}
bool fullbody_owner(std::byte* component, const graph::Owner& owner,
                    std::uint32_t& self, std::uint32_t& entity) noexcept {
    std::array<std::byte, 0x30> body{}, character{}, biped{};
    if (!copy_native(component, body.data(), body.size())
        || !omega_reveal_source::matches(body, {combat::kFullBodyAsset, 0x80803640U, 0x15B8}))
        return false;
    self = read<std::uint32_t>(body.data(), 0x24);
    entity = read<std::uint32_t>(body.data(), 0x2C);
    if (self == UINT32_MAX || entity == UINT32_MAX || entity != owner.entity || resolve_handle(self) != component
        || !copy_native(resolve_handle(owner.character), character.data(), character.size())
        || !copy_native(resolve_handle(owner.biped), biped.data(), biped.size())
        || read<std::uint32_t>(character.data(), 0x2C) != entity
        || read<std::uint32_t>(biped.data(), 0x2C) != entity) return false;
    std::array<std::byte, 0x120> definition{};
    auto* resource = resolve_handle(combat::kFullBodyAsset);
    return resource && copy_native(resource + 0x15B8, definition.data(), definition.size())
        && read<std::uint32_t>(definition.data(), 0x108) == observation::kBankAsset
        && read<std::uint32_t>(definition.data(), 0x110) == 0x80F45169U
        && read<std::uint32_t>(definition.data(), 0x11C) == 0x80F45197U;
}
bool set_left_scalar(std::uint32_t entity, float value, bool right = false) noexcept {
    const auto property=right?combat::kRightProperty:combat::kLeftProperty;
    const ScalarValue scalar{{value, value, value, value}};
    using Set = bool(__fastcall*)(std::uint32_t, const std::uint32_t*, const ScalarValue*) noexcept;
    return native<Set>(0x576420)(entity, &property, &scalar);
}
struct LeftOutput {
    std::array<std::byte, combat::kActionGroupBytes> group{};
    float duration{};
    float blend{};
    bool enabled{};
    unsigned failure{};
};
bool left_output(std::byte* component, LeftOutput& out, bool right = false) noexcept {
    const std::size_t descriptor=right?0x88:0xA0, variant=right?0x200:0x2B0;
    const auto clipIndex=right?combat::kRightClipIndex:combat::kLeftClipIndex;
    // Exact authored action group2 contains declaration0/variant0, whose
    // 80803737 selection is clip-bank index20. Its clock is native seconds.
    std::array<std::byte, 0x328> actions{};
    auto* actionAsset = resolve_handle(combat::kActionAsset);
    if (!copy_native(actionAsset, actions.data(), actions.size())) { out.failure = 1; return false; }
    if (read<std::uint32_t>(actions.data(), 0) != 0x9EC
        || read<std::uint64_t>(actions.data(), 8) != 6
        || read<std::uint64_t>(actions.data(), 0x10) != 0x50
        || read<std::uint8_t>(actions.data(), 0x48) != 0
        || read<std::uint64_t>(actions.data(), descriptor) != 1
        || read<std::uint64_t>(actions.data(), descriptor+8) != (right?0x130U:0x1C8U)
        || read<std::uint32_t>(actions.data(), descriptor+16) != (right?combat::kRightAction:combat::kLeftAction)
        || read<std::uint64_t>(actions.data(), variant) != 1
        || read<std::uint64_t>(actions.data(), variant+8) != 0x48
        || read<std::uint32_t>(actions.data(), variant+0x28) != (right?0x1000FU:0x1000EU)
        || read<std::uint32_t>(actions.data(), variant+0x30) != 0x3F800000
        || read<std::uint32_t>(actions.data(), variant+0x34) != 0x40B00001
        || read<std::uint8_t>(actions.data(), variant+0x3F) != 1
        || read<std::uint32_t>(actions.data(), variant+0x58) != 0x80803737
        || read<std::uint32_t>(actions.data(), variant+0x60) != clipIndex) {
        out.failure = 2; return false;
    }
    out.duration = read<float>(actions.data(), variant+0x34);
    std::array<std::byte, 0x18> bank{};
    auto* bankAsset = resolve_handle(observation::kBankAsset);
    std::uint32_t clip{};
    if (!copy_native(bankAsset, bank.data(), bank.size()) || read<std::uint64_t>(bank.data(), 8) != 25
        || !copy_native(reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(bankAsset)
            + 0x20 + read<std::uintptr_t>(bank.data(), 0x10) + clipIndex * 4), &clip, sizeof clip)
        || clip != (right?combat::kRightClip:combat::kLeftClip)) { out.failure = 3; return false; }
    std::array<std::byte, 0x10> table{};
    if (!copy_native(component + 0x500, table.data(), table.size())
        || read<std::int32_t>(table.data(), 0) != 6
        || read<std::uintptr_t>(table.data(), 8) == 0) { out.failure = 4; return false; }
    const auto rows = reinterpret_cast<std::uintptr_t>(component) + 0x518
        + read<std::uintptr_t>(table.data(), 8);
    if (!copy_native(reinterpret_cast<const void*>(rows + (right?1:2) * combat::kActionGroupBytes),
                     out.group.data(), out.group.size())) { out.failure = 5; return false; }
    std::array<std::byte, 0xA> activity{};
    if (!copy_native(component + 0xB20, activity.data(), activity.size())) { out.failure = 6; return false; }
    out.blend = read<float>(activity.data(), 0);
    // 104A700 publishes these rows only while the custom-animation output is
    // enabled and blended in (this exact asset's always-on flag+48 is zero).
    out.enabled = read<std::uint8_t>(activity.data(), 9) != 0
        && std::isfinite(out.blend) && out.blend > 0.001F && out.blend <= 1.F;
    return true;
}
bool __fastcall fullbody_update(std::byte* component, const void* frame, float dt,
                                std::uint8_t mode, const void* input, void* output) noexcept {
    const hooking::CallGate::Scope call(callGate);
    const bool result = [&] {const frame_timing::PostSpan timer(frame_timing::Kind::fullbody_native);
        return hooking::await_original(fullBodyOriginal)(component, frame, dt, mode, input, output);}();
    const frame_timing::PostSpan postTiming(frame_timing::Kind::fullbody);
    if (!call.accepts_side_effects() || !active()) return result;
    std::array<std::byte, 16> prefix{};
    if (!copy_native(component, prefix.data(), prefix.size())
        || !omega_reveal_source::matches(prefix, {combat::kFullBodyAsset, 0x80803640U, 0x15B8})) return result;
    pump_mission_motion(call);
    pump_mission_crown(call);
    const auto run = state::activity::mission_run_generation();
    namespace mission=omega::mission;
    auto missionSnapshot=mission::runtime::snapshot(run);
    const bool right=missionSnapshot.command.action==mission::Action::right;
    const bool initialArm=missionSnapshot.command.wave<2;
    const bool crownArm=missionSnapshot.command.island==4 && missionSnapshot.command.cycle>0;
    graph::Owner issued{};
    combat::Track track{};
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != run || !runState.queueAccepted
            || (initialArm && runState.graphRetired) || !runState.introIdleSeen) return result;
        if(missionSnapshot.generation && missionSnapshot.command.token!=runState.armToken) {
            if(missionSnapshot.command.action!=mission::Action::left && !right) return result;
            if(runState.left.stage!=combat::Stage::complete) return result;
            runState.left={}; runState.armToken=missionSnapshot.command.token;
        }
        issued = runState.graphOwner; track = runState.left;
    }
    if (track.stage == combat::Stage::requesting || track.stage == combat::Stage::releasing
        || track.stage == combat::Stage::complete || track.stage == combat::Stage::uncertain) return result;
    MemberView member{};
    graph::Owner current{};
    std::uint32_t self{}, entity{};
    if (!current_owner(issued, member, current) || current != issued || !member.enabled
        || (initialArm ? member.head!=0 || !member.exactQueue : crownArm ? !mission_crown_queue(current,member,missionSnapshot.command.cycle) : member.head!=member.count) || !fullbody_owner(component, current, self, entity)) {
        owner_wait(run, 25, "left_fullbody_owner", self, issued.actor); return result;
    }
    if (track.stage != combat::Stage::unissued && (track.self != self || track.entity != entity)) return result;
    if(!missionSnapshot.generation) {
        const mission::Boss boss{run,issued.generation,issued.actor,issued.character,issued.biped,
            issued.entity,issued.member,issued.revision};
        if(!mission::runtime::receipt([&](auto& state){return state.bind(boss);})) return result;
        missionSnapshot=mission::runtime::snapshot(run);
        const std::lock_guard lock(mutex); runState.armToken=missionSnapshot.command.token;
    }
    const auto token=missionSnapshot.command.token;
    ScalarView scalar{};
    if (!left_scalar(entity, scalar, right)) {
        owner_wait(run, 26, scalarGuards[scalar.failure], scalar.actual, scalar.expected); return result;
    }
    LeftOutput nativeOutput{};
    combat::Playback playback{};
    const bool validOutput = left_output(component, nativeOutput, right);
    const bool leftPlaying = result && validOutput && nativeOutput.enabled
        && combat::left_playback(nativeOutput.group, nativeOutput.duration, playback, right);
    if (track.stage == combat::Stage::unissued) {
        if (!validOutput || leftPlaying || !scalar_equals(scalar.value, 0.F)) {
            const auto bits = read<std::uint32_t>(reinterpret_cast<const std::byte*>(scalar.value.lanes.data()), 0);
            owner_wait(run, 27, !validOutput ? "left_output_layout" : leftPlaying ? "left_already_playing" : "left_scalar_baseline",
                !validOutput ? nativeOutput.failure : bits, 0); return result;
        }
        {
            const std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock.owns_lock() || runState.run != run || runState.graphOwner != issued
                || (initialArm && runState.graphRetired) || runState.left.stage != combat::Stage::unissued) return result;
            runState.left.stage = combat::Stage::requesting;
            runState.left.self = self; runState.left.entity = entity;
            runState.left.scalarSelf = scalar.self; runState.left.scalarIndex = scalar.index;
            runState.left.boundScalar = scalar.bound;
        }
        if(!mission::runtime::receipt([&](auto& state){return state.claim(token,right?mission::Action::right:mission::Action::left);})) return result;
        const auto issuedSelf = self, issuedEntity = entity;
        const bool accepted = set_left_scalar(entity, 1.F, right);
        ScalarView after{};
        const bool confirmed = accepted && current_owner(issued, member, current) && current == issued
            && member.enabled && (initialArm ? member.head==0 && member.exactQueue : crownArm ? mission_crown_queue(current,member,missionSnapshot.command.cycle) : member.head==member.count)
            && fullbody_owner(component, current, self, entity) && self == issuedSelf && entity == issuedEntity
            && left_scalar(entity, after, right) && after.self == scalar.self && after.index == scalar.index
            && after.bound == scalar.bound
            && scalar_equals(after.value, 1.F);
        const std::lock_guard lock(mutex);
        if (runState.run != run || runState.graphOwner != issued) return result;
        runState.left.stage = confirmed ? combat::Stage::requested : combat::Stage::uncertain;
        log("ev=omega_reveal stage=%s_request run=%llu actor=%08X character=%08X biped=%08X fullbody=%08X "
            "entity=%08X scalar=%08X index=%d bound=%u accepted=%u confirmed=%u guard=%u receipt=scalar_only",
            right?"right":"left", run, issued.actor, issued.character, issued.biped, self, entity, scalar.self, scalar.index,
            scalar.bound ? 1U : 0U, accepted ? 1U : 0U, confirmed ? 1U : 0U, after.failure);
        return result;
    }
    if (scalar.self != track.scalarSelf || scalar.index != track.scalarIndex
        || scalar.bound != track.boundScalar
        || !scalar_equals(scalar.value, 1.F)) return result;
    bool started{}, release{};
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != run || runState.graphOwner != issued
            || (initialArm && runState.graphRetired) || runState.left.stage != track.stage) return result;
        if (!leftPlaying) {
            runState.left.previousValid = false;
            if (runState.left.rejectedSamples++ < 8)
                log("ev=omega_reveal stage=%s_playback_wait run=%llu actor=%08X output_guard=%u rows=%d clip_index=%u weight=%.4f seconds=%.4f processed=%u blend=%.4f enabled=%u mode=%u mutation=observe_only",
                    right?"right":"left", run, issued.actor, nativeOutput.failure, validOutput ? read<std::int32_t>(nativeOutput.group.data(), 0x70) : -1,
                    static_cast<unsigned>(read<std::uint16_t>(nativeOutput.group.data(), 0xC)),
                    static_cast<double>(read<float>(nativeOutput.group.data(), 4)),
                    static_cast<double>(read<float>(nativeOutput.group.data(), 0x18)),
                    static_cast<unsigned>(read<std::uint8_t>(nativeOutput.group.data(), 0x13)),
                    static_cast<double>(nativeOutput.blend), nativeOutput.enabled ? 1U : 0U, static_cast<unsigned>(mode));
            return result;
        }
        started = track.stage == combat::Stage::requested;
        release = track.stage == combat::Stage::playing && combat::wrapped(runState.left, playback);
        runState.left.stage = release ? combat::Stage::releasing : combat::Stage::playing;
        runState.left.previousTime = playback.time; runState.left.layer = playback.layer;
        runState.left.previousValid = true;
        if (started) log("ev=omega_reveal stage=%s_started run=%llu actor=%08X character=%08X biped=%08X generation=%u "
            "fullbody=%08X clip=%08X time=%.4f weight=%.4f layer=%u receipt=native_fullbody_output",
            right?"right":"left", run, issued.actor, issued.character, issued.biped, issued.generation, self,
            right?combat::kRightClip:combat::kLeftClip,
            static_cast<double>(playback.time), static_cast<double>(playback.weight), static_cast<unsigned>(playback.layer));
    }
    if (started) {
        mission::runtime::receipt([&](auto& state){return state.animation(token,mission::Animation::started);});
        if(!right) omega::lair_start::note_left_started(run, issued.generation, issued.actor, issued.character, issued.biped);
        log("ev=omega_mission stage=arm_started run=%llu wave=%u arm=%s epoch=%u",run,missionSnapshot.command.wave,right?"right":"left",token.epoch);
    }
    if (!release) return result;
    const bool accepted = set_left_scalar(entity, 0.F, right);
    ScalarView after{};
    const bool confirmed = accepted && current_owner(issued, member, current) && current == issued
        && left_scalar(entity, after, right) && after.self == scalar.self && after.index == scalar.index
        && after.bound == scalar.bound
        && scalar_equals(after.value, 0.F);
    const std::lock_guard lock(mutex);
    if (runState.run != run || runState.graphOwner != issued) return result;
    runState.left.stage = confirmed ? combat::Stage::complete : combat::Stage::uncertain;
    if(confirmed) mission::runtime::receipt([&](auto& state){return state.animation(token,mission::Animation::finished);});
    log("ev=omega_reveal stage=%s_released run=%llu actor=%08X confirmed=%u trigger=native_clip_wrap",
        right?"right":"left", run, issued.actor, confirmed ? 1U : 0U);
    return result;
}
