// Included after beyond/beyond_native aliases inside the guarded Scene hook.
namespace future_cast=beyond_future_cast;
struct FutureActorNativeApi {
    std::uint32_t extension{},extensionKind{},created{UINT32_MAX};bool nativeOwned{};
    template<class F> F call(std::uintptr_t rva) const noexcept { return reinterpret_cast<F>(g_image+rva); }
    bool initialize(std::byte* request,std::uint32_t tag) noexcept {
        return call<bool(__fastcall*)(void*,std::uint32_t)>(0x4B2570)(request,tag);
    }
    std::byte* enrich(std::byte* request) noexcept {
        return call<std::byte*(__fastcall*)(void*,std::uint32_t,std::uint32_t)>(0x4B7340)(request,extension,extensionKind);
    }
    void owner(std::byte* request,std::uint32_t self) noexcept { call<void(__fastcall*)(void*,std::uint32_t)>(0x4B4640)(request,self); }
    void pose(std::uintptr_t child,std::byte* out) noexcept { call<void(__fastcall*)(void*,void*)>(0x597ED0)(reinterpret_cast<void*>(child),out); }
    void place(std::byte* request,std::byte* pose) noexcept { call<void(__fastcall*)(void*,void*)>(0x4B4810)(request,pose); }
    void factory(std::uint32_t* out,std::byte* request) noexcept { call<void*(__fastcall*)(void*,void*)>(0x56D990)(&created,request);*out=created; }
    bool bind(std::uintptr_t child,std::uint32_t name,std::uint32_t actor) noexcept {
        return call<bool(__fastcall*)(void*,const std::uint32_t*,std::uint32_t)>(0x58D380)(reinterpret_cast<void*>(child),&name,actor);
    }
    void destroy(std::uint32_t actor) noexcept { call<void(__fastcall*)(std::uint32_t)>(0x56A8F0)(actor);created=UINT32_MAX; }
    bool adopt(std::uintptr_t child,std::uint32_t actor) noexcept {
        Read read;Weak bound{};std::uint8_t owned{};
        if(!read.value(child+0x290,bound) || bound.handle!=actor || bound.serial==UINT32_MAX
            || !read.value(child+0x29C,owned) || owned!=0) { return false; }
        // Native5904AF sets this same flag after binding. Native58E727 checks it
        // before56A8F0 during child cleanup; no source authority is rewritten.
        *reinterpret_cast<volatile std::uint8_t*>(child+0x29C)=1;nativeOwned=true;return true;
    }
    void visibility(std::uintptr_t child,std::uint32_t actor) noexcept {
        Read read;std::uintptr_t objects{};std::uint32_t stride{};std::uint8_t first{},second{};
        if(!read.value(g_image+0x1F93428,objects) || !read.value(g_image+0x1F93430,stride)
            || !objects || stride<0x48 || stride>0x10000 || !read.value(child+0x1C4,first)
            || !read.value(child+0x1C5,second)) { return; }
        std::uint32_t root=UINT32_MAX;
        call<void*(__fastcall*)(void*,void*)>(0x5589F0)(reinterpret_cast<void*>(objects+(actor&0x1FFFU)*stride),&root);
        if(root==UINT32_MAX) { return; }
        std::array<std::uint32_t,256> nodes{};nodes[0]=root;std::size_t count=1;
        // Same native+40/+44 hierarchy as58C220, with an explicit capacity and
        // cycle guard. Apply the authored child visibility to the whole actor.
        for(std::size_t i=0;i<count;++i) {
            const auto object=objects+(nodes[i]&0x1FFFU)*stride;
            std::uint32_t descendants[2]{};
            if(!read.value(object+0x40,descendants[0]) || !read.value(object+0x44,descendants[1])) { return; }
            for(const auto next:descendants) {
                if(next==UINT32_MAX) { continue; }
                bool seen{};for(std::size_t j=0;j<count;++j) { if(nodes[j]==next) { seen=true;break; } }
                if(seen) { continue; }if(count==nodes.size()) { return; }nodes[count++]=next;
            }
        }
        struct Span { std::uint64_t count;const std::uint32_t* data; } list{count,nodes.data()};
        call<void(__fastcall*)(const Span*,std::uint8_t,std::uint8_t)>(0x1151750)(&list,first,second);
    }
};
bool create_beyond_future_actor_safe(std::uintptr_t child,std::uint32_t self,std::uint32_t extension,std::uint32_t kind) noexcept {
    FutureActorNativeApi api{extension,kind};
    __try { return future_cast::create(api,child,self); }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        // A factory success must not leak if binding faults. Once adopted, the
        // ordinary child cleanup owns the actor even if visibility work faults.
        if(api.created!=UINT32_MAX && !api.nativeOwned) {
            __try { api.destroy(api.created); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }
        return api.nativeOwned;
    }
}
void recover_beyond_future_cast(std::uintptr_t component,const beyond_native::SceneSample& sample,const beyond::SceneReceipt& receipt) noexcept {
    const auto request=beyond::request();
    if(request.owner!=receipt.owner || sample.scope.registry!=0x15FFBE16U || sample.scope.definition!=0x80F4608DU || sample.scope.type!=43 || sample.scope.slot!=0
        || receipt.asset!=sample.scope
        || sample.selector.tag!=0x80EC0901U || sample.selector.kind!=0x80806384U || sample.selector.offset!=0x67F8
        || !request.frame.enabled) { return; }
    Read read;Diag diag{};Weak parent{sample.serial,sample.weak},weak{},after{};std::uintptr_t root{},child{},definition{};
    Ref main{},hold{},header{},parameter{};future_cast::Identity identity{};std::uint32_t instance{};std::int32_t stop{};
    if(!read.weak(parent,root,diag) || !read.value(root+0x23F0,main) || main.handle!=0x80EC0901U
        || main.kind!=0x808062FEU || main.offset!=0x7798 || !read.value(root+0x33D0,hold)
        || hold.handle!=main.handle || hold.kind!=main.kind || hold.offset!=0x7E28
        || !read.value(root+0x2418,identity.parent) || !read.value(root+0x2488,identity.mainState)
        || !read.value(root+0x3468,identity.holdState) || !read.value(root+0x2590,stop) || stop>0
        || !read.value(root+0x25A0,weak) || !read.weak(weak,child,diag)
        || !read.value(child,header) || !read.value(child+0x24,identity.self) || !read.value(child+0x2C,instance)
        || instance==UINT32_MAX || !read.value(child+0x270,parameter) || !read.value(child+0x290,identity.actorSerial)
        || !read.value(child+0x294,identity.actorHandle) || !read.value(child+0x29C,identity.owned)
        || !read.value(child+0x29D,identity.associated)) { return; }
    identity.graph=header.handle;identity.kind=header.kind;identity.definition=static_cast<std::uint64_t>(header.offset);
    identity.parameterGraph=parameter.handle;identity.parameterKind=parameter.kind;identity.parameterDefinition=static_cast<std::uint64_t>(parameter.offset);
    identity.weakHandle=weak.handle;identity.expectedParent=sample.self;
    if(identity.graph!=0x80EC0872U || identity.kind!=0x808084E9U || identity.definition!=0x2648
        || identity.parameterGraph!=identity.graph || identity.parameterKind!=0x808084E1U || identity.parameterDefinition!=0x2950
        || identity.self!=weak.handle || identity.parent!=sample.self || identity.mainState!=1 || identity.holdState!=2) { return; }
    // Observe each native child only once. A valid original actor is preserved;
    // later native removal never becomes a request to create another Osiris.
    static future_cast::FirstObservation visited;static SRWLOCK visitedLock=SRWLOCK_INIT;
    AcquireSRWLockExclusive(&visitedLock);
    const bool firstObservation=visited.take(receipt.owner.run,receipt.owner.value,weak.serial,weak.handle);
    ReleaseSRWLockExclusive(&visitedLock);
    if(!firstObservation) { return; }
    if(!future_cast::eligible(identity) || !read.resolve(parameter,definition)) { return; }
    std::uint32_t name{},fallback{};
    if(!read.value(definition+0x10,name) || name!=future_cast::kParameter || !read.value(definition+0x14,fallback)
        || fallback!=UINT32_MAX || !read.value(root+0x25A0,after) || after.handle!=weak.handle || after.serial!=weak.serial
        || !read.value(component+0x2E8,after) || after.handle!=parent.handle || after.serial!=parent.serial) { return; }
    std::uintptr_t first{},second{};std::uint32_t extension{},kind{};
    if(!read.value(g_image+0x1FA5F80,first) || !read.value(g_image+0x1FA60A0,second)
        || !read.value(first,kind) || !read.value(second,extension) || extension==UINT32_MAX || kind==UINT32_MAX) { return; }
    const bool created=create_beyond_future_actor_safe(child,weak.handle,extension,kind);
    std::array<char,220> message{};
    const int n=std::snprintf(message.data(),message.size(),"ev=beyond_infinity stage=future_cast_fallback run=%llu generation=%u child=%08X created=%u actor_definition=80EC1113 native_owned=%u",
        static_cast<unsigned long long>(receipt.owner.run),receipt.owner.value,weak.handle,static_cast<unsigned>(created),static_cast<unsigned>(created));
    if(n>0 && static_cast<std::size_t>(n)<message.size()) { core::log::write(core::log::Channel::client,core::log::Level::info,{message.data(),static_cast<std::size_t>(n)}); }
}
