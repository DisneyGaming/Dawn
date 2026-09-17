// Included after the common Eater fixture helpers. These are controller receipt
// tests, not a substitute for native actor/door gameplay acceptance.
#include "../src/middleware/encoding/bit_writer.h"
#include "../src/client/hooks/bootflow/eater_reinforcement_selection.h"
#include <cstring>

static void reactor_passenger_selection() {
    namespace selection=dawn::client::hooks::bootflow::eater_reinforcement_selection;
    using Kind=m::reinforcements::Kind;
    // The saved delivery uses one category and generation 129. Opaque bytes use
    // nonzero sentinels to catch accidental replacement of the full native row.
    const selection::SelectedMember original{
        {0x8155C432U,0x808099D8U,0x8B0},0,0x19C57E65U,0x12345678U,0x5AU,
        {std::byte{0xA1},std::byte{0xB2},std::byte{0xC3}},129,0xABCDEF01U};
    const selection::Context valid{2,0x8155C432U,129,0,0x80C0FA98U};
    const selection::Donor pyro{selection::donor_spec(Kind::incendior),true};
    auto chosen=original;
    check(selection::apply(valid,pyro,chosen)==selection::Result::substituted,
        "left finale selects its retained Incendior descriptor");
    check(chosen.reference.handle==0x8155C43BU && chosen.reference.offset==0x920
        && chosen.reference.kind==0x808099D8U && chosen.choiceIdentity==0xAAF9AB71U
        && chosen.choiceFlags==0,"Incendior substitution carries a complete native choice");
    check(chosen.categoryIndex==original.categoryIndex && chosen.categoryHash==original.categoryHash
        && chosen.sourceGeneration==original.sourceGeneration
        && chosen.choiceReserved==original.choiceReserved && chosen.tailReserved==original.tailReserved,
        "substitution preserves category ownership, native generation and opaque bytes");

    const auto unchanged=[&](const selection::Context& context,const selection::Donor& donor,
                              selection::SelectedMember input) {
        const auto before=input;
        check(selection::apply(context,donor,input)==selection::Result::passthrough
            && std::memcmp(&before,&input,sizeof(input))==0,
            "unverified selection passes through without changing any native byte");
    };
    auto invalid=valid;invalid.run=0;unchanged(invalid,pyro,original);
    invalid=valid;invalid.sourceTag=0x8155C43BU;unchanged(invalid,pyro,original);
    invalid=valid;invalid.sourceGeneration=130;unchanged(invalid,pyro,original);
    invalid=valid;invalid.sourceGeneration=0;chosen=original;chosen.sourceGeneration=0;
    unchanged(invalid,pyro,chosen);
    invalid=valid;invalid.category=1;unchanged(invalid,pyro,original);
    invalid=valid;invalid.resolvedEntity=0x80BFAA15U;unchanged(invalid,pyro,original);
    chosen=original;chosen.reference.handle=0x8155C435U;unchanged(valid,pyro,chosen);
    chosen=original;chosen.reference.kind=0x80809BB6U;unchanged(valid,pyro,chosen);
    chosen=original;chosen.categoryHash=0;unchanged(valid,pyro,chosen);
    chosen=original;chosen.categoryIndex=1;unchanged(valid,pyro,chosen);
    auto missing=pyro;missing.resolved=false;unchanged(valid,missing,original);
    missing=pyro;missing.spec.reference.offset+=8;unchanged(valid,missing,original);
    missing=pyro;missing.spec.entity=0x80BFAA15U;unchanged(valid,missing,original);
    missing=pyro;missing.spec.choiceIdentity^=1;unchanged(valid,missing,original);
    missing=pyro;missing.spec.choiceFlags=1;unchanged(valid,missing,original);
    unchanged(valid,{selection::donor_spec(Kind::psion),true},original);

    struct Expected {std::uint32_t tag,entity;std::uint16_t slot;unsigned count;Kind kind;};
    constexpr Expected roster[]{
        {0x8155C42FU,0x80C0FA98U,18,2,Kind::colossus},
        {0x8155C432U,0x80BFAA20U,19,3,Kind::incendior},
        {0x8155C435U,0x80C1A8E4U,20,5,Kind::psion}};
    unsigned total{};
    for(const auto& row:roster) {
        check(m::reinforcements::count(row.slot)==row.count,
            "finale passenger requests are two Colossi, three Incendiors and five Psions");
        for(unsigned i=0;i<row.count;++i) {
            // Repeated independent calls model native partial batches and retries.
            auto context=valid;context.sourceTag=row.tag;
            auto member=original;member.reference.handle=row.tag;
            const auto before=member;
            const auto result=selection::apply(context,{selection::donor_spec(row.kind),true},member);
            check(m::reinforcements::select(row.slot,i)==row.kind
                && m::reinforcements::entity(row.kind)==row.entity,"passenger type stays fixed within its native source");
            if(row.kind==Kind::colossus) {
                check(result==selection::Result::native && std::memcmp(&before,&member,sizeof(member))==0,
                    "center Colossi preserve their original native choice");
            } else {
                check(result==selection::Result::substituted && member.sourceGeneration==129
                    && member.categoryHash==0x19C57E65U,"every replacement retains the finale source generation and category");
            }
            ++total;
        }
        check(m::reinforcements::select(row.slot,row.count)==Kind::invalid,"out-of-range roster member has no template");
    }
    check(total==10 && m::reinforcements::count(23)==0,"ten finale enemies leave path sources outside the replacement policy");
}

