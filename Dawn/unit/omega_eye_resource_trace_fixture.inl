unsigned eyeDispatchCalls{};
bool eyeDispatchResult{};
std::byte* expectedEyeChannel{};
const void* expectedEyeRequest{};
bool __fastcall eye_dispatch_fixture(std::byte* channel,const void* request,bool retain,
    std::int32_t context,std::int32_t secondary) noexcept {
    check(channel==expectedEyeChannel && request==expectedEyeRequest && retain && context==-1 && secondary==73,
        "trace forwards all five native arguments unchanged");
    ++eyeDispatchCalls;return eyeDispatchResult;
}
void eye_resource_trace_test() {
    namespace m=omega::mission;
    reach_mission(m::Phase::shield,1);
    const auto before=m::runtime::state.snapshot();
    expectedEyeChannel=resolve_handle(runState.graphOwner.animation)+0x1980;
    std::array<std::byte,12> request{};
    put(request.data(),0,0x7BD05F71U);put(request.data(),8,0x80F4547EU);
    expectedEyeRequest=request.data();eyeDispatchCalls=0;eyeDispatchResult=true;
    eyeResourceToken={};eyeResourceCounts={};eyeResourceOriginal.store(&eye_dispatch_fixture);
    const auto channelHead=read<std::uint32_t>(expectedEyeChannel,0);
    put(expectedEyeChannel,0,before.command.token.boss.character);
    put(expectedEyeChannel,4,runState.graphOwner.animation);
    check(eye_resource_channel(expectedEyeChannel,before.command.token.boss),"trace validates actual native boss channel");
    const auto initial=receipts.size();
    for(unsigned i=0;i<8;++i) check(trace_eye_resource(expectedEyeChannel,request.data(),true,-1,73),"original true return preserved");
    check(eyeDispatchCalls==8 && receipts.size()==initial+4,"diagnostic capped without suppressing native requests");
    eyeDispatchResult=false;put(request.data(),8,0x80F45564U);
    check(!trace_eye_resource(expectedEyeChannel,request.data(),true,-1,73),"original false return preserved");
    check(receipts.back().find("native_result=0")!=std::string::npos,"failed resource dispatch remains failure evidence");
    const auto filtered=receipts.size();
    put(request.data(),8,0x12345678U);
    check(!trace_eye_resource(expectedEyeChannel,request.data(),true,-1,73),"unrelated resource still forwarded");
    put(request.data(),8,0x80F453BFU);put(expectedEyeChannel,0,channelHead^0x10000000U);
    check(!trace_eye_resource(expectedEyeChannel,request.data(),true,-1,73),"wrong owner still forwarded");
    put(expectedEyeChannel,0,channelHead);
    check(receipts.size()==filtered && eyeDispatchCalls==11,"unrelated resources and owners produce no boss diagnostic");
    const auto after=m::runtime::state.snapshot();
    check(after.command.token==before.command.token && after.phase==before.phase && after.command.claimed==before.command.claimed,
        "dispatcher observer never advances encounter state");
    check(callGate.idle(),"trace releases call gate");
}
