// Read-only Eater cranium Carry receipts. The surrounding D99620 hook calls
// its native original once before observe_eater_cranium_carry() runs.
namespace eater_cranium_native {
namespace eater=state::activity::eater_of_worlds;
namespace gn=gateway_native;

struct Slot final {
    eater::CraniumReceipt receipt{};
    std::uint32_t player{UINT32_MAX};
    std::uint8_t mode{};
    bool held{},busy{};
};

struct DeferredDrop final {
    eater::CraniumReceipt receipt{};
    std::size_t index{std::size(eater::kCraniumBindings)};
    std::uint32_t player{UINT32_MAX};
    std::uint8_t mode{};
};

using DeferredDrops=eater_cranium_deferred::Queue<DeferredDrop,4>;

SRWLOCK lock=SRWLOCK_INIT;
std::array<Slot,std::size(eater::kCraniumBindings)> slots{};
std::uint64_t run{};
unsigned lines{};
thread_local DeferredDrops deferredDrops{};

bool same(const eater::CraniumReceipt& a,const eater::CraniumReceipt& b) noexcept {
    return a.generation==b.generation && a.source==b.source && a.sourcePointer==b.sourcePointer
        && a.entity==b.entity && a.serial==b.serial && a.component==b.component;
}

void reset() noexcept {
    AcquireSRWLockExclusive(&lock);
    slots={};run=0;lines=0;
    ReleaseSRWLockExclusive(&lock);
    deferredDrops.reset();
}

bool select_run(std::uint64_t value) noexcept {
    if(!value || eater::native_run()!=value) return false;
    AcquireSRWLockExclusive(&lock);
    if(eater::native_run()!=value) {ReleaseSRWLockExclusive(&lock);return false;}
    if(run!=value) {slots={};run=value;lines=0;}
    ReleaseSRWLockExclusive(&lock);
    return true;
}

bool current(const eater::CraniumReceipt& receipt,std::size_t index,std::uintptr_t expectedComponent) noexcept {
    if(index>=std::size(eater::kCraniumBindings) || receipt.generation.run!=eater::native_run()) return false;
    const auto assetIndex=eater::asset_index(receipt.source);
    if(assetIndex==std::size(eater::kAssets)) return false;
    const auto request=eater::request();
    if(!request.owner.valid() || !request.frame.enabled) return false;
    const auto& desired=request.frame.native[assetIndex];
    if(!desired.managed || !desired.desired || !desired.prepared || !desired.active
        || !desired.acknowledged || desired.generation!=receipt.generation.value) return false;
    gn::Read read{g_image};std::array<std::byte,16> header{};std::uint32_t generation{},committed{};
    std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t component{};
    const auto& binding=eater::kCraniumBindings[index];
    if(binding.source!=receipt.source || !read.copy(receipt.sourcePointer,header)
        || !prefix(header.data(),binding.source.definition,0x80809928U,eater::kAssets[assetIndex].offset)
        || !read.value(receipt.sourcePointer+0x180,generation) || generation!=receipt.generation.value
        || !read.value(receipt.sourcePointer+0x2F0,committed) || committed!=generation
        || !read.value(receipt.sourcePointer+0x188,active) || active!=1
        || !read.value(receipt.sourcePointer+0x440,entity)
        || entity.handle!=receipt.entity || entity.serial!=receipt.serial || !read.weak(entity)
        || !read.resolve(receipt.component,component) || component!=expectedComponent) return false;
    std::array<std::byte,0x30> componentHeader{};
    if(!read.copy(component,componentHeader)
        || !prefix(componentHeader.data(),binding.config,eater::kCraniumCarryKind,binding.offset)
        || at<std::uint32_t>(componentHeader.data()+0x24)!=receipt.component
        || at<std::uint32_t>(componentHeader.data()+0x2C)!=receipt.entity) return false;
    const auto final=eater::request();std::uint32_t finalGeneration{},finalCommitted{};
    return final.owner==request.owner && final.frame.enabled
        && final.frame.native[assetIndex].generation==desired.generation
        && final.frame.native[assetIndex].managed && final.frame.native[assetIndex].desired
        && final.frame.native[assetIndex].prepared && final.frame.native[assetIndex].active
        && final.frame.native[assetIndex].acknowledged
        && read.value(receipt.sourcePointer+0x180,finalGeneration) && finalGeneration==generation
        && read.value(receipt.sourcePointer+0x2F0,finalCommitted) && finalCommitted==committed
        && read.value(receipt.sourcePointer+0x440,again) && again==entity && read.weak(again);
}

bool live_component(const eater::CraniumReceipt& receipt,std::size_t index,
                    std::array<std::byte,0x4A0>& bytes,std::uintptr_t& component) noexcept {
    if(index>=std::size(eater::kCraniumBindings)) return false;
    gn::Read read{g_image};component=0;
    const auto& binding=eater::kCraniumBindings[index];
    return read.resolve(receipt.component,component) && read.copy(component,bytes)
        && prefix(bytes.data(),binding.config,eater::kCraniumCarryKind,binding.offset)
        && at<std::uint32_t>(bytes.data()+0x24)==receipt.component
        && at<std::uint32_t>(bytes.data()+0x2C)==receipt.entity
        && current(receipt,index,component);
}

std::uint32_t local_player() noexcept {
    std::uint32_t player=UINT32_MAX;
    if(!g_controlled || !g_controlled(&player) || player==UINT32_MAX
        || !public_event_deferred_placement_observer::live_entity(player)) return UINT32_MAX;
    return player;
}

bool inventory_holder(std::span<const std::byte> bytes,std::uint32_t player) noexcept {
    if(bytes.size()<0x4A0 || player==UINT32_MAX
        || at<std::uint32_t>(bytes.data()+0x47C)!=0x80804057U
        || at<std::int64_t>(bytes.data()+0x480)!=0x4C8
        || at<std::uint32_t>(bytes.data()+0x494)!=0x80803E63U) return false;
    const auto owner=at<std::uint32_t>(bytes.data()+0x490);
    const auto offset=at<std::int64_t>(bytes.data()+0x498);
    if(owner==UINT32_MAX || offset<0 || offset>0x2000000) return false;
    gn::Read read{g_image};std::uintptr_t base{},resolved{};std::uint32_t ownerIdentity{};
    if(!read.resolve(owner,base) || !read.value(base+0x24,ownerIdentity) || ownerIdentity!=owner
        || base>UINTPTR_MAX-static_cast<std::uintptr_t>(offset)-0x30) return false;
    const auto inventory=base+static_cast<std::uintptr_t>(offset);
    std::array<std::byte,0x30> prefixBytes{};
    if(!read.copy(inventory,prefixBytes)
        || !prefix(prefixBytes.data(),at<std::uint32_t>(bytes.data()+0x478),0x80803E64U,0x468)
        || at<std::uint32_t>(prefixBytes.data()+0x2C)!=player) return false;
    const auto self=at<std::uint32_t>(prefixBytes.data()+0x24);
    return self!=UINT32_MAX && read.resolve(self,resolved) && resolved==inventory;
}

bool holder(void* component,const void* context,std::span<const std::byte> bytes,
            std::uint8_t mode,std::uint32_t& player) noexcept {
    player=local_player();if(player==UINT32_MAX) return false;
    const auto contextual=public_event_deferred_placement_observer::holder_context(context);
    if(mode==3) {
        std::uint32_t attached=UINT32_MAX;
        if(!g_holder || !g_holder(component,&attached) || attached!=player) return false;
        return contextual==UINT32_MAX || contextual==player;
    }
    if(mode!=1 || !inventory_holder(bytes,player)) return false;
    return contextual==UINT32_MAX || contextual==player;
}

} // namespace eater_cranium_native

