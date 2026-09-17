# Omega activity initialization handoff

Date: 2026-08-23  
Target: Destiny 2 PC/Dawn, forced destination `mission_scot` (Omega)  
PS4 reference: Destiny 2 v1.59 EBOOT, analyzed read-only and never executed

## User requirement and current constraint

The goal is for Omega to begin on load and run its authored entities, scenes, dialogue,
objectives, spawners, and completion flow. It must not depend on walking near the distant world
anchor. That anchor and the repeating condition-`0x7A` behavior runners are unrelated world
behavior.

The user cannot use generated wire type-5 updates. Do not propose a server that synthesizes
`sensor_auth_update` as the next implementation. The next investigation must test a client-local
native-authority route or a content-driven local executor.

Do not mix this run with the friend's Shadowkeep/Crescent tests. The current target is Omega:

- package: `mission_scot`
- selected slice ordinal: 15
- region/slice-set value: 120 (`15 * 8`)
- map-global bubble index observed elsewhere: 32
- slice ordinal 15 is not "bubble 0"

## Bottom line

Omega's content registration now works. The latest run registered six groups, exposed 57 roster
slots, and materialized 54 authority objects. Opening Ghost made the objective marker visible.
That event produced a real client sense update for registry `D00142CF`, slot type 30, index 20.

No mission execution followed. The activity-script manager, mission director, scene state, and
spawner remained neutral. No entity, dialogue, directive, or completion transition appeared.

The objective marker is therefore concrete proof of navigation/sensor registration, not proof that
the mission graph started.

## PS4 EBOOT result

The corrected binary trace is in:

- `evidence/ps4_type6_native_chain_out.txt`
- `evidence/run_ps4_type6_probe.py`
- `evidence/ps4_type6_native_chain.py`
- `analysis/ps4-eboot-159-sense-host-boundary-20260823.md`

The native direction is proven:

1. `sensor_sense_update` wire type 6 is produced by the client.
2. It is sent outward through `c_activity_client` to the activity host.
3. The incoming activity-client dispatcher has no type-6 case.
4. Wire type 5 is the demonstrated host-return path into the client's sensor-authority apply core.
5. Adjacent local event `0x15` is definition 24,
   `claim_authority_over_abandoned_entity_slots`; it is not a sensor-sense event.

tpolicy and does not prove that type 6 can be echoed into a client-local queue.

The apparent PC type-6 dispatcher at `+0x16E0940` is a different DTLS/peer-channel protocol. Do
not feed the svc8 activity-message payload into it.

## Strongest no-type-5 theory

The best remaining concrete theory is that Omega's native local activity script exists but was
never bound or enabled. The latest log repeatedly shows:

- an activity-script definition exists
- `definition_activity=-1`
- `definition_enabled=0`
- selected/registered/active are all 0
- the local authority owner matches slice 15
- authority runtime reports `initialized=0`, `dirty=0`
- the native collect/apply/finalize publisher runs but collects zero changed objects

This suggests storage construction completed without attaching `mission_scot` to the native
activity-script manager.

Investigate in this order:

1. Trace the normal setter/callback that changes the script definition from `activity=-1` and
   `enabled=0` to a bound, enabled activity. Prefer a native setter over raw memory writes.
2. Trace the missing local-authority initialization call after ownership is assigned. The owner
   already matches; initialization and dirty state do not.
3. After the script manager advances, observe whether the native changed-object collector becomes
   nonzero and whether the existing local publish cycle applies director/spawner changes without a
   wire type-5 message.
4. If the native manager cannot be activated, recover Omega's authored machine conditions/actions
   and implement a local executor. Do not select the first edge or invent proximity/timer rules.

Success criteria for the first experiment:

- `definition_activity` becomes the current activity rather than -1
- `definition_enabled`, selected, registered, or active changes through a normal native path
- the director leaves selector/value/active zero
- the spawner leaves requested/count/generation/active zero
- the changed-object collector becomes nonzero
- scene/dialogue/entity behavior follows without a synthetic wire type-5 response

## Friend implementation caveat

The friend's reported Shadowkeep/Crescent result may be genuine, but the archived executor was
not only "init plus spawner." It contains guessed first-edge selection, proximity/time rules,
direct placement requests, nearby-device opening, and guessed squad/spawner association.

