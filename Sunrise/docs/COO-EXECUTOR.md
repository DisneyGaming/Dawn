# CoO executor migration

## Current implementation - generic format and named receipts

Format 2 is implemented in `coo/mission_script.h/.cpp`. It accepts trusted native capability profiles and compiles authored graphs, forward dependencies, named command/receipt mappings, composition modules/facts and generic presentation sets into immutable definitions. It has no Omega dependency. `script_views.h` is generic; `omega_script_views.h` retains the once-only Omega publication bridge.

Opening, Forest, reveal and ending callbacks resolve semantic receipt names against their pinned executor definition, preserving run/incarnation/command ownership. Combat already iterates authored commands and qualified facts; it now selects graphs through named roles. The generic loader imposes no compiled Omega graph count, step names, command slots or pointer identity. Omega's profile retains its native phase prerequisites and receipt contracts.

The format-2 candidate is installed: DLL SHA-256 `37417A4F140C6399FBBC87DC27140A9C1BA4491112876B0CDEFD09D13248EC29`, script SHA-256 `39E161E9481A930EA76BDEB5305D2A377C5834B7BBEC8DCA7D1A05E921DBCFD1`, script FNV-1a64 `B8E30A738495A251`. Evidence is tracked under `build/coo/validation-generic/`: 39 regression runs passed, Release built with zero warnings/errors, and 1,521 source files were verified against the archive. The accepted DLL, settings and format-1 JSON are backed up at `.sunrise/backups/coo-20260906-184644-30b5850f/`; installed and rollback file hashes were verified. This build is accepted for the recorded normal Omega progression run. Evidence: `build/coo/accepted-generic-20260906-190055/`. The log confirms format 2 with the expected fingerprint, completed opening and Forest (zero skipped landmarks), all Crown progression and final death, supported ending skip, complete composition (`000001FF`), one queued Mercury launch and native Mercury in-world arrival. No executor failures occurred. Three retail-log/core shutdown errors occurred after arrival. Enumerated retry cases, consecutive launches and unskipped ending playback are not established by this log. The user confirmed that all changes work. The accepted format-1 build below remains the parity reference. See `Sunrise/scripts/FORMAT.md` for authoring and integration boundaries.

### Accepted format-1 JSON reference

The accepted format-1 `omega.json` supplied the executor graphs and presentation data at runtime. `coo/omega_script.cpp` parses and validates a complete immutable document; `coo/script_views.h` publishes it once before executor admission. The DLL resolves the script path relative to its own directory. Supported edits take effect on the next game process without rebuilding the DLL. Invalid/missing scripts block executor publication and log `ev=coo_script result=failed`; no automatic legacy fallback occurs. Explicit legacy mode still uses compiled definitions.

Format 1 preserves required native step/command identities and receipt dependencies, resolves JSON names to stable receipt slots, and validates native asset/population contracts. It supports additional earlier dependencies and editable presentation cues, dialogue durations, queue timing and Forest line delays. Native mechanics, codecs and population construction remain C++. See `Sunrise/scripts/README.md` for editable fields, limits and validation commands. This is not yet a general loader for arbitrary second-mission scripts.

That format-1 candidate was installed with DLL `2B9252E7F72D19CDB0FEB96DD170EA04707ACB3A630ECD6E64922245D677B0AF`, script SHA-256 `E343CB217DE5932E4DFAEEA6E354FE70A3966FE286965D519FFD8871697113C7`. Evidence is tracked in `build/coo/validation-scripts/`: 37 regression runs passed, Release built with zero warnings/errors, and 1,514 source files were verified against the archive. The accepted DLL and settings are backed up at `.sunrise/backups/coo-20260906-175237-e58c9199/`. This build is accepted for normal progression. Evidence: `build/coo/accepted-json-20260906-182150/`. The log confirms the expected JSON fingerprint, completed opening and Forest (no skipped landmarks), final Crown/death completion, ending completion, one queued Mercury launch and native Mercury in-world arrival. No executor failures occurred. Death/retry coverage on the prior build and consecutive-launch coverage remain separate. Existing parity suites load the shipped JSON before testing, and the script suite checks malformed input, reordered definitions, immutable publication, invalid-script admission and real scheduling changes from edited JSON. The accepted build described below remains the comparison reference.

### Accepted shared-services reference

Omega's normal sequence and shared-service refactor are accepted on DLL `930A20D5B0AEEB90D15543021A6A20C41DE3F2FCC212E668995705AB6467B85D`. Evidence: `build/coo/accepted-shared-20260906-172544/`. The native log confirms mission composition and ending completion, a single Mercury handoff, and Mercury in-world arrival. The user also confirmed that death and retries work. Individual retry cases were not enumerated; consecutive launches and unskipped ending playback remain separate unchecked cases.

Candidate source, 35 passing regression runs, build hashes and installation receipts are under `build/coo/validation-shared/`. The latest run used the supported ending skip. Forest `skipped=2` records a presentation landmark passed without being observed; the graph retained its accepted reached-or-passed behavior. Gateway remains unmapped and unimplemented. Gateway asset/schema mapping and a playable second mission remain future work.

### Shared execution and native services

- `coo/mission_runtime.h` owns run selection, publisher leases, definition validation, ordered publication, retained observations and reset. Its `MissionDefinition` supplies a DAG, up to eight module bindings, and up to 32 fact-to-command bindings. Module count, IDs, update order and milestone layout are data. A different definition cannot replace a live run's definition. Invalid or ambiguous bindings fail before any publisher is called.
- `coo/native_services.h` provides the common command bridge used by opening, Forest, reveal, combat, ending and composition. Before a mission binding receives a command, it checks the full run/incarnation/step/command token, explicit schema, operation, asset, argument and wait against the immutable definition. Each binding still interprets its native assets and builds the schema-specific authority. Cancellation retires requested bookkeeping in reverse dependency order; native retirement remains its own acknowledged operation.
- `coo/receipt_queue.h` provides the bounded FIFO used by presentation and ending. Native intake validates ownership and request eligibility before queueing. Only equivalent adjacent samples coalesce, retaining their first timestamp. Overflow stays latched through discard and clears only on reset. Opening retains its authenticated packet-batch intake; immediate boss/action claims and population admission remain synchronous because their native return values control admission and retry.
- `coo/dialogue_service.h` owns request deduplication, delayed queues, native submission acknowledgement, generation publication, dispatch timeouts, spacing, and cancellation of unpublished stale cues. Bank rows, timing, scene ownership and objective restrictions are supplied as data. An offered row survives traversal until acknowledgement or its accepted timeout; an audio submission is not proof that the clip was audible.
- `coo/population_service.h` owns cohort enablement, admitted identities, duplicate rejection, salted native death matches, count bounds, and clearance checks. Excess required population reports failure; extra members of an optional cohort do not introduce a kill barrier. The Omega mechanic still chooses which cohort is current and which native summon enables it.
- `coo/scene_service.h` owns retained Scene commands, bounded positive generations, full owner tokens, duplicate events, stop commands and milestone bits. The Omega binding supplies Scene indices and qualified observations. Rescue conditions and the native Scene codec remain outside the shared ledger.
- `coo/presentation_services.h` dispatches authored traversal/objective/dialogue requests; `coo/presentation_cues.h` applies ordered objective/dialogue actions selected by event and cycle.

