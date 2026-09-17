# Lua mission authoring

Updated 9 September 2026 after Deep Storage. Existing Lua, Gateway, Deadly Trial, Omega and Beyond Infinity guidance is retained. For the next mission, copy [the implementation template](MISSION-IMPLEMENTATION-TEMPLATE.md), fill its records, and use this guide for authoring decisions.

A Deadly Trial, Gateway, Omega, Beyond Infinity and Deep Storage load mission definitions from Lua. Deadly Trial and Gateway also put their story progression decisions in Lua; Omega retains additional native phase constraints described below. The existing universal executor runs the resulting graphs; Lua evaluates once at load time and its VM closes before gameplay starts.

## Ownership

- `scripts/deadly_trial.lua` and `scripts/gateway.lua` choose encounters, dialogue, objectives, dependencies, alternative travel conditions, phase order, and completion gates.
- `src/state/activity/deadly_trial/bindings.h` and `src/state/activity/gateway/bindings.h` contain the named native capabilities and identifiers each adapter supports. The corresponding `profile.h` files are thin compatibility wrappers. They contain no required mission sequence.
- Native catalogs retain recovered package geometry, population sources, tactical mappings, dialogue metadata, and binary layouts. Native adapters project requested actions and authenticated observations through the shared population, scene, object, dialogue, objective, lifecycle, and executor services.

These binding files are compiled C++ manifests, not another mission scripting language. Changing flow among existing capabilities requires editing Lua. Registering a new native object or unsupported interaction still requires binding/native work and a rebuild. Lua does not discover package behavior automatically, and this change does not make every future mission a guaranteed one-pass reconstruction.

- `scripts/omega.lua` supplies Omega's 14 graphs and presentation data. `coo/omega_script.cpp` supplies its native profile and validates the phase/receipt interface required by its existing C++ controllers.

## Loading

Opening objectives and briefing requests start on the first confirmed mission-arrival update, with no extra timer or player-position prerequisite. Keep the runtime's authenticated run, seed-ready and world-arrived checks. A location gate belongs on the encounter, interaction or later story beat that actually requires it. Shipped opening graphs omit `observation_start`; optional custom observation latches remain supported.

Gateway requests the shelf and final-platform initial populations when both first-platform cohorts clear, and requests both mainland defense populations when the final platform clears. Reinforcements retain their authored trigger/death conditions. Deadly Trial requests lower-room Marauders on the final tower death, independently of tower dialogue and the player's drop; revival still requires both populations dead and the real interaction.

Deadly Trial also retains required unsubmitted dialogue across dispatch timeouts with the same generation. At native roster decode, its presentation binding repair reconnects the existing dialogue/objective records if public-activity retirement cleared their associations. It validates the current activity, definitions, self handles, schemas and allocated salted records before writing associations; native dispatch and packet application still own acknowledgement and presentation bodies.

The DLL loads the five mission `.lua` files relative to its own directory. Missing or invalid scripts fail selection. Logs record `format=lua`, the path, and a source fingerprint. Edits apply in the next game process; there is no live reload.

The mission JSON loader and shipped mission JSON files are removed. Lua produces a neutral owned definition tree directly, without serializing JSON. Deploy the matching DLL and scripts together. Historical rollback archives retain older scripts for their matching DLLs.

## Authoring interface

A script returns one `mission{...}` definition. Builders construct data; they do not call native game functions.

```lua
local arrived = condition("arrived", any_of("square.entered", "pike.mounted"))
local clear = condition("clear", all_of("tower.cleared", "lair.cleared"))

local opening = graph("opening", "Example", {
    step("arrival", "arrived"),
    step("fight", "square.fallen", {after={"arrival"}}),
    step("release", "barrier.open", {after={"fight"}}),
})
```

Use the complete mission files as working templates; this fragment illustrates controls already exposed by the Deadly Trial binding manifest.

