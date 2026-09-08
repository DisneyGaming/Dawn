# Next mission handoff

Updated 7 September 2026. Workspace: `C:\Destiny 2 Development`. Shell: PowerShell. Project: `Sunrise` (C++20, MSVC v145, x64). This is a handoff for another mission reconstruction; the next mission has not been named yet.

## Start here

Implement the next user-selected mission using the shared Lua loader, executor, and shared native services. Preserve the accepted Omega and Gateway implementations. Recover the new mission's own identities, launch configuration, encounters, dialogue, scene events and device states, then supply its trusted bindings and runtime adapter. Add shared functionality only when an observed mechanic cannot be expressed with the existing services.

First obtain the mission name and supplied references if the next conversation does not contain them. Do not assume that the next story mission is the requested one. Read this handoff, then inspect current files and hashes before modifying code. No new task or separate repository has been created by this handoff.

## Current baseline

All three missions load Lua. Deadly Trial and Gateway own their story sequence in Lua; Omega's 14 graphs and presentation use Lua while its native state machines retain phase constraints. See [Lua mission authoring](LUA-MISSION-AUTHORING.md) and the [scripts README](../scripts/README.md).

Current DLL, script, source, and test evidence is recorded per build under `build/coo/`. Omega's initial Lua install is in `validation-omega-lua-first-20260908`; its replay checks are in `validation-omega-lua-replay-20260908`. The subsequent parser/tool cleanup has its own validation and installation records. Check the installed DLL hash against those records before assuming which build is active.

Gateway's earlier return-cue acceptance remains historical evidence in `build/coo/validation-return-cue/`, including its separate user confirmation. Older rollback archives retain their matching DLLs and scripts. Native readiness or checkpoint policy changes still need explicit implementation and validation.

## What is universal today

The executor sequences registered commands and joins authenticated observations. Shared services provide:

1. Enemy admission, health/AI/tactical readiness, real death accounting, and intentional idle-reveal policies.
2. Object preparation, creation, verified entity/controller binding, and starting-state acknowledgement.
3. Destructible immunity, vulnerability, confirmed destruction and linked device/source retirement.
4. NPC preload, retained authored scene events, animation prerequisites, dialogue coordination, and separate conversation/parent-scene milestones.
5. Event-relative timers with qualified owner identities; duplicate receipts do not restart a timer.
6. Objective and marker set, replace and clear operations.
7. Stalled-step diagnostics, including unresolved native bindings and known/unknown population capacity.
8. Reset/completion ownership with advancing generations and shared mission-complete state 6 publication.

The core does not discover assets, recover retail encounter counts, infer scene event IDs, repair arbitrary engine versions, or choose a mission's checkpoint behavior. A new mechanic may still need a new shared capability. There is no known need for another broad executor rewrite before starting the next mission.

## Lua versus native bindings

**Mission Lua** selects trusted capabilities, graph dependencies, encounter/cohort requests, dialogue rows, marker targets and permitted timing arguments. Gateway uses profile `gateway.ending.v2`.

**The trusted C++ profile/catalog and adapter** supply native registry/definition/type/slot identities, population source counts and categories, tactical assignments, trigger geometry, scene event IDs and signals, device values, launch ownership, authority encoding and receipt authentication. The adapter connects those observations to the shared services.

A new mission still needs verified native bindings and integration. Deadly Trial and Gateway use compiled `bindings.h` manifests with thin `profile.h` wrappers; they no longer require a compiled story sequence. Lua can arrange supported capabilities, but it cannot register engine objects or change fixed native spawn counts. Omega retains additional phase/receipt constraints in its older adapter. Keep mission IDs and story branches out of shared services.

Current executor limits are 32 steps per graph, 8 commands per step and a 128-event queue. `MissionRuntime` supports up to 8 modules. Use deliberate sections/modules for longer missions; do not silently exceed limits. Gateway uses separate opening and ending executor sections.

## Read these files