All stored runtime, queue, dialogue, Scene and population state is value-only. Polymorphic services exist on the update stack. The production session reset/copy regression remains required because its secure wipe cannot preserve embedded vtables.

### Omega definitions and native bindings

`coo/omega_definition.h` now declares `kMission`: three logical module bindings, their publication order, and eight retained milestone observations. The former `Adapter` class is gone. `omega_adapter.h/.cpp` keeps the existing include/API boundary as a thin native integration module: it binds presentation, encounter and ending producers, performs existing admission/lock checks, supplies qualified facts and logs diagnostics. It contains no separate executor lifecycle or hard-coded lease scheduler. The shared runtime invokes each producer once in the accepted order and `omega_projection.h` feeds the unchanged server snapshot publisher.

`omega_presentation_definition.h` contains Omega's dialogue bank, row metadata, objective selectors, objective-dependent stale-cue bindings, landmark actions, per-cycle encounter actions, reveal completion cue, rescue Scene identities and authored line spacing. `omega_presentation_rules.h` qualifies native mechanics and delegates scheduling and cue application to the shared services. Panoptes's boss admission/flight, eye/health behavior, Arc-charge ownership, rescue decisions, and native transition rules remain Omega mechanics. Fixed native Forest generation and gate readiness remain in the accepted native implementation.

Section definitions remain in `omega_opening.h`, `omega_forest.h`, `omega_reveal.h`, `omega_combat_definition.h`, and `omega_ending_definition.h`. They own progression; the composition graph coordinates their publishers and records qualified milestones. New logs use `ev=coo_executor mission=omega mode=composition`. The complete mask remains `000001FF`; the ending's normal complete mask remains `0000007F`. Handoff completion means the native launch was queued. Actual destination in-world arrival is a separate observation.

### Validation and adding a mission

Run `python tools/coo/verify.py --out build/coo/<new-candidate>`. The verifier protects all accepted source files outside the explicit ten-file extraction boundary, including native hooks, roster construction, catalogs, authority codecs, the corrected Ikora lookup and the Osiris hold fix. It verifies the differential oracles directly against the accepted source archive. The new suite compares the latest accepted presentation and composition implementations, including all Crown cue choices, missing/wrong/late submissions, pending rows, reset and timeouts. Independent contract fixtures exercise another schema and module layout, dialogue bank, population catalog and Scene layout. These fixtures establish code reuse; they are not a playable second mission.

To add a mission:

1. Verify the actual activity, authored assets, native receipts, and encode/parse schema. Use registry/definition/type/slot identities; never extraction indices or a guessed Omega schema.
2. Define immutable section DAGs, module bindings, observation mappings and presentation tables. The definition must remain alive and immutable for its entire run. Bindings must implement every declared module ID.
3. Supply typed authority producers through `MissionPorts<Frame>` and native request bindings through `NativeServices`. Reuse the dialogue, Scene and population services where their contracts fit. Keep special mechanics in a mission plugin.
4. Validate receipts at intake with the native owner captured when the command was issued. Preserve immediate native admission/claim contracts; drain asynchronous receipts on the serialized owner. Commands, readiness, completion and destination arrival remain separate facts.
5. Replay against captured evidence and compare actual authority bytes. Then validate native sections, full runs and consecutive launches before claiming mission compatibility.

The legacy selector remains available for comparison. Typed C++ definitions now provide the frozen native contract and legacy reference; executor mode uses the validated JSON views. Mapping and proving a second real mission and additional native retry/relaunch coverage remain separate work.

## Historical migration checkpoints

The sections below preserve the original checkpoint descriptions and acceptance evidence. Their pending-work statements describe those earlier builds, not the current implementation.

## Preserved reference

`build/coo/omega-baseline-20260906/baseline.zip` contains 2,819 hash-verified files: active source, project, vendor code, settings, installed DLL, mission cache, exports, fixtures and both recent logs. `manifest.json` records every file hash and the Git revision. `tools/coo/freeze_baseline.py` refuses to replace an existing baseline or freeze a different installed DLL.

Accepted DLL SHA-256: `F4FFA03E31DDC8A0F5927D4038448AA71DE2870A649E0064873D1EB34F1FE92F`.

The older saved log records the complete baseline handoff at `t=901156`, run 1, epoch 20, actor `51F4200D`, `complete=1`, `handoff=3`. The latest log records the corrected second hold at `t=599828`, Scene 27, child `80EC0E00`, parameter `2 -> 0`, marker 133, native success. These are baseline evidence, not acceptance of the new executor.

## Active dependency map

The DLL project compiles `omega_presentation.cpp`, `omega_first_lair_runtime.cpp`, `omega_ending.cpp`, `omega_lair_cinematic.cpp`, the native receipt bridges, and the archive authority codecs. Older `omega/omega_progression.cpp`, `omega/omega_lair_start.cpp`, and `omega_reveal_native.cpp` are not DLL inputs. Their old fixtures cannot substitute for the active archive encounter tests.

The admitted local `mission_scot` roster now calls `coo::omega::update`. Its adapter leases the existing presentation, encounter and ending controllers. It calls them once in the original order and projects their fields through `omega_projection.h`. The normal server publisher still encodes and sends the resulting snapshot. No second publisher runs alongside it.

The adapter reads qualified, copied controller observations on the normal publication path. Opening type-6 receipts now use deferred intake, described below. Other native callbacks continue using the accepted controller APIs at this checkpoint. Their immediate claim/receipt return values participate in native retry and ownership logic; converting these to deferred events requires separate fixture and native validation. They have **not** all been moved onto the new event queue.

