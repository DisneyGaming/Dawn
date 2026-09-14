# Eater traversal wiring, bounded at barrier-arena arrival

13 September 2026. This record covers the route after the reactor holdout and ends when the player naturally enters the first Argos/barrier arena. It does not implement or arm the barrier encounter.

The user explicitly confirmed that the final requested door is after the full
underbelly traversal, at its exit toward Argos. It is separate from the grate
opened by the Loyalist clear. The current Lua preserves that entire walking route.

## Proven route identities

The generated catalog and `runtime-bindings.json` establish these native identities:

- Mouth devices: false floor `0x5654D7FD:23:0` and crossing door `0x5654D7FD:23:1`. Their package monitors are type-30 slots 3 and 4; their respective volumes are slots 6 and 5.
- Mouth progression: two distinct `pt_phase_mouth_crossing` owners (`0x18C48E46` and `0xDA089B5B`) share identical geometry but remain distinct task identities. `pt_phase_mouth_traversal` is in `0x0941F43C`; its route volume is slot 3 and `pm_door_opens` is slot 1.
- Belly devices: ejection tube type-23 slot 0, airlock entrance/exit type-23 slots 1/2, wind object slot 3, seven hoop objects in authored suffix order, piston type-23 slot 12, exit-door object slot 13, and three secondary objects slots 14-16 in registry `0x93BF5E9D`.
- Airlock observations: interior/exterior type-30 monitors slots 18/19 join to volume slots 31/36. The exterior toggle is a separate type-32 slot 20.
- Belly progression: `pt_phase_belly_traversal`, `pm_phase_belly_traversal`, and `pt_thundering_wall_spawnpoint_update` are registry `0xE34F861D` slots 0/1/2. The monitor joins to belly traversal volume slot 4. The checkpoint task has the nearby spawnpoint-update volume slot 3.
- Natural arena arrival: Argos `tv_quarantine_breakout` (`0xE8D290A0`, slot 379) and barrier `tv_quarantine_cooking` (`0x91264981`, slot 319) have the same recovered polygon. Either is an arena-arrival observation, not permission to begin combat.
- Probable final underbelly door: the group has exactly three type-23 gates using the same `0x80804F45` component and `0x80804F47`/`0x80804F48` sense/authority schemas. Slots 1 and 2 are the two named airlock doors. By package name and exclusion, the remaining slot 0, `d_ejection_tube`, is the candidate for the final same-class tube door and launcher before the receiving flight. The matching wire class is proven; the physical-role identification remains an inference until observed in play or joined through a behavior reference. Its descriptor contains no world transform, so no exact door coordinate is claimed. The separate `o_airlock_exit_door` object is at `(-71.21055, -519.00085, -1037.48840)` in the early airlock and is not the arena-end door.
- Safe checkpoint spawn: all six points of spawn set `0x7DA4DB1D` are inside both `tv_thundering_wall_spawnpoint_update` and `tv_thundering_wall_safe_area_19`. A representative authored point is `(6.1650009, -162.9543457, -929.0970459)`.
- Arena spawn: all six points of spawn set `0x68C397B7` are inside `tv_phase_belly_traversal`, `tv_quarantine_breakout`, and `tv_quarantine_cooking`. A representative authored point is `(-43.0622292, -1303.7320557, -1909.2849121)`.

A byte-level comparison at each type-23 descriptor's catalog offset gives a tighter boundary on the final-door evidence. Ejection tube, both airlock doors, and piston carry the same referenced schema tags throughout the descriptor (`0x80809FBD`, `0x8080919F`, `0x808091A4`, `0x80809B1E`, `0x80809A6E`, `0x808094EB`, and `0x80800065`). Their differing fields are the source identity, package slot, per-entry hash, and name. No ejection-specific behavior reference or transform occurs in this descriptor. Consequently, static package evidence proves that `.on` reaches the same position channel as the working airlock doors, but it cannot prove that position `1` performs the authored ejection animation or moves the player.