void reset_eater_cranium_receipts() noexcept {eater_cranium_native::reset();}

void observe_eater_cranium_source(void* raw,std::size_t assetIndex) noexcept {
    namespace native=eater_cranium_native;namespace eater=native::eater;namespace gn=native::gn;
    const auto activeRun=eater::native_run();
    if(!activeRun || assetIndex>=std::size(eater::kAssets)) return;
    const auto index=eater::cranium_index(eater::kAssets[assetIndex].asset);
    if(index==std::size(eater::kCraniumBindings) || !native::select_run(activeRun)) return;
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{g_image};
    const auto& binding=eater::kCraniumBindings[index];
    std::array<std::byte,16> header{};
    if(!read.copy(source,header)
        || binding.source!=eater::kAssets[assetIndex].asset
        || !prefix(header.data(),binding.source.definition,0x80809928U,
                   eater::kAssets[assetIndex].offset)) return;
    const auto request=eater::request();const auto& desired=request.frame.native[assetIndex];
    if(!request.owner.valid() || !request.frame.enabled || !desired.managed || !desired.desired
        || !desired.prepared || !desired.active || !desired.acknowledged) return;
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};
    std::uintptr_t row{},component{},resolved{};
    if(!read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)
        || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !coo_native::component<gn::Read,1024>(read,bundle,entity.handle,eater::kCraniumCarryKind,component)) return;
    std::array<std::byte,0x4A0> bytes{};
    if(!read.copy(component,bytes)
        || !prefix(bytes.data(),binding.config,eater::kCraniumCarryKind,binding.offset)
        || at<std::uint32_t>(bytes.data()+0x2C)!=entity.handle) return;
    const auto self=at<std::uint32_t>(bytes.data()+0x24);
    if(self==UINT32_MAX || !read.resolve(self,resolved) || resolved!=component
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again)) return;
    const eater::CraniumReceipt receipt{{activeRun,generation},binding.source,source,
        entity.handle,entity.serial,self};
    if(!native::current(receipt,index,component)) return;
    AcquireSRWLockExclusive(&native::lock);
    if(native::run==activeRun && !native::same(native::slots[index].receipt,receipt))
        native::slots[index]={receipt,UINT32_MAX,0,false,false};
    ReleaseSRWLockExclusive(&native::lock);
}