`omega_presentation::reset()` cancels adapter leases before the existing ending, encounter and presentation teardown. The run selector is latched. Changing `experiments.omega.coo_executor` during a run does not switch publishers. Missing/false selects legacy; true selects the adapter on the next run. The default settings retain false. The candidate installer explicitly enables the adapter for testing.

## Executor contracts

`coo/executor.h` is a bounded C++20 executor, independent of Windows and Omega. Definitions contain an explicit schema, named steps, dependencies and typed commands. Earlier-step dependency masks form a validated acyclic graph. Independent steps and commands run concurrently; every command's wait must be satisfied before a step joins. The limit is 32 steps, 8 commands per step and 128 queued observations.

Operations cover scenes, populations, objectives, dialogue, devices, cinematics, traversal, mechanics and verified observations. The current production adapter implements controller leases and milestone observations. Concrete shared implementations of the other operation kinds remain part of the section migrations; the enum does not mean those services have been extracted.

Every command carries the external run, a monotonically increasing executor incarnation, step and command identity. Cancellation followed by reuse of the same external run cannot accept an earlier incarnation. Events for unrequested, inactive or foreign commands are rejected. Enqueue does not advance the mission. The owner serializes queue access and invokes `update`; service callbacks cannot reenter the executor.

Request acceptance, native readiness and completion are separate facts. A native operation cannot complete before readiness. An observation operation accepts an explicitly verified observation; it does not pretend a native object was created. Controller leases join on request acceptance and retain their lifetime until teardown. Queue overflow and publication/native failure fail the run and retire acquired leases in reverse dependency order. Cleanup is idempotent. Definition storage must outlive the active run; cancellation releases it.

`Asset` identifies registry, component definition, type and slot. `resolve` requires one exact match and rejects ambiguous/missing bindings regardless of catalog order. The production Ikora fix remains the accepted registry-key lookup, untouched. The new generic resolver does not replace or weaken it.

## Authority schema boundary

The mission definition explicitly selects `omegaArchive`. Production still selects `Snapshot::archiveOmega` only for `mission_scot`; `activity_message_route.cpp` likewise selects `parse_omega_sense_update` only for Omega. The separate other-mission codecs remain compiled. A CoO label does not authorize reusing Omega's wire schema for Gateway.

## Diagnostics

Change-only `ev=coo_executor mission=omega mode=adapter` logs record run, incarnation, phase, active and completed masks, failure and the current wait label. The milestone labels cover opening/Ikora, Forest, Lair encounters, each Crown cycle, ending and queued handoff. These labels observe the accepted controllers; they do not replace the native completion conditions. A direct Forest/Lair launch may satisfy earlier traversal labels without replaying skipped content.

`handoff queued` means the native selection request was queued, not that the destination loaded. The final native handoff may occur after the last roster update; retain the existing `omega_ending stage=handoff_queued` receipt when checking completion. Never infer handoff success from an absent executor log.

For a stalled step, inspect its command states (`requested`, `ready`, `completed`), then match the native receipt's complete identity, authored source and epoch. Timers never complete a missing mechanic. Panoptes health, rescue Scenes, charge ownership, movement and native animation timing remain in their accepted implementations.

## Validation and candidate

Run `python tools/coo/verify.py` from the repository root. It verifies the frozen archive, builds the available Debug/Release configurations, compares the existing encounter and protocol outputs against the frozen source, runs the Ikora and other-mission regressions, runs the Osiris hold fixture, and builds an isolated Release DLL. The parser project supports Release only; the runner does not claim a nonexistent Debug test.

The new executor fixture covers parallel joins, stale/duplicate receipts, completion-before-readiness rejection, queue overflow, service failure, cancellation and same-id restart. Its adapter test compares **12 synthetic authority phases** through the actual projection and body encoder, including producer call order and a latched selector. It is not an in-game replay or proof that every native hook executes correctly.

The accepted adapter results remain in `build/coo/validation`; opening candidate results are in `build/coo/validation-opening`. The candidate source archive and per-file hashes identify the source used for the DLL. The verifier rejects source changes during the DLL build. `tools/coo/install_candidate.ps1` checks the candidate hash, the expected installed baseline, process closure, settings syntax, verified backups and rollback. It changes only the DLL and the executor selector; it never launches the game or clears the cache.

For a legacy comparison, set `experiments.omega.coo_executor` to false before a new run. For exact binary rollback, close the game and restore both `steam_api64.dll` and `settings.json` from the recorded installation backup. Leave the frozen baseline intact.

## Omega acceptance checklist

Every item must record candidate DLL hash, source manifest, run identity, observations and a saved full log. Offline fixture passes do not check the gameplay boxes.

- [ ] Fresh opening, normal dialogue/objectives and Ikora approach; native orb/lattice release and real portal entry.
- [ ] Forest stairs, generated route, native enemy populations, required combat completion, eligible forward gates and exit. Backtracking does not reopen retired progression.
- [ ] Lair fly-in, camera alignment, actor readiness, dialogue, all four encounters/departures/cannons and Crown arrival.
- [ ] Cycle 1 waves, deletion, first Osiris hold, charge route, pickup/drop/dunk, eye opening/damage, body checkpoint and return.
- [ ] Cycle 2 waves, Scene 27 second hold at the corrected marker 133, charge/eye/body sequence and return.
- [ ] Cabal escape, final traversal/cannon, cycle 3 waves, final rescue/charge/eye, verified native boss death.
- [ ] Final dialogue, native roster retirement, Lighthouse state 121 arrival, cinematic active/inactive or supported skip, queued handoff and observed destination arrival.
- [ ] Second Omega launch after completion; no stale dialogue, mechanics, leases or old receipts.
- [ ] Supported death/retry and direct-start behavior match baseline. Existing unsupported wipe/checkpoint behavior is recorded as a limitation, not silently declared fixed.
- [ ] Other-mission protocol and launch regressions remain clear.

After adapter acceptance, migrate ownership in the proposed order: opening/Ikora, Forest, Lair arrival, encounters, Crown, ending/handoff. Reuse the same checklist for each section, and repeat the complete Omega run before opening the Gateway gate. Keep legacy selection until both missions have passed final acceptance.

## Adding a mission after the gate

