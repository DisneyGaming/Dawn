# Omega: how we started Panoptes

We connected the lair entry checks to Panoptes's authored spawn, animation graph, camera cinematic, and encounter actions. The working flow is:

`Enter doorway → spawn Panoptes → queue intro → observe flight → play camera → approach hovering boss → summon → left/right waves → clear enemies → boss departs`

1. **Entering the lair made the boss eligible.** In loaded bubble `14`, crossing the authored `tv_spawn_boss` doorway volume latches `bossDoorReached`. The larger `tv_intro_area` also catches a player who has already passed the thin doorway. With the route at least `crownEntrance`, we request one Panoptes from native `sq_boss` (`0x80F4756A`) through spawn entry `+0x4E2E80`; native code chooses the member and `sr_boss_location_1` placement.

2. **We started his actual intro animation graph.** Once the boss member is enabled, its character/animation/biped bindings are valid, and the cinematic is registered, we submit a kind-9 named-sequence command through `+0xAB6C60`. It targets group `0xAFB11A12`, sequence `0x65D2379F`, using graph `0x80F45178`. We confirm the queue was accepted and observe native graph playback after `+0xF4E660` returns. This is what starts his authored lead-in and flight motion.

3. **His flight triggered the camera cutscene.** A confirmed node-1 flight playback sets `flightSeen`. The cinematic hook then resolves selector `0xA74B2200` through `+0xC4C1A0` and calls native start `+0x1069CC0` on intro component `0x80F478D3` (type 6, registry `0xF4D0E0B2`, slot `22`). We observe the native playing flag at `component + 0x260`; seeing it active and then inactive records completion. Resource registration and real flight playback gate the start.

4. **Moving closer released the summon transition.** The player coming within 18 units of `(-1491.7739, 415.2429, -26.9186)` advances the route to `reveal`. Once Panoptes is in native hover, we add named condition `0xC0F9C866` through `+0xC620F0` on his character. The graph takes its authored summon transition and returns to idle. Observing summon followed by idle sets `introIdleSeen`, which allows the combat handoff. Camera completion is tracked separately; the combat gate is this native animation sequence plus the closer approach.

5. **His arm animations started the fight.** After `introIdleSeen`, we bind the mission state and request the left summon by setting `panoptes_summon_left` (`0xA2AE120F`) to `1.0` through native property setter `+0x576420`. We confirm clip `0x80F4518B` is playing in the full-body output. That playback receipt admits the first 12 enemies across source slots `3..8`, two per source. When the native clip wraps, we reset the property to `0.0` and advance to the right summon (`0x8496ABD2`, clip `0x80F4518A`). The initial batches can overlap.

6. **Clearing the waves made him move on.** After the right animation finishes and both initial cohorts are confirmed dead, mission state requests `depart`. We stop the intro sequence through `+0xC693F0`, wait for its queue to finish, then invoke native movement selector `0x80F45176` through `+0x10C6AF0` with the authored destination. Its mode-4 action handles the disappearance, placement, reappearance, and effects. We acknowledge departure only after native movement stages, cleanup, selector idleness, and the published position agree. Player arrival and prepared launchers then gate the next island encounter.

The essential implementation detail was **requesting authored native actions and advancing on confirmed playback/completion**. We also keep the run, generation, actor, character, biped, and entity identities matched throughout; those handles represent different objects.

Code references:

- [Entry checks](<C:/Destiny 2 Development/Sunrise/src/state/activity/omega/omega_progression.h:100>) and [boss spawn](<C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/omega_boss_spawn.h:24>).
- [Intro queue and summon condition](<C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/omega_boss_graph_runtime.inl:280>) and [camera start/completion](<C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/omega_cinematic_runtime.inl:2>).
- [Arm animation handoff](<C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/omega_boss_combat_runtime.inl:161>), [mission progression](<C:/Destiny 2 Development/Sunrise/src/state/activity/omega/omega_mission_state.h:57>), and [native departure](<C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/omega_mission_motion.inl:130>).

Based on the current source.
