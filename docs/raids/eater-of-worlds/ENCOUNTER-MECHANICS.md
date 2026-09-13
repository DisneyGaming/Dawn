# Encounter mechanics and native assets

This document puts player-visible behavior beside installed native evidence. Timings and flow from community sources remain labeled as gameplay references until an installed constant or live receipt confirms them.

## Objective spine proven in packages

Objective table `0x80C43A4C`, class `0x80804F72`, contains seven event rows:

1. `0x54DECB72` — **Explore the Leviathan**: “The Leviathan appears to be accessible. It might pay to explore.”
2. `0x70E54B0C` — the same title and body under a distinct event hash. Preserve both rows.
3. `0x440F76C5` — **Escape the reactor**: “Find a way out of the Leviathan's reactor room.”
4. `0x7F4355DF` — **Defeat the Loyalists**: “Eliminate the attacking Loyalists.”
5. `0xAF9A3F59` — **Delve deeper**: “Venture deeper into the Leviathan.”
6. `0x6BC38FD1` — **Break the barrier**: “Destroy the barrier protecting Argos, Planetary Core.”
7. `0x4E2D6094` — **Destroy Argos**: “Destroy Argos, Planetary Core to rectify Leviathan's engine problem.”

The event hashes and strings are installed proof. The order above is serialized row order and agrees with known play flow, but a separate authoring graph must still bind the transition condition for each row.

## Phase 1: reactor platforms and holdout

### Known gameplay behavior

A team advances across appearing platforms. Occupied platforms become unsafe and eventually sink, so players move in order. The route pauses at checkpoints. At the final holdout, the team splits and clears Loyalists, including heavy enemies, before the exit opens.

Community references give approximately 30 seconds before expiry, an 8-second red warning, and a 2-second delay before the next platform. Treat those timings as test targets, not build-86657 constants.

### Installed native evidence

Group `0x80C43FF4` provides the complete mouth encounter surface:

- 56 `o_platform` devices: first path 13, second 11, third 13, fourth 19.
- Four path goal volumes: `pm_first_path_goal` through `pm_fourth_path_goal`.
- Encounter volumes `pm_encounter_start`, `pm_encounter_space`, and `pm_total_alive_count`.
- Five type-3 objective records: `holdout_objective`, `holdout_door_objective`, `anchor_objective`, `dash_objective`, and `cluster_objective`.
- Four gates: `d_klaxon_light_trigger` and checkpoint light triggers 1 through 3.
- 57 filters: `of_platforms_held` and one `of_all_players` filter for every platform.
- Audio sequences `seq_door_alarm_audio` and `seq_holdout_complete_audio`.
- Exit grate, raid banner, encounter loot chest/effect, path-complete and drop-path incident proxies, engagement sensor, and death action.

The 32 squads divide into holdout snipers; left/right primary and secondary fodder; left, right, and center anchors; three finale squads; three island-anchor squads; six dash snipers; and three cluster snipers. Their source entity candidates are preserved per six-lane selection in the JSON. The recurring installed candidates include `0x80C1A8E4`, `0x80BFAA15`, `0x80BFAA1A`, `0x80BFAA21`, `0x80C197CF`, `0x80C0FA98`, and `0x80BFAA20`.

The names and candidate rows prove roster topology. They do not prove how many entries a live wave selects or that every inventoried squad should be requested together.

## Phase 2: mouth-to-belly traversal

### Known gameplay behavior

The party passes the false floor and airlocks, crosses the piston room, uses the ejection cannon, and flies through seven rings to reach the deeper engine space.

A community reference describes a piston arc blast around a 13-second cadence. Keep that as a verification target until a native timer or live observation establishes the actual value.

### Installed native evidence

- Mouth group `0x80C43AAF`: `d_false_floor`, `d_crossing_entry_door`, `pm_false_floor`, `pm_crossing_entry_door`, and engagement sensor.
- Mouth progression groups: two `pt_phase_mouth_crossing` tasks, `pt_phase_mouth_traversal`, `pm_door_opens`, and the mouth marker.
- Belly traversal group `0x80C420D4`: wind volume, treasure chest, seven named hoop objects, airlock exit door, three secondary volumes, `d_ejection_tube`, both airlock doors, `d_piston`, interior/exterior airlock volumes, exterior toggle, marker, and engagement sensor.
- Belly progression group `0x80C43A3E`: `pm_phase_belly_traversal`, `pt_phase_belly_traversal`, and `pt_thundering_wall_spawnpoint_update`.

These names prove the objects and phase interfaces. The trigger geometry, device values, ring-success aggregation, checkpoint update rule, and piston cadence remain unresolved.

## Phase 3: break Argos's barrier

### Known gameplay behavior

Players carry Vex craniums from sockets to Solar, Arc, and Void charging stations. Charged craniums destroy matching elemental targets around Argos. Adds and Minotaurs pressure the three sides. The encounter wipes if the team misses required targets.