1. Establish the exact local package/activity and authored registry identities from the installed build. Record region and slice bindings, scenes, populations, objective/dialogue sources, completion receipts and uncertainty.
2. Verify encode and parse schema compatibility independently. Leave the definition unregistered when compatibility is unresolved.
3. Add immutable asset bindings and a typed definition. Put sequencing/dependencies in the definition; reusable operations in services; specialized native behavior in a mechanics module.
4. Feed validated observations with the captured command identity into the queue. Never label an observation with whatever run happens to be current when a delayed callback arrives.
5. Exercise overlapping operations, joins, retries, stale receipts and cleanup. Compare actual authority bytes and native identities, not only stage names.
6. Validate sections in-game, then full runs, then consecutive launches of both missions. Retire the temporary adapter only after those gates pass.

## Research basis and Gateway limits

The architecture follows the separation of event intake from processing described by Robert Nystrom in [Event Queue](https://gameprogrammingpatterns.com/event-queue.html), and uses concurrent steps to avoid the single-state limitations described in [State](https://gameprogrammingpatterns.com/state.html). Local compiled sources, authored records and accepted fixtures are the authority for this build's behavior.

[Bungie's ActivityDefinition API schema](https://bungie-net.github.io/multi/schema_Destiny-Definitions-DestinyActivityDefinition.html) describes public activity metadata. It is not evidence for the private registry identities or authority protocol in this legacy executable. The local source/export search found an Omega definition/export for `mission_scot`, but no verified dedicated Gateway definition. Gateway's package, activity, regions, assets, native sequence and schema compatibility remain unresolved. No guessed Gateway mission or automatic success transitions have been added.

## Installed adapter checkpoint - September 6, 2026

The first adapter candidate was installed with Destiny closed. DLL SHA-256:
`677B39E4FD50ECB83A9E5318F22E8A35A6568CCF40AD1A7956519CF6ADF91435`.
Only `experiments.omega.coo_executor: true` was added to the active settings; all other parsed settings match the verified backup. The game was not launched.

Rollback DLL/settings: `.sunrise/backups/coo-20260906-115308-c793e68b/`.
Installed receipt: `build/coo/validation/installation.json`.
Acceptance state: `build/coo/validation/acceptance.json`.
The baseline also has an `accepted-fixes.zip` supplement preserving both fixes' native evidence and the Osiris fixture.

All 19 available regression runs passed. The new executor/adapter suite passed 528,228 checks in each configuration. The frozen/local archive protocol output matches at 71,412 bodies, 12 packets and digest `A5BE474333FF4DDF`; the full encounter replay matches at 939 checks. The accepted second-hold fixture passes 1,047 checks per configuration. Release DLL compilation completed with zero warnings/errors.

**Normal playthrough accepted:** the September 6 run confirmed the adapter through native cinematic completion and arrival in `mercury_freeroam`. The next implementation section is opening/Ikora ownership migration. Second-launch, supported retry and backtracking edge cases remain unverified; controller extraction and Gateway are unfinished.

## Accepted adapter playthrough

The user reported successful play. The log confirms the installed candidate hash, PID 49280, run 1, executor incarnation 1. Executor milestones progressed through Ikora, Forest, all Lair encounters and all three Crown cycles with `failure=0`. The second Osiris hold used Scene 27 parameter 0 / marker 133 with native success at `t=626046`.

Native boss death advanced the ending at `t=766203`; the exact old roster completed retirement at `t=776625`. The ending movie started at `t=779046`, completed at `t=950953`, and handed off at `t=950968`. The world changed to `mercury_freeroam` at `t=959015` and entered that activity in-world at `t=966500`. This confirms actual destination arrival, not just a queued request.

There were no error/fatal log records, executor failures, encounter failures, ending failures, or dialogue dispatch timeouts in the saved snapshot. Expected early/stale native-observation rejection diagnostics are present and did not prevent later accepted progression.

The final executor diagnostic remains `waiting="handoff queued"`, as documented: roster publication ends before it samples the accepted handoff. The native terminal and destination-arrival receipts establish gameplay completion. Closing this diagnostic lifecycle gap remains part of the ending/handoff migration.

Evidence archive: `build/coo/accepted-adapter-20260906-121452/`. It preserves the full log snapshot, accepted DLL, settings, candidate source archive, offline results and a hash manifest with exact milestone line numbers. This accepts the normal adapter playthrough; it does not mark unexercised retry/second-launch cases or Gateway as complete.

## Opening host sequence candidate - September 6, 2026

`coo/omega_opening.h` now owns roster admission, the exact approach edge, Scene request, native Scene readiness, the lattice-release output and the independent Forest entrance observation. The definition binds the authored selector graph `80EC0F95` at `D00142CF/43/1` and device definition `80F47BA0` at `D00142CF/23/16`. These identities come from the preserved local Ikora investigation, not registry array positions. The reconstructed host join remains unchanged; this migration does not claim recovery of the retail host script.

The graph requests `C7ECAA77` after admitted approach. A fresh, fully validated native Scene delta establishes readiness. Only its `792AAA50` output satisfies the separate portal-output observation and activates the device-release command. This output is **not whole-Scene completion or actor retirement**. The native graph retains its timer, actor ownership and dialogue. The host does not add a timer or infer success from request delivery.

`omega_opening_intake.h` reuses the exact accepted roster matcher, monitor parser and complete Scene extractor. Under the existing session lock it copies binding identity, packet ordinal and validated observations into a bounded queue. It performs no progression or service publication. Events within a packet retain the original policy order: reset, roster, approach, Scene outputs, entrance. Earlier-than-admission observations are discarded when processed; they cannot become future triggers merely because update was delayed. Duplicate acknowledgements preserve the current graph; a bound reset retires it and increments the next executor incarnation. Patch-epoch changes and binding replacement discard the entire owned observer and pending queue.

The normal keepalive consumes the queue into the persistent session **before** region-debt handling or detached snapshot construction. Delivery retries copy already-committed semantic state and preserve the released gate even when an outbound packet is discarded. Snapshot construction only projects that state. Authority body writers and the accepted registry/Osiris fixes are unchanged. Overflow retires opening leases and fails closed until the observation owner is replaced; it does not switch to legacy.

The selector is now shared and latched from the first admitted Omega intake/publication, including the period before the in-world seed. Opening and the existing adapter therefore cannot select different modes after a settings reload. `coo_executor: false` retains the original inline opening policy and direct controllers. Foreign-session and other-mission paths retain their existing behavior.

The new change-only `ev=coo_opening` log carries the binding, graph incarnation, source packet ordinal, active/completed masks, roster readiness, Scene request, release, entrance and failure. Type-6 intake logs say `queued`; they are not claims that the mission has already advanced. A normal opening finishes all six steps after release and entrance. A direct Forest start can satisfy entrance while leaving the skipped Ikora branch pending.

### Offline evidence

The replay fixture preserves **40 complete, unmodified type-6 packets** from the accepted adapter log, including the actual native release at `t=97484` and entrance at `t=102093`. Each packet records its source line, time and ordinal; validation checks all bytes and the full source-log hash. The comparison oracle is extracted from the frozen production policy; only diagnostic logging and the external quiescence read are substituted. Validation verifies that the frozen block has not drifted.

The opening test replays those packets with updates after every packet, after seven packets, and after all forty. It compares the opening-related authority through the actual body writers with quiescence enabled and disabled. Additional cases cover early/stale/duplicate events, invalid authentication/epoch, malformed Scene metadata, coalesced approach/output/entrance, last-monitor-state semantics, reset with queued receipts, changed bindings, direct Forest entry, retry copies and overflow. These are offline checks; they do not establish new in-game acceptance or timing.

Ghost dialogue and objective scheduling still live in the presentation controller leased by the adapter. Forest traversal, Lair, Crown and ending ownership also remain there. Extracting reusable dialogue/objective/native mechanics services and closing the ending diagnostic gap remain required before final Omega parity and Gateway work.

### Candidate use

Run `python tools/coo/verify.py` to stage this candidate independently. The default output is now `build/coo/validation-opening`; use `--out` for another directory under `build/coo`. Validation refuses to overwrite installed-candidate evidence. The installer defaults to the accepted adapter's expected DLL hash, backs it up, and writes a fresh opening-candidate receipt. It never closes or launches Destiny and never clears the cache.

Next in-game check: launch through the usual CMD, play the normal opening through Ikora's lattice release and native portal entry into the Forest, and preserve the log with the candidate DLL hash. Confirm the native orb/beam, dialogue and objective timing remain intact. Direct-start, second-launch and retry behavior still need their applicable in-game checks. This candidate is not yet accepted in-game.

### Installed opening candidate

Installed with Destiny closed on September 6, 2026. DLL SHA-256: `CBB2B7A3674C376A0192C4EB8BDE0D6BA9A6CE1DE80CAA614521935F7CCC8520`.

All 21 offline regression runs passed; the Release DLL built with zero warnings/errors. The opening suite passed 371,634 checks and compared 28,459 authority bodies in each configuration. The installed settings are byte-for-byte identical to the accepted adapter settings.

Rollback: `.sunrise/backups/coo-20260906-123139-ea2d59ba`. Installation and pending acceptance receipts: `build/coo/validation-opening/installation.json` and `acceptance.json`. The game was not launched; opening-candidate in-game validation remains pending.

## Opening load-crash correction

The first opening candidate (`CBB2B7A3...`) failed on its first in-game load. The native crash report for PID 42832 records `EXCEPTION_ACCESS_VIOLATION` reading address zero at `0x00007FFE98761F4A`; the subsequent network-update stall caused the visible application hang. The first roster packet was queued, but no opening step was logged. That candidate is rejected, not accepted.

The regression was a C++ object-lifetime error introduced by storing a `Run` derived from virtual `Services` inside `Session`. The existing connection/authentication lifecycle calls `SecureZeroMemory(&session, sizeof session)` and then assigns default state. Secure wiping erased the embedded vtable pointer. Assignment restored the data members but did not reconstruct that pointer. The first executor service call dereferenced it.

The isolated regression now uses the actual production `Session` and its secure-wipe/default-assignment path before replaying the captured initial roster. Against the failed implementation it reproduces native exception `0xC0000005`. Policy-only Observer fixtures had not exercised this lifecycle. The fix removes virtual inheritance from stored opening state and creates the polymorphic service bridge only for the duration of `Run::update`. Stored `Run` is now a plain, trivially copyable value. Compile-time guards enforce that property for both `Run` and the whole `Session`.

The regression also exercises authentication-state restoration and deferred matchmaking retirement, ensuring active requests and queued edges are cleared while required connection fields and sentinels are preserved. The original secure-erasure behavior is retained. Failure evidence and the failing test output are preserved under `build/coo/opening-load-failure-20260906/`. New validation uses `build/coo/validation-opening-loadfix` so the rejected binary and its evidence remain intact.

### Installed load-crash fix

DLL SHA-256: `2ADBDCFAC9B925F72FC6EA32947860C096A9E0B14603C0247FA311AB4532841F`. All 21 regression runs passed; the opening suite passed 371,667 checks per configuration, including all three production secure-reset paths. The Release DLL built with zero warnings/errors. Installed with Destiny closed; settings were unchanged and the game was not launched. Native load retry remains pending.

Evidence: `build/coo/validation-opening-loadfix/`. The accepted working adapter remains recoverable from `.sunrise/backups/coo-20260906-123139-ea2d59ba/`; the immediately previous rejected candidate is separately backed up at `.sunrise/backups/coo-20260906-124425-e33573bc`.

### Opening-to-Forest acceptance

The user confirmed that Ikora through entry into the Infinite Forest works on the corrected DLL `2ADBDCFA...`. The log independently confirms binding 2, incarnation 1, admitted roster at `t=92859`, approach at `t=114813`, native Scene release output at `t=127531`, and native lattice position 0 / revision 2 at `t=127703`. All six opening steps completed with mask `0000003F` and `failed=0` at `t=133078`; the native Forest region 88 was observed at `t=133391`. The first-load crash correction and this normal opening path are now accepted.

This does not establish full Forest traversal, a full run on the migrated implementation, or second-launch/retry coverage. Next: move Forest progression ownership and the remaining dialogue/objective scheduling into shared services, preserving the native generator, populations, combat-conditioned gates and exit. Then validate the route from Forest entry to Lair arrival before continuing the Lair migration.

Three hook-retention errors occurred during shutdown at `t=194500`, after session departure, with none during the accepted opening. They are retained as separate cleanup evidence; this is not a claim that shutdown was error-free.

Accepted evidence: `build/coo/accepted-opening-20260906-124941`.


## Forest-to-Lair ownership migration - September 6, 2026

`coo/omega_forest.h` defines eight steps: independent observations for reaching or passing the tunnel, vista, Forest exit and Lair, each followed by its authored presentation requests. A later direct arrival skips earlier presentation requests without claiming that their volumes were entered. Ordinary traversal retains all four requests in order. Returning to an earlier volume does not regress the route or replay dialogue.

Bindings use the shipped type-60 footprints `BA5F26EF/60/5`, `2763EC97/60/4`, `2763EC97/60/5` and `A3928C71/60/2`. The shared `presentation_services.h` dispatches exact, unambiguous traversal/objective/dialogue bindings. The Omega target retains the accepted audio scheduler and native generations: rows 6, 7 and 9 in bank `80F1FD07`, Forest objective `1EBF4621`, then Lair objective `3517D4D5`. The tunnel line keeps its 1500 ms spacing. The definition owns which requests occur; the existing scheduler still owns audio arbitration and encounter-specific stale-cue policy. These one-shot requests wait for service acceptance, not an invented native playback-completion receipt.

`omega_forest_controller.h` copies native position, dialogue, Scene, intro and encounter observations into a bounded FIFO under the existing presentation lock. Position coalescing retains the first timestamp only for consecutive samples with identical volume and waypoint membership. The owner drains before publication/scheduling; a fast crossing cannot overtake an earlier submission or replay scenery already passed. Intake rejects unrequested dialogue acknowledgements and discards inactive or unrelated Scene apply traffic before queueing; those events cannot affect the accepted presentation policy. Reset discards the queue and starts a fresh incarnation even when the numeric run ID repeats. Overflow stops this sequence and logs failure without selecting legacy. The controller contains no vtable or owning resources.

The remaining Lair reveal/encounter logic is unchanged but its asynchronous observations share this FIFO to preserve order across the handoff. Synchronous boss/flight claims keep their existing native contracts. A queued camera observation carries whether flight had already been observed at intake; a later flight receipt cannot retroactively release it. The owner clock is clamped to processed receipt time to avoid unsigned timeout underflow when publication waited for the lock.

The production adapter supplies its latched mode to presentation. `coo_executor: false` uses the accepted synchronous legacy path. Native callbacks never enter the adapter mutex while holding the presentation lock. Both normal snapshot publication and periodic publication checks drain on the owner side. Native audio receipts are still admitted during streaming; new volume observations retain the original in-world admission requirement.

The native Forest generator, deterministic recipe/anchors, Vex population selection, enemy clearance, gate eligibility, final-gate route retirement, authored volume geometry, and roster group membership are byte-for-byte protected against the accepted opening candidate. No enemy kill, gate completion or boss creation is inferred from elapsed time. The native generator's unverified authority body remains disabled. Forest exit geometry and native final-gate readiness remain separate facts. This section ends at the Lair objective; Lair reveal/encounters, Crown and ending ownership still await their migrations.

Validation adds frozen presentation replay, actual type-53/type-68 byte comparisons, direct arrivals, skipped volumes, backtracking, in-flight audio, FIFO ordering, stale owners, overflow and reset checks. A separate executable compiles the production adapter and presentation implementations with only game/encounter dependencies substituted, exercising admission, streaming, selected/legacy mode, reset and concurrent callbacks. The accepted timeline fixture reconstructs volume timing from changed publications and uses recorded audio acknowledgements; it is not a captured position trace. Native playthrough acceptance is required separately.

Candidate validation and installation evidence: `build/coo/validation-forest/`. Native acceptance is pending a normal Ikora -> Forest -> Lair run, including Forest combat gates, exit dialogue, Lair objective and the existing reveal.

The old standalone `omega_forest_recipe_tests` target still calls APIs from the retired implementation (`apply_cached_recipe`, `RunSeed::get`). A trial build confirmed that pre-existing incompatibility; its failure log is retained under the candidate directory. It is not part of the accepted regression set. The new Forest suite instead checks the compiled `prepare_worker`/`RunSeed::select` API, exact worker/node ownership, terminal progress, gate-open qualification and destination-bubble handoff, with those production sources protected against the frozen candidate.

Final Forest candidate installed with Destiny closed: `7651F7334053443F86468898C9FE33EBB8CE8E22E5CD6E1050C1B1523210B3FE`. All 25 regression runs passed. The Forest suite passes 1,267,889 checks and 5,232 authority-body comparisons per configuration; the compiled runtime suite passes 25 checks. The full Release build has zero warnings/errors. The 1,479-file source archive is verified against both the workspace and its hash manifest. Settings are byte-identical to the backup. Rollback: `.sunrise/backups/coo-20260906-131658-469cd37b/`. Native Forest-to-Lair acceptance remains pending; the game was not launched.


### Forest-to-Lair native acceptance

The user played the installed Forest candidate `7651F7334053443F86468898C9FE33EBB8CE8E22E5CD6E1050C1B1523210B3FE` through Panoptes's spawn. The saved log confirms opening completion at `t=284688`, native Forest population clearance and final gate/route retirement at `t=532375`, exit dialogue submission at `t=539375`, and all eight Forest steps complete at `t=556813` (`complete=000000FF`, `skipped=0`, `failure=0`) with Lair objective `3517D4D5`. Tunnel and vista dialogue both have native dispatch receipts.

The existing reveal also completed: Panoptes member ready at `t=558391`, native flight ready at `t=559391`, camera active at `t=559704`, native camera completion at `t=568157`, and Ghost row 12 submitted at `t=568672`. This accepts the normal Forest migration and its compatibility with the following reveal. The reveal remains owned by the existing Intro controller; migrating that dependency chain is next, followed by the Lair encounter/cannon sequence. This run did not exercise the first arena, Crown, ending, a second launch or retry.

There were no executor failures or dialogue dispatch timeouts. The same three retail-log/core shutdown retention errors occurred at `t=578844`, after session departure at `t=578454`; they remain a separate cleanup issue.

Evidence: `build/coo/accepted-forest-20260906-153040/`. It preserves the complete log, DLL, settings, verified candidate source archive, offline results, installation receipt and milestone/file hashes.


## Reveal, Lair and Crown ownership batch — September 6, 2026

`coo/omega_reveal.h` defines the reveal joins: camera/audio eligibility, owned boss flight, native camera activation, and native camera completion. A separate retry definition preserves the accepted three-attempt limit and command revisions. Deadlines can abort; they cannot complete the reveal. One receipt satisfies only the phase in which it was processed. Presentation keeps the existing FIFO, captured flight readiness, immediate boss/flight claims, audio arbitration, and completion dialogue mapping.

`coo/omega_combat_definition.h` defines seven consecutive sections: initial Lair, chase islands A/B/C, and Crown cycles 1/2/3. The definitions choose summons, required-population barriers, departure/arrival joins, wave changes, deletion, rescue Scenes, charge route, eye exposure, recovery, relocation, final arrival and ending handoff. Each section runs in the generic DAG executor. Section completion retires its command bookkeeping and starts the next section with a fresh incarnation.

The mechanic binding `{95FB2E01, 80F45253, 0, 0}` identifies the Omega boss plugin; type/slot zero are **not a claim about a native roster type or slot**. Its services resolve the unchanged accepted population, Scene, charge and transit catalogs. Native admission/death identity checks, actor salt, generation/epoch checks, one-shot claims, repeatable charge pickup/drop behavior and native animation ownership remain in `omega_first_lair_encounter.h` and the existing hooks. Summon-start receipts still release their own accepted population immediately, so native admission does not race a delayed owner update. In executor mode, the old next-wave/cycle/departure helpers do not run.

Recovery joins native animation recovery, the body-health checkpoint and creation of the current return launcher. Final death joins actual boss death and native death-animation completion. Cycle-two escape uses the accepted **right-arm** summon and does not wait for the Cabal to die. Final relocation joins fold/path completion, transit preparation and player arrival; early final-platform arrival is retained. No time-based kill, arrival, readiness or completion was added.

The combat runtime latches the same per-run selection passed by the adapter. Native callbacks validate and record facts synchronously under the encounter lock; only `authority()` and `publication_due()` advance the executor. Read-only status calls and claims never advance it. `ev=coo_combat` reports section, incarnation, active/completed masks, failure and the exact waiting step. A failed definition or native ledger stops progression without selecting legacy. Stored controllers remain trivially copyable; virtual service objects exist only on the update stack.

The accepted Forest source archive supplies a frozen combat oracle. Verification compares every existing source file outside the explicit sequencing boundary against that archive, including native hooks, catalogs, unrelated missions and codecs. The combat authority projection itself must remain text-identical, and the existing protocol suite must retain its baseline digest. Differential replay compares all population enables, action epochs, phases, Crown/charge/transit flags and retained Scene generations, tokens and events after each owner update.

The new replay exercises 48 complete four-island/three-cycle fights: all six recovery receipt orders, both death receipt orders, early final arrival, late initial idle, deaths before/after summon completion, surviving Cabal, and multiple recovery facts before one owner update. Reveal tests cover retries, inactive/stale camera receipts, flight eligibility, direct arrival, skip and abort deadlines. A separate executable compiles the actual combat runtime to test callback-versus-owner behavior, salted deaths, mode latching, reset, admission and concurrent intake/publication.

The definitions are still compiled C++ headers. External `.def`/JSON loading and a portable definition format are not implemented. Omega-specific presentation mappings and mechanic internals remain compiled plugins. The ending movie, roster retirement and destination handoff retain their accepted controller; this batch hands control to it after the native death join. Full in-game acceptance of this batch, consecutive launches and supported retry cases remain pending.

Candidate output: `build/coo/validation-lair-crown/`. Use its installation and acceptance receipts for the final DLL identity and native test status; offline parity does not establish in-game acceptance.

Installed combined candidate: `E2EB62E836582FD62E403C3819B225CCD6A40BE22966432F09F2D46757DA0344`. All 29 regression runs passed; the Release build has zero warnings/errors. Each configuration passed 6,724,391 new checks with 41,220 frozen combat-state comparisons across 48 full-fight variants, plus 138 checks of the compiled combat runtime. All 1,486 archived source files and the installed DLL were hash-verified. Settings are byte-identical. Rollback: `C:\Destiny 2 Development\.sunrise\backups\coo-20260906-155602-7fc35dc0`. Installation was performed with Destiny closed; the game was not launched. Native full-run acceptance remains pending.


### Combined batch native acceptance

The user completed Omega on DLL `E2EB62E836582FD62E403C3819B225CCD6A40BE22966432F09F2D46757DA0344`. The log confirms every combat section (0–6), the final native death join at `t=723266`, roster retirement at `t=733578`, ending playback at `t=737438`, supported skip at `t=761407`, and native completion at `t=762735`. The outer adapter completed all nine milestones (`000001FF`, `failure=0`) at `t=762938`; Mercury free-roam started in-world at `t=778203`. Opening and Forest also completed without failures. The saved snapshot has 0 error/fatal records, no executor/encounter/ending failures, and no dialogue dispatch timeouts. This accepts the normal full run with the supported ending skip. It does not establish a second launch, death/retry, or an unskipped ending on this candidate.

Evidence: `build/coo/accepted-lair-crown-20260906-161049`. Remaining implementation work is ending/handoff ownership, reusable native services and receipt routing, definition-driven presentation bindings, and replacing the temporary Omega controller adapter with shared mission composition. A second mapped mission is still required to establish reuse. External definition serialization is optional and follows stable service contracts.


## Ending and native handoff ownership — September 6, 2026

`coo/omega_ending_definition.h` now owns the normal ending sequence: final dialogue, native Lair roster retirement, bookend region/camera eligibility, native camera activation, native camera completion, cinematic-command retirement and the single Mercury launch request. The final step waits for the native launcher to report that the selection was queued. Queue acceptance remains distinct from actual destination arrival; Mercury in-world arrival is checked separately in native acceptance evidence. The existing temporary outer adapter still observes mission milestones.

`coo/omega_ending_controller.h` is a value-only controller with short-lived service objects. In selected mode it replaces the old ending state machine's sequencing. Retirement, arrival, camera, skip and launch-result callbacks copy receipts into a bounded FIFO. Arrival and command eligibility are checked at intake; a camera observation made before native arrival cannot become eligible because arrival was processed later. Consecutive identical camera samples coalesce without moving their first timestamp. Reset discards pending receipts and preserves a fresh executor incarnation for a reused run. Overflow fails the selected path without falling back to legacy.

Both the serialized publication owner and the existing game-frame handoff owner drain the queue. The frame path remains active during the movie and drains the launch result immediately after native commit, so shutdown of Omega publication cannot strand completion. A claimed launch result can finish bookkeeping after the old run stops being current; that path cannot publish a new cinematic, teleport or launch. Native claims stay synchronous and single-use. Start and finish are forwarded outside the ending lock, including when one owner update consumes both native camera transitions.

The retry definition begins at camera eligibility. It retains the accepted three-attempt cap and command revisions, and does not replay roster retirement or the host teleport transaction. The accepted preparation and playback deadlines can abort only. A skip publishes the next stop revision and remains incomplete until the matching native inactive/resource-ready receipt arrives. Duplicate or unsolicited launch results cannot complete an unclaimed request. Development preview and explicitly selected legacy runs retain the accepted controller.

The existing native handoff implementation is unchanged except for three explicit calls to the new owner update. Verification checks those exact additions and preserves the launch descriptor, nonce handling, guards, forced-selection suspension, native exit and loading-cinematic handling. The native retirement hooks, movie hooks, teleport policy, authority codecs and the previously accepted opening/reveal/combat implementation are byte-protected against the accepted Lair/Crown archive. The old ending class remains unchanged as the selectable legacy implementation; a separately frozen copy is the differential oracle.

The new ending suite compares 714 states and 6,426 actual authority bodies per configuration, including the accepted ending timeline, unskipped and skipped completion, all acquisition attempts, resource loss, stale tokens/revisions, abort deadlines, native retirement, handoff success/failure, queued observations, overflow and reset. The timeline reconstructs API inputs from recorded phase/revision boundaries; it is not a raw camera-callback capture. A separate executable compiles the actual ending runtime and verifies native teleport ownership, deferred callbacks, start/finish forwarding, per-run selection, preview, concurrent intake and completion after native commit changes the current run.

Candidate output: `build/coo/validation-ending/`. Full in-game acceptance remains pending. After acceptance, the normal Omega ownership migration is complete through launch handoff. The shared native service extraction and mission composition work can proceed against the preserved full-run reference and offline replays; native integration changes may still require targeted in-game checks. Consecutive launch and supported death/retry coverage remain explicit outstanding validation items.

Installed ending candidate: `922886E2524E9B00F21BA95A6D938AD8B488703CBC5DAAD2CDDDFF53FDCE70C2`. All 33 regression runs passed; Release compilation has zero warnings/errors. All 1493 archived source files and the installed DLL were verified. Settings are unchanged. Rollback: `C:\Destiny 2 Development\.sunrise\backups\coo-20260906-162743-0f0ac154`. Installation was performed with Destiny closed; the game was not launched. Native acceptance is pending the normal full run, preferably with unskipped ending playback, and observed Mercury in-world arrival.


### Ending and native handoff acceptance

The user confirmed the ending cutscene on DLL `922886E2524E9B00F21BA95A6D938AD8B488703CBC5DAAD2CDDDFF53FDCE70C2`. The full log confirms all earlier sections and the final native death join at `t=1003031`, native Lair retirement at `t=1013421`, bookend arrival at `t=1015328`, and native movie activation at `t=1015500`. A supported skip was requested at `t=1053203`; native cinematic completion followed at `t=1055812`. The ending executor completed all seven steps (`0000007F`, `failure=0`) and the single Mercury launch was queued at that same tick. The outer mission adapter completed all nine milestones (`000001FF`, `failure=0`) at `t=1056000`. Mercury free-roam was independently observed in-world at `t=1071328` with the matching launch nonce.

This accepts normal Omega progression through the executor-owned ending and native handoff, with the supported skip. No executor failures or dialogue dispatch timeouts occurred. Three known retail-log/core shutdown retention errors occurred later at `t=1230984`; they remain a separate cleanup issue. Unskipped ending playback on this candidate, consecutive launches, and supported death/retry remain unverified.

Evidence: `build/coo/accepted-ending-20260906-165454/`. The normal progression ownership migration is complete. Remaining implementation work is shared native services and receipt routing, definition-driven presentation bindings, replacing the temporary outer Omega adapter with shared mission composition, and mapping a second mission to establish reuse. Omega-specific mechanics remain mission plugins. Definitions are still compiled C++; external definition loading is optional later work. Native integration changes may still require targeted in-game validation.


## Shared services candidate — September 6, 2026

Both requested code-side extractions are implemented and installed. Candidate DLL: `930A20D5B0AEEB90D15543021A6A20C41DE3F2FCC212E668995705AB6467B85D`. All 35 available Debug/Release regression runs passed. The new shared suite passes 809,515 checks per configuration, including 61,717 frozen presentation state comparisons and 64 old/new composition variants. The existing opening, Forest, full-fight, ending, compiled runtime, protocol, settings, other-mission and Osiris-hold suites also pass. Release compilation has zero warnings/errors, and all 1505 source files are hash-verified against the archive and workspace. The protocol digest remains `A5BE474333FF4DDF`.

Source archive SHA-256: `f44c69d87d57f8593972b55cf3eaf1ab87095d5e1d483f02332dd2cb61d68b62`. Source manifest SHA-256: `918bab736188cbb52488ad65b0b3fd744e9a87e9f595be868ee44837193350c0`. Evidence: `build/coo/validation-shared/`. Installation kept settings byte-identical and preserved the last accepted ending DLL at `C:\Destiny 2 Development\.sunrise\backups\coo-20260906-171228-49582591`. Destiny was closed and was not launched. This is offline-verified implementation, with native validation of this candidate pending; no new in-game acceptance is claimed.


### Shared services native acceptance

The user validated the installed shared-services candidate `930A20D5B0AEEB90D15543021A6A20C41DE3F2FCC212E668995705AB6467B85D` and confirmed working death/retry behavior. The captured run completed opening at `t=110157`, Forest at `t=131735` (`skipped=2`), the final native death join at `t=534704`, native retirement at `t=545063`, and movie activation at `t=547516`. The ending skip was requested at `t=585641`. Native completion and the seven-step ending graph (`0000007F`, `failure=0`) completed at `t=587875`; exactly one Mercury launch was queued. Shared mission composition completed (`000001FF`, `failure=0`) at `t=588063`, followed by Mercury in-world arrival at `t=603266` with matching nonce `2DE8296E-36907760`.

The saved snapshot contains 0 error/fatal records, no executor failures, and no dialogue dispatch timeouts. Death/retry acceptance is based on the explicit user report, not an inferred reset count or an exhaustive retry matrix. Unskipped ending playback and consecutive launches remain separately unverified. Evidence: `build/coo/accepted-shared-20260906-172544/`.
