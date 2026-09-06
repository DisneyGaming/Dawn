unsigned clipCalls{},clipBuildCalls{};
bool clipBuildResult{};
const void* expectedClipEvent{};
const void* expectedClipContext{};
std::byte* expectedClipBiped{};
std::array<std::byte,32> clipBuilderFixture{};
bool __fastcall clip_builder_fixture(void* builder,std::uint32_t resource) noexcept {
    check(builder==clipBuilderFixture.data() && (resource==0x80F4547EU || resource==0x12345678U),
        "clip builder arguments forwarded unchanged");
    ++clipBuildCalls;return clipBuildResult;
}
void __fastcall clip_dispatch_fixture(const void* event,const void* context,std::byte* biped,std::uint8_t policy) noexcept {
    check(event==expectedClipEvent && context==expectedClipContext && biped==expectedClipBiped && policy==0,
        "actual clip dispatcher receives all original arguments");
    ++clipCalls;
    check(trace_eye_clip_builder(clipBuilderFixture.data(),0x12345678U)==clipBuildResult,"unrelated builder return preserved");
    check(trace_eye_clip_builder(clipBuilderFixture.data(),0x80F4547EU)==clipBuildResult,"scoped builder return preserved");
}
void eye_clip_trace_test() {
    namespace m=omega::mission;
    reach_mission(m::Phase::shield,1);const auto before=m::runtime::state.snapshot();
    const auto& boss=before.command.token.boss;
    expectedClipBiped=resolve_handle(boss.biped);
    check(eye_clip_owner(expectedClipBiped,boss),"actual typed biped is accepted for clip events");
    std::array<std::byte,32> event{},context{};
    event[2]=std::byte{3};put(event.data(),0x18,0x80F4547EU);
    put(context.data(),0,boss.biped);put(context.data(),8,UINT32_MAX);
    expectedClipEvent=event.data();expectedClipContext=context.data();
    eyeClipToken={};eyeClipCounts={};eyeClipOriginal.store(&clip_dispatch_fixture);
    eyeClipBuilderOriginal.store(&clip_builder_fixture);clipCalls=clipBuildCalls=0;clipBuildResult=true;
    const auto initial=receipts.size();
    for(unsigned i=0;i<8;++i) trace_eye_clip(event.data(),context.data(),expectedClipBiped,0);
    check(clipCalls==8 && clipBuildCalls==16 && receipts.size()==initial+4,"clip log cap never suppresses native calls");
    check(receipts.back().find("builder_calls=1 builder_accepted=1")!=std::string::npos,"only matching builder counted");
    check(eyeClipBuild==nullptr,"nested builder scope restored after dispatch");
    eyeClipToken={};clipBuildResult=false;
    trace_eye_clip(event.data(),context.data(),expectedClipBiped,0);
    check(receipts.back().find("builder_calls=1 builder_accepted=0")!=std::string::npos,"builder rejection remains explicit");
    const auto filtered=receipts.size();
    put(event.data(),0x18,0x12345678U);trace_eye_clip(event.data(),context.data(),expectedClipBiped,0);
    put(event.data(),0x18,0x80F4547EU);put(expectedClipBiped,0x2C,boss.entity^0x10000000U);
    trace_eye_clip(event.data(),context.data(),expectedClipBiped,0);put(expectedClipBiped,0x2C,boss.entity);
    check(receipts.size()==filtered && eyeClipBuild==nullptr,"unrelated resource and stale owner excluded");
    callGate.quiesce();trace_eye_clip(event.data(),context.data(),expectedClipBiped,0);callGate.accept();
    check(receipts.size()==filtered && callGate.idle(),"quiescing trace still forwards and releases gate");
    eyeCursorToken={};eyeCursorCounts={};auto frame=snapshot(4);
    graph::write(frame,0xA8,crown::cycles[0].graph);graph::write(frame,0xB4,crown::cycles[0].graph);
    graph::write(frame,0xA4,3);graph::write(frame,0xB0,3);graph::write(frame,0x30,16);frame[0x20]=std::byte{0};
    graph::write(frame,0x50,std::int16_t{0});graph::write(frame,0x52,std::int16_t{3});
    const auto cursorStart=receipts.size();
    {const hooking::CallGate::Scope call(callGate);
        for(unsigned i=0;i<6;++i) observe_eye_clip_cursor(nullptr,frame,call);
    }
    check(receipts.size()==cursorStart+3,"cursor diagnostics bounded per stage");
    check(receipts.back().find("event_begin=0 event_end=3")!=std::string::npos
        && receipts.back().find("context_present=0")!=std::string::npos,"event cursor and missing context kept distinct");
    const auto after=m::runtime::state.snapshot();
    check(after.command.token==before.command.token && after.phase==before.phase && after.command.claimed==before.command.claimed,
        "clip diagnostics do not manufacture vulnerability or progression");
}