- `command(capability, {id="alias", argument=value})` selects a registered capability. A string is shorthand when no options are needed. An argument override needs a distinct unused alias; only registered adjustable event delays accept overrides.
- `parallel(...)` starts several commands in one step. Each retains its native receipt policy, and all must satisfy it before the step completes.
- `step(id, commands, {after={...}})` declares dependencies. A plain step list supports parallel branches and joins. `sequence(...)` makes each step depend on the previous step. Forward references are accepted; dependency cycles are rejected.
- `condition(id, any_of(...))` and `condition(id, all_of(...))` combine readonly native observations. Groups can nest, and strings may reference other named conditions. Declare each condition before using its name as a command, and include its returned declaration in `mission.conditions`. Named references inside expressions may point forward, but cannot form cycles.
- Conditions query actual observations. They cannot execute actions or turn a timer, volume, or unauthenticated callback into an enemy death. A population-cleared observation also waits for the registered native population to exist and clear.
- `graph(id, title, steps, options)` creates a bounded executor graph. `domain` defaults to its ID. Native capabilities with domain `*` can be used in any non-composition graph; composition capabilities remain separate. Receipt names default to waiting command IDs; explicit complete receipt maps are supported.
- `mission.phases={"opening", "ending"}` chooses the graph execution order. Names and ordering are authored; controllers do not require the old opening/ending step lists. `roles` remain named references for composition and diagnostics.
- `mission.observation_start="landing.entered"` tells the current mission adapters which real volume starts retaining travel observations. Omit it to retain observations immediately. Use a registered volume observation for these two adapters.
- `mission.entry`, `modules`, and `observations` connect the outer mission composition to the native module interface. They identify services and completion facts, not a required gameplay sequence.
- `presentation{...}` merges authored presentation data with validated native dialogue defaults. `array{}` explicitly marks an empty array.

## Migrated decisions

Deadly Trial's Lua defines the downstream travel alternatives, Pike-or-travel condition, Walker death branch that immediately enables the tower population, clearance before revival, accepted pedestal interaction and prelude audio, and native scene/audio completion before mission completion. The native death callback pumps the existing graph; it contains no Walker-to-tower special case. Native Skiff animation, Ghost/Sagira ownership, the persistent marker, and the lifetime association repair retain their established C++ implementations.

Gateway's Lua defines reinforcement alternatives, shelf passage, return contact, observation reset on the return trip, module exposure/unlock, Vance greeting and approach, preroll audio, ascent timing, and completion. Its adapter tracks each authentic dialogue submission in a reusable row clock; Lua specifies the selected cue delays. Native scene casting, turn/conversation receipts, and device revisions remain C++ controls.

## Validation and limits

Shared validation checks registered operations and native references, supported arguments and receipt kinds, graph dependencies, condition references, bounds, module mappings, and exact run/incarnation/step/command identity. Deadly Trial and Gateway do not compare the graph to a compiled mission sequence or require every capability to appear exactly once. Omega additionally validates its native phase prerequisites, required capabilities and receipts, and producer order. Deadly Trial and Gateway authors choose the intended mission gates; structurally valid alternative stories are deliberately accepted. Omega edits must also satisfy its native state machines.

Limits remain 1 MiB of Lua source, 8 MiB of VM allocation, 1,000,000 Lua instructions, 30,000 converted values and aggregate container slots, depth 16 for returned data, and 256-byte ASCII strings. Numeric data uses nonnegative uint32 Lua integers. There are at most 8 distinct phases, 64 conditions, and 64 expanded nodes / 8 levels per condition. Each executor graph supports 32 steps and 8 commands per step.

Only basic iteration, assertions, conversions, and builders are exposed. File/process access, pointers, hooks, packages, debug facilities, dynamic loaders, coroutines, metatables, and protected calls are absent. Text chunks only; sparse/mixed/cyclic tables and functions in returned data are rejected. Load failures include source context. Initialization and execution are protected, and retained values are copied before the VM closes.

Lua 5.4.9 is embedded, with source provenance in `vendor/lua/README.dawn.md` and the MIT license in `vendor/lua/LICENSE.txt`. No external Lua DLL is required. Third-party warning settings are isolated in `lua-items.props`; project code builds with `/W4 /WX`.

## Verification and reconstruction workflow

Run `python tools/coo/verify_lua.py` from the workspace root. The runner writes isolated Debug/Release outputs to a fresh timestamped directory under `build/coo/`, runs 20 suites in Debug and Release, and builds both DLL configurations (42 results). It does not install or launch the game. Use a separate `--out` for focused invocations because each run replaces its own result summary.