- [Shared-service overview](<C:/Destiny 2 Development/Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md>) explains the eight service contracts. Its older pending-validation notes describe earlier candidates.
- `Sunrise/src/state/activity/coo/executor.h`, `mission_runtime.h`, `mission_script.h/.cpp`, `script_views.h`: execution, composition and trusted Lua loading.
- The shared headers `population_service.h`, `object_service.h`, `scene_orchestration.h`, `event_timeline.h`, `objective_service.h`, `lifecycle_service.h`, `stall_diagnostics.h` in that same directory.
- [Gateway document](<C:/Destiny 2 Development/Sunrise/scripts/gateway.lua>) and `Sunrise/src/state/activity/gateway/{profile.h,controller.h,controller.cpp,runtime.cpp,frame.h}`: current working integration example.
- `gateway/{catalog.h,traversal_catalog.h,ai_bindings.h,service_bindings.h,preparation.h,authority.h,ending_cadence.h}`: authored assets, policies and publication cadence.
- `Sunrise/src/client/hooks/bootflow/{coo_native_components.h,coo_enemy_readiness.h}`: shared native component/readiness probes.
- `Sunrise/src/server/bap/encrypted/push/activity/{gateway_roster.h,activity_roster_snapshot.cpp,activity_keepalive_push.cpp}`: roster selection, snapshot routing and timely publication.
- The existing `omega_dialogue_dispatch_probe.cpp`, `omega_enemy_lair_receipts.cpp` and `omega_rescue_scene_receipts.cpp` in `client/hooks/bootflow/` also route Gateway receipts. Their filenames do not mean they serve only Omega. Extend routing deliberately; do not attach another mission to Gateway's asset filters.
- `Sunrise/unit/coo_universal_services_tests.cpp`: independent `chamber.native.v1` fixture, useful for verifying shared changes without either mission's identities.
- `Sunrise/unit/gateway_opening_tests.cpp` and its `.vcxproj`: complete Gateway progression, native identity, timing, reset and wire tests.

## Implement the next mission

### 1. Establish identity and launch

Recover the activity index, package name, investment hash, activity asset, scenario, launch descriptor, bubble/state, packed region and spawn-set hash. These are different identifiers. Confirm the spawn against authored landing geometry; "same bubble" does not establish the right spawn.

Inspect `Sunrise/src/state/activity/forced/{definition.h,prelaunch_profile.h,activity_forced_destination.cpp}` and `client/hooks/bootflow/towerfall_executor_bootstrap.cpp` if the mission needs a bootstrap. Correct the native selection before Destiny builds dependent launch data. A late package rename can leave the donor activity's contract active.

Gateway's validated example is **Activity override -> Gateway opening -> launch Chosen**: Chosen 282/282 becomes 292/292/`mission_abs`, bubble 15, slice 120, spawn set `69F52B3E`. These values belong to Gateway; do not reuse them for a different mission.

Prove native launch, world arrival, controlled player, fade release and the correct roster before diagnosing missing story scripting. Preserve mission ownership across ordinary region changes. Gateway once reset its entire controller on the valid region transition 120 -> 128.

### 2. Recover an evidence-backed mission map

Use the supplied walkthrough/video for order, pacing, visible behavior, dialogue and triggers. Use installed packages for native identities and geometry. Use live captures to establish actual entity/controller/health/AI/scene behavior.

Create a new research directory under `build/coo/<mission>-research/` and a readable reconstruction document. For every step, record: trigger, prerequisites, enemy sources/count assumptions, required versus optional kills, AI intent, object starting states, dialogue ownership, scene events, objective/marker, transition and reset behavior. Label unknowns and distinguish retail evidence from reconstruction choices.

Useful existing tools to adapt: `tools/coo/package_read.py`, `extract_gateway_bindings.py`, `identify_gateway_opening_spawn.py`, `generate_gateway_catalog.py`, and the `verify_gateway_*_bindings.py` scripts. Some tools generate files without `--check`; inspect their entry points before running them. Do not overwrite Gateway catalogs to investigate another mission.

### 3. Add the document and adapter

Create `Sunrise/scripts/<mission>.lua` and a mission-specific directory under `Sunrise/src/state/activity/`. Add its trusted profile, verified catalog, controller/service wiring, runtime entry points, native roster/authority routing and qualified observers. Register new compilation units in the project as required.

Use existing service operations for enemies, objects, destructibles, scenes, timers, objectives and completion. Native receipts must match the current run/generation and correct source/entity/controller identities. A publication request is not proof of native consumption. Reaching a trigger, elapsed time or an empty observation list is not proof of enemy death.

