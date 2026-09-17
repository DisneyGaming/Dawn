# Eater reactor: live location and recovered trigger geometry

Observed 13 September 2026, build 86657. This is inspection evidence, not gameplay acceptance.

## What the current session proves

The game loaded `raid_envy_v310` using public activity 536, scenario-local bubble **7**, slice set **56**, and forced spawn set **`0x1E8DBF89`**. The successful world-change and activity-start messages appear in `Dawn/logs/dawn.log` for process 65372, session `0x9EAA300100200008`, mission run 4. Map-global bubble index is 26; it is not interchangeable with the local bubble ordinal.

A read-only snapshot of Dawn's published local-player position returned **(67.859596, 127.178253, -767.907654)** at the final baseline capture. The snapshot used matching installed DLL symbols, a read-only process handle, and equal even sequence reads around the vector. Its nearest point in the selected spawn set is about 1.71 world units away. An earlier point in the same session, (59.830776, 132.760559, -767.910278), was also inside the first-path goal.

That live position lies inside the authored **`tv_first_path_goal`** polygon and vertical bounds, and outside **`tv_encounter_start`**. All six native points in spawn set `0x1E8DBF89` also lie inside the first-path goal volume. This identifies the location as the goal area for reactor path one. It does not establish the raid's original entrance spawn.

The user clarified that the “Waiting for other players” display can be toggled off. It is not treated here as proof of a missing multiplayer requirement or an encounter blocker.

## Baseline capture is complete

The user can leave this location. The point-in-time capture checked all 891 descriptor source tags from the inventory. **221 authored descriptors** were readable with source-tag, component-class, registry/type/slot, and repeated-header checks. This includes **all 187 recovered reactor descriptors**, 20 shared/global descriptors, and 14 other mouth-area descriptors. The remaining 670 did not pass the readability/identity checks; their state is recorded as unknown, not proof that the game lacks those assets.

A bounded native entity census found 602 allocated slots. It completed header and component checks for 599; three remained unverified. The allocation bitmap and table pointers were stable across the scan. No component in those verified entity rows matched the inventory's authored descriptor source tags. This is not an absence claim: the scan does not enumerate non-entity activity sensor components, which are needed to prove monitor and source activation.

This baseline is sufficient to move on with area inventory. It does not capture every possible state of the reactor. Platform rising/falling, occupied/unoccupied monitors, holdout waves, checkpoint changes, failure, and reset each require a later active-state observation.

## Recovered joins

All seven reactor player monitors have one serialized type-60 volume reference at source offset `0x270`, and each resolves to one named volume. The shared registry is **`0x686321C8`**. The volume definitions are in **`0x8155C3FC`**, in package `06AE`.

- First path: monitor `0x8155C468`, type 30 / slot 107 -> volume slot 235, offset `0xB50`, `tv_first_path_goal`. All six points of spawn **`0x1E8DBF89`** are inside.
- Second path: monitor `0x8155C46B`, type 30 / slot 108 -> volume slot 236, offset `0xC70`, `tv_second_path_goal`. All six points of spawn **`0xC4FDE8CD`** are inside.
- Third path: monitor `0x8155C46E`, type 30 / slot 109 -> volume slot 237, offset `0xD90`, `tv_third_path_goal`. All six points of spawn **`0xFD39D01C`** are inside.
- Fourth path: monitor `0x8155C471`, type 30 / slot 110 -> volume slot 238, offset `0xEB0`, `tv_fourth_path_goal`. All six points of spawn **`0x10BB2205`** are inside.
- Encounter start: monitor `0x8155C474`, type 30 / slot 111 -> volume slot 240, offset `0x10F0`, `tv_encounter_start`.
- Encounter space: monitor `0x8155C477`, type 30 / slot 112 -> volume slot 239, offset `0xFD0`, `tv_encounter_space`.
- Total alive count: monitor `0x8155C47A`, type 30 / slot 113 -> the same encounter-space volume, slot 239.

The start volume has bounds **(-76.5, 218.878510, -788)** through **(136.25, 240.112305, -679)**. Its full polygon and all goal polygons are saved in the geometry evidence. Use polygon containment plus height, not only an axis-aligned bounding box, when defining a spatial observer.

The recovery scanned descriptor classes `0x80809C36` and `0x808099D6` in packages `0221`, `06AE`, `0223`, and `01A4`, and recovered 329 volumes across those packages. That count is not the number of reactor volumes. The native registry/slot references select the six distinct reactor volumes above.