The tests preserve established native receipt, audio, marker, population, lifecycle, and wire-format checks. They also execute deliberately changed Lua flows for both real missions, reordered phases, AND/OR conditions, adjustable cue timing, unknown references, cycles, sandbox limits, and stale command tokens. Omega conversion parity was checked before removing the old mission files. Its ongoing suites compare Lua execution with the established native behavior. Deadly Trial and Gateway also exercise authored decisions that previously lived in C++.

For another mission: recover and register its native identifiers, expose any missing reusable interaction, author its Lua graph and conditions, connect its native projection, and run the same checks. Existing capabilities should be reused. Add engine code only where the native interaction is actually missing. The Deadly Trial evidence generator now regenerates its native catalog and tactical joins only; it cannot overwrite the hand-authored bindings or restore a compiled sequence.

Package a complete validation run with `python tools/coo/package_lua.py --validation <directory>`, then install with `tools/coo/install_candidate.ps1 -ValidationDirectory <directory>` after closing Destiny. Packaging rejects changed source or binaries; installation verifies and backs up the matching DLL/script set. Build records, package research, and settings remain separate from mission scripting.

A live playthrough provides evidence beyond unit/replay checks. Installation and live-run evidence belongs with its exact build under `build/coo/`; consult those receipts for current acceptance rather than historical DLL hashes in older reconstruction notes.

## Deep Storage

`deep_storage.lua` authors the complete Rupture-to-coordinates route. The profile exposes recovered native sources, plates, Ghost interactions and portal receiving volumes. The controller does not pin a compiled story order. Full-route tests exercise receipt policies using synthetic inputs. See [the implementation record](DEEP-STORAGE-IMPLEMENTATION.md) for assumptions and live acceptance checks.


## Authoring the next mission: lessons from Deep Storage

Use [Deep Storage Lua](../scripts/deep_storage.lua), [its implementation record](DEEP-STORAGE-IMPLEMENTATION.md), and [its route tests](../unit/deep_storage_tests.cpp) as additional examples. The record distinguishes recovered native behavior from the user's chosen encounter schedule. Its last combined candidate passed 42 build/test results and was installed; that establishes build and installation status, not a complete fresh native playthrough. Consult the matching [acceptance record](../../build/coo/deep-storage-door-waves-beam-20260909/acceptance.md) for that historical candidate.

### Write separate contracts for each beat

Before writing dependencies, identify these events separately:

1. **Preload:** when a source must exist or scenery must first be visible.
2. **Arm:** when interaction, charge, contest or damage is permitted.
3. **Request:** which exact population or device commands begin at the cue.
4. **Ready:** which native admission, health, AI, entity or controller evidence is required before a dependent action.
5. **Advance:** the movement, interaction, destruction or explicitly required deaths that release progression.
6. **Retire:** when pending commands end, and which visible objects or live populations must remain.

Do not let a convenient shared prerequisite stand in for all six. Deep Storage's pillars needed to appear during the preceding encounter, its descent enemies needed requests when the entrance door opened, and its box needed real destruction after both plates. Those are three different contracts.

### Choose request, readiness and death waits deliberately

A command's completion policy comes from its registered capability. Lua cannot invent a `.request` alias or override `Wait` merely by changing the spelling. In Deep Storage, 17 exact descent sources gained `.request` capabilities using `Wait::requested`; their existing `.spawn` capabilities still use `Wait::nativeReady`. The controller publishes the same native population request in either case. Requested completion permits the graph to proceed while native streaming later admits the enemies. It does not fabricate admission, health, readiness or death.

Use early requests when the intended cue precedes streaming readiness. Use native readiness when a dependent action actually needs the actor or object. Preserve ownership and readiness diagnostics for both policies. Request only the encounter sources established by the beat contract; the door-opening rule is not permission to enable every population in a new mission.

Native readiness and death are also different requirements. A player crossing a traversal volume need not clear every earlier enemy unless the intended mission explicitly requires it. Deep Storage's warp portal requires its Hydra; the drop barrier requires the Cyclops and its defender cohorts. Later user-directed plate reinforcements explicitly require their own first-wave deaths. Encode those exact dependencies rather than a general room-wide clear.

