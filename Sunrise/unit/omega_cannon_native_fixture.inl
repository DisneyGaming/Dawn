// Actual 9F19F0 / 10699C0 instructions, captured device layouts and production
// discovery/delivery. Entity factories, weak references and deferred scheduling
// are modeled; this is not a physical launch/contact simulation.
unsigned cannonFactories{},cannonSchedules{},cannonDeferred{};
const std::byte* cannon_key(const std::byte* key,std::uint32_t schema) noexcept {
    check(read<std::uint32_t>(key,0)==schema,"native handler receives its actual schema wrapper");
    return read<const std::byte*>(key,8);
}
std::uint32_t* cannon_weak(const void*,std::uint32_t* result) noexcept {*result=UINT32_MAX;return result;}
void cannon_factory(std::byte*) noexcept {++cannonFactories;}
void cannon_schedule(std::byte*,const void*,void*) noexcept {++cannonSchedules;}
void cannon_defer(std::byte*,std::uint32_t) noexcept {++cannonDeferred;}
bool cannon_authoritative(std::byte*) noexcept {return true;}
void cannon_delivery_test(const char* nativeImage) {
    namespace delivery=omega_cannon_delivery;
    namespace m=omega::mission;
    const auto owner=runState.graphOwner;
    auto bindMission=[&](std::uint32_t generation) {
        m::runtime::state={};
        check(m::runtime::state.bind({owner.run,generation,owner.actor,owner.character,owner.biped,
            owner.entity,owner.member,owner.revision}),"cannon fixture binds mission");
    };
    bindMission(owner.generation);
    std::array<std::byte*,12> components{},objects{},bodies{};
    std::array<std::uint32_t,12> handles{};
    for(std::size_t i=0;i<12;++i) {
        const auto& d=delivery::kDevices[i];
        handles[i]=(owner.member&~0x1FFFU)|static_cast<std::uint32_t>(80+i);
        components[i]=bind(handles[i],0x500);
        objects[i]=bind(0x20F260D0U+static_cast<std::uint32_t>(i),0x80);
        bodies[i]=allocate(0x170);
        char name[64]{};std::snprintf(name,sizeof name,"device-%08X",d.asset);
        const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures"/"omega_cannons";
        copy_file((folder/(std::string(name)+".bin")).string().c_str(),components[i],0x480);
        copy_file((folder/(std::string(name)+"-object.bin")).string().c_str(),objects[i],0x70);
        // Capture retained 150 bytes; remaining native 20-byte tail is modeled
        // from initialized source storage. Original handler copies all 170.
        if(d.kind!=2) std::memcpy(bodies[i]+0x150,components[i]+0x2D0,0x20);
        copy_file((folder/(std::string(name)+"-authority.bin")).string().c_str(),bodies[i],d.kind==2?0x18:0x150);
        put(components[i],d.reference,handles[i]);
        put(components[i],0x170,0x20F260D0U+static_cast<std::uint32_t>(i));
        put(objects[i],0x40,bodies[i]);
        fixtureDeviceMembers.emplace_back(components[i],handles[i]);
        check(delivery::source({components[i],0x330},d),"captured core/FX/gate layout accepted");
        if(d.kind!=2) {
            check(!delivery::authority(d,m::runtime::state.snapshot(),{objects[i],0x70},{bodies[i],d.bodyBytes}),
                "captured generation2 authority cannot prepare generation8 mission");
            put(bodies[i],0,owner.generation);
            auto* definition=bind(d.asset,0x600);
            put(definition,0x4C8+0x94,static_cast<std::uint8_t>(d.kind==1));
        }
        check(delivery::authority(d,m::runtime::state.snapshot(),{objects[i],0x70},{bodies[i],d.bodyBytes}),
            "captured current exact decoded authority accepted");
    }
    DWORD old{};
    for(auto page:{0x9F1000U,0x1069000U,0x4A6000U,0x352000U,0x9EF000U,0x4E8000U})
        check(VirtualProtect(image+page,0x1000,PAGE_READWRITE,&old)!=0,"prepare private native cannon instructions");
    copy_file(nativeImage,image+0x9F19F0,0x2F3,0x9F19F0);
    copy_file(nativeImage,image+0x10699C0,0x4A,0x10699C0);
    fixture_thunk(0x4A6340,reinterpret_cast<void*>(&cannon_key));
    fixture_thunk(0x352310,reinterpret_cast<void*>(&cannon_weak));
    fixture_thunk(0x9EFFC0,reinterpret_cast<void*>(&cannon_factory));
    fixture_thunk(0x9EF680,reinterpret_cast<void*>(&cannon_schedule));
    fixture_thunk(0x9EFAE0,reinterpret_cast<void*>(&cannon_defer));
    fixture_thunk(0x4E8060,reinterpret_cast<void*>(&cannon_authoritative));
    for(const auto pair:std::array<std::pair<std::size_t,std::uint32_t>,2>{{{0x9F1A11,0x8080992F},{0x10699DC,0x80804F48}}}) {
        const auto displacement=read<std::int32_t>(image,pair.first-4);
        auto* schema=allocate(4);put(schema,0,pair.second);
        put(image,pair.first+displacement,schema);
    }
    for(auto page:{0x9F1000U,0x1069000U,0x4A6000U,0x352000U,0x9EF000U,0x4E8000U})
        check(VirtualProtect(image+page,0x1000,PAGE_EXECUTE_READ,&old)!=0,"execute original cannon handlers");
    FlushInstructionCache(GetCurrentProcess(),image,0x6270000);
    g_portalApplyOriginal.store(reinterpret_cast<DeviceApply>(image+0x9F19F0));
    cannonDelivery={};
    const hooking::CallGate::Scope call(callGate);
    auto pump=[&] {cannonDelivery.nextPoll=0;pump_cannon_delivery(call);};
    // Reject stale source identity despite a correct prefix and authority body.
    fixtureDeviceMembers.back().second^=0x10000000;
    put(objects[0],0x18,std::uint8_t{1});
    pump();
    check(m::runtime::state.snapshot().cannonPrepared==14,"pending first core cannot forge readiness");
    check(cannonDelivery.references[11].member==UINT32_MAX,"recycled source rejected at discovery");
    fixtureDeviceMembers.back().second^=0x10000000;
    cannonDelivery.cursor=0;
    put(objects[0],0x18,std::uint8_t{0});
    pump();
    check(m::runtime::state.snapshot().cannonPrepared==15,"all four original native post-applies acknowledge preparation");
    check(m::runtime::state.snapshot().cannons==0,"preparation alone cannot enable launchers");
    for(std::size_t i=0;i<12;++i) check(delivery::adopted({components[i],0x330},delivery::kDevices[i],{bodies[i],delivery::kDevices[i].bodyBytes}),
        "native handlers adopt all twelve captured device bodies");
    const auto preparedSchedules=cannonSchedules;
    pump();pump();check(cannonSchedules==preparedSchedules && cannonFactories==0,"stable preparation never replays native apply or creates cores");
    const auto previousArcPolls=arcPolls;
    cannonDelivery.nextPoll=GetTickCount64()+10000;
    pump_cannon_delivery(call);
    check(arcPolls==previousArcPolls,"carry reconciliation shares device poll cadence");
    pump();check(arcPolls==previousArcPolls+1,"one carry reconciliation per eligible device poll");
    auto publish=[&] {
        const auto snapshot=m::runtime::state.snapshot();
        for(std::size_t i=0;i<12;++i) {
            const auto& d=delivery::kDevices[i];const bool enabled=(snapshot.cannons&(1U<<d.cannon))!=0;
            if(d.kind!=2) {put(bodies[i],0,snapshot.generation+(d.kind==1&&enabled?1U:0U));put(bodies[i],8,static_cast<std::uint8_t>(enabled));}
            else {put(bodies[i],0,enabled?1.F:0.F);put(bodies[i],4,static_cast<std::int16_t>(enabled?1:0));}
        }
    };
    reach_mission(m::Phase::departure,0,0);
    auto snapshot=m::runtime::state.snapshot();
    check(m::runtime::state.claim(snapshot.command.token,m::Action::depart),"claim departure command");
    check(m::runtime::state.animation(snapshot.command.token,m::Animation::departureFinished),"first departure authorizes three cannons");
    publish();
    put(objects[1],6,std::uint16_t{99}); // wrong FX slot must not reach factory
    pump();
    check(read<std::uint8_t>(components[1],0x188)==0,"wrong slot blocks FX delivery");
    put(objects[1],6,delivery::kDevices[1].slot);pump();
    for(std::size_t i=0;i<12;++i) {
        const auto& d=delivery::kDevices[i];
        check(delivery::adopted({components[i],0x330},d,{bodies[i],d.bodyBytes}),"first departure delivers cores FX and gate channels");
        if(d.kind!=2) check(read<std::uint8_t>(components[i],0x188)==(d.cannon<3),"fourth launcher remains inactive during first chase");
    }
    check(cannonFactories==3,"original active core branch requests exactly three native factories");
    const auto activeSchedules=cannonSchedules;const auto deferred=cannonDeferred;
    pump();pump();check(cannonSchedules==activeSchedules && cannonDeferred==deferred && cannonFactories==3,"stable activation is not replayed");
    reach_mission(m::Phase::departure,0,3);snapshot=m::runtime::state.snapshot();
    check(m::runtime::state.claim(snapshot.command.token,m::Action::depart),"claim departure command");
    check(m::runtime::state.animation(snapshot.command.token,m::Animation::departureFinished),"final chase departure authorizes fourth cannon");
    publish();pump();check(cannonFactories==4 && read<std::uint8_t>(components[9],0x188)==1,"fourth core activates only at final milestone");
    bindMission(owner.generation+1);publish();
    callGate.quiesce();pump();check(read<std::uint8_t>(components[9],0x188)==1,"quiesced bridge cannot mutate native state");callGate.accept();
    pump();check(m::runtime::state.snapshot().cannonPrepared==15,"new generation discards cached progress and prepares again");
    check(read<std::uint8_t>(components[9],0x188)==0,"new generation clears old fourth-cannon active state");
    for(std::size_t i=0;i<12;++i) check(delivery::adopted({components[i],0x330},delivery::kDevices[i],{bodies[i],delivery::kDevices[i].bodyBytes}),"reset adopts current inactive bodies");
}