void observe_eater_cranium_carry(void* component,const void* holderContext) noexcept {
    namespace native=eater_cranium_native;namespace eater=native::eater;
    const auto activeRun=eater::native_run();if(!native::select_run(activeRun)) return;
    std::array<std::byte,0x4A0> bytes{};if(!copy(component,bytes)) return;
    const auto self=at<std::uint32_t>(bytes.data()+0x24);
    const auto entity=at<std::uint32_t>(bytes.data()+0x2C);
    if(self==UINT32_MAX || entity==UINT32_MAX) return;
    std::size_t index=std::size(eater::kCraniumBindings);native::Slot prior{};unsigned matches{};
    AcquireSRWLockShared(&native::lock);
    for(std::size_t i=0;i<std::size(native::slots);++i) if(native::slots[i].receipt.component==self
        && native::slots[i].receipt.entity==entity) {index=i;prior=native::slots[i];++matches;}
    ReleaseSRWLockShared(&native::lock);
    if(matches!=1 || index==std::size(eater::kCraniumBindings) || prior.busy
        || !prefix(bytes.data(),eater::kCraniumBindings[index].config,
                   eater::kCraniumCarryKind,eater::kCraniumBindings[index].offset)
        || !native::current(prior.receipt,index,reinterpret_cast<std::uintptr_t>(component))) return;
    const auto mode=at<std::uint8_t>(bytes.data()+0x470);
    const bool held=mode==1 || mode==3;std::uint32_t player=prior.player;
    if(held && !native::holder(component,holderContext,bytes,mode,player)) return;
    AcquireSRWLockExclusive(&native::lock);
    auto& slot=native::slots[index];
    if(native::run!=activeRun || slot.busy || !native::same(slot.receipt,prior.receipt)
        || (held && slot.held && slot.player!=player) || (!held && !slot.held)) {
        ReleaseSRWLockExclusive(&native::lock);return;
    }
    if(held) {
        native::deferredDrops.cancel([&](const native::DeferredDrop& drop) noexcept {
            return native::same(drop.receipt,slot.receipt);
        });
        if(slot.held) {slot.mode=mode;ReleaseSRWLockExclusive(&native::lock);return;}
    }
    if(!held && native::deferredDrops.active()) {
        native::deferredDrops.cancel([&](const native::DeferredDrop& drop) noexcept {
            return native::same(drop.receipt,slot.receipt);
        });
        const bool queued=native::deferredDrops.defer({slot.receipt,index,slot.player,mode});
        if(queued) slot.mode=mode;
        ReleaseSRWLockExclusive(&native::lock);return;
    }
    player=held?player:slot.player;slot.busy=true;
    ReleaseSRWLockExclusive(&native::lock);
    const bool stillCurrent=native::current(prior.receipt,index,reinterpret_cast<std::uintptr_t>(component));
    const bool accepted=stillCurrent && eater::observe_cranium(prior.receipt,held,player);
    AcquireSRWLockExclusive(&native::lock);
    auto& final=native::slots[index];
    if(final.busy && native::same(final.receipt,prior.receipt)) {
        final.busy=false;final.mode=mode;
        if(held && accepted) {final.held=true;final.player=player;}
        else if(!held && stillCurrent) {final.held=false;final.player=UINT32_MAX;}
    }
    const bool reportLine=accepted && native::lines++<96;
    ReleaseSRWLockExclusive(&native::lock);
    if(reportLine) report("ev=eater_of_worlds stage=cranium_%s run=%llu registry=%08X slot=%u source=%08X generation=%u entity=%08X component=%08X player=%08X state=%u mutation=observe_only",
        held?"carried":"dropped",static_cast<unsigned long long>(prior.receipt.generation.run),
        prior.receipt.source.registry,static_cast<unsigned>(prior.receipt.source.slot),
        prior.receipt.source.definition,prior.receipt.generation.value,prior.receipt.entity,
        prior.receipt.component,player,static_cast<unsigned>(mode));
}

