# Server-native mission migration

This document records the first migration. [The second-pass report](SERVER-NATIVE-HOOKS-PASS2.md) supersedes its plate-pose and interaction-enable status. The pass-two arm claim was corrected: it changed an unused legacy path. The third pass implements active arm delivery. The user completed Omega end to end on the first installed build; the second build still needs a fresh playthrough.

The embedded server owns mission rules and publishes the recovered native authority messages. The client still executes the engine and supplies qualified observations. This change does not make the DLL a standalone dedicated server, and it does not eliminate every behavior-changing hook.

## Implemented paths

- **Progression:** the existing server roster/update path continues to run authored stages, prerequisites, relative timers, completion and reset. Existing launch/checkpoint selections are preserved; no new persistent checkpoint format was introduced.
- **Populations:** existing server-owned population requests and death/readiness requirements remain. Gateway, A Deadly Trial, Deep Storage, Hijacked, Tree of Probabilities and A Garden World no longer read client actor memory from their server snapshot functions. A guarded game-thread observer answers bounded readiness requests, retaining exact actor/run/generation validation. A Deadly Trial mounted-player sampling also moved to that observer.
- **Devices:** existing native device authority continues to own position/power/lock requests. Capture devices now carry the recovered 80804FCA dynamic controller record in their ordinary type-4 source authority.
- **Capture plates:** Beyond Infinity, Deep Storage and Hijacked publish native timer commands from the server. Repeated publications keep the same anchor; occupancy changes produce a new command. Completion still requires the original native timer's qualified completion event. The server consumes queued plate and position evidence. Deep Storage and Hijacked evaluate contest geometry against the current living population; missing samples are never treated as an empty plate.
- **Objectives:** native server objective/marker publication remains authoritative. Omega's duplicate synthetic client HUD insertion was removed; the observer only checks native consumption.
- **Scans/interactions:** Deep Storage and Hijacked copy raw, identity-checked scan playback evidence into their mission inboxes. The server classifies start/completion and decides progression. Existing interaction validation remains in place.
- **Dialogue/scenes:** mission dialogue uses the shared native writer, explicitly clearing retired optional timestamps while preserving consumed generations. Native scene orchestration already drives scene requests and waits for actual scene/conversation/audio receipts.

## Hooks retained at the first migration

This is the historical first-pass list. The user subsequently authorized all feasible native replacements; it is not a current approval requirement. Later reports distinguish migrated behavior from retained adapters.

- **Damage, immunity and phase floors:** boss/cube/lens protection must run at the synchronous native damage boundary. A compatible server health/immunity authority contract has not been proved. Removing protection would allow damage through shields or skip phases.
- **Boss motion, targeting, ability delivery and special transitions:** retain each engine action without a verified equivalent native authority producer/consumer. Server phase decisions do not by themselves replace the engine's action delivery.
- **Models, shield visibility, animation inputs and Ikora/Panoptes VFX:** retain render/provider adapters when no matching recovered authority schema exists. An unrelated generic device channel is not a substitute.
- **Plate appearance:** the authored plate's pose correction still needs its bound client device setter. Timer authority and pose delivery are separate.
- **Portal transport and activity/cinematic handoff:** playback requests already have native authority where recovered. Activity selection, transition classification and world-thread wrappers remain hooked. The existing code records that calling transport from the server worker can freeze the transition.
- **Placement, streaming and cleanup:** source requests and retirement intent stay server-owned; missing local authored placements and child/facet cleanup still require validated native constructors and the correct allocator/thread context.
- **Startup, spawn holds and fade release:** retain the native identity/bootstrap and local presentation adapters that bring the mission to the point where server authority can be consumed.

## Validation and installation evidence

Combined validation and the candidate DLL are under `build/coo/validation-server-native/`. `root-results.json` records executed regression suites; `production-source-manifest.json` and `production-result.json` identify the final DLL inputs and hash. Agent-specific focused results are also recorded in the task's validation notes. Compiled-only tests requiring unavailable capture evidence are not counted as executed passes.

An installation receipt is written only after verified backups, copying the built DLL/PDB to both load locations, and checking the installed hashes. Settings and the original Steam DLL are preserved. A fresh game process is required; native playback, plate pose and full mission completion still need in-game validation.

## First playtests

1. Omega: opening objective appears once, dialogue advances without stale replay, Ikora's portal works, Panoptes phases and the ending handoff still work.
2. Beyond Infinity, Deep Storage and Hijacked: charge a plate, step off, re-enter, contest it with an enemy where applicable, finish it, and verify the intended pose persists. Run the mission scans through to the next objective.
3. A Deadly Trial: mount the Pike and pass its traversal gates. In each affected mission, verify waves become active and progression waits for actual enemy deaths.
4. Restart a mission and check that old plate, scan, dialogue or enemy receipts cannot complete the new attempt.

## Installed candidate

Installed on 2026-09-12 to both `steam_api64.dll` and `bin/x64/steam_api64.dll`. SHA256: `90280f8bff03bca3e0ca966f76843007945f48263d82050e4126ad8aafa2efe1`. The corresponding PDBs and the alternate runtime mission scripts were installed and hash-verified. Backup: `.dawn/backups/server-native-20260912-110004/`. Full receipt: `build/coo/validation-server-native/installation.json`.

The final Release build has zero compiler warnings. Executed regression results are summarized in `validation-summary.json`. The broad Omega fixture initially failed identically against unchanged HEAD; it was missing the required native return-launcher receipt. Only that fixture was corrected, after which Release and Debug each passed 18,904 checks. No production behavior was changed to satisfy the fixture.

See [SERVER-NATIVE-MECHANISMS.md](SERVER-NATIVE-MECHANISMS.md) for plate/scan details and recovered native evidence. The installed build has not yet been played through in-game.