### Give each plate or encounter branch its own waves

For independent plates, define an entry observation, initial cohort, genuine clearance observation and follow-up cohort for each side. Share a first-entry voice line separately if required. Avoid sending both sides' populations from `any_of(left.occupied, right.occupied)` and then using plate charge to compensate for the missing second entry trigger.

The following is a dependency sketch, not new executable DSL:

~~~text
left occupied  -> request left wave 1  -> left wave 1 genuinely cleared  -> request left wave 2
right occupied -> request right wave 1 -> right wave 1 genuinely cleared -> request right wave 2
first occupied -> enqueue shared incoming dialogue once
left charged  -> remove left rod/beam
right charged -> remove right rod/beam
both charged  -> expose box
owned box destroyed -> remove remaining beam, reveal conflux
~~~

Recover source placement, side assignment, variants and count separately from deciding the wave partition. One native source can produce multiple actors. Deep Storage's two-wave partition follows the user's timing correction; it is not proof that the retail scheduler used the same partition. Exercise both visitation orders, repeated entry, delayed admissions and charging while enemies survive.

### Audit whole-graph completion, not only the final step

The executor normally completes a graph only when all its required steps complete. Removing a direct enemy dependency from `scan` or `reveal` does not help if another branch still waits for optional enemies forever. Audit every pending death, readiness, scene and dialogue wait before declaring an encounter complete.

Where gameplay explicitly allows leaving unfinished encounter branches, define a supported phase-completion operation with a clear native prerequisite. Deep Storage's `map.encounter.finish` is scoped to its `map_room` domain and follows the real final scan. Its adapter cancels unfinished executor commands, invalidates their tokens, clears its phase-completion flag on transition/reset, and preserves already published native sources and the dialogue queue. It cannot bypass an executor failure.

That capability is currently specific to Deep Storage; it is not a general builder option or a universally available `cancel` command. For another mission, reuse a suitable supported operation or implement the missing shared contract with tests. Cancellation must have an explicit policy for already requested actors/devices, queued speech and late native callbacks. Never synthesize clear receipts to make a graph finish.

### Keep presentation state separate from gameplay state

A source existing, its device being active and its interaction being armed are independent facts. Preload scenery early enough to be visible from the approach. Before gameplay arming, a plate can present its waiting appearance while its timer remains stopped and early occupancy cannot charge it.

Deep Storage's final plates use a red waiting pose with rods and side beams visible. Completing one plate removes only its own rod, catch and beam; completion remains latched after departure and reentry. Its entrance plate has different completed presentation. Do not reuse a visual mode by appearance alone. Include arming in the native command's change detection: an occupied plate must start when armed even if its occupancy and revision have not otherwise changed.

For persistence, inspect both the current native pose and the pending target. A future transition can undo a correct-looking current pose. Apply corrections through the verified native setter on the correct engine thread, require the owned revision to be acknowledged, and relinquish ownership at the authored cleanup point. Never reset the gameplay completion latch to hide an unwanted visual.

Keep a per-beat state record for the plate, rod, beam, catch, box shield, covers, conflux and ending hologram. They can belong to different sources or channels. Deep Storage's lingering center beam was caused by an explicit `.on` in the post-destruction reveal step. Inspect later graph commands before adding another persistence repair.

### Verify device modes and revision changes

Logical off is not always native zero. Recover mode branches, animation actions, model state and collision state for the exact registry/definition/type/slot. Test active and inactive mappings and nearby identities. Deep Storage's unused doorway wall requires native `.2` to remove it; zero raises it. Its pit barrier uses zero closed and one removed. Neither mapping is a universal barrier convention.

Visibility, vulnerability and destruction need separate native evidence. Both final plates expose Deep Storage's box at `.75`; its authenticated health destruction selects zero. Reserve a fresh device revision when such a semantic change occurs even if the logical active bit stays true. Otherwise the native consumer can reject the changed value as an already consumed generation. Reject stale acknowledgements and duplicate destruction. Let Lua choose which plate/death observations expose a box; let the service enforce the owned damage and destruction contract.

### Observe the effective Ghost scan duration

Read the controller's effective duration after source overrides, not only its base resource default. Deep Storage's base resource specifies three seconds, while its entrance and final sources override that to five and nine seconds. A hard-coded three-second check rejected completed scans.

