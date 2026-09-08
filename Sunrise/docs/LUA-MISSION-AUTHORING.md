# Lua mission authoring

A Deadly Trial, Gateway, and Omega load mission definitions from Lua. Deadly Trial and Gateway also put their story progression decisions in Lua; Omega retains additional native phase constraints described below. The existing universal executor runs the resulting graphs; Lua evaluates once at load time and its VM closes before gameplay starts.

## Ownership

- `scripts/deadly_trial.lua` and `scripts/gateway.lua` choose encounters, dialogue, objectives, dependencies, alternative travel conditions, phase order, and completion gates.
- `src/state/activity/deadly_trial/bindings.h` and `src/state/activity/gateway/bindings.h` contain the named native capabilities and identifiers each adapter supports. The corresponding `profile.h` files are thin compatibility wrappers. They contain no required mission sequence.
- Native catalogs retain recovered package geometry, population sources, tactical mappings, dialogue metadata, and binary layouts. Native adapters project requested actions and authenticated observations through the shared population, scene, object, dialogue, objective, lifecycle, and executor services.

These binding files are compiled C++ manifests, not another mission scripting language. Changing flow among existing capabilities requires editing Lua. Registering a new native object or unsupported interaction still requires binding/native work and a rebuild. Lua does not discover package behavior automatically, and this change does not make every future mission a guaranteed one-pass reconstruction.

- `scripts/omega.lua` supplies Omega's 14 graphs and presentation data. `coo/omega_script.cpp` supplies its native profile and validates the phase/receipt interface required by its existing C++ controllers.

## Loading

The DLL loads all three mission `.lua` files relative to its own directory. Missing or invalid scripts fail selection. Logs record `format=lua`, the path, and a source fingerprint. Edits apply in the next game process; there is no live reload.

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

Lua 5.4.9 is embedded, with source provenance in `vendor/lua/README.sunrise.md` and the MIT license in `vendor/lua/LICENSE.txt`. No external Lua DLL is required. Third-party warning settings are isolated in `lua-items.props`; project code builds with `/W4 /WX`.

## Verification and reconstruction workflow

Run `python tools/coo/verify_lua.py` from the workspace root. The runner writes isolated Debug/Release outputs to a fresh timestamped directory under `build/coo/`, runs 16 suites, and builds both DLL configurations. It does not install or launch the game. Use a separate `--out` for focused invocations because each run replaces its own result summary.

The tests preserve established native receipt, audio, marker, population, lifecycle, and wire-format checks. They also execute deliberately changed Lua flows for both real missions, reordered phases, AND/OR conditions, adjustable cue timing, unknown references, cycles, sandbox limits, and stale command tokens. Omega conversion parity was checked before removing the old mission files. Its ongoing suites compare Lua execution with the established native behavior. Deadly Trial and Gateway also exercise authored decisions that previously lived in C++.

For another mission: recover and register its native identifiers, expose any missing reusable interaction, author its Lua graph and conditions, connect its native projection, and run the same checks. Existing capabilities should be reused. Add engine code only where the native interaction is actually missing. The Deadly Trial evidence generator now regenerates its native catalog and tactical joins only; it cannot overwrite the hand-authored bindings or restore a compiled sequence.

Package a complete validation run with `python tools/coo/package_lua.py --validation <directory>`, then install with `tools/coo/install_candidate.ps1 -ValidationDirectory <directory>` after closing Destiny. Packaging rejects changed source or binaries; installation verifies and backs up the matching DLL/script set. Build records, package research, and settings remain separate from mission scripting.

A live playthrough provides evidence beyond unit/replay checks. Installation and live-run evidence belongs with its exact build under `build/coo/`; consult those receipts for current acceptance rather than historical DLL hashes in older reconstruction notes.
