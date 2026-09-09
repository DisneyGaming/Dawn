// Included inside omega_rescue_scene_receipts.cpp's guarded hook namespace.
// Uses that translation unit's Read, Ref, Weak and verified group/weak resolvers.
namespace beyond=state::activity::beyond_infinity;
namespace beyond_native=beyond_infinity_native;
#include "beyond_infinity_future_cast.inl"
bool read_beyond_scene(Read& read,std::uintptr_t component,beyond_native::SceneSample& sample) noexcept {
    Ref ref{};std::uintptr_t definition{};
    if(!read.value(component,ref) || ref.kind!=0x80806266U || ref.offset!=0x368) { return false; }
    const beyond::SceneBinding* binding{};
    for(const auto& scene:beyond::kScenes) { if(scene.asset.definition==ref.handle) { binding=&scene;break; } }
    if(!binding || !read.resolve(ref,definition)) { return false; }
    std::array<std::byte,8> scope{};
    if(!read.copy(definition+0x30,scope)) { return false; }
    sample.definition={ref.handle,ref.kind,ref.offset};
    sample.scope={at<std::uint32_t>(scope.data()),ref.handle,at<std::uint16_t>(scope.data()+4),at<std::uint16_t>(scope.data()+6)};
    if(sample.scope!=binding->asset) { return false; }
    std::uint16_t index{};Weak weak{};Diag diag{};std::uintptr_t selector{};
    if(!group_handle(read,component,sample.group,index) || !read.value(component+0x170,sample.sensor)
        || !read.value(component+0x254,sample.generation) || !read.value(component+0x258,sample.complete)
        || !read.value(component+0x2E8,weak) || !read.weak(weak,selector,diag)) { return false; }
    Ref selected{};Weak after{};
    if(!read.value(selector,selected) || !read.value(selector+0x24,sample.self) || !read.value(selector+0x2C,sample.parent)
        || !read.value(component+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial) { return false; }
    sample.selector={selected.handle,selected.kind,selected.offset};sample.weak=weak.handle;sample.serial=weak.serial;return true;
}

// This edit is confined to an authenticated Scene instance on the native tick
// thread. It never changes a package graph or the shared dialogue bank.
bool clear_beyond_callback(std::uintptr_t address,std::uint32_t expected) noexcept {
    __try {
        auto* handle=reinterpret_cast<volatile LONG*>(address);
        return InterlockedCompareExchange(handle,static_cast<LONG>(UINT32_MAX),static_cast<LONG>(expected))==static_cast<LONG>(expected);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void prepare_beyond_scene(Read& read,std::uintptr_t component,const beyond_native::SceneSample& sample,
    const beyond::Request& request) noexcept {
    if(!beyond_native::silent_voice(sample,request.frame)) { return; }
    Weak weak{sample.serial,sample.weak},after{};Diag diag{};std::uintptr_t root{};
    if(!read.weak(weak,root,diag)) { return; }
    std::uint32_t count{};std::int64_t relative{};
    if(!read.value(root+0x68,count) || count>256
        || !read.value(root+0x70,relative) || relative<0 || relative>0x20000) { return; }
    const auto table=root+0x70+static_cast<std::uintptr_t>(relative)+0x10;
    const auto& scene=beyond::kScenes[beyond::scene_index(sample.scope)];
    for(const auto& speech:scene.speech) {
        Ref node{},input{};std::uint32_t state{},counter{},fallback{};
        const auto signal=speech.offset+0xD0U;
        if(speech.input>=count || !read.value(root+speech.offset,node) || node.handle!=scene.selectorGraph
            || node.kind!=0x808062F6U || node.offset!=speech.definition
            || !read.value(root+speech.offset+0x98,state) || state!=0
            || !read.value(table+speech.input*16,input) || input.handle!=sample.self
            || input.kind!=0x80806388U || input.offset!=signal
            || !read.value(root+signal+0x60,counter) || counter!=0
            || !read.value(root+signal+0x50,fallback) || fallback!=UINT32_MAX) { continue; }
        std::array<std::byte,24> callback{};
        if(!read.copy(root+signal+0x20,callback) || !beyond_native::silent_callback(callback,sample.self,speech.offset)
            || !read.value(component+0x2E8,after) || after.serial!=weak.serial || after.handle!=weak.handle) { continue; }
        static_cast<void>(clear_beyond_callback(root+signal+0x28,sample.self));
    }
}
// Observe the two actor-bearing children across their handoff. No actors,
// package bytes or native requests are changed by this diagnostic.
void log_beyond_future_cast(std::uintptr_t component,const beyond_native::SceneSample& sample,
    const beyond::SceneReceipt& receipt) noexcept {
    if(sample.scope.registry!=0x15FFBE16U || sample.scope.slot!=0 || sample.selector.tag!=0x80EC0901U) { return; }
    struct Child { std::uint32_t id,node,definition,graph,root,actor; };
    constexpr Child children[]{{27,0x33D0,0x7E28,0x80EC0910U,0x7F8,0xB00},
        {17,0x23F0,0x7798,0x80EC0872U,0x2648,0x2950}};
    static std::array<std::atomic_uint64_t,2> previous{};
    for(std::size_t i=0;i<std::size(children);++i) {
        const auto& c=children[i];Read read;Diag diag{};std::uintptr_t root{},child{},actor{};
        Weak rootWeak{sample.serial,sample.weak},weak{},after{},cast{};Ref node{},header{},parameter{};
        std::uint32_t parent{},self{};std::uint8_t state{};bool valid{},actorValid{};
        if(!read.weak(rootWeak,root,diag) || !read.value(root+c.node,node)
            || node.handle!=0x80EC0901U || node.kind!=0x808062FEU || node.offset!=c.definition
            || !read.value(root+c.node+0x28,parent) || parent!=sample.self
            || !read.value(root+c.node+0x98,state) || state>2
            || !read.value(root+c.node+0x1B0,weak)) { continue; }
        if(read.weak(weak,child,diag) && read.value(child,header) && header.handle==c.graph
            && header.kind==0x808084E9U && header.offset==c.root
            && read.value(child+0x24,self) && self==weak.handle
            && read.value(child+0x270,parameter) && parameter.handle==c.graph
            && parameter.kind==0x808084E1U && parameter.offset==c.actor
            && read.value(child+0x290,cast)) {
            valid=true;actorValid=read.weak(cast,actor,diag);
        }
        if(!read.value(root+c.node+0x1B0,after) || after.handle!=weak.handle || after.serial!=weak.serial
            || !read.value(component+0x2E8,after) || after.handle!=rootWeak.handle || after.serial!=rootWeak.serial) { continue; }
        std::uint64_t key=1469598103934665603ULL;
        for(const auto value:{receipt.owner.run,std::uint64_t{receipt.owner.value},std::uint64_t{sample.self},
            std::uint64_t{state},std::uint64_t{weak.serial},std::uint64_t{weak.handle},
            std::uint64_t{cast.serial},std::uint64_t{cast.handle},std::uint64_t{valid},std::uint64_t{actorValid}}) {
            key=(key^value)*1099511628211ULL;
        }
        if(previous[i].exchange(key,std::memory_order_relaxed)==key) { continue; }
        std::array<char,400> message{};
        const int count=std::snprintf(message.data(),message.size(),
            "ev=beyond_infinity stage=future_cast run=%llu generation=%u child=%u state=%u child_valid=%u weak=%08X/%08X actor=%08X/%08X actor_valid=%u",
            static_cast<unsigned long long>(receipt.owner.run),receipt.owner.value,c.id,static_cast<unsigned>(state),
            static_cast<unsigned>(valid),weak.serial,weak.handle,cast.serial,cast.handle,static_cast<unsigned>(actorValid));
        if(count>0 && static_cast<std::size_t>(count)<message.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,{message.data(),static_cast<std::size_t>(count)});
        }
    }
}
void observe_beyond_speech(Read& read,std::uintptr_t component,const beyond_native::SceneSample& sample,
    const beyond::SceneReceipt& receipt) noexcept {
    const auto index=beyond::scene_index(receipt.asset);if(index==std::size(beyond::kScenes)) { return; }
    const auto& scene=beyond::kScenes[index];Weak weak{sample.serial,sample.weak};Diag diag{};std::uintptr_t root{};
    if(!read.weak(weak,root,diag)) { return; }
    for(const auto& speech:scene.speech) {
        Ref node{};std::uint32_t state{};Weak after{};
        if(!read.value(root+speech.offset,node) || !read.value(root+speech.offset+0x98,state)
            || !beyond_native::speech_node(scene,speech,{node.handle,node.kind,node.offset},state)
            || !read.value(component+0x2E8,after) || after.serial!=weak.serial || after.handle!=weak.handle) { continue; }
        beyond::observe_scene_speech(receipt,speech.row,state);
    }
    for(const auto& cue:scene.cues) {
        Ref node{},signal{};std::uint32_t state{},starts{};Weak after{};
        if(!read.value(root+cue.offset,node) || !read.value(root+cue.offset+0x98,state)
            || !read.value(root+cue.signal,signal) || !read.value(root+cue.signal+0x60,starts)
            || !beyond_native::cue_node(scene,cue,{node.handle,node.kind,node.offset},
                {signal.handle,signal.kind,signal.offset},sample.self,state,starts)
            || !read.value(component+0x2E8,after) || after.serial!=weak.serial || after.handle!=weak.handle) { continue; }
        beyond::observe_scene_cue(receipt,cue.id,state,starts);
    }
}
void finish_beyond_scene(Read& read,std::uintptr_t component,const beyond_native::SceneSample& before,
    state::activity::coo::Generation owner) noexcept {
    const auto request=beyond::request();if(request.owner!=owner) { return; }
    beyond::SceneReceipt receipt{};
    if(!beyond_native::scene_sample(before,request.frame,owner,receipt)) { return; }
    beyond_native::SceneSample after{};
    if(read_beyond_scene(read,component,after) && beyond_native::same_scene(before,after)
        && beyond_native::scene_sample(after,request.frame,owner,receipt)) {
        beyond::observe_scene(receipt,false);observe_beyond_speech(read,component,after,receipt);
        recover_beyond_future_cast(component,after,receipt);
        log_beyond_future_cast(component,after,receipt);
        if(after.complete==1) { beyond::observe_scene(receipt,true); }return;
    }
    // Native B438B0 can dispose its selector while setting complete. Require
    // the pre-call authenticated owner and unchanged component identity then.
    Read terminal;Ref definition{};std::uint32_t group{},sensor{},generation{};std::uint16_t index{};std::uint8_t complete{};Weak weak{};
    if(!terminal.value(component,definition) || definition.handle!=before.definition.tag || definition.kind!=before.definition.kind || definition.offset!=before.definition.offset
        || !group_handle(terminal,component,group,index) || group!=before.group || !terminal.value(component+0x170,sensor) || sensor!=before.sensor
        || !terminal.value(component+0x254,generation) || generation!=before.generation || !terminal.value(component+0x258,complete) || complete!=1
        || !terminal.value(component+0x2E8,weak) || weak.handle!=UINT32_MAX || weak.serial!=UINT32_MAX) { return; }
    beyond::observe_scene(receipt,false);beyond::observe_scene(receipt,true);
}