bool current_eater_cranium_holder(std::uint32_t registry,std::uint32_t player,
                                  eater_cranium_native::eater::CraniumReceipt& receipt) noexcept {
    namespace native=eater_cranium_native;namespace eater=native::eater;
    const auto activeRun=eater::native_run();
    if(!native::select_run(activeRun) || native::local_player()!=player) return false;
    std::array<native::Slot,std::size(native::slots)> snapshot{};
    AcquireSRWLockShared(&native::lock);snapshot=native::slots;ReleaseSRWLockShared(&native::lock);
    std::size_t found=std::size(snapshot);unsigned matches{};
    for(std::size_t i=0;i<std::size(snapshot);++i) {
        const auto& slot=snapshot[i];
        if(!slot.held || slot.busy || slot.player!=player || slot.receipt.source.registry!=registry) continue;
        std::array<std::byte,0x4A0> bytes{};std::uintptr_t component{};
        if(!native::live_component(slot.receipt,i,bytes,component)) continue;
        const auto mode=at<std::uint8_t>(bytes.data()+0x470);std::uint32_t holder=UINT32_MAX;
        if((mode!=1 && mode!=3) || !native::holder(reinterpret_cast<void*>(component),nullptr,
                                                   bytes,mode,holder) || holder!=player) continue;
        found=i;receipt=slot.receipt;++matches;
    }
    if(matches!=1 || found>=std::size(snapshot)) return false;
    unsigned cached{};bool exact{};
    AcquireSRWLockShared(&native::lock);
    for(const auto& slot:native::slots) if(slot.held && slot.player==player
        && slot.receipt.source.registry==registry) {
        ++cached;
        if(!slot.busy && native::same(slot.receipt,receipt)) exact=true;
    }
    ReleaseSRWLockShared(&native::lock);
    return cached==1 && exact;
}

eater_cranium_native::DeferredDrops::Scope begin_eater_cranium_station_use(bool active) noexcept {
    return eater_cranium_native::deferredDrops.begin(active);
}

void finish_eater_cranium_station_use(eater_cranium_native::DeferredDrops::Scope scope) noexcept {
    namespace native=eater_cranium_native;namespace eater=native::eater;
    native::deferredDrops.finish(scope,[](const native::DeferredDrop& drop) noexcept {
        if(drop.index>=std::size(native::slots) || drop.player==UINT32_MAX) return;
        std::array<std::byte,0x4A0> before{};std::uintptr_t component{};
        if(!native::live_component(drop.receipt,drop.index,before,component)) return;
        auto mode=at<std::uint8_t>(before.data()+0x470);
        if(mode==1 || mode==3) return;
        AcquireSRWLockExclusive(&native::lock);
        auto& slot=native::slots[drop.index];
        if(slot.busy || !slot.held || slot.player!=drop.player
            || !native::same(slot.receipt,drop.receipt)) {
            ReleaseSRWLockExclusive(&native::lock);return;
        }
        slot.busy=true;ReleaseSRWLockExclusive(&native::lock);
        std::array<std::byte,0x4A0> after{};std::uintptr_t stableComponent{};
        const bool current=native::live_component(drop.receipt,drop.index,after,stableComponent)
            && stableComponent==component;
        if(current) mode=at<std::uint8_t>(after.data()+0x470);
        const bool stillDropped=current && mode!=1 && mode!=3;
        const bool accepted=stillDropped && eater::observe_cranium(drop.receipt,false,drop.player);
        AcquireSRWLockExclusive(&native::lock);
        auto& final=native::slots[drop.index];
        if(final.busy && native::same(final.receipt,drop.receipt)) {
            final.busy=false;
            if(stillDropped) {final.held=false;final.mode=mode;final.player=UINT32_MAX;}
        }
        const bool reportLine=accepted && native::lines++<96;
        ReleaseSRWLockExclusive(&native::lock);
        if(reportLine) report("ev=eater_of_worlds stage=cranium_dropped run=%llu registry=%08X slot=%u source=%08X generation=%u entity=%08X component=%08X player=%08X state=%u mutation=observe_only",
            static_cast<unsigned long long>(drop.receipt.generation.run),drop.receipt.source.registry,
            static_cast<unsigned>(drop.receipt.source.slot),drop.receipt.source.definition,
            drop.receipt.generation.value,drop.receipt.entity,drop.receipt.component,drop.player,
            static_cast<unsigned>(mode));
    });
}
