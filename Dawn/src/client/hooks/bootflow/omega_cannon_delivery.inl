// Called on the existing mission directive lane. Discover the exact authored
// cannons and charge routes; retain full member references, never native pointers.
std::mutex cannonDeliveryMutex;
omega_cannon_delivery::Track cannonDelivery;

// Called only after exact decoded authority adoption, under the delivery lock.
// At most two records per cycle/epoch: adoption and native entity creation.
void report_eye_transport_source(const omega::mission::Snapshot& snapshot,
    const omega_cannon_delivery::Device& device,const omega_cannon_delivery::Reference& ref,std::byte* component) noexcept {
    if(device.kind==2 || device.transitIndex<0 || !snapshot.chargeDunked || snapshot.eyePlatform
        || snapshot.command.cycle<1 || snapshot.command.cycle>3) return;
    const auto& route=omega::transit::sources[static_cast<std::size_t>(device.transitIndex)];
    if(route.role!=omega::transit::Role::portal || route.cycle!=snapshot.command.cycle
        || !omega::transit::status(snapshot,route).active) return;
    const auto index=snapshot.command.cycle-1;
    std::array<std::byte,8> weak{};std::uint32_t entity=UINT32_MAX;
    if(copy_native(component+0x440,weak.data(),weak.size()))
        native<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(0x352310)(weak.data(),&entity);
    const bool created=entity!=UINT32_MAX && entity==read<std::uint32_t>(weak.data(),4);
    if(cannonDelivery.eyeSourceEpoch[index]==snapshot.command.token.epoch
        && (cannonDelivery.eyeSourceCreated[index] || !created)) return;
    cannonDelivery.eyeSourceEpoch[index]=snapshot.command.token.epoch;
    cannonDelivery.eyeSourceCreated[index]=created;
    log("ev=omega_mission stage=eye_transport_source run=%llu epoch=%u cycle=%u registry=%08X slot=%u source=%08X entity=%08X adopted=1 created=%u receipt=source_only",
        snapshot.command.token.boss.run,snapshot.command.token.epoch,snapshot.command.cycle,
        device.registry,device.slot,ref.member,entity,created?1U:0U);
}

bool mission_source_identity(std::byte* component,std::span<const std::byte> bytes,
    std::size_t reference,std::uint32_t metadata,omega_cannon_delivery::Reference& out) noexcept {
    if(bytes.size()<reference+16 || read<std::uint32_t>(bytes.data(),reference+4)!=metadata) return false;
    out={read<std::uint32_t>(bytes.data(),reference),read<std::int64_t>(bytes.data(),reference+8)};
    if(out.member==UINT32_MAX || out.offset<0 || out.offset>0x2000000) return false;
    auto* datum=resolve_handle(out.member);
    if(!datum || datum+out.offset!=component) return false;
    // Source +24 is not a self handle. 4E5C60 reads the current salted owning
    // datum from the entity record selected by source+20 (live capture 59180).
    std::uint32_t current=UINT32_MAX;
    native<std::uint32_t*(__fastcall*)(const std::byte*,std::uint32_t*) noexcept>(0x4E5C60)(component,&current);
    return current==out.member;
}

bool rescue_cast_adopted(const omega::mission::Snapshot& snapshot,std::uint16_t slot) noexcept {
    const auto* scene=omega::rescue::scene(slot);if(!scene) return false;
    const auto expected=omega_rescue_delivery::expected(snapshot.generation,true);
    const auto body=std::as_bytes(std::span(expected));
    std::array<omega_cannon_delivery::Reference,omega::rescue::sources.size()> references{};
    {const std::lock_guard lock(mutex);
     if(runState.run!=snapshot.command.token.boss.run) return false;
     references=runState.rescueReferences;}
    for(unsigned i=0;i<scene->count;++i) {
        const auto* source=omega::rescue::source(scene->sources[i]);if(!source) return false;
        const auto ref=references[static_cast<std::size_t>(source-omega::rescue::sources.data())];
        auto* base=resolve_handle(ref.member);if(!base || ref.offset<0 || ref.offset>0x2000000) return false;
        auto* component=base+ref.offset;std::array<std::byte,0x690> bytes{};
        omega_cannon_delivery::Reference current;
        if(!copy_native(component,bytes.data(),bytes.size()) || omega_rescue_delivery::source(bytes)!=source
            || !mission_source_identity(component,bytes,0x160,0x80809A3B,current)
            || current.member!=ref.member || current.offset!=ref.offset
            || !omega_lair_delivery::adopted(bytes,body)) return false;
    }
    return true;
}