## Next location to inspect

Update after capture 02: the user loaded `0xAD98065D` successfully in bubble 7 / slice 56, mission run 5, session `0x9EAA30010020000A`. The read-only player position was **(5.913398, 367.385834, -771.161316)**, inside `tv_crossing_entry_door`, `tv_encounter_space`, and `tv_phase_mouth_traversal`. This supports the reactor-approach location described by the user as the entrance after the dropdown; the dropdown traversal itself was not observed. All 221 previously readable descriptors remained readable, including all 187 reactor descriptors, with no added or lost descriptor identities. The entity pool had 601 allocated slots, 598 verified rows, and three unverified rows; its bitmap/table checks were stable. **Capture 02 is complete; no additional reactor spawn is needed for this initial content-availability pass.** See its [summary](../../../build/coo/eater-live-inspection-20260913/capture-02-dropdown/capture-summary.json) and [file-hash manifest](../../../build/coo/eater-live-inspection-20260913/capture-02-dropdown/manifest.json).

The shared barrier/Argos destination is **`raid_envy_v310`, bubble 6 / slice 48 / spawn `0x68C397B7`**. All six spawn points lie in Argos registry `0xE8D290A0` volumes `tv_quarantine_breakout` and `tv_float_away_area`, and in barrier registry `0x91264981` volume `tv_quarantine_cooking`. The cache marks this hash as available in the map package for map-global bubble 25. [Capture 03](LIVE-ARGOS-20260913.md) now verifies successful live arrival and the area's descriptor availability. The [spawn-volume joins](../../../build/coo/eater-live-inspection-20260913/arena-spawn-research/spawn-volume-joins.json) also distinguish the treasure-chamber spawn `0x6641A7E6` and traversal spawns from this destination.

The reactor-approach spawn **`0xAD98065D`** has six points around **(0, 311, -772)**, on the higher-Y side of the start-trigger strip, while all four path-goal regions are on the lower-Y side. Capture 02 verifies a successful arrival. An explicit native mission-start command remains unrecovered, and none of the spawn points is inside the start trigger itself.

The user should operate the launcher and movement controls. The current location is already useful for reactor recovery; moving is not required to preserve or use the evidence gathered in this pass. Continue arrival/entrance work before treating a reactor research spawn as the complete raid's accepted opening.

## What remains unproved

- The original arrival/entrance spawn and route into the reactor.
- Live registration, activation, and occupancy receipts for the reactor monitors and objects. Package presence and coordinate containment do not prove those runtime states.
- Platform raise/lower operations, persistence, player-count predicates, and path-completion scheduling.
- Loyalist wave admission, authentic deaths, exit unlocking, and checkpoint reset.
- The solo adaptations and a successful unassisted playthrough.

The existing inventory's 56 platforms, four paths, 32 squad sources, and 15 spawn regions remain package evidence. This pass adds spatial joins and a live location, not a claim that those objects are all active.

## Evidence and reproduction

- [Saved geometry and spawn joins](evidence/reactor-geometry-20260913.json): source tags, offsets, SHA-256 hashes, polygons, cache provenance, and containment results.
- [Read-only player snapshot](../../../build/coo/eater-live-inspection-20260913/player-position.json): process, installed DLL identity, symbol addresses, sequence, timestamp, and live position.
- [Position reader](../../../build/coo/eater-live-inspection-20260913/read_position.py): takes the current game PID; has no UI or memory-write operations.
- [Native area snapshot](../../../build/coo/eater-live-inspection-20260913/native-area-snapshot.json): descriptor identity checks, entity census, stable-pool result, and explicit limits.
- [Native census reader](../../../build/coo/eater-live-inspection-20260913/capture_native.py): enabled by `--capture-native` on the position reader; checks the pinned installed executable and mapped resolver prefix before reading native tables. The local RE-image hash is recorded separately because it differs from the older lifecycle contract's reference image.
- [Launch and session log extract](../../../build/coo/eater-live-inspection-20260913/launch-and-session-log.txt): bounded relevant lines, with network payload dumps excluded.
- [Geometry recovery](../../../build/coo/eater-live-inspection-20260913/recover_reactor_geometry.py): reads installed packages and the saved position. It checks unique monitor joins and all 24 goal-spawn points.

No runtime source, installed DLL, game settings, destination, or player state was changed by these readers. The user has explicitly prohibited further computer takeover; future inspection must respect that restriction.
