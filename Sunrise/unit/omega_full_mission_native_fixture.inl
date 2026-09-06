// Production callbacks with private, serial-qualified allocations. Native queue,
// motion stepping and named-channel submission are modeled boundaries; native
// event-table add/remove instructions and the production owner resolver execute.
void reach_mission(omega::mission::Phase phase,unsigned cycle,unsigned island=255) {
    namespace m=omega::mission;
    const auto& owner=runState.graphOwner;
    auto& state=m::runtime::state;state={};
    check(state.bind({owner.run,owner.generation,owner.actor,owner.character,owner.biped,owner.entity,owner.member,owner.revision}),"full fixture binds native boss");
    for(std::uint8_t i=0;i<4;++i) check(state.prepared(7,8,i,false),"prepare chase");
    for(std::uint8_t i=0;i<7;++i) check(state.prepared(7,8,i,true),"prepare Crown");
    std::uint32_t identity=0x12341000;
    for(unsigned step=0;step<100;++step) {
        const auto s=state.snapshot();const auto token=s.command.token;
        if(s.phase==m::Phase::shield && !s.eyePlatform)
            check(state.route_arrival(token,true,0x12),"fixture post-dunk receiving-platform receipt");
        if(s.phase==phase && s.command.cycle==cycle && (island==255 || s.command.island==island)) return;
        if(!s.command.claimed) check(state.claim(token,s.command.action),"fixture action claim");
        if(s.phase==m::Phase::summon) {
            check(state.animation(token,m::Animation::started),"fixture native summon receipt");
            for(const auto& row:m::kSources) if(row.wave==s.command.wave) for(std::uint8_t category=0;category<row.categories;++category)
                for(unsigned i=0;i<row.requested[category];++i) {
                    const auto actor=++identity;
                    const m::ActorReceipt receipt{7,8,row.registry,0x34561000U+row.slot,actor,actor+0x10000000U,actor+0x20000000U,actor+0x30000000U,row.slot,category};
                    check(state.admit(receipt),"fixture exact native actor admitted");
                    if(row.wave!=11) check(state.death(receipt),"fixture native death receipt");
                }
            check(state.animation(token,m::Animation::finished),"fixture summon finish");
        } else if(s.phase==m::Phase::departure) {
            check(state.animation(token,m::Animation::departureFinished),"fixture departure finish");
            check(state.arrival(token,static_cast<std::uint8_t>(s.command.island+1),0x12),"fixture native player arrival");
        } else if(s.phase==m::Phase::deletion) {
            check(state.animation(token,m::Animation::started),"fixture deletion starts");
            check(state.animation(token,m::Animation::deletionHold),"fixture deletion holds");
            check(state.rescue_ready(token,s.command.cycle==1?9:s.command.cycle==2?27:46),"fixture native Scene ready");
        } else if(s.phase==m::Phase::route) {
            const std::array<std::uint32_t,3> registries{0x0040BF06,0x0040BF05,0x0040BF03};
            const std::array<std::uint16_t,3> sources{18,1,0},sinks{20,3,2};
            const m::ChargeReceipt charge{token,0x122,8,0x233,0x12,0x344,registries[s.command.cycle-1],sources[s.command.cycle-1],sinks[s.command.cycle-1]};
            check(state.pickup(charge) && state.dunk(charge),"fixture native charge transaction");
        } else if(s.phase==m::Phase::shield) {
            check(state.animation(token,m::Animation::eyeExposed),"fixture native eye exposed");
            check(state.health(token,m::Health::eyeCrossed),"fixture downward crossing");
        } else if(s.phase==m::Phase::recovery) {
            check(state.health(token,m::Health::bodyCheckpoint),"fixture native body checkpoint");
            check(state.animation(token,m::Animation::finished),"fixture recovery finished");
        } else check(false,"unexpected fixture mission phase");
    }
    check(false,"bounded mission fixture did not reach requested stage");
}
void full_mission_native_test() {
    namespace m=omega::mission;
    missionNativeFixture=true;
    DWORD keyProtection{};
    for(auto rva:{0x6260000U,0x584D000U}) check(VirtualProtect(image+rva,0x1000,PAGE_EXECUTE_READWRITE,&keyProtection)!=0,"model process encoding keys");
    fixture_thunk(0x6260781,reinterpret_cast<void*>(&world_key_xy));
    fixture_thunk(0x584DFF2,reinterpret_cast<void*>(&world_key_zw));
    for(auto rva:{0x6260000U,0x584D000U}) check(VirtualProtect(image+rva,0x1000,PAGE_EXECUTE_READ,&keyProtection)!=0,"protect modeled key boundaries");
    FlushInstructionCache(GetCurrentProcess(),image,0x6270000);
    const auto owner=runState.graphOwner;
    constexpr std::uint32_t arenaHandle=0x08F9EA23,movementHandle=0x6AF9EB72,selectorHandle=0x5DF9EA3B,physicsHandle=0x66F9EB6D,healthHandle=0x41F9EA70;
    auto* arena=bind(arenaHandle,0x1800);auto* movement=bind(movementHandle,0x100);
    auto* selector=bind(selectorHandle,0x100);auto* physics=bind(physicsHandle,0x240);
    source(arena,0x815B5A43,0x808069EE,0x1010);put(arena,0x24,arenaHandle);put(arena,0x2C,owner.entity);put(arena,0x170,owner.entity);put(arena,0x17C,movementHandle);
    put(arena,0x50,UINT64_C(3));put(arena,0x60,UINT64_C(3));put(arena,0x70,UINT64_C(0x3F0));
    put(arena,0x58,INT64_C(0x400-0x68));put(arena,0x68,INT64_C(0x440-0x78));put(arena,0x78,INT64_C(0x500-0x88));
    put(arena,0x1E4,std::uint32_t{0x80F83641});put(arena,0x1E8,std::uint32_t{0x80F83881});put(arena,0x2A2,std::uint8_t{1});
    const auto empty=[&] {for(unsigned i=0;i<3;++i) {put(arena,0x400+i*4,UINT16_MAX);put(arena,0x402+i*4,std::uint8_t{255});put(arena,0x444+i*6,UINT16_MAX);}put(arena,0x80,std::uint32_t{0});};empty();
    source(movement,0x80F4516E,0x80803A00,0xE78);put(movement,0x24,movementHandle);put(movement,0x2C,owner.entity);
    source(selector,0x80F45176,0x80806751,0x188);put(selector,0x24,selectorHandle);put(selector,0x2C,owner.entity);put(selector,0x30,owner.character);put(selector,0x78,arenaHandle);
    put(characterBody,0x428,arenaHandle);put(characterBody,0x588,INT64_C(0));
    put(characterBody,0x618,std::uint32_t{0x80FEE862});put(characterBody,0x620,selectorHandle);put(characterBody,0x624,std::uint32_t{0x80806750});put(characterBody,0x628,INT64_C(0));
    put(physics,0x24,physicsHandle);put(physics,0x2C,owner.entity);put(physics,0x238,std::uint8_t{1});
    MissionMotionView view{};check(mission_motion_view(owner,view),"production motion joins distinct movement owner and selector");
    put(selector,0x30,owner.character^0x01000000U);check(!mission_motion_view(owner,view),"recycled selector character rejected");put(selector,0x30,owner.character);
    const hooking::CallGate::Scope call(callGate);
    m::Token token{};
    for(unsigned island=0;island<5;++island) {
    reach_mission(m::Phase::departure,island==4?2:0,island);token=m::runtime::state.snapshot().command.token;
    const auto callbacksBefore=motionCallbacks,cleanupBefore=cleanupCallbacks;
    if(island==4) runState.crown.stopClaimed=true;
    auto target=motion::destinations[island];target[2]-=.377071F; // actual native adjusted destination
    empty();
    check(m::runtime::state.claim(token,m::Action::depart),"claim departure callback test");
    runState.departure={};runState.departure.token=token;runState.departure.submitted=runState.departure.requestClaimed=runState.departure.stopClaimed=true;
    put(arena,0x400,std::uint16_t{0});put(arena,0x402,std::uint8_t{0x3D});put(arena,0x403,std::uint8_t{0});
    put(arena,0x440,std::uint16_t{0});put(arena,0x442,std::uint16_t{0xF0});put(arena,0x444,std::uint16_t{0});put(arena,0x80,std::uint32_t{1});
    auto* raw=arena+0x500;std::memcpy(raw+0x10,target.data(),16);
    put(raw,0x30,owner.entity);put(raw,0x34,physicsHandle);put(raw,0x38,movementHandle);put(raw,0x58,selectorHandle);put(raw,0x60,INT64_C(0));put(raw,0x6C,std::uint32_t{0x3D});put(raw,0xA4,std::uint8_t{1});
    put(raw,0x38,owner.biped);mission_motion_update(nullptr,nullptr,raw);check(runState.departure.stage==0,"animation biped cannot impersonate movement body");
    put(raw,0x38,movementHandle);put(raw,0xA4,std::uint8_t{1});
    {graph::Owner o{};m::Snapshot s{};check(mission_motion_owner(o,s),"departure owner preflight");motion::Identity id{};std::array<std::byte,0xF0> b{};check(mission_motion_raw(o,s.command.token,raw,id,b),"departure allocation preflight");}
    mission_motion_update(nullptr,nullptr,raw);check(runState.departure.stage==2,"production pre/post captures1 then2");
    mission_motion_cleanup(nullptr,raw,nullptr);check(!runState.departure.cleaned,"cleanup before stage3 rejected");
    mission_motion_update(nullptr,nullptr,raw);mission_motion_cleanup(nullptr,raw,nullptr);check(runState.departure.cleaned && motionCallbacks==callbacksBefore+3 && cleanupCallbacks==cleanupBefore+2,"ordered lifecycle and original exactly once");
    put(memberBody,0x228,std::int32_t{1});world_position(target);
    pump_mission_motion(call);check(m::runtime::state.snapshot().phase==m::Phase::departure,"live3D allocation cannot finish");empty();
    put(selector,0x90,std::uint8_t{1});pump_mission_motion(call);check(m::runtime::state.snapshot().phase==m::Phase::departure,"active selector cannot finish");put(selector,0x90,std::uint8_t{0});
    world_position({0.F,0.F,0.F,1.F});pump_mission_motion(call);check(m::runtime::state.snapshot().phase==m::Phase::departure,"wrong native physics position cannot finish");world_position(target);
    // A stationary velocity must not be confused with the actual world position.
    put(physics,0x150,0.F);
    auto* world=worldRows+(entityHandle&0x1FFF)*0x100;
    put(world,0xC,entityHandle^0x2000);pump_mission_motion(call);
    check(m::runtime::state.snapshot().phase==m::Phase::departure,"recycled world entity cannot finish");
    put(world,0xC,entityHandle);
    pump_mission_motion(call);
    if(m::runtime::state.snapshot().phase!=m::Phase::arrival) {
        motion::Point actual{};const bool valid=mission_physics_position(physicsHandle,owner.entity,actual);
        std::fprintf(stderr,"entity=%08X fixture=%08X stride=%X world=%p native_rows=%p\n",owner.entity,entityHandle,read<std::uint32_t>(image,0x1F93430),worldRows,read<void*>(image,0x1F93428));
        std::fprintf(stderr,"island=%u valid=%u actual=%f,%f,%f,%f target=%f,%f,%f stage=%u cleaned=%u completed=%u\n",island,valid,actual[0],actual[1],actual[2],actual[3],target[0],target[1],target[2],runState.departure.stage,runState.departure.cleaned,runState.departure.completed);
        std::fprintf(stderr,"encoded=%08X,%08X,%08X,%08X keys=%08X,%08X masks=%08X,%08X\n",read<std::uint32_t>(world,0xD0),read<std::uint32_t>(world,0xD4),read<std::uint32_t>(world,0xD8),read<std::uint32_t>(world,0xDC),reinterpret_cast<std::uint32_t(*)()>(image+0x6260781)(),reinterpret_cast<std::uint32_t(*)()>(image+0x584DFF2)(),read<std::uint32_t>(image,0x1B9E420),read<std::uint32_t>(image,0x1B9E430));
    }
    check(m::runtime::state.snapshot().phase==m::Phase::arrival,"production retirement plus actual position finishes departure");
    check((m::runtime::state.snapshot().cannons&(island==3?15:7))==(island==3?15:7),"all stage cannon milestones retained");
    const auto completedEpoch=m::runtime::state.snapshot().command.token.epoch;
    mission_motion_update(nullptr,nullptr,raw);mission_motion_cleanup(nullptr,raw,nullptr);
    check(m::runtime::state.snapshot().command.token.epoch==completedEpoch,"retired departure callbacks cannot advance next stage");
    }
    // Production post-apply observer, all four cores; original must run first.
    reach_mission(m::Phase::summon,0);
    m::runtime::state={};
    check(m::runtime::state.bind({owner.run,owner.generation,owner.actor,owner.character,owner.biped,owner.entity,owner.member,owner.revision}),"device fixture binds fresh mission");
    std::uint32_t generation=owner.generation;
    for(unsigned i=0;i<4;++i) {
        auto* core=bind(0x20F27000+i,0x500);
        source(core,omega::mission_devices::kCannons[i].coreAsset,0x80809928,0x4C8);
        put(core,0x160,0x20F27000+i); put(core,0x164,std::uint32_t{0x80809927});
        fixtureDeviceMembers.emplace_back(core,0x20F27000+i);
        put(core,0x180,generation-1);
        observe_mission_device(core);
        check(m::runtime::state.snapshot().cannonPrepared==((1U<<i)-1),"stale generation cannot prepare cannon");
        portal_visual_apply(core,reinterpret_cast<const std::byte*>(&generation));
        check(m::runtime::state.snapshot().cannonPrepared==((1U<<(i+1))-1),"original post-apply prepares each authored cannon");
    }
    check(deviceApplyCalls==4,"original device apply forwarded once per core");
    auto* health=bind(healthHandle,0x340);source(health,0x815B5A40,0x80804B8A,0xF98);put(health,0x24,healthHandle);put(health,0x2C,owner.entity);
    source(characterBody,0x80F6690B,0x80806832,0x738);put(characterBody,0x2E8,healthHandle);put(characterBody,0x2EC,std::uint32_t{0x80804BEE});put(characterBody,0x2F0,INT64_C(0));
    for(unsigned cycle=1;cycle<=3;++cycle) {
        reach_mission(m::Phase::summon,cycle);runState.crown={};put(memberBody,0x228,std::int32_t{1});
        pump_mission_crown(call);check(runState.crown.accepted && runState.crown.cycle==cycle,"production native queue installs exact Crown cycle");
        auto frame=snapshot(4);
        const auto node=[&](int index,int bankRow,bool loop) {
            graph::write(frame,0xA8,crown::cycles[cycle-1].graph);graph::write(frame,0xB4,crown::cycles[cycle-1].graph);
            graph::write(frame,0xA4,index);graph::write(frame,0xB0,index);graph::write(frame,0x30,bankRow);frame[0x20]=loop?std::byte{1}:std::byte{0};
        };
        node(cycle==3?6:5,13,false);observe_mission_crown(frame,true,call);
        const auto summoned=m::runtime::state.snapshot();unsigned budget{};
        for(std::size_t i=0;i<m::kSources.size();++i) if(m::kSources[i].wave==summoned.command.wave) budget+=summoned.requested[i][0]+summoned.requested[i][1];
        check(budget==m::kWaveTotals[summoned.command.wave],"production Crown callback releases exact opening budget");
        check(runState.crown.summoned,"Crown summon observed");
        node(0,1,true);observe_mission_crown(frame,true,call);check(m::runtime::state.snapshot().phase==m::Phase::clearance,"actual idle finishes Crown opening");
        // Reach deletion through the real controller; preserve the accepted native queue.
        reach_mission(m::Phase::deletion,cycle);pump_mission_crown(call);
        check(runState.crown.lease.state()==graph::LeaseState::owned,"original native event add confirms Crown lease");
        node(1,0,false);observe_mission_crown(frame,true,call);
        check(runState.crown.lease.state()==graph::LeaseState::released,"native node consumes exact owned event");
        node(2,cycle==1?17:6,true);observe_mission_crown(frame,true,call);check(m::runtime::state.snapshot().phase==m::Phase::rescue,"native hold advances actual rescue phase");
        reach_mission(m::Phase::shield,cycle);
        fixtureEye=1.F;fixtureBody=1.F;runState.health={};
        auto* channel=animationBody+0x1980;put(channel,0,owner.character);put(channel,4,owner.animation);
        for(unsigned i=0;i<8;++i) put(channel,0x20+i*0x108,std::uint32_t{0x811C9DC5});
        pump_mission_crown(call);
        check(!runState.crown.uncertain && read<std::uint32_t>(channel,0x20)==0xC57C61EB,"production shield submits authored eye refill and confirms native slot");
        node(3,cycle==1?16:cycle==2?3:11,false);graph_update(.01F,nullptr,frame.data(),nullptr);
        check(runState.crown.lease.state()==graph::LeaseState::released,"shield event released only at actual eye opening");
        check(runState.health.sampled && runState.health.previous==1.F,"Crown opening callback establishes native health baseline");
        node(cycle==3?5:6,cycle==3?4:14,true);graph_update(.01F,nullptr,frame.data(),nullptr);
        check(m::runtime::state.snapshot().phase==m::Phase::eye,"production native eye node enables damage mechanic");
        fixtureEye=.95F;graph_update(.01F,nullptr,frame.data(),nullptr);
        check(m::runtime::state.snapshot().phase==m::Phase::eye,"eye remains exposed above native damage threshold");
        fixtureEye=.9F;
        auto foreignFrame=frame;graph::write(foreignFrame,0x14,owner.biped^0x2000U);
        graph_update(.01F,nullptr,foreignFrame.data(),nullptr);
        check(!runState.health.crossed,"foreign graph cannot produce a health receipt");
        graph_update(.01F,nullptr,frame.data(),nullptr);
        check(m::runtime::state.snapshot().command.action==(cycle==3?m::Action::finalDeath:m::Action::recover),"production health crossing selects recovery or death");
        pump_mission_crown(call);
        node(cycle==3?4:7,cycle==3?12:10,false);observe_mission_crown(frame,cycle!=3,call);
        const auto expected=cycle==1?0xB52CCA3BU:cycle==2?0xB52CCA38U:0x9527E28AU;
        check(read<std::uint32_t>(channel,0x128)==expected,"production checkpoint or final kill is a native named slot");
        if(cycle<3) {
            fixtureBody=cycle==1?.55F:.10F;graph_update(.01F,nullptr,frame.data(),nullptr);
            node(0,1,true);observe_mission_crown(frame,true,call);
            check(m::runtime::state.snapshot().phase==m::Phase::summon,"native checkpoint plus idle resumes exact next cohort");
        } else {
            auto* eventArena=allocate(0x20);auto* eventBytes=allocate(0x100);put(image,0x274F778,eventArena);put(eventArena,0x10,eventBytes);
            put(health,0x338,std::uint32_t{1});put(eventBytes,0x34,std::uint32_t{0x80804C53});
            observe_mission_boss_death(characterBody,0);check(!m::runtime::state.snapshot().nativeBossDead,"wrong native event cannot finish boss");
            put(eventBytes,0x34,std::uint32_t{0x80804C54});observe_mission_boss_death(characterBody,0);
            check(m::runtime::state.snapshot().phase==m::Phase::ending,"typed native death plus actual terminal graph requests ending");put(health,0x338,std::uint32_t{0});
            namespace ending=omega::ending;namespace speech=omega::mission_presentation;
            auto final=m::runtime::state.snapshot();final.rescueReadyMask=0;final.rescueStartedMask=0;final.scenes={};speech::queue={};
            check(speech::observe(final,100).pending==33 && speech::dispatch(7,33,1,100),"D20 actual dispatch fixture");
            ending::runtime::update(7,100);auto end=ending::runtime::update(7,9202);
            const state::activity::ActivityInstanceKey activity{77,{9}};
            auto command=ending::runtime::project(7,activity,88,true,true,{},112).host;command.state=3;
            check(ending::runtime::project(7,activity,88,true,true,command,121).arrived,"exact ending arrival fixture");
            constexpr std::uint32_t movieHandle=0x40F9EA60,controllerHandle=0x43F9EA61,movieOwner=0x1BFA2062;
            auto* movie=bind(movieHandle,0x100);source(movie,0x80C177DD,0x80806647,0x10578);put(movie,0x24,movieHandle);put(movie,0x2C,movieOwner);
            auto* controller=bind(controllerHandle,0x270);source(controller,0x80F47BCB,0x80804F07,0x2E8);put(controller,0x24,controllerHandle);
            auto* directoryRow=allocate(0x58);put(directoryRow,0x18,movieHandle);endingDirectory=directoryRow;
            put(movie,4,std::uint32_t{0x80806646});observe_mission_ending(controller);check(!ending::runtime::snapshot(7).play,"wrong registered resource cannot arm movie");put(movie,4,std::uint32_t{0x80806647});
            observe_mission_ending(controller);check(ending::runtime::snapshot(7).play,"production directory lookup arms exact bookend authority");
            put(controller,0x190,end.revision+1);put(controller,0x260,std::uint8_t{1});observe_mission_ending(controller);check(!ending::runtime::snapshot(7).started,"foreign native revision cannot start ending");
            put(controller,0x190,end.revision);observe_mission_ending(controller);check(m::runtime::state.snapshot().endingStarted,"actual callback starts movie receipt");
            endingDirectory=nullptr;put(controller,0x260,std::uint8_t{0});observe_mission_ending(controller);check(m::runtime::state.snapshot().phase==m::Phase::ending,"unregistered movie cannot complete");
            endingDirectory=directoryRow;observe_mission_ending(controller);check(m::runtime::state.snapshot().phase==m::Phase::finished,"production matching inactive callback completes movie");
            endingDirectory=nullptr;

        }

    }
    reach_mission(m::Phase::shield,1);token=m::runtime::state.snapshot().command.token;check(m::runtime::state.claim(token,m::Action::shield),"health fixture shield claimed");runState.health={};
    fixtureEye=.89F;observe_mission_health(call);check(!runState.health.crossed,"already low sample is not crossing");fixtureEye=.95F;observe_mission_health(call);fixtureEye=.90F;observe_mission_health(call);
    check(runState.health.crossed && m::runtime::state.snapshot().phase==m::Phase::shield,"production downward crossing retained during opening");
    check(m::runtime::state.animation(token,m::Animation::eyeExposed),"exposure consumes native early crossing");
    token=m::runtime::state.snapshot().command.token;check(m::runtime::state.claim(token,m::Action::recover),"native recovery claim");fixtureBody=.55F;observe_mission_health(call);check(m::runtime::state.snapshot().nativeBodyCheckpoint,"production health getter supplies exact checkpoint");
    missionNativeFixture=false;
}