void discover_cannons(std::uint32_t seed) noexcept {
    namespace delivery=omega_cannon_delivery;
    const std::byte *registry{},*directoryTables{};
    if(seed==UINT32_MAX || !copy_value(image+0x2439C70,registry) || !registry
        || !copy_value(registry,directoryTables) || !directoryTables) return;
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(seed)>>13);
    const auto bucket=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    std::array<std::byte,0x38> table{};
    if(!copy_native(directoryTables+bucket*0x40,table.data(),table.size())) return;
    const auto stride=read<std::int32_t>(table.data(),0x30);
    const auto elements=read<std::uintptr_t>(table.data(),8);
    if(!elements || stride!=0x10 || read<std::int32_t>(table.data(),0x34)!=-1) return;
    // The native handle has a thirteen-bit index. Bound discovery to 128 rows
    // per poll; stop entirely when every scoped source has a current reference.
    for(unsigned n=0;n<128;++n) {
        const auto index=cannonDelivery.cursor++%8192;
        const auto row=elements+index*0x10;
        std::uint64_t relocation{};
        if(!copy_native(reinterpret_cast<const void*>(row+8),&relocation,sizeof relocation)) continue;
        auto* component=reinterpret_cast<std::byte*>(row-static_cast<std::uintptr_t>(relocation));
        std::array<std::byte,0x1A0> bytes{};
        std::uint32_t tag{};
        if(!copy_value(component,tag)) continue;
        for(std::size_t i=0;i<delivery::kDevices.size();++i) {
            const auto& d=delivery::kDevices[i];
            if(d.asset!=tag || cannonDelivery.references[i].member!=UINT32_MAX) continue;
            if(!copy_native(component,bytes.data(),bytes.size()) || !delivery::source(bytes,d)) continue;
            delivery::Reference ref;
            if(mission_source_identity(component,bytes,d.reference,d.metadata,ref)) cannonDelivery.references[i]=ref;
        }
    }
}