Own dialogue either in the native scene or in the host queue. Do not queue scene-owned lines again. Latch approach/entry events while other prerequisites finish. Use actual submission/conversation-start timestamps for cues, and ensure publication cadence can deliver them promptly.

Preserve the installed actor limit and heap configuration. Spawn encounter populations as authored. Do not enable every inventoried source at world arrival, reintroduce the reverted stationed-squad experiment, or raise capacity to 120 as a substitute for understanding the population schedule.

### 4. Validate and install

Add meaningful tests for the new mission's progression, delayed and out-of-order receipts, early boss kills, missing health/AI/controller bindings, optional survivors, exact timer boundaries, repeated triggers, stale generations, restart and completion. Compare new wire layouts with independently recovered native evidence.

Run the new mission tests plus affected shared/protocol tests. Preserve Gateway and Omega regressions when touching shared code, receipt hooks, schemas or routing. Do not change existing assertions merely to accept a regression.

Use `python tools/coo/verify_lua.py --out build/coo/validation-<change>` for the current Debug/Release suites and DLLs. Then use `package_lua.py --validation <directory>` and `install_candidate.ps1 -ValidationDirectory <directory>`. Obsolete mission JSON snapshot validators have been retired; their archived evidence remains under `build/coo/`.

`tools/coo/verify.py` supplies `build(project, configuration)`. A fresh Python driver can set `verify.OUT` to a new absolute directory and build explicit `.vcxproj` files in Debug/Release. Build the DLL with `verify.build(ROOT / 'Sunrise/Sunrise.vcxproj', 'Release')` after tests pass. The helper checks compiler warnings, executes unit binaries, and records build/test logs.

Available MSBuild: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe`. Freeze the candidate source/script manifest, preserve the previous DLL and script together, and verify hashes on install. Complete the candidate build and checks before asking the user to close the game. The running game must release the DLL before replacement. Do not launch or terminate the game on the user's behalf unless asked.

Lua is loaded once per process in the current Gateway adapter. Editing a script does not hot-reload the active mission; install a matching DLL/document pair and restart. Do not copy live-memory addresses between processes. If a live intervention is requested, resolve current owners again and preserve evidence before making the scoped change.

Unit tests establish code behavior, not visible placement or audible timing. Ask for targeted live checks on the new mission. Keep user reports, code checks and native captures clearly identified as separate evidence. Record the accepted final build and leave a rollback path.

## Gateway lessons to carry forward

- Repeated reflected metadata rows can alias one valid controller. Accept the same validated component address; reject distinct conflicting controllers. `unit/fixtures/gateway_controller_components.bin` captures the real failure that produced the invisible cube.
- An entity can exist with health bound and still be visually inactive. Verify its controller and starting device state too.
- Gateway's cube uses position 1 while immune, 0.75 after the boss kill, and 0 on confirmed destruction. Its beam and portal blocker retire on cube destruction. Those values are specific to these devices.
- Vance is preloaded. Entry starts the invitation and native turn. Turn completion, retained approach and completed entry dialogue release the conversation. His continuing parent idle scene must not prevent mission completion.
- Lighthouse ascent is 22,640 ms after the real conversation starts; conversation completion is authored at 31,000 ms. Mission state 6 follows the required ending joins.
- The latest return cue is `vance.return_cue`: 8,960 ms after actual submission of row 5, including Ghost's preceding line. Both return cohorts 9/10 and the Lighthouse objective publish in the same frame. It uses the existing 100 ms cadence while pending. The next line still respects the full voice window. Native audio evidence is under `build/coo/gateway-return-cue/`.

## Deliberate limits and deferred work

The Lighthouse exit teleporter remains deferred. Exact retail enemy density is not fully recovered. Gateway-specific checkpoint/death recovery has not been separately established by the latest cue acceptance; Omega's earlier successful death/retry tests do not prove another mission's checkpoint policy. Video-prologue content absent from the reconstructed gameplay segment and automatic next-mission handoff are not implicitly included.

These are mission/content follow-ups, not reasons to restart the universal executor project. For the next mission, first map the content against the existing capabilities and identify any concrete missing mechanic.
