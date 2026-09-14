-- Eater of Worlds: a single real player, native actors and native encounter receipts.
-- The graph owns encounter order. Native adapters own platform/cranium/boss cycles.
-- A missing native receipt keeps its gate pending; elapsed time cannot clear it.
local composition=graph("composition","Eater of Worlds - Solo",{
    step("raid",parallel("mission.module","mission.checked")),
})
local arrival=graph("arrival","Enter the Leviathan",sequence(
    step("arrive","region.entrance"),
    step("welcome",parallel("objective.0","dialogue.2","respawn.allow")),
    step("door_approach","entrance.pm_entry_door"),
    step("door","entrance.d_entry_door.on"),
    step("enter","region.mouth")
))
local reactorSteps={
    step("dropdown_approach","reactor_entry.pm_false_floor"),
    step("dropdown_open","reactor_entry.d_false_floor.on",{after={"dropdown_approach"}}),
    step("crossing_approach","reactor_entry.pm_crossing_entry_door",{after={"dropdown_open"}}),
    step("crossing_open","reactor_entry.d_crossing_entry_door.on",{after={"crossing_approach"}}),
}
local paths={"first","second","third","fourth"}
local counts={13,11,13,19}
local previous="crossing_open"
for path=1,4 do
    local id="path"..path
    local begin={"reactor.path"..path..".begin"}
    if path==1 then
        begin[#begin+1]="objective.2";begin[#begin+1]="respawn.restrict"
        begin[#begin+1]="reactor.o_exit_grate.on"
    end
    reactorSteps[#reactorSteps+1]=step(id.."_begin",begin,{after=previous and {previous} or {}})
    local prior=id.."_begin"
    for first=0,counts[path]-1,8 do
        local commands={}
        local last=first+7
        if last>=counts[path] then last=counts[path]-1 end
        for n=first,last do
            commands[#commands+1]="reactor.u_falling_platforms_"..paths[path].."_path["..n.."].o_platform.on"
        end
        local create=id.."_create"..first
        reactorSteps[#reactorSteps+1]=step(create,commands,{after={prior}})
        prior=create
    end
    previous=id.."_completed"
    reactorSteps[#reactorSteps+1]=step(previous,"reactor.path"..path..".finished",{after={prior}})
end
local reactor=graph("reactor","Escape the Reactor",reactorSteps)
-- Loyalist passengers: two Colossi center, three Incendiors left, five Psions right.
local holdoutNames={"reactor.sq_holdout_finale_center","reactor.sq_holdout_finale_left",
    "reactor.sq_holdout_finale_right"}
local requests,ready,clear={},{},{}
for _,name in ipairs(holdoutNames) do
    requests[#requests+1]=name..".request";ready[#ready+1]=name..".ready";clear[#clear+1]=name..".cleared"
end
local holdoutSteps={
    step("begin",parallel("objective.3","dialogue.5","respawn.restrict")),
    step("reinforcements",requests,{after={"begin"}}),
    step("ready",ready,{after={"reinforcements"}}),
    step("clear",clear,{after={"ready"}}),
    step("grate_created","reactor.o_exit_grate.ready",{after={"clear"}}),
    step("exit",parallel("respawn.allow","objective.4","reactor.exit.open",
        "reactor.encounter_base_loot.o_chest.on","reactor.encounter_base_loot.o_chest_fx.on"),{after={"grate_created"}}),
    step("exit_opened","reactor.exit.opened",{after={"exit"}}),
}
local holdout=graph("holdout","Defeat the Loyalists",holdoutSteps)
local crossingReached=condition("route.crossing",any_of("group_18C48E46.pt_phase_mouth_crossing",
    "group_DA089B5B.pt_phase_mouth_crossing","group_18C48E46.tv_phase_mouth_crossing","group_DA089B5B.tv_phase_mouth_crossing"))
local arenaReached=condition("route.arena",any_of("argos.tv_quarantine_breakout","barrier.tv_quarantine_cooking"))
local hoops={}
for hoop=0,6 do hoops[#hoops+1]="traversal.u_hoop_puzzle.o_hoops["..hoop.."].on" end
local traversal=graph("traversal","Delve deeper",sequence(
    step("briefing",parallel("objective.4","dialogue.4","respawn.allow")),
    step("leave_reactor","route.crossing"),
    step("belly_loaded","region.belly"),
    step("hoops",hoops),
    step("route_objects","traversal.o_airlock_exit_door.on"),
    step("entrance_open","traversal.d_airlock_entrance.on"),
    step("airlock_interior","traversal.pm_airlock_interior"),
    step("exit_open","traversal.d_airlock_exit.on"),
    step("exit_opened","traversal.exit.opened"),
    step("tube_open","traversal.d_ejection_tube.on"),
    step("tube_opened","traversal.ejection.opened"),
    step("suction",parallel("traversal.o_wind_volume.on","traversal.o_secondary_volume[0].on",
        "traversal.o_secondary_volume[1].on","traversal.o_secondary_volume[2].on","argos.arrival.begin")),
    step("airlock_exterior","traversal.pm_airlock_exterior"),
    step("ejection","traversal.d_piston.on"),
    step("arena_arrival","route.arena"),
    step("hoop_and_arrival","argos.arrival.landed")
))
local barrier=graph("barrier","Break the barrier",sequence(
    step("begin",parallel("objective.5","respawn.restrict","barrier.cycle.begin")),
    step("targets","barrier.cycle.finished")
))
local argos=graph("argos","Destroy Argos",sequence(
    step("begin",parallel("objective.6","dialogue.1","argos.cycle.begin","respawn.restrict")),
    step("boss_source","argos.boss_manager.sq_boss.request"),
    step("intro","argos.boss_manager.scene_boss_intro_animation.on"),
    step("boss_created","argos.boss_manager.sq_boss.ready"),
    step("victory","argos.cycle.finished")
))
local ending=graph("ending","Calus's invitation",sequence(
    step("safe",parallel("respawn.allow","dialogue.0")),
    step("closing_exchange","dialogue.finished.0"),
    step("complete","mission.finish")
))
return mission{
    id="eater_of_worlds",graphs={composition,arrival,reactor,holdout,traversal,barrier,argos,ending},
    roles={mission="composition"},entry="composition",modules={"mission"},observations={"mission.checked"},
    -- One hoop plus the arena trigger stages the objective and darkness zone. The execution
    -- boundary stays pending until the barrier fight is implemented later.
    phases={"arrival","reactor","holdout","traversal","barrier"},
    conditions={crossingReached,arenaReached},
    parameters={platform_hold_ms=500,convergence_window_ms=90000,damage_window_ms=45000},
}
