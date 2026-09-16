// Included inside the population observer's anonymous namespace. All retained
// records use g_pendingMutex; no native original or destructor runs under it.
namespace streaming {
namespace policy=native_population_streaming;
using Weak=gateway_native::Weak;
using Bind=void(__fastcall*)(std::uint32_t,std::uint32_t) noexcept;
using Dispatch=NativePopulationDispatch;
using Destroy=void(__fastcall*)(void*) noexcept;
using Consume=void(__fastcall*)(void*,std::uint32_t) noexcept;
using Delete=void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept;
std::atomic<Bind> bind{};
std::atomic<Destroy> destroy{};
std::atomic<Consume> consume{};
Delete deleteFacet{};
struct Facet final {
    std::uintptr_t row{};Weak net{};
    std::uint32_t handle{UINT32_MAX},object{UINT32_MAX};
    std::int8_t owner{-3};
};
struct Source final {
    nativeEvents::Receipt receipt{};Weak identity{};policy::Counters counters{};
    bool frozen{},destroyed{},settled{};
    std::uint32_t rejectedHandle{UINT32_MAX};
};
struct Root final {
    nativeEvents::Receipt receipt{};state::activity::coo::PopulationActor actor{};
    Weak entity{},member{};bool retired{},closed{};
    Facet network{};
};
static_assert(policy::kSourceCapacity==nativeEvents::kBindingCapacity);
std::array<Source,policy::kSourceCapacity> sources{};
std::array<Root,512> roots{};
std::atomic_uint lines{};
template<class... Args> void report(const char* format,Args... args) noexcept {
    if(lines.fetch_add(1,std::memory_order_relaxed)>=512)return;
    std::array<char,512> line{};const auto size=std::snprintf(line.data(),line.size(),format,args...);
    if(size>0 && static_cast<std::size_t>(size)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
}
void prune() noexcept {
    for(auto& source:sources)if(source.receipt && nativeEvents::capture(source.receipt.lease)!=source.receipt)source={};
    for(auto& root:roots)if(root.receipt && ((root.closed && root.retired)
        || nativeEvents::capture(root.receipt.lease)!=root.receipt))root={};
}
bool source_identity(void* instance,nativeEvents::Receipt& receipt,Weak& identity) noexcept {
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    Read read;Actor value;std::uintptr_t resolved{};
    if(!read.value(address+0x48,value.source.handle))return false;
    value.source.kind=0x80809A3BU;
    gateway_native::Read native{g_image};
    return native.make_weak(value.source.handle,identity) && native.resolve(identity.handle,resolved)
        && resolved==address && registered_source(read,value,receipt) && receipt.lease.discardStreamedReplicas;
}
bool counters(void* instance,policy::Counters& result) noexcept {
    Read read;const auto address=reinterpret_cast<std::uintptr_t>(instance);
    Ref definition{};std::uintptr_t resolved{};
    return read.value(address,definition) && read.resolve(definition,resolved)
        && read.value(resolved+0xA8,result.categories) && read.value(address+0x650,result.requested)
        && read.value(address+0x268,result.consumed) && read.value(address+0x670,result.pending)
        && (result.categories!=2 || (read.value(address+0x654,result.secondRequested)
            && read.value(address+0x26C,result.secondConsumed) && read.value(address+0x674,result.secondPending)));
}
// The actor's root may attach after A0D510. Retry this from the normal admission
// poll as well as immediately before native detachment.
void remember(const pending::Birth& birth) noexcept {
    if(!birth.event.lease.discardStreamedReplicas)return;
    for(const auto& root:roots)if(root.receipt==birth.receipt && root.actor==birth.event.actor)return;
    gateway_native::Read read{g_image};Weak entity{},member{};std::uintptr_t row{};
    std::uint32_t handle{UINT32_MAX};
    if(!read.make_weak(birth.event.actor.entity,entity) || !read.entity_row(entity,row)
        || !read.value(row+0x4C,handle) || !read.make_weak(handle,member))return;
    for(auto& root:roots)if(!root.receipt) {
        root={birth.receipt,birth.event.actor,entity,member};return;
    }
    report("ev=native_population_streaming stage=retain_root result=capacity actor=%08X",birth.event.actor.actor);
    nativeEvents::observation_lost();
}
void retired(const nativeEvents::Event& event) noexcept {
    for(auto& root:roots)if(root.actor==event.actor) {root.retired=true;if(root.closed)root={};}
}
bool net_row(gateway_native::Read& read,std::uint32_t handle,std::uintptr_t& row) noexcept {
    std::uintptr_t table{};std::uint32_t stride{};
    if(handle==UINT32_MAX || !read.value(g_image+0x2037D48,table)
        || !read.value(g_image+0x2037D50,stride) || table<0x10000 || stride<12 || stride>0x1000)return false;
    row=table+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;return true;
}
bool facet(std::uint32_t handle,Facet& out,bool retained=false) noexcept {
    gateway_native::Read read{g_image};std::uintptr_t net{};
    if(!read.make_weak(handle,out.net) || !net_row(read,handle,net)
        || !read.value(net,out.handle) || out.handle==UINT32_MAX || !read.value(net+4,out.object))return false;
    std::array<std::byte,128> bitmap{};
    if(!read.copy(g_image+0x30B0340,bitmap))return false;
    for(std::size_t i=0;i<1024;++i) {
        if((std::to_integer<unsigned>(bitmap[i/8])&(1U<<(i%8)))==0)continue;
        const auto address=g_image+0x30B0440+i*0x70;
        std::array<std::byte,12> header{};
        if(!read.copy(address,header))return false;
        if(at<std::uint32_t>(header.data()+8)!=out.handle)continue;
        out.owner=at<std::int8_t>(header.data()+1);
        std::uint16_t flags{};std::uintptr_t sync{};std::uint32_t peers{};
        if(at<std::uint32_t>(header.data()+4)!=handle || !read.value(address+0x50,flags)
            || !read.value(address+0x60,sync) || (sync && !read.value(sync+4,peers))
            || !(retained ? policy::retained_facet : policy::local_facet)(
                at<std::int8_t>(header.data()),out.owner,flags,peers))return false;
        out.row=address;return true;
    }
    return false;
}
bool owned_root(const Facet& value,Root& result) noexcept {
    gateway_native::Read read{g_image};std::uintptr_t object{},member{},allocation{};
    std::uint32_t self{},rootHandle{},entity{};Weak root{};
    if(!read.resolve(value.object,object) || !read.value(object+0xC0,self) || self!=value.object
        || !read.value(object+0xC8,rootHandle) || !read.make_weak(rootHandle,root)
        || !read.resolve(rootHandle,member,&allocation) || !read.value(allocation+0x10,entity))return false;
    for(const auto& saved:roots)if(saved.receipt && !saved.closed && saved.member==root
        && saved.entity.handle==entity && nativeEvents::capture(saved.receipt.lease)==saved.receipt) {
        result=saved;return true;
    }
    return false;
}
// Capture network ownership while the admitted entity is still intact. Its
// member allocation can be released before the engine detaches the network
// object. The retained net allocation and full facet handle survive that gap.
void capture_network_roots() noexcept {
    bool needed{};
    for(const auto& root:roots)needed|=root.receipt && !root.retired && !root.closed && root.network.net.handle==UINT32_MAX;
    if(!needed)return;
    gateway_native::Read bitmapRead{g_image};std::array<std::byte,128> bitmap{};
    if(!bitmapRead.copy(g_image+0x30B0340,bitmap))return;
    for(std::size_t i=0;i<1024;++i) {
        if((std::to_integer<unsigned>(bitmap[i/8])&(1U<<(i%8)))==0)continue;
        gateway_native::Read read{g_image};std::array<std::byte,12> header{};
        const auto address=g_image+0x30B0440+i*0x70;
        if(!read.copy(address,header) || at<std::int8_t>(header.data())!=0 || at<std::int8_t>(header.data()+1)!=-1)continue;
        const auto net=at<std::uint32_t>(header.data()+4);std::uintptr_t row{};Facet candidate;Root owned;
        if(!net_row(read,net,row) || !read.value(row+4,candidate.object) || candidate.object==UINT32_MAX
            || !owned_root(candidate,owned))continue;
        Facet verified;
        if(!facet(net,verified) || verified.row!=address || verified.object!=candidate.object)continue;
        for(auto& root:roots)if(root.receipt==owned.receipt && root.member==owned.member
            && root.network.net.handle==UINT32_MAX) {
            root.network=verified;
            report("ev=native_population_streaming stage=network_retained registry=%08X actor=%08X net=%08X facet=%08X",
                root.receipt.lease.source.source.registry,root.actor.actor,net,verified.handle);
            break;
        }
    }
}
__declspec(noinline) void __fastcall bind_hook(std::uint32_t net,std::uint32_t object) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    Facet before{};Root root{};bool qualified{};
    if(scope.accepts_side_effects() && object==UINT32_MAX) {
        std::lock_guard lock(g_pendingMutex);
        finish_native_admissions();prune();
        for(std::size_t i=0;i<g_admittedActors.size();++i)remember(g_admittedActors[i]);
        capture_network_roots();
        for(const auto& saved:roots)if(saved.receipt && !saved.closed && saved.network.net.handle==net) {
            root=saved;break;
        }
        qualified=root.receipt && facet(net,before,true) && before.net==root.network.net
            && before.handle==root.network.handle && before.row==root.network.row
            && (before.object==root.network.object || before.object==UINT32_MAX)
            && nativeEvents::capture(root.receipt.lease)==root.receipt;
    }
    hooking::await_original(bind)(net,object);
    if(!scope.accepts_side_effects() || !root.receipt)return;
    if(!qualified) {
        report("ev=native_population_streaming stage=replica_discard result=identity_rejected net=%08X facet=%08X owner=%d",
            net,before.handle,static_cast<int>(before.owner));return;
    }
    // 1704870 can defer its write. Only its eventual immediate invocation may
    // close this facet. The full native deletion path also releases saved state,
    // descendants, manager membership and the global allocation bit.
    Facet after{};
    if(!facet(net,after,true) || after.net!=before.net || after.handle!=before.handle
        || after.row!=before.row || after.object!=UINT32_MAX
        || nativeEvents::capture(root.receipt.lease)!=root.receipt)return;
    deleteFacet(net,0);
    gateway_native::Read read{g_image};std::uintptr_t row{};std::uint32_t remaining{};
    const bool closed=read.weak(before.net) && net_row(read,net,row) && read.value(row,remaining) && remaining==UINT32_MAX;
    {
        std::lock_guard lock(g_pendingMutex);
        for(auto& saved:roots)if(saved.receipt==root.receipt && saved.member==root.member) {
            saved.closed=closed;if(saved.closed && saved.retired)saved={};
        }
    }
    report("ev=native_population_streaming stage=replica_discard registry=%08X slot=%u actor=%08X net=%08X facet=%08X closed=%u",
        root.receipt.lease.source.source.registry,root.receipt.lease.source.source.slot,root.actor.actor,net,before.handle,closed?1U:0U);
}
void checkpoint_source(void* instance,bool freeze) noexcept {
    nativeEvents::Receipt receipt;Weak identity;policy::Counters value;
    if(!source_identity(instance,receipt,identity) || !counters(instance,value))return;
    std::lock_guard lock(g_pendingMutex);
    for(auto& source:sources)if(source.receipt==receipt && source.identity==identity) {
        if(!source.frozen && policy::checkpoint(value))source.counters=value;
        source.frozen|=freeze;return;
    }
}
__declspec(noinline) void __fastcall consume_hook(void* instance,std::uint32_t mode) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(scope.accepts_side_effects())checkpoint_source(instance,true);
    hooking::await_original(consume)(instance,mode);
}
__declspec(noinline) void __fastcall destroy_hook(void* instance) noexcept {
    const hooking::CallGate::Scope scope{g_gate};nativeEvents::Receipt receipt;Weak identity;
    const bool qualified=scope.accepts_side_effects() && source_identity(instance,receipt,identity);
    policy::Counters value{};const bool settled=qualified && counters(instance,value) && value.pending==0;
    if(qualified) {
        checkpoint_source(instance,true);
        std::lock_guard lock(g_pendingMutex);
        finish_native_admissions();
        for(std::size_t i=0;i<g_admittedActors.size();++i)remember(g_admittedActors[i]);
        capture_network_roots();
    }
    hooking::await_original(destroy)(instance);
    if(qualified && scope.accepts_side_effects()) {
        std::lock_guard lock(g_pendingMutex);
        for(auto& source:sources)if(source.receipt==receipt && source.identity==identity) {
            source.destroyed=true;source.settled=settled;
            report("ev=native_population_streaming stage=source_destroyed registry=%08X slot=%u source=%08X consumed=%d settled=%u",
                receipt.lease.source.source.registry,receipt.lease.source.source.slot,identity.handle,source.counters.consumed,settled?1U:0U);
        }
    }
}
void prepare_source(void* instance) noexcept {
    nativeEvents::Receipt receipt;Weak identity;policy::Counters current;
    if(!source_identity(instance,receipt,identity) || !counters(instance,current) || !policy::checkpoint(current))return;
    std::lock_guard lock(g_pendingMutex);prune();
    Source* previous{};Source* empty{};
    for(auto& source:sources) {
        if(source.receipt==receipt)previous=&source;
        if(!source.receipt && !empty)empty=&source;
    }
    if(!previous) {
        if(empty)*empty={receipt,identity,current};
        else nativeEvents::observation_lost();
        return;
    }
    if(previous->identity==identity)return;
    gateway_native::Read read{g_image};Weak oldAllocation{};
    const bool released=read.make_weak(previous->identity.handle,oldAllocation) && oldAllocation!=previous->identity;
    if(!previous->settled || !policy::restore(previous->counters,current,previous->destroyed,released)) {
        if(previous->rejectedHandle!=identity.handle) {
            previous->rejectedHandle=identity.handle;
            report("ev=native_population_streaming stage=source_recreated result=unqualified registry=%08X slot=%u previous=%08X source=%08X destroyed=%u released=%u settled=%u saved=%d/%d current=%d/%d",
                receipt.lease.source.source.registry,receipt.lease.source.source.slot,previous->identity.handle,identity.handle,
                previous->destroyed?1U:0U,released?1U:0U,previous->settled?1U:0U,
                previous->counters.consumed,previous->counters.requested,current.consumed,current.requested);
        }
        return;
    }
    // Every earlier actor retirement must already be queued before rebinding.
    for(std::size_t i=0;i<g_admittedActors.size();++i)
        if(g_admittedActors[i].receipt==receipt && g_admittedActors[i].event.sourceHandle==previous->identity.handle)return;
    const auto saved=previous->counters.consumed;const auto savedSecond=previous->counters.secondConsumed;
    auto* consumed=reinterpret_cast<volatile LONG*>(reinterpret_cast<std::uintptr_t>(instance)+0x268);
    if(InterlockedCompareExchange(consumed,saved,current.consumed)!=current.consumed)return;
    auto* secondConsumed=consumed+1;
    if(current.categories==2
        && InterlockedCompareExchange(secondConsumed,savedSecond,current.secondConsumed)!=current.secondConsumed) {
        InterlockedCompareExchange(consumed,current.consumed,saved);return;
    }
    const nativeEvents::Event event{receipt.lease,{receipt.lease.source},identity.handle,
        nativeEvents::Kind::sourceRecreated,0,UINT32_MAX,previous->identity.handle};
    if(!nativeEvents::submit(event,receipt)) {
        if(current.categories==2)InterlockedCompareExchange(secondConsumed,current.secondConsumed,savedSecond);
        InterlockedCompareExchange(consumed,current.consumed,saved);return;
    }
    report("ev=native_population_streaming stage=source_recreated registry=%08X slot=%u generation=%u previous=%08X source=%08X consumed=%d requested=%d",
        receipt.lease.source.source.registry,receipt.lease.source.source.slot,receipt.lease.source.generation,
        previous->identity.handle,identity.handle,saved,current.requested);
    current.consumed=saved;current.secondConsumed=savedSecond;*previous={receipt,identity,current};
}
__declspec(noinline) void dispatch_source(std::uint32_t* instance,std::uint32_t mode,
    const std::byte* authority,Dispatch original) noexcept {
    const hooking::CallGate::Scope scope{g_gate};Read read;std::uint8_t spawnMode{UINT8_MAX};
    const bool active=scope.accepts_side_effects()
        && read.value(reinterpret_cast<std::uintptr_t>(authority)+0xBD,spawnMode) && (spawnMode==0 || spawnMode==2);
    if(active)prepare_source(instance);
    original(instance,mode,authority);
    if(active && scope.accepts_side_effects())checkpoint_source(instance,false);
}
void reset() noexcept {sources={};roots={};lines.store(0,std::memory_order_relaxed);deleteFacet=nullptr;}
}