void pump_cannon_delivery(const hooking::CallGate::Scope& call) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::cannon_delivery);
    namespace delivery=omega_cannon_delivery;
    if(!call.accepts_side_effects() || !active()) return;
    const auto run=state::activity::mission_run_generation();
    const auto snapshot=omega::mission::runtime::snapshot(run);
    if(!snapshot.generation || snapshot.generation>=0x7FFFFFFFU) return;
    const std::unique_lock lock(cannonDeliveryMutex,std::try_to_lock);
    if(!lock.owns_lock()) return;
    if(cannonDelivery.run!=run || cannonDelivery.generation!=snapshot.generation) {
        cannonDelivery={}; cannonDelivery.run=run; cannonDelivery.generation=snapshot.generation;
    }
    const auto now=GetTickCount64();
    if(now<cannonDelivery.nextPoll) return;
    cannonDelivery.nextPoll=now+100;
    if(std::any_of(cannonDelivery.references.begin(),cannonDelivery.references.end(),
        [](const auto& ref){return ref.member==UINT32_MAX;})) discover_cannons(snapshot.command.token.boss.member);
    for(std::size_t i=0;i<delivery::kDevices.size();++i) {
        const auto& d=delivery::kDevices[i];
        auto& ref=cannonDelivery.references[i];
        if(ref.member==UINT32_MAX) continue;
        auto* datum=resolve_handle(ref.member);
        auto* component=datum?datum+ref.offset:nullptr;
        std::array<std::byte,0x330> before{},after{};
        const std::size_t componentBytes=d.kind==2?0x200:0x330;
        delivery::Reference current;
        if(!copy_native(component,before.data(),componentBytes) || !delivery::source(before,d)
            || !mission_source_identity(component,before,d.reference,d.metadata,current)
            || current.member!=ref.member || current.offset!=ref.offset) {ref={}; continue;}
        const auto handle=read<std::uint32_t>(before.data(),0x170);
        auto* object=resolve_handle(handle);
        std::array<std::byte,0x70> objectBytes{},freshObject{};
        if(!copy_native(object,objectBytes.data(),objectBytes.size())
            || read<std::uint32_t>(objectBytes.data(),0xC)!=d.schema) continue;
        using Resolve=const std::byte*(__fastcall*)(std::byte*,std::uint32_t) noexcept;
        const auto* authority=native<Resolve>(0x9FEC30)(object,d.bodyBytes);
        std::array<std::byte,0x170> body{},fresh{};
        const auto view=std::span<const std::byte>{body}.first(d.bodyBytes);
        if(!copy_native(authority,body.data(),d.bodyBytes) || !delivery::authority(d,snapshot,objectBytes,view)) continue;
        if(delivery::adopted(before,d,view)) {
            report_eye_transport_source(snapshot,d,ref,component);
            if(d.kind==3) mission_scene_sense(component);
            else if(d.kind!=2) observe_mission_device(component);
            continue;
        }
        // Never replay a sent packet or change sync dirty flags. Deliver only
        // current, stable decoded authority to its exact native source binding.
        const auto latest=omega::mission::runtime::snapshot(run);
        if(!call.accepts_side_effects() || state::activity::mission_run_generation()!=run
            || latest.generation!=snapshot.generation || !delivery::authority(d,latest,objectBytes,view)
            || resolve_handle(handle)!=object || !copy_native(object,freshObject.data(),freshObject.size())
            || freshObject!=objectBytes || !copy_native(authority,fresh.data(),d.bodyBytes)
            || fresh!=body || !copy_native(component,after.data(),componentBytes)
            || !delivery::source(after,d) || read<std::uint32_t>(after.data(),0x170)!=handle
            || !mission_source_identity(component,after,d.reference,d.metadata,current)
            || current.member!=ref.member || current.offset!=ref.offset || delivery::adopted(after,d,view)) continue;
        struct Message {std::uint32_t schema,padding;const std::byte* body;};
        const Message message{d.schema,0,body.data()};
        using Apply=void(__fastcall*)(std::byte*,const Message*) noexcept;
        // Type4 traverses the single existing post-apply hook. Gate 10699C0
        // copies the three native channel states; its ordinary tick owns them.
        if(d.kind==3) {
            if(!rescue_cast_adopted(latest,d.slot)) continue;
            // The native Scene reconciler reads its own current decoded body,
            // starts the authored selector, and deduplicates retained events.
            native<void(__fastcall*)(std::byte*) noexcept>(0xB41330)(component);
            mission_scene_sense(component);
        } else native<Apply>(d.kind==2?0x10699C0:0x9F19F0)(component,&message);
        const bool adopted=copy_native(component,after.data(),componentBytes)
            && delivery::source(after,d) && delivery::adopted(after,d,view);
        if(adopted) report_eye_transport_source(snapshot,d,ref,component);
        if(cannonDelivery.reports[i]<8) {
            ++cannonDelivery.reports[i];
            log("ev=omega_mission stage=%s run=%llu registry=%08X index=%u kind=%u slot=%u member=%08X generation=%u adopted=%u receipt=native_apply",
                d.kind==3?"rescue_scene_delivery":d.transitIndex<0?"cannon_delivery":"transit_delivery",run,d.registry,d.cannon,d.kind,d.slot,ref.member,snapshot.generation,adopted?1U:0U);
        }
    }
    if(call.accepts_side_effects() && active()) poll_mission_arc_carry();
}
