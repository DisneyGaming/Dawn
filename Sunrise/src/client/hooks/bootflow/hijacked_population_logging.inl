// Observation only, under the existing admission CallGate and g_lock. These
// samples never enter a mission ledger or supply missing native ownership.
struct HijackedBirthTrace {
    std::uint32_t actor{UINT32_MAX},parent{UINT32_MAX},definition{};
    std::uint64_t started{};
};
std::uint64_t g_hijackedTraceRun{},g_hijackedTracePoll{};
unsigned g_hijackedTraceLines{},g_hijackedTracePaths{};
std::array<HijackedBirthTrace,32> g_hijackedTracePending{};
void hijacked_trace_run(std::uint64_t value) noexcept {
    if(g_hijackedTraceRun==value)return;
    g_hijackedTraceRun=value;g_hijackedTracePoll=0;
    g_hijackedTraceLines=g_hijackedTracePaths=0;g_hijackedTracePending={};
}
template<class... Args> void hijacked_trace_report(const char* format,Args... args) noexcept {
    if(g_hijackedTraceLines>=1024)return;
    std::array<char,768> line{};
    const auto count=std::snprintf(line.data(),line.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<line.size()) {
        ++g_hijackedTraceLines;
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(count)});
    }
}
void trace_hijacked_birth(std::uint64_t runValue,const Actor& value,const Source& origin,
    std::uint32_t definition,unsigned rejection,bool accepted) noexcept {
    hijacked_trace_run(runValue);
    hijacked_trace_report("ev=hijacked_population stage=created run=%llu result=%s reason=%u definition=%08X actor=%08X parent=%08X entity=%08X source_ref=%08X source_kind=%08X member_ref=%08X flags=%04X registry=%08X slot=%u generation=%u sense_generation=%u accepted=%u",
        static_cast<unsigned long long>(runValue),value.source.handle==UINT32_MAX?"source_absent":rejection?"source_rejected":"source_qualified",
        rejection,definition,value.handle,value.parent,value.entity,value.source.handle,value.source.kind,value.member.handle,
        static_cast<unsigned>(value.flags),origin.registry,origin.slot,origin.generation,origin.senseGeneration,accepted?1U:0U);
    if(value.source.handle!=UINT32_MAX)return;
    // Follow only witnessed source-less births, without scanning the actor pool.
    bool retained{};
    for(auto& pending:g_hijackedTracePending) {
        if(pending.actor==value.handle) {retained=true;break;}
        if(pending.actor==UINT32_MAX) {
            pending={value.handle,value.parent,definition,GetTickCount64()};retained=true;break;
        }
    }
    if(!retained)hijacked_trace_report("ev=hijacked_population stage=attachment result=trace_capacity run=%llu actor=%08X",
        static_cast<unsigned long long>(runValue),value.handle);
    if(g_hijackedTracePaths>=16)return;
    ++g_hijackedTracePaths;
    std::array<void*,24> frames{};
    const auto depth=RtlCaptureStackBackTrace(0,static_cast<ULONG>(frames.size()),frames.data(),nullptr);
    std::array<char,256> path{};std::size_t used{};
    for(USHORT i=0;i<depth;++i) {
        const auto frame=reinterpret_cast<std::uintptr_t>(frames[i]);
        if(frame<g_image || frame-g_image>=0x1C00000U)continue;
        const auto written=std::snprintf(path.data()+used,path.size()-used,"%s%llX",
            used?",":"",static_cast<unsigned long long>(frame-g_image));
        if(written<=0 || static_cast<std::size_t>(written)>=path.size()-used)break;
        used+=static_cast<std::size_t>(written);
    }
    hijacked_trace_report("ev=hijacked_population stage=source_less_path run=%llu definition=%08X actor=%08X parent=%08X native_rvas=%s",
        static_cast<unsigned long long>(runValue),definition,value.handle,value.parent,path.data());
}
void trace_hijacked_attachments(std::uint64_t runValue) noexcept {
    hijacked_trace_run(runValue);const auto now=GetTickCount64();
    if(now<g_hijackedTracePoll)return;
    g_hijackedTracePoll=now+250;
    for(auto& pending:g_hijackedTracePending) {
        if(pending.actor==UINT32_MAX)continue;
        Read read;Actor current;std::uintptr_t parent{};
        std::array<std::byte,0x30> header{};std::uint32_t child{UINT32_MAX};
        if(!actor(read,pending.actor,current) || current.parent!=pending.parent
            || !read.resolve({pending.parent,0,0},parent) || parent>UINTPTR_MAX-0x1470
            || !read.copy(parent,header) || !read.value(parent+0x1470,child)
            || !omega_enemy_native_admission::identity(pending.parent,at<std::uint32_t>(header.data()+4),
                at<std::uint32_t>(header.data()+0x24),child,current.handle,current.parent)) {
            hijacked_trace_report("ev=hijacked_population stage=attachment run=%llu actor=%08X parent=%08X result=identity_lost",
                static_cast<unsigned long long>(runValue),pending.actor,pending.parent);
            pending={};continue;
        }
        const auto parentEntity=at<std::uint32_t>(header.data()+0x2C);
        const bool attached=current.entity!=UINT32_MAX && current.entity==parentEntity;
        if(!attached && now-pending.started<5000)continue;
        hijacked_trace_report("ev=hijacked_population stage=attachment run=%llu result=%s definition=%08X actor=%08X parent=%08X entity=%08X parent_entity=%08X source_ref=%08X source_kind=%08X member_ref=%08X flags=%04X elapsed_ms=%llu",
            static_cast<unsigned long long>(runValue),attached?"attached":"timeout",pending.definition,
            current.handle,current.parent,current.entity,parentEntity,current.source.handle,current.source.kind,current.member.handle,
            static_cast<unsigned>(current.flags),static_cast<unsigned long long>(now-pending.started));
        pending={};
    }
}
void trace_hijacked_retirement(std::uint64_t runValue,std::uint32_t handle) noexcept {
    hijacked_trace_run(runValue);Read read;Actor current;
    if(!actor(read,handle,current))return;
    hijacked_trace_report("ev=hijacked_population stage=retirement_begin run=%llu actor=%08X parent=%08X entity=%08X source_ref=%08X member_ref=%08X flags=%04X",
        static_cast<unsigned long long>(runValue),current.handle,current.parent,current.entity,current.source.handle,current.member.handle,
        static_cast<unsigned>(current.flags));
}
