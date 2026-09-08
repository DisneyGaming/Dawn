# Universal mission services

Gateway now uses the shared services in `src/state/activity/coo/`. The independent `coo_universal_services_tests` fixture uses a separate `chamber.native.v1` profile with no Gateway or Omega asset IDs. These services execute and verify authored bindings; they do not discover mission assets or invent missing events.

## Responsibilities

1. **Enemy readiness — `population_service.h`.** Regular enemies and bosses use the same admission ledger. A verified policy requires creation, a typed health owner, an AI owner, and the authored tactical registry, slot and row. `EnemyIntent::idleReveal` permits an intentionally unassigned tactical state while still requiring health and AI. Changing to combat rechecks the assignment. A confirmed native death remains terminal evidence when it arrives before the readiness poll. A readiness check never requires motion or fabricates a death.
2. **Object initialization — `object_service.h`.** Begin with a generation lease and `ObjectBinding` records. A preparation acknowledgement requests creation, optionally using the reserved next generation. Entity and controller receipts then permit the starting position. The matching device revision acknowledges consumption; it does not prove that an animation has ended. Stale generations, serials and controllers are rejected.
3. **Destructibles — `DestructibleService`.** A native receipt binds the owner. Exposure changes immunity to vulnerability. Only confirmed destruction of that same owner advances to destroyed. Authored `LinkedDevice` entries select each object's immune, vulnerable and destroyed positions and whether destruction retires its source.
4. **Scenes — `scene_orchestration.h`.** Preload the cast and bind its native scene owner. Authored event IDs declare prerequisite signal masks. Arrival, approach, animation and preroll signals are retained, so an early approach need not be repeated. Native conversation start, conversation completion and the parent scene's completion are distinct signals.
5. **Relative time — `event_timeline.h`.** Mark a qualified event once, using the real receipt timestamp. `elapsed` waits relative to that event and rejects stale owners; duplicate receipts do not restart the clock. Lua's `eventAfter` operation supplies a delay in milliseconds through a bounded registered capability. The adapter selects which authenticated event starts that timer.
6. **Objectives — `objective_service.h`.** Setting an objective replaces its text event and marker together. Markers may also be replaced or cleared independently. Completion clears the active objective and marker while retaining an inactive publication for native cleanup. Lua chooses a registered marker target; shared native encoding writes its scope and locator.
7. **Diagnostics — `stall_diagnostics.h`.** Rate-limited reports identify missing admission, health, AI, tactical assignment, death, preparation, entity/controller, device revision, scene binding/event, conversation origin or dialogue submission. The clock resets when the missing evidence or command owner changes. Native capacity comes from the actor allocation bitmap; unavailable or inconsistent metadata is reported as unknown. The diagnostic never changes the population limit.
8. **Lifecycle — `lifecycle_service.h`.** A reset clears owned state but preserves the generation high-water mark. Reusing a run ID cannot accept an earlier attempt's receipts. The registered `complete` operation publishes mission-complete state 6 through a common `CompletionPublication`; both native authority schemas support it.

## Wiring another mission

Keep registry IDs, native definitions, tactical assignments, scene event IDs, marker targets and device state values in the mission's trusted binding profile. Keep graph dependencies, encounter counts, dialogue choices, marker selections and allowed timing values in the mission Lua. Supply native observers that authenticate the relevant receipt before calling a service.

The native adapters in `client/hooks/bootflow/coo_native_components.h` and `coo_enemy_readiness.h` reuse the current engine's component layout. They validate scoped source identity, generation, actor links, health type and AI ownership. They recheck identity before accepting a sample. These probes are read-only and bounded; they are not a repair mechanism for arbitrary engine versions.

Gateway polls unresolved enemy readiness at most every 500 ms, with at most 12 pending actors per poll, plus the admission-time sample. It keeps the existing actor limit and spawn schedule. Initializing objects uses the existing 100 ms publication cadence until the controllers acknowledge their state. Missing readiness is logged without replacing genuine kill requirements.

## Gateway integration

`gateway/service_bindings.h` supplies the cube, beam and portal barrier states. The shielded cube starts at 1, exposes at 0.75 after the boss dies, and retires at 0 on confirmed destruction. The barrier and beam are cleared and their sources retired in that same destruction projection.

The Vance cast is requested on arrival. The invitation is submitted at Lighthouse entry; the native turn event follows that submission. Approach remains latched while the turn and greeting finish. The actual conversation-start receipt starts the Lua clocks: 22,640 ms to raise the Lighthouse, 31,000 ms to complete the conversation and then the mission. The NPC's longer idle scene does not block completion.

`gateway.lua` now selects Forest gate, Lighthouse portal, cube and Vance markers through `presentation.markers`. Its enemy populations, opening graph, ending dependencies and dialogue rows remain unchanged by this migration. Omega also loads Lua; its established native bindings and state-machine constraints remain. Adding shared capabilities does not opt it into new readiness policies.

## Verification

Run `python tools/coo/verify_universal_services.py`. It protects unrelated source files and launch settings, validates the package-derived Gateway catalogs, runs shared service, Gateway, Omega and native protocol regressions in their supported Debug/Release configurations, and builds the Release DLL with zero compiler warnings. The results and frozen candidate are written under `build/coo/validation-universal-services/`.

The independent fixture covers reordered scene signals, exact timer boundaries, intentionally idle enemies, early boss death, stale owners, wrong controllers, full/unknown population capacity, marker replacement/clear encoding, generation reuse and an independent mission completing at state 6. Gateway integration tests cover all three native object acknowledgements, linked retirement, the retained approach, ending timing and both completion encoders.

Fresh in-game validation is still required for the newly added native readiness probes and marker placement. Unit tests and native wire parity do not prove visual placement or audible playback. Installation records that distinction and does not launch the game.

## Native component alias correction

A live Gateway capture showed ten reflected metadata rows pointing to each single cube, barrier and beam controller. The component lookup now accepts repeated rows for the same validated address. It still rejects two distinct matching components, wrong entity ownership and broken self references. The readiness requirement remains enabled.

`unit/fixtures/gateway_controller_components.bin` preserves the relevant reads from that failing run. The shared-service regression replays all three controllers: the pre-fix implementation fails and the corrected implementation passes. Installation and regression evidence are under `build/coo/validation-component-aliases/`. A fresh run of the installed DLL must still confirm visible activation and the ending sequence.