For this verified native playback model, start requires the current generation, expected mode, active playback, a participating Ghost and positive progress before the effective duration. Finish requires a retained authentic start, inactive playback and elapsed time at or beyond a finite positive effective duration. Native overshoot is valid; exact floating-point equality is not a completion requirement. Reject invalid/nonfinite values, stale owners and completion without start.

These rules live in [ScanPlayback](../src/state/activity/deep_storage/scan_playback.h); a new native interaction still needs its own verified controller/layout binding. If a scan stalls, distinguish missing source/entity/controller from a bound controller waiting for start or finish. Do not replay dialogue or write observer flags as the permanent fix for an invalid predicate.

### Diagnose retained objects before recreating them

A visible object can retain an older creation generation while a later source command is applied. Deep Storage's ending dome had exactly that mismatch. Its repair validates the exact source, reserved same-lease transition, applied and creation generations, salted entity, generic component identity and current ownership. It does not accept arbitrary old generations.

Treat [the retained-dome owner check](../src/state/activity/deep_storage/hologram_owner.h) as an evidence-backed exception, not a universal fallback. Determine whether the object is retained, faded, retired or never acknowledged before requesting another creation. For this ending, the source binds while hidden, the coordinates exchange begins, the verified persistent display mode activates, and mission success follows the full required exchange. Presentation remains until its authored retirement.

### Reuse native boundaries and shared mechanics

Keep the executor responsible for commands, dependencies, receipts and cancellation. Shared services should own reusable scan/plate state, population readiness/deaths, object lifecycle and destructible contracts. Native adapters validate engine identity and publish those facts; mission bindings supply layouts/modes and Lua supplies story decisions.

Much of that infrastructure already exists in [universal mission services](UNIVERSAL-MISSION-SERVICES.md), but scan playback, final plate presentation and the retained-dome exception still have Deep Storage-specific implementations. This document does not claim a universal scan/plate refactor has already happened. When the next mission needs the same behavior, extract the proven state/receipt logic behind mission-specific bindings, preserve the existing mission's tests, and add an independent binding fixture. Keep exceptional ownership acceptance narrowly scoped until a broader native contract is proved.

Deep Storage added helper code at existing scan, timer, object, damage and enemy boundaries, with zero new intercepted engine addresses. Check actual registrations and callers rather than filenames: files prefixed `omega_` or `gateway_` can serve other missions. A native setter call, callback-slot replacement, platform API detour and engine detour are different integrations. Record actual new targets separately from helper files and optional diagnostic registrations.

If enemies exist but never become ready, inspect reflected component metadata and typed ownership before changing spawn logic. Deep Storage encountered resources with more reflected rows than the old reader's bound. The correction expanded a bounded component read after native evidence established the valid health owner; it did not increase actor capacity or declare missing enemies dead.

### Carry evidence through validation and installation

Add meaningful replay cases for delayed streaming, untouched optional enemies, both plate orders, early unarmed entry, interrupted and retained charge, actual box destruction, current/target visual drift, full scan duration/overshoot, stale owners, terminal cancellation and complete ending speech. Check nonzero native values and revisions with independent wire fixtures. Replays establish logic, not rendered appearance, audible playback or a fresh native full run.

Before a live repair, retain the exact build, process creation identity, owner, original bytes and intended small change. Re-resolve after every restart or owner change, use the supported engine thread, and record user observation separately from native readback. Existing authorization applies within its scope; do not repeatedly ask for the same approved action. Keep temporary observer reconciliation out of the permanent completion logic.

Freeze documentation along with code before the final configured validation. Preserve superseded runs instead of relabeling them as the new candidate. Keep both the exact pre-install files and a coherent previous validated DLL/script set: working scripts may already have been edited while the installed DLL is older. After installing, verify the entire payload and unchanged settings; after restart, verify the loaded DLL and script fingerprint. An installation receipt or startup log alone does not establish a successful mission playthrough.

Recover checkpoint/wipe behavior explicitly when requested. A new-process or activity-reset replay does not prove same-room checkpoint restoration; that remained an unresolved Deep Storage fidelity item. Keep that distinction visible in the next mission's brief and delivery record.