`doors.h` now preserves these joins as typed constants. The seven-hoop array is indexed by authored hoop suffix. The package slot order puts hoop 6 before hoops 3-5, so raw slot order must not be treated as puzzle order.

## Directly supported wiring

The existing Eater authority can publish type-23 position bodies for the false floor, crossing door, both airlock doors, piston, and ejection tube. An `.on` request sends position `1` to `d_ejection_tube` through exactly the same native gate schema used by the two airlock doors. This proves the correct wire channel for the probable final door. The physical-role inference and complete launch/player-control handoff still need live acceptance. Its generic type-4 object path can create and acknowledge the wind object, hoops, traversal chest, exit-door object, and secondary objects. Type-30 and type-31 bodies are already published, and current position observation can latch every recovered type-60 volume.

This supports a conservative graph that opens the already-bound mouth devices, observes the mouth route, creates visible belly objects, sequences the two airlock doors against interior/exterior observations, and ends on one of the overlapping arena volumes. Device application acknowledgement exists for type 23 in the controller, but no client producer calling `observe_device` was found in current source. A route step must therefore use the matching natural monitor/volume receipt as its passage gate rather than claim a type-23 acknowledgement that cannot currently arrive.

## Integrated route policy

The Lua traversal graph now replaces the earlier `briefing -> pm_airlock_interior -> region.belly` shortcut with bounded route steps. Its supported ordering is:

1. Present objective 4/dialogue 4 and allow respawn.
2. After the holdout, observe either mouth-crossing owner or its matching slot-2 volume while keeping both task identities in the condition. Their polygons are identical and current evidence does not select one owner as the universal variant. Do not use `pt_phase_mouth_traversal` as this post-holdout gate: its slot-3 volume covers the earlier reactor approach and may already be latched.
3. Create the wind object, seven hoops, exit-door object and secondary objects in batches of at most eight commands. Their creation acknowledgement proves visibility only.
4. Command the airlock entrance open, wait for `traversal.pm_airlock_interior`, command the entrance closed, command the exit open, then wait for `traversal.pm_airlock_exterior`. Do not command the type-32 exterior toggle until its authority body is recovered.
5. Command the piston only if merely enabling its recovered type-23 device is the intended conservative policy. Do not encode the community 13-second cadence or infer damage/safe-zone behavior.
6. Command the ejection tube and wind object, but keep progression waiting on natural receiving observations. The current source does not prove an interaction prompt, launch operation, or control-handoff receipt.
7. Observe belly traversal and finish only when either `argos.tv_quarantine_breakout` or `barrier.tv_quarantine_cooking` is entered.

The thundering-wall checkpoint is not a safe late gate for the ejection command. Its recovered bounds are `x [-3.58, 16.01]`, `y [-168.80, -161.12]`, `z [-939.20, -914.20]`; the later airlock-exterior bounds are `x [-88.08, -54.29]`, `y [-409.39, -296.70]`, `z [-1044.25, -1021.25]`. The checkpoint is on the earlier, higher part of the natural route. The airlock-exterior observation is the latest exact pre-arena observation currently recovered, so commanding the candidate ejection door after that receipt is the narrowest supported placement. The graph must still wait for natural arena entry.

The parsed phase list and `valid_document` now end at traversal (four phases:
arrival, reactor, holdout, traversal). Natural arena entry sets `routeComplete`
without starting the barrier graph or claiming full raid completion.

The controller now accepts the exact volume fallbacks for mouth crossings,
airlock passage, belly traversal and the thundering-wall checkpoint. It also
publishes the recovered checkpoint and arena spawn sets on natural passage.

## Unresolved mechanics

The evidence does not establish the type-32 toggle body, piston animation and
damage clocks, thunder-wall owner activation, ejection interaction/movement
owner, individual hoop collection observations, hoop aggregation or traversal
chest reward. Checkpoint placement is recovered; the commit timing is explicit
solo policy. Creating or positioning visible device/object sources does not
complete those other mechanics. Same-room death/retry and a full
reactor-exit-to-arena run remain live acceptance work.