Community material describes three mines per set, 55-second intervals, a 72-second mine timer, about 28 seconds to charge a cranium, 40 shots per cranium, and about 23 shots per mine. None of those values has been recovered from installed constants here.

### Installed native evidence

Barrier group `0x80C43A1D` provides:

- Six `oracle_spawner` structures. Each has top, middle, and bottom object triplets and three `seq_oracle_spawn` sequences.
- Six oracle geometry objects and six matching device gates.
- Per-spawner wipe action, instant-kill action, all-player and in-volume filters, and a shared wipe channel.
- Fifteen side squads: two regular, one cloud, and two anchor squads for each of altar, cliffs, and grove.
- Three encounter objectives: `obj_altar`, `obj_cliffs`, `obj_grove`.
- Six side spawn regions: two each for altar, cliffs, and grove.
- Fifteen relic groups with four relic devices and four relic channels each.
- Nine fire devices, interactables, channels, and trigger volumes.
- `d_cooking_boss_shield`, chest/effect, engagement sensor, and raid death mechanic.

The side squad candidates use `0x80C0D038` for regular/cloud enemies, `0x80C0D298` for anchors, and sometimes `0x80BFBDAF` in the first regular squad on each side. The data does not assign the player-facing Solar/Arc/Void labels to altar/cliffs/grove. Bind that relationship only after a native field, string, visual test, or authoritative capture proves it.

The package calls the targets `oracle` while community guides call them quantum mines. Preserve the native names in bindings and use the familiar term only in presentation or comments.

## Phase 4: destroy Argos

### Known gameplay behavior

Teams charge elemental craniums and fire them at matching shield nodes so three beams converge and open a damage phase. During damage, Argos can detain players in cages. Later it prepares a wipe attack; players break exposed weak points to interrupt it. The cycle repeats until the boss dies.

Community references describe a roughly 30-second shield breach window, a damage boost while standing in unstable energy, and a final wipe interrupt that requires two of six weak points. Use these only as acceptance hypotheses.

### Installed native evidence

Argos group `0x80C42FA3` provides:

- `boss_manager.sq_boss`, object candidate `0x80C75F82`, and boss cell `sq_boss__cell_1`.
- `boss_manager.obj_boss`, alpha-strike device, three shield sections, and one shield-state channel per section.
- Nine named missiles under each shield section: three missile managers with three missiles each, for 27 missile objects total.
- `seq_missile_spawn` and `seq_missile_fail`.
- One Harpy swarm objective, four swarm squads, and a swarm spawn region per shield section.
- Section-specific DPS buff actions and filters.
- 23 boss-geometry devices and matching gates, plus `d_cooking_boss_shield` and `d_environment_shield`.
- Boss instant kill, damage buff, damage debuff, immunity, burning, float-away, and death-mechanic actions with their filters.
- Nine fire stations and fifteen four-channel relic groups.
- Circle, rig, and stairs regular squads, anchor squads, shield-pressure squads, objectives, and spawn regions.
- Player teleporter destinations/returns and actions to teleport, return, kill, or hide waypoints.
- Boss intro cinematic, two native scene descriptors, raid banner, loot chest/effect, treasure-chamber phase task, and engagement sensor.

Regular arena squads use entity candidate `0x80C0D038`, anchors use `0x80C0D298`, Harpy swarms use `0x80C0D08A`, and Argos uses `0x80C75F82`. All recovered squad definitions carry the sentinel rule registry `0x811C9DC5`, type 255, slot 65535. This proves that a separate explicit rule binding was not serialized there; it does not authorize inventing one.

### Scenes and dialogue

The two type-43 descriptors are:

- `boss_manager.scene_boss_alpha_strike_facing_1`, source `0x80C421A1` offset 872, reaching scene graph `0x80F444FB`.
- `boss_manager.scene_boss_intro_animation`, source `0x80C424FF` offset 872, reaching scene graph `0x80F44500`.

The type-6 reveal descriptor is `specops_envy_boss_reveal.boss_intro_cinematic._cinematic`, source `0x8155C01E` offset 744. The two scene graph tags do not supply their event prerequisites or cast contract by themselves.

Dialogue bank `0x80F1F9A5`, class `0x80808D54`, has six selector rows:

- `0x0B6F9EC9`, 14,656 ms: opening variants about the Leviathan's engine problem.
- `0xE00FB2F6`, 8,665 ms: invitation to dive deeper variants.
- `0xDA634408`, 13,281 ms: return/welcome variants.
- `0x2A61BC44`, 9,957 ms: Argos defeated and engine restored variants.
- `0x55C33B57`, 38,476 ms: final safety/reward variants.
- `0x74F16395`, 11,628 ms: Loyalists stand down and repeat-run variants.

The JSON preserves every recovered text alternative. Selector order in the bank is not story order. Scene-owned and host-owned dialogue must be distinguished before authoring to avoid duplicate playback.

