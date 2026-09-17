#pragma once
// Exercise actual controller publications and native receipt ownership for both
// documents. The small authorized graph isolates the three-room encounter.
inline void strike_pact_phase_tests() {
    check(m::kGrandmasterEnemySubstitutions.size()==18,"18 installed Tree GM category substitutions");
    std::size_t candidateSources{};
    for(std::size_t i=0;i<m::kGrandmasterEnemySubstitutions.size();++i) {
        const auto& value=m::kGrandmasterEnemySubstitutions[i];
        check(m::grandmaster_substitution_source(value.registry,value.source),"Tree GM source candidate lookup");
        check(m::grandmaster_substitution(value.registry,value.source,value.category,value.grandmasterEntity)==&value,
            "Tree GM entity lookup keeps category identity");
        if(i==0 || value.registry!=m::kGrandmasterEnemySubstitutions[i-1].registry
            || value.source!=m::kGrandmasterEnemySubstitutions[i-1].source)++candidateSources;
    }
    check(candidateSources==17,"Tree has 17 GM substitution sources and one two-category source");
    check(!m::grandmaster_substitution_source(m::kOpening,1)
        && !m::grandmaster_substitution(m::kOpening,14,0xBF95E58CU,0x80C0FA98U),
        "ordinary source and standard entity are not GM substitutions");
    for(const bool campaign:{false,true}) {
        const std::string lua=std::string(R"(
local composition=graph("composition","test",{step("mission",parallel("opening.module","opening.checked"))})
local boss=graph("boss","three rooms",{
 step("prepare","boss.prefight"),
 step("clear","boss.prefight.cleared",{after={"prepare"}}),
 step("reveal","boss.reveal",{after={"clear"}}),
 step("minotaur_spawn","boss.minotaur.spawned",{after={"prepare"}}),
 step("minotaur_dead","boss.minotaur.dead",{after={"minotaur_spawn"}}),
 step("defeat_objective","objective.defeat",{after={"minotaur_dead"}}),
 step("one","boss.fight1",{after={"reveal"}}),
 step("exit_one","boss.exit1",{after={"one"}}),
 step("enter_two","boss.room2.entered",{after={"exit_one"}}),
 step("two","boss.fight2",{after={"enter_two"}}),
 step("exit_two","boss.exit2",{after={"two"}}),
 step("enter_three","boss.room3.entered",{after={"exit_two"}}),
 step("three","boss.fight3",{after={"enter_three"}}),
 step("dead","boss.death",{after={"three"}}),
})
return mission{id=")")+(campaign?"mission_pact":"strike_pact")+R"(",
 graphs={composition,boss}, roles={mission="composition",opening="boss",ending="boss"},
 entry="composition",modules={"opening"},observations={"opening.checked"},phases={"boss"}}
)";
        std::string error;auto doc=coo::script::MissionDocument::parse_lua(lua,m::kProfile,error);
        if(!doc) std::fprintf(stderr,"phase Lua: %s\n",error.c_str());
        check(doc&&m::valid_document(doc->views()),"phase test uses authorized mission commands");
        m::Controller controller;check(controller.select(doc->views(),900),"phase test select variant");
        std::uint64_t now{};auto tick=[&]{for(int i=0;i<3;++i)(void)controller.update(900,++now,true,0);};tick();
        const auto generation=controller.frame().spawnGeneration;
        std::uint32_t nextActor=10;
        auto clear=[&](std::uint8_t cohort) {
            for(const auto& source:m::kAllSpawns) {
                if(source.cohort!=cohort)continue;
                for(unsigned i=0;i<source.count;++i){
                    m::EnemyReceipt r{900,nextActor++,77,generation,source.source,source.registry};
                    check(controller.admitted(r),"room source admitted");
                    check(controller.died(r),"room clear requires admitted death");
                }
            }tick();
        };
        clear(m::boss_cohort(m::kCohortPrefight));
        check(controller.frame().boss.sceneGeneration==generation,"native boss reveal requested");
        m::EnemyReceipt boss{900,nextActor++,78,generation,m::kBossSquad,m::kBoss};
        check(controller.admitted(boss)&&controller.boss_request().enemy==boss,"reserved named boss owns floor despite zero loose count");
        auto impostor=boss;++impostor.actor;
        check(!controller.admitted(impostor)&&!controller.health(impostor,0.F),"other salted actor cannot consume boss phase");
        check(m::boss_damage::blocked(controller.boss_request(),1.F),"boss immune during native reveal");
        m::EnemyReceipt minotaur{900,nextActor++,79,generation,m::kMinotaurSquad,m::kBoss};
        auto stale=minotaur;++stale.run;
        check(!controller.admitted(stale)&&!controller.frame().boss.minotaurSpawned,"stale Minotaur cannot announce spawn");
        check(controller.admitted(minotaur)&&controller.frame().boss.minotaurSpawned,"real Minotaur admission announces spawn");
        tick();check(controller.frame().objective!=m::kDefeatThuun,"Thuun objective waits through living Minotaur");
        auto wrong=minotaur;++wrong.actor;
        check(!controller.died(wrong)&&!controller.frame().boss.minotaurDead,"unadmitted Minotaur death rejected");
        check(controller.died(minotaur)&&controller.frame().boss.minotaurDead,"native Minotaur death retained immediately");
        tick();check(controller.frame().objective==m::kDefeatThuun&&!controller.frame().boss.sceneFinished,
            "Defeat Valus Thuun publishes on Minotaur death before full scene finishes");
        dawn::middleware::bap::activity_message::scene_sense::Output scene{};
        scene.delta=scene.completed=true;scene.generationWire=0x80000000U+generation;
        controller.scene(900,m::kBoss,m::kSceneMinotaur,scene);tick();
        check(controller.frame().boss.fights==1&&!controller.boss_request().requested,"room one opens after native scene");
        for(std::uint8_t room=1;room<=2;++room) {
            const auto request=controller.boss_request();const auto floor=m::boss_damage::floor(request.stage);
            check(request.stage==room-1&&!request.requested,"new room exposes next health third");
            check(controller.health(boss,floor),"native applied floor receipt accepted");
            check(controller.frame().boss.immune&&(controller.frame().boss.retreated&(1U<<(room-1))),"floor atomically arms retreat immunity");
            check(m::boss_damage::blocked(controller.boss_request(),floor),"later hits rejected during crossing");
            tick();check(controller.frame().boss.room==room,"room cannot advance without clear and arrival");
            clear(m::boss_cohort(static_cast<std::uint8_t>(m::kCohortRoom1+room-1)));
            check(controller.monitor(900,m::kBoss,m::kBossRooms[room].monitor,true,1,m::kLedgeMonitorOccupancyValue),"next-room native arrival accepted");
            tick();check(controller.frame().boss.room==room+1&&!controller.frame().boss.immune,"arrival releases immunity for next fight");
        }
        check(controller.boss_request().stage==2&&!m::boss_damage::blocked(controller.boss_request(),0.F),"final room permits lethal damage");
        controller.reset();check(!controller.boss_request().owner.valid()&&!controller.health(boss,0.F),"replay clears old health ownership");
    }
}