The archive README also refers to a crafted sensor-state driver that is not present in the supplied
source. The exact known-working DLL, matching source, and matching log are needed before claiming
that route generalizes to Omega.

## Current source changes

The archive contains the complete current dirty `Dawn-src` snapshot, including untracked source
files. It intentionally excludes `.git`, `.vs`, generated `build` output, caches, and user settings.
Do not reset or discard changes: this worktree contained substantial pre-existing user work.

Relevant changes include:

- a generic type-6 sense parser and explicit `host_action=none` diagnostic route
- corrected naming: mission-authority storage initialization is not called mission simulation
- dynamic authored roster discovery with no Omega literal-key admission switch
- scenario-root selection from the scenario hash
- same-package third-registry local selection from explicit selected-slice `ObjectBubble`
- real descriptor slot indices persisted and encoded
- cache format 40
- 2,048 static roster layouts for a measured installed census of 1,883
- fail-closed handling for hard object/descriptor reads, registry-key layout conflicts, and
  ordinary/authored capacity overflow
- runtime selection from aligned `region / 8`

Installed-content census:

- 63 ordinary layouts
- 204 scenario roots
- 1,640 same-package explicit-slice locals
- 1,883 total, leaving 165 entries of catalog headroom

Fresh Omega resolution remains:

- ordinary: `4786C0E0`, `29D7B029`
- scenario root: `82FB58B7`
- selected-slice locals: `BA5F26EF`, `D00142CF`, `F7A6CE7F`
- total: 6 groups

These keys are listed here as validation evidence. The roster selector does not hard-code them.

## Runtime evidence

The latest log is packaged as `evidence/dawn.latest.log`.

Important observations:

- roster: `dest=mission_scot groups=6 top=3 sub=1 subkeys=3`
- region: 120
- script manager stays `definition_activity=-1 definition_enabled=0`
- script selected/registered/active stay 0
- mission director stays inactive
- spawner stays requested/counts/generation/active/mode 0
- type-6 point update appears for `D00142CF`, type 30, index 20
- nothing authoritative changes afterward

The old deployed log uses `sim_init`/`sim_preserve` labels. Source naming has since been corrected to
authority initialization/preservation because storage creation is not proof of a simulator.

## Build and deployed binary

Build/deploy command:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -ClearCache
```

Final Release x64 build passed with zero errors and was deployed to
`C:\Destiny 2 Development\bin\x64\steam_api64.dll`.

- size: 12,088,832 bytes
- SHA-256: `CADCD208DF80B5EAF92C1D1B22A7CB947931834B977A826F25839BAC24289DE4`
- cache format: 40
- cache was cleared; the next launch has a slower first boot while package data rebuilds

This binary corrects generic registration and diagnostics. It is not an Omega activation fix.

## Files to start with

- `Dawn-src/Dawn/src/server/bap/encrypted/activity_message/activity_message_route.cpp`
- `Dawn-src/Dawn/src/middleware/bap/activity_message/activity_sense_update_parser.cpp`
- `Dawn-src/Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`
- `Dawn-src/Dawn/src/client/content/scenarios/scenario_roster_build.cpp`
- `Dawn-src/Dawn/src/client/content/scenarios/scenario_roster_groups.cpp`
- `Dawn-src/Dawn/src/client/content/scenarios/scenario_roster_publish.cpp`
- `Dawn-src/Dawn/src/client/hooks/bootflow/activity_script_upstream_probe.cpp`
- `Dawn-src/Dawn/src/client/hooks/bootflow/activity_schema_decode_probe.cpp`
- `Dawn-src/Dawn/src/state/activity/activity_world_arrival.cpp`

## Do not repeat these false leads

- Do not treat the distant anchor as Omega's trigger.
- Do not call the Ghost point update a proximity trigger.
- Do not conflate Crescent/Shadowkeep with Omega.
- Do not call selected ordinal 15 bubble 0.
- Do not inject svc8 type 6 into the PC DTLS type-6 dispatcher.
- Do not enable the archived guessed executor as though it were retail behavior.
- Do not claim that registered type-18/type-35 storage means mission simulation initialized.
- Do not fabricate an Omega-specific state sequence without new runtime or binary evidence.

