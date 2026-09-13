# Native package and object map

## Scenario record

The cache row at scenario index 424 is `raid_envy_v310`:

- scenario tag `0x80B49E7A`, class `0x80809994`
- package-definition hash `0x3B50D933`
- scenario package `0x01A4`, files `w64_leviathan_activities_01a4_{0,1,2,3,5}.pkg`
- spawn stem `leviathan`
- eight bubbles, one state in each bubble
- one global roster group, no bubble-intersection groups
- zero cached authored-overlay groups in every bubble

## Public activity rows

Public table `0x81327CF0`, class `0x808076F0`, has three records whose entry hash, record hash, and native name agree:

- ordinal 536, identity `0xB8218A8C`, record offset 634,256
- ordinal 537, identity `0x81029D0A`, record offset 634,824
- ordinal 538, identity `0x303AF7C6`, record offset 635,400

All three carry power 750 and difficulty-settings tag `0x813206EE`, class `0x80807D82`. Ordinals 536 and 537 use gameplay-settings hash `0x69A905F7`; ordinal 538 uses `0x8A1DF706`.

Display table `0x81327D35`, class `0x80805E0A`, maps each ordinal to the identical title `Leviathan, Eater of Worlds` and description `"In the belly of the beast."` It does not distinguish a normal/prestige or other variant label. The differing identities and settings are proven, but their player-facing meaning remains unresolved.

The direct scenario walk resolves these slice entries and registries:

1. Map index 13, hash `0x5BEA37AE`, name unresolved in the hash-name cache: entry `0x80B49E72`, registry `0x80B49EE9`.
2. Index 14, `raid_gluttony_baths`, hash `0x5EF90E99`: entry `0x80B49E73`, registry `0x80B49EE9`.
3. Index 15, `raid_gluttony_berth`, hash `0x110C94E4`: entry `0x80B49E74`, registry `0x80B49EDB`.
4. Index 16, `raid_gluttony_castellum`, hash `0x5E7FE1E7`: entry `0x80B49E75`, registry `0x80B49EE9`.
5. Index 17, `raid_gluttony_gardens`, hash `0x413547F5`: entry `0x80B49E76`, registry `0x80B49EE9`.
6. Index 24, `raid_gluttony_throne`, hash `0xA3FA668F`: entry `0x80B49E77`, registry `0x80B49EE9`.
7. Index 25, `raid_wing_envy_belly`, hash `0x8CAD7143`: entry `0x80B49E78`, registry `0x80C43A40`.
8. Index 26, `raid_wing_envy_mouth`, hash `0x217C510E`: entry `0x80B49E79`, registry `0x80C46065`.

The named map roots include `0x80B49E79` (`map:leviathan:77c491bb:root` and `map:leviathan:root`) and `0x80EDCB16` (`map:leviathan_v310:root`). Spaceflight names include `raid_envy_v310:spaceflight_filler` (`0x80FEB810`) and `raid_envy_v310:spaceflight_out` (`0x80FB501A`). These are name records, not a proved arrival spawn set.

## Package roles

The extraction follows every referenced tag rather than assigning meaning from filenames. The mechanically important package families are:

- `0x01A4` — `w64_leviathan_activities`: scenario, slice entries, and shared Berth objects.
- `0x0221` — `w64_leviathan_activities`: the Eater belly and mouth registries and the three large encounter object groups.
- `0x0223` — supporting Eater activity descriptors reached from the placed-object graph.
- `0x06AE` — `w64_leviathan_v310_activities`: shared v310 descriptors used by Eater objects.
- `0x023A` — `w64_activities`: the installed detain-sphere object behavior.
- `0x03A1` — `w64_raids`: thunder-wall safety and alpha-strike test/wipe behaviors.
- `0x03A2` — `w64_activities`: the production alpha-strike and detain-finale behavior, plus the two referenced scene graphs.
- `0x038F` — dialogue bank `0x80F1F9A5`.
- `0x03D1` and `0x0238` — localized objective/dialogue containers reached from the recovered banks.
- `0x01F5`, `0x01FD`, `0x0206`, `0x0207`, `0x0208`, `0x020C`, `0x020D`, `0x03DA`, and `0x06D0` — entity, descriptor, or linked-asset packages reached by resolved encounter records. Their exact file lists and byte sizes are in the JSON evidence.