static void reactor_source_authority() {
    reactor_passenger_selection();
    namespace bits=dawn::middleware::encoding::bits;
    struct Binding {std::uint16_t source,rule;std::uint8_t requested;};
    constexpr Binding bindings[]{
        {18,311,2},{19,293,3},{20,309,5},
        {23,295,2},{24,297,2},{25,299,2},{27,321,2},{28,321,2},{29,323,2},{30,323,2},
        {31,325,2},{32,325,2},{34,184,2},{35,186,2},{36,188,2}
    };
    for(const auto binding:bindings) {
        const auto asset=m::find(m::kReactorRegistry,1,binding.source)->asset;
        const auto spawn=m::spawn_index(m::kReactorRegistry,binding.source);
        check(m::reactor_spawn_rule(asset)==binding.rule,"reactor combat source maps to its named native placement rule");
        m::Frame frame{};frame.enabled=true;frame.spawnGeneration=17;
        auto& state=frame.native[m::asset_index(asset)];state.managed=state.desired=state.active=true;state.generation=19;
        frame.taskPlusOne[spawn]=1;
        const auto expected=m::kSpawns[spawn].categories==2?coo::native_combatant::kTwoCategorySourceBits
            :coo::native_combatant::kSourceBits;
        const auto type=static_cast<std::uint8_t>(asset.type);
        check(m::body_bits(frame,asset.registry,type,asset.slot)==expected,"mapped reactor source uses complete native rule authority");
        Wire active{};check(m::write_body(active,frame,asset.registry,type,asset.slot)
            && active.valid && active.bits==expected,"mapped reactor source writes exact complete authority width");
        // Independent MSB-first inspection of reflected 80807EC9 field offsets.
        // The second category inserts 32 bits after the first request count.
        const std::size_t extra=m::kSpawns[spawn].categories==2?32U:0U;
        std::array<std::byte,96> body{};
        const auto field=[&](std::size_t start,unsigned width) {
            std::uint64_t result{};
            for(unsigned bit=0;bit<width;++bit) {
                const auto at=start+bit;
                result=(result<<1U)|((std::to_integer<unsigned>(body[at/8U])>>(7U-at%8U))&1U);
            }
            return result;
        };
        bits::Writer activeBody(body);
        check(m::write_body(activeBody,frame,asset.registry,type,asset.slot),
            "active reactor source serializes to real packet bytes");
        check(field(1,32)==asset.registry && field(33,7)==4
            && field(40,16)==32768U+m::kSpawns[spawn].objective,
            "combat assignment names the exact native type-three objective");
        if(binding.source>=18 && binding.source<=20) {
            check(field(40,16)==32768U+22U && m::kSpawns[spawn].taskCount==5,
                "delivered Loyalists use the final platform objective with five authored tasks");
        }
        const auto requested=binding.requested;
        check(field(117,4)==m::kSpawns[spawn].categories && field(121,32)==0x80000000U+requested
            && (!extra || field(153,32)==0x80000000U+requested),
            "active population requests the bounded actor count in every authored category");
        check(field(382+extra,1)==1 && field(383+extra,32)==asset.registry
            && field(415+extra,7)==67 && field(422+extra,16)==32768U+binding.rule,
            "active packet resolves the selected type-sixty-six spawn region");
        check(field(173+extra,31)==state.generation && field(495+extra,31)==state.generation
            && field(566+extra,5)==1 && field(572+extra,31)==state.generation,
            "source and tactical revisions use the current source lease rather than the raid generation");
        check(field(603+extra,2)==2 && field(605+extra,3)==1,
            "active infantry retains ordinary native ownership and loose-spawn mode");
        state.active=false;state.retiring=true;++state.generation;
        check(m::body_bits(frame,asset.registry,type,asset.slot)==expected,"retirement publishes one complete native generation boundary");
        Wire retiring{};check(m::write_body(retiring,frame,asset.registry,type,asset.slot)
            && retiring.valid && retiring.bits==expected,"retirement writes zero-count native-owned cleanup body");
        bits::Writer retiredBody(body);
        check(m::write_body(retiredBody,frame,asset.registry,type,asset.slot),
            "retiring source serializes to real packet bytes");
        check(field(121,32)==0x80000000U && (!extra || field(153,32)==0x80000000U)
            && field(173+extra,31)==state.generation && field(572+extra,31)==state.generation,
            "retirement advances the source revision and requests zero new actors");
        check(field(1,32)==0x811C9DC5U && field(383+extra,32)==0x811C9DC5U
            && field(603+extra,2)==1 && field(605+extra,3)==1,
            "retirement selects native owned cleanup with no tactical or spawn-rule request");
        state.retiring=false;
        check(m::body_bits(frame,asset.registry,type,asset.slot)==0,"inactive acknowledged source stops publishing authority");
    }
    const auto unrelated=m::find(0xE8D290A0U,1,18)->asset;
    check(m::reactor_spawn_rule(unrelated)==UINT16_MAX,"non-reactor source cannot inherit a reconstructed placement rule");
}
static m::Point eater_volume_point(coo::Asset asset) {
    for(const auto& volume:m::kVolumes) {
        if(volume.asset.registry!=asset.registry || volume.asset.slot!=asset.slot) continue;
        for(unsigned x=1;x<20;++x) for(unsigned y=1;y<20;++y) {
            const m::Point point{volume.min.x+(volume.max.x-volume.min.x)*static_cast<float>(x)/20.F,
                volume.min.y+(volume.max.y-volume.min.y)*static_cast<float>(y)/20.F,
                (volume.min.z+volume.max.z)*0.5F};
            if(m::contains(volume,point)) return point;
        }
    }
    check(false,"authored route volume has a usable interior point");return {};
}
static void holdout_and_traversal(m::Controller& controller,std::uint64_t run,
                                  std::uint64_t& now,std::uint32_t player,bool retry) {
    const auto tick=[&](int region=56,unsigned count=4) {
        for(unsigned i=0;i<count;++i) static_cast<void>(controller.update(run,now+=100,true,region));
    };
    tick();
    auto frame=controller.frame();
    unsigned crossingSources{},crossingActors{},holdoutActors{};
    for(const auto& source:m::kSpawns) {
        const auto& state=frame.native[m::asset_index(source.asset)];
        if(m::path_source(source.asset)) {
            check(state.active && !state.retiring,"every crossing source survives all four checkpoints");
            ++crossingSources;crossingActors+=m::expected_enemy_count(source);
        }
        if(m::holdout_source(source.asset)) {
            check(state.active==m::solo_holdout_source(source.asset),"only selected final reinforcement sources activate");
            if(state.active) holdoutActors+=m::expected_enemy_count(source);
        }
    }
    check(crossingSources==12 && crossingActors==30 && holdoutActors==10,
        "all authored crossing categories plus exactly ten final Loyalists are requested");
    const auto grate=m::find(m::kReactorRegistry,4,0)->asset;
    check(frame.native[m::asset_index(grate)].desired,"exit remains closed before required kills");
    check(controller.prepared(controller.owner(),grate),"prepare retained grate source");
    const auto grateGeneration=controller.frame().native[m::asset_index(grate)].generation;
    check(controller.object({{run,grateGeneration},grate,555,666}),"observe native grate creation before opening it");
    const auto checkpoint=frame.checkpointSpawnSet;
    std::array<m::EnemyReceipt,10> receipts{};
    const auto admit=[&](std::uint32_t base) {
        std::size_t count{};
        for(const auto slot:m::kSoloHoldoutSources) {
            const auto index=m::spawn_index(m::kReactorRegistry,slot);const auto& source=m::kSpawns[index];
            const auto generation=controller.frame().native[m::asset_index(source.asset)].generation;
            for(unsigned actor=0;actor<m::expected_enemy_count(source);++actor) {
                auto& receipt=receipts[count];receipt={run,base+static_cast<std::uint32_t>(count),base+100+slot,generation,slot,m::kReactorRegistry};
                check(controller.admitted(receipt),"exact selected native birth is admitted");
                check(!controller.admitted(receipt),"duplicate birth cannot consume another population slot");
                check(controller.readiness(receipt,{true,true,true,true,base+200+static_cast<std::uint32_t>(count),m::kReactorRegistry,source.objective,-1}),
                    "native readiness can arrive before tactical selection");
                ++count;
            }
        }
        check(count==receipts.size(),"ten actual actor receipts are required");
        tick();
        check(controller.frame().section==2 && controller.diagnostics().active==(1U<<2),
            "cost-only row minus one cannot acknowledge working combat");
        for(const auto slot:m::kSoloHoldoutSources) {
            const auto index=m::spawn_index(m::kReactorRegistry,slot);const auto& source=m::kSpawns[index];
            coo::TaskCosts costs{};costs.initialized=costs.hasRevision=true;costs.mask=1;costs.cost[0]=1;
            costs.revision=controller.frame().native[m::asset_index(source.asset)].generation+1;
            controller.costed(m::kReactorRegistry,slot,costs);
            check(controller.frame().taskPlusOne[index]==0,"stale task costs cannot assign this wave");
            --costs.revision;
            costs.mask=1U<<5;costs.cost[5]=1;
            controller.costed(m::kReactorRegistry,slot,costs);
            check(controller.frame().taskPlusOne[index]==0,"finale rejects task rows outside the five-row door objective");
            costs.mask=1U<<4;costs.cost[4]=1;
            controller.costed(m::kReactorRegistry,slot,costs);
            check(controller.frame().taskPlusOne[index]==5,"last authored door objective row remains selectable");
            costs.mask=1;costs.cost[0]=0;controller.costed(m::kReactorRegistry,slot,costs);
            check(controller.frame().taskPlusOne[index]==1,"current reachable native task is selected");
        }
        for(const auto& receipt:receipts) {
            const auto& source=m::kSpawns[m::spawn_index(receipt.registry,receipt.source)];
            check(controller.readiness(receipt,{true,true,true,true,base+300,receipt.registry,source.objective,0}),
                "assigned tactical task plus health and AI supplies readiness");
        }
        tick();
        check(controller.diagnostics().active==(1U<<3),"ready actors advance to the required real death gate");
    };
    admit(30000);
    auto unknown=receipts[0];unknown.actor+=700;
    check(!controller.died(unknown),"unknown actor death cannot complete a selected source");
    if(retry) {
        for(std::size_t i=0;i<3;++i) check(controller.died(receipts[i]),"first attempt records actual partial kills");
        static_cast<void>(controller.player_health(run,player,false));
        check(controller.player_health(run,player,true),"death restarts the holdout checkpoint");
        check(controller.frame().combatRetry && !controller.frame().restricted
            && controller.frame().reactor.completed.all() && controller.frame().checkpointSpawnSet==checkpoint,
            "holdout failure preserves the complete crossing and permits respawn");
        for(const auto& receipt:receipts) check(!controller.died(receipt),"retiring wave rejects late deaths");
        tick();check(controller.frame().combatRetry,"time cannot acknowledge native retirement");
        for(const auto slot:m::kSoloHoldoutSources) {
            const auto source=m::find(m::kReactorRegistry,1,slot)->asset;
            const auto state=controller.frame().native[m::asset_index(source)];
            check(state.retiring && !state.active,"each old holdout source requests native cleanup");
            check(!controller.source_retired(run,m::kReactorRegistry,slot,state.generation-1),"old cleanup acknowledgement rejected");
            check(controller.source_retired(run,m::kReactorRegistry,slot,state.generation),"matching native cleanup acknowledgement accepted");
            check(!controller.source_retired(run,m::kReactorRegistry,slot,state.generation),"cleanup acknowledgement is idempotent");
        }
        tick();check(controller.frame().combatRetry,"retirement cannot respawn enemies while the player is confirmed dead");
        check(controller.player_health(run,player,false),"real respawn releases retry");
        tick();check(!controller.frame().combatRetry,"fresh holdout starts after player and native cleanup are ready");
        for(const auto& receipt:receipts) check(!controller.admitted(receipt) && !controller.died(receipt),"old births and deaths cannot enter the new source generation");
        admit(40000);
    }
    for(std::size_t i=0;i<receipts.size()-1;++i) {
        check(controller.died(receipts[i]),"real required kill accepted");
        check(!controller.died(receipts[i]),"duplicate death cannot count twice");
    }
    static_cast<void>(controller.update(run,now+=60000,true,56));
    check(controller.frame().section==2 && controller.frame().native[m::asset_index(grate)].desired,
        "nine of ten deaths and elapsed time do not open the exit");
    check(controller.died(receipts.back()),"tenth required native death completes the holdout");
    tick();frame=controller.frame();
    check(frame.section==2 && frame.grate.open && !frame.grate.poseAcknowledged,
        "required kills request the hatch but cannot acknowledge its physical opening");
    m::GratePoseReceipt gratePose{{{run,grateGeneration},grate,555,666},777,888,
        static_cast<std::int32_t>(frame.grate.revision),1.F};
    auto staleGrate=gratePose;++staleGrate.object.entity;
    check(!controller.grate_pose(staleGrate),"grate pose rejects a different object entity");
    staleGrate=gratePose;++staleGrate.object.serial;
    check(!controller.grate_pose(staleGrate),"grate pose rejects a different object serial");
    check(controller.grate_pose(gratePose),"current native grate pose opens the path");
    tick();frame=controller.frame();
    check(frame.section==3 && !frame.restricted && !frame.finished,"ten kills release traversal without claiming raid victory");
    check(frame.objective==m::kObjectives[4],"completion selects Delve deeper from the native objective table");
    check(frame.native[m::asset_index(grate)].desired && frame.grate.open && frame.grate.revision!=0,
        "required clear opens the retained grate through its native pose control");
    check(frame.native[m::asset_index(m::find(m::kReactorRegistry,4,94)->asset)].desired
        && frame.native[m::asset_index(m::find(m::kReactorRegistry,4,95)->asset)].desired,
        "clear requests the authored reward chest and its effect");
    for(const auto& source:m::kSpawns) {
        const auto& state=frame.native[m::asset_index(source.asset)];
        if(m::path_source(source.asset) || m::solo_holdout_source(source.asset))
            check(!state.active && state.retiring,"completed reactor populations retire before traversal");
        if(source.asset.registry!=m::kReactorRegistry) check(!state.managed,"arrival slice never requests barrier or Argos enemies");
    }
    controller.position(run,eater_volume_point(m::kMouthCrossings[0].volume));tick();
    tick(48,6);frame=controller.frame();
    check(frame.native[m::asset_index(m::kAirlock.entranceDoor)].active,"route opens the underbelly entrance");
    check(!frame.routeComplete,"loading the belly bubble does not prove arena arrival");
    controller.position(run,eater_volume_point(m::kThunderingWallCheckpoint.volume));tick(48);
    check(controller.frame().checkpointSliceSet==48 && controller.frame().checkpointSpawnSet==m::kThunderingWallCheckpointSpawn,
        "safe traversal volume commits its package-proved respawn set");
    check(!controller.frame().native[m::asset_index(m::kEjectionTube)].managed,
        "early thunder-wall checkpoint cannot open the final underbelly gate");
    controller.position(run,eater_volume_point(m::kAirlock.interiorVolume));tick(48);
    check(controller.frame().native[m::asset_index(m::kAirlock.entranceDoor)].active
        && controller.frame().native[m::asset_index(m::kAirlock.exitDoor)].active,"interior passage sequences the airlock doors");
    check(!controller.frame().native[m::asset_index(m::kWindVolume)].managed,
        "requesting an open exit cannot start suction before physical acknowledgement");
    const auto applyGate=[&](coo::Asset gate) {
        const auto& state=controller.frame().native[m::asset_index(gate)];
        check(controller.device({run,state.generation},gate,static_cast<std::int16_t>(state.generation),1.F),
            "exact applied native open pose acknowledges the gate");
        tick(48);
    };
    applyGate(m::kAirlock.exitDoor);
    check(!controller.frame().native[m::asset_index(m::kWindVolume)].managed,
        "suction also waits for the ejection tube to physically open");
    applyGate(m::kEjectionTube);
    controller.position(run,eater_volume_point(m::kAirlock.exteriorVolume));tick(48);
    check(controller.frame().native[m::asset_index(m::kEjectionTube)].active,"final underbelly gate receives its native open request");
    tick(48,4);frame=controller.frame();
    check(frame.section==3 && frame.region==48 && frame.arrival.launched,
        "opened traversal gates launch the authenticated hoop arrival attempt");
    unsigned shellObjects{},shellGates{};
    for(std::uint16_t slot=0;slot<6;++slot) {
        const auto asset=m::find(m::kBarrierRegistry,4,slot)->asset;
        const auto& state=frame.native[m::asset_index(asset)];
        check(state.managed && state.desired,"arrival introduction requests each debris-shell object");
        ++shellObjects;
    }
    for(std::uint16_t slot=6;slot<12;++slot) {
        const auto asset=m::find(m::kBarrierRegistry,23,slot)->asset;
        check(!frame.native[m::asset_index(asset)].managed,"shell geometry gate waits for every object consumer");
    }
    for(std::uint16_t slot=58;slot<=80;++slot) {
        const auto control=m::find(m::kArgosRegistry,23,slot)->asset;
        const auto source=m::find(m::kArgosRegistry,4,static_cast<std::uint16_t>(slot+23))->asset;
        const auto& controlState=frame.native[m::asset_index(control)];
        check(controlState.managed && !controlState.desired && !controlState.active && controlState.position==0.F,
            "arrival explicitly suppresses each Argos platform control at pose zero");
        check(!frame.native[m::asset_index(source)].managed,"arrival never requests any of the 23 Argos platform object sources");
    }
    check(shellObjects==6,"the introduction has exactly six debris-shell objects");
    for(std::uint16_t slot=0;slot<6;++slot) {
        const auto asset=m::find(m::kBarrierRegistry,4,slot)->asset;
        check(controller.prepared(controller.owner(),asset),"debris-shell object accepts native preparation");
        const auto generation=controller.frame().native[m::asset_index(asset)].generation;
        check(controller.object({{run,generation},asset,static_cast<std::uint32_t>(700+slot),
            static_cast<std::uint32_t>(800+slot)}),"debris-shell object creation is acknowledged");
        if(slot!=5) {
            tick(48,1);
            for(std::uint16_t gate=6;gate<12;++gate)
                check(!controller.frame().native[m::asset_index(m::find(m::kBarrierRegistry,23,gate)->asset)].managed,
                    "five shell objects cannot pose any geometry gate");
        }
    }
    tick(48,1);frame=controller.frame();
    for(std::uint16_t slot=6;slot<12;++slot) {
        const auto asset=m::find(m::kBarrierRegistry,23,slot)->asset;
        const auto& state=frame.native[m::asset_index(asset)];
        check(state.managed && state.desired && state.position==1.F,"all six consumers release each shell geometry gate at pose one");
        ++shellGates;
    }
    check(shellGates==6 && frame.arrival.shellPosed,"exactly six shell geometry gates pose after all consumers exist");
    auto attempt=frame.arrival.attempt;
    const auto motion=[&](m::Point point,std::uint32_t receiptAttempt) {
        tick(48,1);
        return controller.arrival_motion({run,now,player,receiptAttempt,point});
    };
    const auto& hoop=m::kHoopCylinders[0];
    if(retry) {
        static_cast<void>(controller.player_health(run,player,false));
        check(controller.player_health(run,player,true),"death during ejection starts a fresh arrival attempt");
        check(controller.frame().arrival.attempt!=attempt && !controller.frame().arrival.hoop,
            "arrival retry discards the prior attempt's traversal proof");
        check(!motion({hoop.center.x,hoop.center.y,hoop.center.z-3.F},attempt),
            "a delayed motion receipt from the prior attempt is rejected");
        check(controller.player_health(run,player,false),"authenticated respawn enables the new arrival attempt");
        attempt=controller.frame().arrival.attempt;
    }
    check(!motion({hoop.center.x+hoop.radius+0.01F,hoop.center.y,hoop.center.z-3.F},attempt),
        "a motion sample outside the hoop rim does not prove traversal");
    check(!motion({hoop.center.x+hoop.radius+0.01F,hoop.center.y,hoop.center.z+3.F},attempt)
        && !controller.frame().arrival.hoop,"crossing the hoop plane beyond its radius is rejected");
    controller.position(run,eater_volume_point(m::kBarrierArenaArrivalVolumes[0]));tick(48);
    check(!controller.frame().arrival.landed && controller.frame().section==3,
        "region load and an earlier arena visit cannot replace a later hoop crossing");
    tick(48,3);
    check(!motion({hoop.center.x,hoop.center.y,hoop.center.z-3.F},attempt),
        "a discontinuous first sample cannot create a hoop crossing");
    check(motion({hoop.center.x,hoop.center.y,hoop.center.z+3.F},attempt)
        && controller.frame().arrival.hoop && !controller.frame().arrival.landed,
        "continuous motion through an authored hoop proves only hoop traversal");
    const auto arena=eater_volume_point(m::kBarrierArenaArrivalVolumes[0]);
    check(motion(arena,attempt) && controller.frame().arrival.landed,
        "one authored hoop followed by current presence in the Argos arrival trigger completes arrival");
    tick(48,4);frame=controller.frame();
    check(frame.routeComplete && frame.section==4 && frame.restricted && !frame.finished
        && frame.checkpointSpawnSet==m::kBarrierArenaSpawn,
        "hoop traversal plus the Argos arrival trigger completes the bounded route and publishes its checkpoint");
    check(frame.waitingMechanic==110 && frame.objective==m::kObjectives[5],
        "the completed route starts the barrier encounter at its native cycle gate");
}
