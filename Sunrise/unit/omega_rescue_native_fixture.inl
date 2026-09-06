// Execute original B41330 reconciliation against captured Scene storage.
// Selector creation and graph event consumption are modeled boundaries.
unsigned rescueStarts{},rescueEvents{};
constexpr std::uint32_t rescueSelector=0x33F92005;
const std::byte* rescue_current(std::byte* component) noexcept {
    return cannon_authority(resolve_handle(read<std::uint32_t>(component,0x170)),0xD4);
}
void rescue_start(std::byte* component,std::uint32_t generation) noexcept {
    ++rescueStarts;put(component,0x254,generation);put(component,0x258,std::uint8_t{});
    put(component,0x2E8,1U);put(component,0x2EC,rescueSelector);
}
std::uint32_t* rescue_weak(const void* weak,std::uint32_t* result) noexcept {
    *result=read<std::uint32_t>(static_cast<const std::byte*>(weak),4);return result;
}
void rescue_event(std::byte* selector,const std::uint32_t* event) noexcept {
    check(selector==resolve_handle(rescueSelector) && *event==0x1E9C04B7,"exact Scene release routed to native selector");++rescueEvents;
}
void rescue_delivery_test(const char* nativeImage) {
    namespace d=omega_cannon_delivery;namespace m=omega::mission;
    reach_mission(m::Phase::deletion,1);
    auto snapshot=m::runtime::state.snapshot();
    check(m::runtime::state.claim(snapshot.command.token,m::Action::deletion),"rescue fixture claims deletion");
    check(m::runtime::state.animation(snapshot.command.token,m::Animation::started),"deletion requests Osiris Scene");
    snapshot=m::runtime::state.snapshot();
    const auto it=std::find_if(d::kDevices.begin(),d::kDevices.end(),[](const auto& row){return row.kind==3 && row.slot==9;});
    check(it!=d::kDevices.end(),"Scene participates in production discovery");const auto& device=*it;
    const auto owner=runState.graphOwner;
    const auto handle=(owner.member&~0x1FFFU)|120U;
    auto* component=bind(handle,0x900);auto* object=bind(0x31F260FF,0x70);auto* body=allocate(0xD4);
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_rescue";
    copy_file((folder/"80F479BF-369.bin").string().c_str(),component,0x900);
    copy_file((folder/"80F479BF-369-object.bin").string().c_str(),object,0x70);
    copy_file((folder/"80F479BF-369-authority.bin").string().c_str(),body,0xD4);
    put(component,0x160,handle);put(component,0x170,0x31F260FFU);put(object,0x40,body);
    put(body,0,snapshot.generation);put(body,0x4C,snapshot.generation);
    fixtureDeviceMembers.emplace_back(component,handle);bind(rescueSelector,0x100);
    DWORD old{};
    for(auto page:{0xB41000U,0xB43000U,0xB3C000U,0x4E7000U,0xDAC000U,0x352000U})
        check(VirtualProtect(image+page,0x1000,PAGE_READWRITE,&old)!=0,"map private Scene handler");
    copy_file(nativeImage,image+0xB41330,0x1AF,0xB41330);
    fixture_thunk(0x4E7F70,reinterpret_cast<void*>(&cannon_authoritative));
    fixture_thunk(0xB3C050,reinterpret_cast<void*>(&rescue_current));
    fixture_thunk(0xB43220,reinterpret_cast<void*>(&rescue_start));
    fixture_thunk(0x352310,reinterpret_cast<void*>(&rescue_weak));
    fixture_thunk(0xDAC460,reinterpret_cast<void*>(&rescue_event));
    for(auto page:{0xB41000U,0xB43000U,0xB3C000U,0x4E7000U,0xDAC000U,0x352000U})
        check(VirtualProtect(image+page,0x1000,PAGE_EXECUTE_READ,&old)!=0,"protect original Scene handler");
    FlushInstructionCache(GetCurrentProcess(),image,0x6270000);
    cannonDelivery={};const hooking::CallGate::Scope call(callGate);
    auto pump=[&]{cannonDelivery.nextPoll=0;pump_cannon_delivery(call);};
    put(object,0x18,std::uint8_t{1});pump();check(rescueStarts==0,"pending Scene object never starts");
    put(object,0x18,std::uint8_t{0});cannonDelivery.cursor=0;pump();
    check(rescueStarts==0,"Scene waits for all authored cast sources to adopt");
    for(unsigned i=0;i<omega::rescue::scene(9)->count;++i) {
        const auto slot=omega::rescue::scene(9)->sources[i];const auto* row=omega::rescue::source(slot);
        const auto member=(owner.member&~0x1FFFU)|(200U+i);
        auto* npc=bind(member,0x900);
        copy_file((folder/"80F4799D-360.bin").string().c_str(),npc,0x900);
        put(npc,0,row->definition);put(npc,0x5DE,slot);put(npc,0x160,member);put(npc,0x168,std::int64_t{});
        const auto expected=omega_rescue_delivery::expected(snapshot.generation,true);
        std::memcpy(npc+0x180,expected.data(),0xC4);
        fixtureDeviceMembers.emplace_back(npc,member);
        runState.rescueReferences[static_cast<std::size_t>(row-omega::rescue::sources.data())]={member,0};
        if(i+1<omega::rescue::scene(9)->count) {pump();check(rescueStarts==0,"partial cast cannot start Scene");}
    }
    check(rescue_cast_adopted(snapshot,9),"all live cast authority adopted");
    const auto firstRef=runState.rescueReferences[0];auto* first=resolve_handle(firstRef.member)+firstRef.offset;
    put(first,0x180+0x7C,snapshot.generation+1);pump();check(rescueStarts==0,"stale cast generation cannot start Scene");
    put(first,0x180+0x7C,snapshot.generation);
    put(first,0x160,firstRef.member^0x2000U);pump();check(rescueStarts==0,"recycled cast identity cannot start Scene");
    put(first,0x160,firstRef.member);
    pump();check(rescueStarts==1 && d::adopted({component,0x330},device,{body,0xD4}),"original native reconciliation adopts captured Scene and starts selector once");
    pump();pump();check(rescueStarts==1,"stable Scene authority does not restart animation");
    check(m::runtime::state.animation(snapshot.command.token,m::Animation::deletionHold),"deletion reaches hold");
    check(m::runtime::state.rescue_ready(snapshot.command.token,9),"modeled selector readiness opens route");
    snapshot=m::runtime::state.snapshot();
    const m::ChargeReceipt charge{snapshot.command.token,0x122,8,0x233,0x12,0x344,0x0040BF06,18,20};
    check(m::runtime::state.pickup(charge) && m::runtime::state.dunk(charge),"qualified charge releases Scene");
    put(body,0x50,1U);put(body,0x54,0x1E9C04B7U);pump();pump();
    check(rescueEvents==1 && rescueStarts==1,"original native reconciliation delivers retained release once without restarting cast");
}