Other filename-adjacent map packages, such as `w64_leviathan_036e_*` and `w64_leviathan_v310_06ad_*`, are useful residency candidates. They are not added to the proven dependency set unless a recovered handle reaches them.

## Placed-object groups

The direct object graph yields 19 registry-key groups and 891 resolved client descriptors.

### Cached shared groups

- `0x80FA2C97`, key `0x24C67333`: 20 of 20 descriptors. It carries `scoreboard`, `hard_wipe_globals`, `lifetime_timer`, `lifetime`, and `players[0]` through `players[15]`.
- `0x80B49EB8`, key `0xA9E6185F`: 4 of 5 descriptors. It carries `seq_berth_music`, `d_entry_door`, `m_engagement_sensor`, and `pm_entry_door`; one declaration is host-only.
- `0x80B49EDA`, key `0xB270AC62`: 1 of 2 descriptors, `pt_phase_berth_traversal`; one declaration is host-only.

### Belly groups

- `0x80C420D4`, key `0x93BF5E9D`: 22 client descriptors from 44 declarations. Airlock, ejection tube, piston, wind, seven hoops, treasure chest, marker, and engagement sensor.
- `0x80C42FA3`, key `0xE8D290A0`: 335 client descriptors from 506 declarations. Argos, three shield sections, twenty-seven missiles, twelve Harpy swarm squads, combat spaces, fifteen relic groups, nine fire stations, teleporter, damage/immunity/wipe actions, two scenes, reveal cinematic, loot, and cleanup.
- `0x80C43A1D`, key `0x91264981`: 301 client descriptors from 432 declarations. Six oracle spawners, three elemental-side objectives, fifteen side squads, relic/fire channels, oracle wipes, six spawn-region pairs, gates, loot, and engagement.
- `0x80C43A21`, `0x80C43A26`, `0x80C43A2A`, `0x80C43A2E`: four small groups whose only resolved client descriptor is the `raid_wing_envy_belly` marker.
- `0x80C43A3E`, key `0xE34F861D`: `pm_phase_belly_traversal`, `pt_phase_belly_traversal`, and `pt_thundering_wall_spawnpoint_update`.
- `0x80C43A46` and `0x80C43A4B`: zero declared slots; they remain graph roots with keys `0x9FFBBD32` and `0x96E0A5E5`.
- `0x80C43A59`, key `0xD6E30062`: directive, music, and dialogue sensors. It reaches objective table `0x80C43A4C` and dialogue bank `0x80F1F9A5`.

### Mouth groups

- `0x80C43AAF`, key `0x5654D7FD`: false floor, crossing entry door, their trigger volumes, and engagement sensor.
- `0x80C43FF4`, key `0x686321C8`: 187 client descriptors from 331 declarations. It holds 56 falling platforms across four paths, 32 squads, 15 spawn regions, five encounter objectives, four checkpoint/klaxon gates, seven progression volumes, two sequences, loot, raid banner, and death mechanic.
- `0x80C43FFF`, key `0x18C48E46`: mouth-crossing phase task and mouth marker.
- `0x80C46009`, key `0xDA089B5B`: a second mouth-crossing task and mouth marker.
- `0x80C46064`, key `0x0941F43C`: mouth-traversal phase task and `pm_door_opens`.

## Cache publication gap

Only the three shared groups are published under their original object tags in the version-54 `RosterGroupRecord` domain. A fourth matching registry key (`0x9FFBBD32`) is published for a different object tag and layout. The package-resolved Eater groups in `0x0221` are absent, and the scenario row's authored group counts are all zero.

This follows the current same-package authored selection rule: the scenario is in package `0x01A4`, while the exclusive encounter roots are in `0x0221`. The authoring layer cannot safely refer to those groups until the catalog accepts and publishes the cross-package provenance. Preserve the original slot indices and the declared-versus-resolved distinction when closing this gap.
