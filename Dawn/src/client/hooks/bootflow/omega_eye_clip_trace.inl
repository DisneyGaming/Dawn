// Clip kind3 events follow F49BC0 -> 1050570 -> 104D2D0 -> 104DA00.
// C71C30 is a different, named-resource route. Observe both without replaying.
using EyeClipDispatch=void(__fastcall*)(const void*,const void*,std::byte*,std::uint8_t) noexcept;
using EyeClipBuilder=bool(__fastcall*)(void*,std::uint32_t) noexcept;
std::atomic<EyeClipDispatch> eyeClipOriginal{};
std::atomic<EyeClipBuilder> eyeClipBuilderOriginal{};
struct EyeClipBuildObservation { std::uint32_t resource{};unsigned calls{},accepted{}; };
thread_local EyeClipBuildObservation* eyeClipBuild{};
std::mutex eyeClipMutex;
omega::mission::Token eyeClipToken{},eyeCursorToken{};
std::array<unsigned,3> eyeClipCounts{};
std::array<unsigned,2> eyeCursorCounts{};
bool eye_clip_owner(std::byte* biped,const omega::mission::Boss& boss) noexcept {
    std::array<std::byte,0xD4> bytes{};
    return resolve_handle(boss.biped)==biped && copy_native(biped,bytes.data(),bytes.size())
        && omega_reveal_source::matches(bytes,{0x80F66907U,0x808036CFU,0x1B48})
        && read<std::uint32_t>(bytes.data(),0x24)==boss.biped
        && read<std::uint32_t>(bytes.data(),0x2C)==boss.entity
        && read<std::uint32_t>(bytes.data(),0xD0)==observation::kBankAsset;
}
bool __fastcall trace_eye_clip_builder(void* builder,std::uint32_t resource) noexcept {
    const hooking::CallGate::Scope call(callGate);
    auto* scope=eyeClipBuild;
    const bool observed=call.accepts_side_effects() && scope && scope->resource==resource;
    const bool result=hooking::await_original(eyeClipBuilderOriginal)(builder,resource);
    if(observed) {++scope->calls;if(result) ++scope->accepted;}
    return result;
}
void __fastcall trace_eye_clip(const void* event,const void* context,std::byte* biped,std::uint8_t policy) noexcept {
    const hooking::CallGate::Scope call(callGate);
    constexpr std::array<std::uint32_t,3> resources{0x80F453BF,0x80F45568,0x80F4547E};
    std::array<std::byte,32> bytes{},contextBytes{};
    omega::mission::Snapshot snapshot{};unsigned ordinal{};
    if(call.accepts_side_effects() && active() && copy_native(event,bytes.data(),bytes.size())
        && read<std::uint8_t>(bytes.data(),2)==3) {
        const auto resource=read<std::uint32_t>(bytes.data(),0x18);
        const auto it=std::find(resources.begin(),resources.end(),resource);
        if(it!=resources.end()) {
            snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
            if(snapshot.chargeDunked && snapshot.command.cycle>=1 && snapshot.command.cycle<=3
                && eye_clip_owner(biped,snapshot.command.token.boss)) {
                const std::lock_guard lock(eyeClipMutex);
                if(eyeClipToken!=snapshot.command.token) {eyeClipToken=snapshot.command.token;eyeClipCounts={};}
                auto& count=eyeClipCounts[static_cast<std::size_t>(it-resources.begin())];
                if(count<4) ordinal=++count;
            }
        }
    }
    const bool contextRead=ordinal && copy_native(context,contextBytes.data(),contextBytes.size());
    EyeClipBuildObservation build{read<std::uint32_t>(bytes.data(),0x18)};
    struct Scope {
        EyeClipBuildObservation* previous{eyeClipBuild};
        explicit Scope(EyeClipBuildObservation* value) noexcept {eyeClipBuild=value;}
        ~Scope() {eyeClipBuild=previous;}
    } scope(ordinal?&build:nullptr);
    hooking::await_original(eyeClipOriginal)(event,context,biped,policy);
    if(ordinal) {
        const auto after=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
        const bool current=call.accepts_side_effects() && after.command.token==snapshot.command.token
            && eye_clip_owner(biped,snapshot.command.token.boss);
        log("ev=omega_mission stage=eye_clip_dispatch run=%llu epoch=%u cycle=%u resource=%08X frame=%d policy=%u context_read=%u context_biped=%08X context_source=%08X context_offset=%lld context_flags=%08X builder_calls=%u builder_accepted=%u current=%u ordinal=%u receipt=dispatch_and_builder_only",
            snapshot.command.token.boss.run,snapshot.command.token.epoch,snapshot.command.cycle,build.resource,
            read<std::int16_t>(bytes.data(),0),static_cast<unsigned>(policy),contextRead?1U:0U,
            read<std::uint32_t>(contextBytes.data(),0),read<std::uint32_t>(contextBytes.data(),8),
            read<std::int64_t>(contextBytes.data(),0x10),read<std::uint32_t>(contextBytes.data(),0x18),
            build.calls,build.accepted,current?1U:0U,ordinal);
    }
}
void observe_eye_clip_cursor(const void* context,std::span<const std::byte> bytes,
    const hooking::CallGate::Scope& call) noexcept {
    if(!call.accepts_side_effects() || bytes.size()<observation::kGraphBytes) return;
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    const auto& boss=snapshot.command.token.boss;const auto cycle=snapshot.command.cycle;
    if(!snapshot.chargeDunked || cycle<1 || cycle>3
        || read<std::uint32_t>(bytes.data(),0)!=boss.entity
        || read<std::uint32_t>(bytes.data(),4)!=boss.character
        || read<std::uint32_t>(bytes.data(),0x14)!=boss.biped
        || !eye_clip_owner(resolve_handle(boss.biped),boss)) return;
    std::array<std::byte,0x440> bank{};crown::Observation observed{};
    if(!copy_native(resolve_handle(observation::kBankAsset),bank.data(),bank.size())
        || !crown::decode(bytes,bank,cycle,true,observed)
        || (observed.node!=crown::Node::eyeOpening && observed.node!=crown::Node::eye)) return;
    const auto index=observed.node==crown::Node::eyeOpening?0U:1U;
    unsigned ordinal{};
    {
        const std::lock_guard lock(eyeClipMutex);
        if(eyeCursorToken!=snapshot.command.token) {eyeCursorToken=snapshot.command.token;eyeCursorCounts={};}
        if(eyeCursorCounts[index]>=3) return;
        ordinal=++eyeCursorCounts[index];
    }
    std::array<std::byte,0x178> clip{};std::array<std::byte,32> contextBytes{};
    const bool clipRead=copy_native(resolve_handle(observed.clip),clip.data(),clip.size());
    const bool contextRead=copy_native(context,contextBytes.data(),contextBytes.size());
    log("ev=omega_mission stage=eye_clip_cursor run=%llu epoch=%u cycle=%u clip=%08X time=%.6f event_begin=%d event_end=%d clip_read=%u event_total=%u context_present=%u context_read=%u context_biped=%08X ordinal=%u receipt=playback_only",
        boss.run,snapshot.command.token.epoch,cycle,observed.clip,static_cast<double>(read<float>(bytes.data(),0x3C)),
        read<std::int16_t>(bytes.data(),0x50),read<std::int16_t>(bytes.data(),0x52),clipRead?1U:0U,
        read<std::uint32_t>(clip.data(),0x160),context?1U:0U,contextRead?1U:0U,
        read<std::uint32_t>(contextBytes.data(),8),ordinal);
}
