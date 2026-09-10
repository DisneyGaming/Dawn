# Reconstructing a new mission with Sunrise

Contributor instructions and attachment checklist. Written against the Lua architecture on 8 September 2026. Examples use `C:/Destiny 2 Development`; substitute your workspace root. Confirm the current source and installed build before applying an older handoff.

The intended result is a playable mission whose story decisions are authored in Lua, whose native objects and receipts are handled in C++, and whose reconstruction choices are traceable to evidence.

For a fill-in mission brief, per-mechanic records, and the lessons from Beyond Infinity's live reconstruction, copy [Mission implementation template](MISSION-IMPLEMENTATION-TEMPLATE.md). Its 9 September update includes scene/dialogue separation, native portal contact, both Forest endpoints, native clocks, and fresh-build acceptance.

The additional 9 September Deep Storage update is now in the [implementation template](MISSION-IMPLEMENTATION-TEMPLATE.md) and [Lua authoring lessons](LUA-MISSION-AUTHORING.md#authoring-the-next-mission-lessons-from-deep-storage). It adds separate preload/arming/request/readiness/kill/retirement contracts, independent plate waves, scan duration overrides, native device modes, retained-object ownership and survivor-safe ending behavior. The earlier reconstruction guidance below remains applicable.

## 1. What the requester should provide

**Minimum to start:** the mission name and scope, a complete reference walkthrough, and access to the working repository and relevant game packages. You do not need to write a Lua script or a C++ profile before requesting a new mission.

### Mission brief

Attach a short `mission-request.md`, or paste the template at the end of this guide. Include:

- Exact mission title, game/content version, and difficulty or variant if known.
- Scope: full mission or selected segment; starting point; intended ending and destination afterward.
- Required behavior: encounters, dialogue, objectives, devices, scenes, boss mechanics, and any checkpoint/retry behavior you want reproduced.
- Whether to reproduce the observed run closely or permit documented reconstruction choices where evidence is missing.
- Current status: not started, already launches, partially reconstructed, or broken at a specific step.

A mission title does not identify all of its native objects. Activity IDs, package names, registry hashes, and spawn identifiers can be left unknown initially; the implementer must recover them.

### Video and dialogue references

Prefer one continuous walkthrough from launch/arrival through the ending. Supply the original URL or an accessible video file. Include the relevant time range when the video contains multiple missions.

Keep objectives, subtitles, sound, player movement, enemy arrivals, and interactions visible. For unclear mechanics, add short clips showing the lead-up, the action, and its aftermath. Separate clips for death/retry, skipped cutscenes, alternate routes, and unusual boss behavior are useful when those behaviors are in scope.

Attach subtitles (`.srt` or `.vtt`) or a timestamped dialogue transcript when available. Mark the speaker and whether a timestamp is relative to the full video or a clip. Identify edited videos, muted sections, speed changes, and loading cuts. Missing footage is an unknown, not evidence that nothing happens there.

A transcript or video establishes observed dialogue/order/timing. It does not establish native hashes, exact spawn counts, hidden trigger conditions, or which component owns a scene.

### Repository, existing scripts, and packages

On a shared machine, give absolute paths. For someone working elsewhere, provide an accessible repository checkout or matching source archive; a path on your own computer alone is insufficient.

Include the branch/commit and any uncommitted changes. If a playable candidate exists, include its installation receipt and matching source/payload archive. Identify the game executable, installed `steam_api64.dll`, and their SHA-256 hashes. Include matching PDB symbols when available; symbols for another DLL are misleading.

For an existing partial reconstruction, attach its current `.lua` file plus the matching native bindings, catalog, controller/runtime integration, and relevant tests. Include the current version even if it is broken. For a brand-new mission, there is no existing mission script to attach.

Provide access to the relevant installed packages and recovered extracts. A shared package-directory path is usually enough to begin; there is no need to attach an entire game installation to the request. Keep original package identities, source hashes, and extraction notes with exported data.

Old mission JSON can be historical evidence, but it is not executable input to the current loader. Settings, package-research reports, and build receipts may still use JSON.

### Current-run evidence, when available

For a bug or partial mission, include:

- Exact launch route and settings/override used, plus the steps to reproduce the problem.
- Expected behavior versus actual behavior, with a video timestamp or screenshot of the location.
- The complete relevant run log, including mission selection and the script-load line. The primary log here is `C:/Destiny 2 Development/Sunrise/logs/sunrise.log`; preserve it before rotation.
- Whether the run contained memory edits, debug interventions, skipped steps, or a DLL/script change.
- Relevant native captures with their process/build/owner metadata, as described below.

Do not replace the full log with only the last error line. Requests, native acknowledgements, and earlier ownership changes can explain the failure.

## 2. Code and reference files to use

Read [Lua mission authoring](</C:/Destiny 2 Development/Sunrise/docs/LUA-MISSION-AUTHORING.md>), [universal mission services](</C:/Destiny 2 Development/Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md>), and the [scripts README](</C:/Destiny 2 Development/Sunrise/scripts/README.md>).

Use [Deadly Trial Lua](</C:/Destiny 2 Development/Sunrise/scripts/deadly_trial.lua>) and [Gateway Lua](</C:/Destiny 2 Development/Sunrise/scripts/gateway.lua>) as the main examples of authored mission decisions. Pair each example with its own native files; matching names from different missions do not imply matching native assets.

Useful source entry points:

- [Deadly Trial bindings](</C:/Destiny 2 Development/Sunrise/src/state/activity/deadly_trial/bindings.h>) and [profile](</C:/Destiny 2 Development/Sunrise/src/state/activity/deadly_trial/profile.h>).
- [Gateway bindings](</C:/Destiny 2 Development/Sunrise/src/state/activity/gateway/bindings.h>), [controller](</C:/Destiny 2 Development/Sunrise/src/state/activity/gateway/controller.cpp>), and [runtime](</C:/Destiny 2 Development/Sunrise/src/state/activity/gateway/runtime.cpp>).
- [Shared executor](</C:/Destiny 2 Development/Sunrise/src/state/activity/coo/executor.h>), [mission compiler](</C:/Destiny 2 Development/Sunrise/src/state/activity/coo/mission_script.cpp>), [Lua front end](</C:/Destiny 2 Development/Sunrise/src/state/activity/coo/script_lua.cpp>), and [builder definitions](</C:/Destiny 2 Development/Sunrise/src/state/activity/coo/script_lua_dsl.h>).
- [Omega Lua](</C:/Destiny 2 Development/Sunrise/scripts/omega.lua>) and [Omega native profile](</C:/Destiny 2 Development/Sunrise/src/state/activity/coo/omega_script.cpp>) for its specific mechanics and composition interface.

Omega still enforces native phase prerequisites, required capabilities/receipts, and producer order. Its conversion to Lua preserved those constraints. Do not copy that fixed story contract into a new mission by default. Deadly Trial and Gateway are the better progression-ownership examples.

If the recipient has the full matching checkout, attach these references by path rather than assembling a second incomplete copy of the engine. If they do not, provide the matching source archive and the mission brief/reference media.

## 3. What belongs in Lua and what belongs in C++

**Lua owns mission choices:** which supported encounters are requested, dependencies and alternative conditions, dialogue choices, objectives/markers, permitted cue delays, phase order, and completion gates.

**Native bindings identify objects and capabilities:** registry/definition/type/slot identities, source and placement data, scene events, dialogue selectors, device values, and supported receipt policies. These bindings are currently compiled C++ manifests.

**C++ implements game interactions and ownership:** native creation, population readiness, real death/interaction reporting, device revisions, scene/audio acknowledgement, authority encoding, and cleanup/reset. Reuse the shared services and expose a missing interaction there when it can serve multiple missions.

A readable name such as `tower_enemies` is an author-chosen alias. A Lua step called `followers_dead` is not automatically a native engine event. Its registered commands and authenticated observations determine what it means.

Lua evaluates once at load time, creates a validated immutable mission definition, and closes its VM. The C++ executor runs that definition during gameplay. There is currently no persistent gameplay Lua VM, direct Lua memory API, or live Lua reload. Changing a script file requires a new game process to load it.

## 4. Reconstruction workflow

### A. Establish the baseline and launch

Inspect the current source, installed DLL/script hashes, settings, and prior acceptance records. Preserve unrelated work. Create a new evidence directory, for example `C:/Destiny 2 Development/build/coo/<mission>-research/`.

Recover the mission's activity identity and launch configuration: activity index, package/scenario, investment/activity asset, bubble, region/slice, and spawn set as applicable. Confirm the actual landing geometry and player control. A correct-looking level with the wrong activity contract is not a working mission launch.

Inspect the existing forced-destination/prelaunch integration before adding new routing. Choose the correct native schema; do not copy Omega's authority schema merely because an example uses it.

### B. Build an evidence map

For each encounter or transition, record the visible trigger, prerequisites, requested sources, required versus optional deaths, dialogue, objective/marker, device/scene changes, exit condition, and reset behavior.

For each native binding, record the proposed alias, package/registry/definition/type/slot, placement or source, associated component/event, evidence location, and confidence. Distinguish package facts, live observations, video observations, and reconstruction assumptions.

Use package extraction for native identities and geometry, video for observed progression, and live reads/hooks for runtime behavior. A guessed population grouping or nearest tactical assignment remains a reconstruction choice until corroborated.

Useful extraction examples include [package reader](</C:/Destiny 2 Development/tools/coo/package_read.py>), [Gateway extraction](</C:/Destiny 2 Development/tools/coo/extract_gateway_bindings.py>), and [Deadly Trial extraction](</C:/Destiny 2 Development/tools/coo/extract_deadly_trial_bindings.py>). Inspect output paths before adapting a generator so it writes the new mission's files rather than overwriting another mission's catalog.

### C. Implement a playable section, then extend it

Connect launch, one observed trigger, one encounter, its authentic completion receipt, and the next objective. Use this section to verify the native binding and ownership model before adding the full route.

Author the remaining story in Lua. Existing builders support steps, parallel commands, sequences, dependency graphs, and combined observations through `any_of`/`all_of`. Their capability names must already exist in the selected native profile. A new observation needs actual native evidence, not just a name in Lua.

Choose what must die before progression, what may survive, and when dormant sources become active. Do not activate every inventoried population at arrival or raise actor/heap limits to hide a scheduling problem. A spawn request is not proof of readiness; a timer, empty roster, or reached volume is not proof of a kill.

Assign each dialogue line to either native scene ownership or the host dialogue service to avoid duplicates. Start relative timers from their actual acknowledged event, not from when Lua requested an action. Latch valid early observations while other prerequisites finish.

### D. Add only the native interaction that is missing

Check whether an existing engine hook and shared service already observe the event. A hook observes or intercepts an engine call; it should authenticate the relevant owner and forward the fact to the correct mission adapter. Mission-specific filtering can be necessary without adding a new hook address for every mission.

If a new native hook is required, recover the target for this executable build and verify its calling convention and forwarding behavior. Keep story branches in Lua. Preserve run/generation ownership, reset, and teardown behavior in the native layer.

## 5. Files normally needed per mission

This is a suggested organization, not a required set of filenames:

```text
C:/Destiny 2 Development/
  Sunrise/scripts/<mission>.lua
  Sunrise/src/state/activity/<mission>/
    bindings.h                 # readable names -> registered native capabilities
    profile.h                  # profile declaration; may remain a thin wrapper
    catalog.h                  # recovered sources, placements, native metadata
    controller.h / .cpp        # graph/native-service integration and owned state
    runtime.h / .cpp           # selection, loading, update/reset, publication
    authority.h / frame.h      # only if the native integration needs these
  Sunrise/unit/<mission>_tests.cpp
  Sunrise/unit/<mission>_tests.vcxproj
  Sunrise/docs/<MISSION>-RECONSTRUCTION.md
  build/coo/<mission>-research/ # evidence, extracts, captures, unresolved mappings
```

Reuse shared executor, Lua compiler, services, and native probes. Do not duplicate them wholesale. Specialized native mechanics may need additional files; a simple mission may need fewer. A binding scaffold and an initial Lua draft can be quick when mappings are known. Unknown native interactions and lifetime bugs require investigation and iteration.

Register the new source/project items, mission selection/prelaunch path, native roster/authority routing, and observer dispatch. A new `.lua` file alone does not register a playable mission.

## 6. Native memory viewing and editing

Native runtime inspection and editing remain possible. The Lua migration did not eliminate entities, health/AI components, device controllers, scene state, or C++ mission state. No general-purpose live inspector/editor UI is promised by this architecture; use available debugging tools and verified probes.

### Viewing and capturing

Discover the current PID, process start time, game image identity, module bases, and installed DLL identity. Resolve the current mission/run and the intended object through its native handle/component chain. Addresses from another launch or an older executable are not reusable mappings.

Useful read-side code includes [current-process memory reader](</C:/Destiny 2 Development/Sunrise/src/client/memory/current_process_memory.h>), [component resolver](</C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/coo_native_components.h>), and [enemy readiness probes](</C:/Destiny 2 Development/Sunrise/src/client/hooks/bootflow/coo_enemy_readiness.h>). The current-process reader operates inside its own process; it is not an external attach-and-edit tool.

For every useful capture, save:

- Capture time and matching log/video event; PID and process start time.
- Game executable, DLL, and script hashes; module base and validated RVA when applicable.
- Mission/run, incarnation/generation, entity handle and salt where applicable, and relevant controller/component ownership.
- Address/range, byte length, raw bytes, interpreted type/layout, and how the address was resolved.
- Whether the capture was read-only or followed an intervention; before/after values for writes.

A screenshot without those details is supporting context, not a reproducible mapping. Limit each capture to the relevant objects/fields and retain the raw bytes alongside the interpretation.

### Editing an identified value

Work within the session's agreed live-edit scope. First state the hypothesis: for example, whether a device's existing native state controls the observed barrier. Record the original value, exact field/type, expected result, and how to undo the experiment.

Immediately before a write, re-resolve the owner and verify that its run/generation/handle and expected old value still match. Stop that write if they changed. Use the smallest understood change; do not treat a stale pointer, partially understood structure, or guessed flag as a confirmed control.

Prefer an existing native service or controlled C++ debug command when it manages the value. A raw write can be overwritten on the next update or leave host/native copies inconsistent. Inspect the resulting native receipt and visible behavior, not just whether the memory bytes changed.

Undo or retire the experiment while the same owner is valid. After a restart, despawn, reset, or generation change, discard old addresses; never restore old bytes into a reused allocation. Avoid manually changing immutable executor graphs, queued tokens, or fabricated death/scene receipts to claim that the mission works.

A successful experiment is evidence for an implementation. Move the lasting behavior into the correct Lua/native-binding/service layer, then test a fresh unmodified run. Forcing a gate open proves neither its natural trigger nor the preceding encounter's completion.

### What can change live

Native memory values may be inspected and, where understood, changed during the current process. Native control code can overwrite such changes. Lua source edits take effect after restarting the game. New compiled bindings or C++ code require a new matching DLL and process. Hot reload or a live authoring/debug bridge would be a separate feature.

## 7. Validation, deployment, and playthrough

Test the new mission's progression and its actual failure cases: delayed or repeated observations, early arrivals/kills, missing native readiness, stale owners, reset, retry, and completion. Keep existing mission/shared/native-protocol regressions when their code paths are affected. Include alternate Lua flows so tests do not silently recreate a required hardcoded story sequence.

**Update deployment when adding a mission.** Checked against the source on 9 September 2026; inspect the current lists before changing them:

- [verify_lua.py](</C:/Destiny 2 Development/tools/coo/verify_lua.py>) lists 18 suites in `TESTS`; register the new suite and ensure both Debug/Release configurations work.
- [package_lua.py](</C:/Destiny 2 Development/tools/coo/package_lua.py>) explicitly lists four files in `SCRIPTS`; include the new mission script in the payload and manifest.
- [install_candidate.ps1](</C:/Destiny 2 Development/tools/coo/install_candidate.ps1>) explicitly lists the same payload files and expects 38 build/test results. Update its file list, rollback coverage, and expected check count together with the runner. These counts must follow the configured suites and DLL builds; do not bypass checks to accept another script.
- Include additional data/build inputs in source hashing and packaging when the new mission introduces them.

After those changes, run from the workspace root using a fresh validation directory. Replace `new-mission` below with your own identifier:

```powershell
Set-Location 'C:\Destiny 2 Development'
python tools/coo/verify_lua.py --out build/coo/validation-new-mission
python tools/coo/package_lua.py --validation build/coo/validation-new-mission
powershell -File tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/validation-new-mission -ValidateOnly
# Close Destiny before the installation command.
powershell -File tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/validation-new-mission
```

The current toolchain is C++20, MSVC v145, x64, with embedded Lua. Use the repository's build configuration and installed dependencies. A focused `--project` run helps development, but packaging requires the complete configured suite and both DLL builds against unchanged source.

The installer preserves the previous matching DLL/PDB/license/scripts. It does not select the new mission or launch the game. Launch through [launch-destiny.cmd](</C:/Destiny 2 Development/launch-destiny.cmd>), then use the newly wired mission selection route. Do not assume that another mission's donor selection, activity index, or spawn set applies.

Confirm the log reports the intended mission, `format=lua`, and the expected script fingerprint. Play from the supported start through completion. Verify native visibility, interaction, audio, ending, and any requested retry/checkpoint/handoff behavior. A passing parser or completed executor graph alone does not prove that the native mission finished correctly.

If the user wants an early playable build, validate and install that candidate first, preserve its exact source and script hashes, and continue subsequent work in a separate candidate. Do not replace a DLL while that process is running. Keep playthrough reports associated with the build actually tested.

## 8. Deliverables for the next person

Leave the authored Lua, native bindings/integration, tests, updated build/deployment registration, and a short reconstruction document. Include the launch instructions, exact installed build/script identities, supported mechanics, remaining unknowns, reconstruction choices, test results, and live acceptance evidence.

Retain the original media/extracts or accessible locations, relevant logs/captures, and a matching rollback set. Clearly identify any debug-only commands or live interventions. Report only behavior established by the available evidence.

## 9. Copy-paste request template

```text
Reconstruct this mission using the current Sunrise Lua/C++ architecture.

Mission and version/variant:
Scope (start, ending, required handoff):
Difficulty / player count if relevant:
Required mechanics, cutscenes, checkpoints, and retry behavior:
Acceptable reconstruction choices / exclusions:

Workspace or repository access:
Branch/commit and uncommitted changes:
Game executable and installed DLL identity/hash:
Installed candidate/source archive or installation receipt:
Relevant package directory / recovered extracts:

Full walkthrough URL or accessible video file:
Mission time range within that video:
Transcript/subtitles and timestamp convention, if available:
Extra clips for boss mechanics, death/retry, or alternate paths:

Existing mission Lua and native code, if any:
Current launch route / override:
What already works:
Expected versus actual behavior at the failure:
Current-run log and matching video time:
Native captures, process/build metadata, and previous memory edits:
Live work scope (inspection, targeted edits, launch/restart handling):

Provide a short implementation outline, then carry out the reconstruction
within the stated scope. Reuse the shared executor and native services.
Put story decisions in Lua and verified native identities in bindings.
Investigate missing native mechanics; do not invent receipts or treat
unknown mappings as established facts. Keep existing missions working.
Include the new mission in validation, packaging, installation, and rollback.
Deliver clear launch instructions, an evidence-backed status, and remaining
unknowns. If an early playable candidate is requested, make that candidate
available before continuing broader cleanup or optional work.
```


## Deep Storage addendum for the next mission

Start by copying [MISSION-IMPLEMENTATION-TEMPLATE.md](MISSION-IMPLEMENTATION-TEMPLATE.md) into the new mission's implementation record. Preserve its existing Gateway/Omega/Beyond Infinity lessons and fill in the added Deep Storage records from the new mission's own evidence. Read the [authoring guide](LUA-MISSION-AUTHORING.md) for the detailed receipt and phase-completion contracts.

- Separate when scenery loads from when gameplay arms. Early occupied plates must not charge while unarmed, and arming must still be observed when occupancy is unchanged.
- Request the sources named by the actual cue. Deep Storage explicitly requests its descent encounter when the door opens; that is a scoped schedule, not blanket activation of all inventoried enemies. Native streaming readiness and required deaths remain separate facts.
- Give independent plates their own entry and wave-clear branches. Keep traversal and the ending free of unrelated death waits, including waits elsewhere in the same graph.
- Verify source overrides and effective native scan duration. Preserve the actual participating start and accept valid completion overshoot; do not use a fixed default duration or force observer flags as the implementation.
- Recover exact device mode direction and model/collision behavior. Logical off may be nonzero. A changed semantic state can need a new native revision even when logical active does not change.
- Track each rod, catch, beam, shield, cover, conflux and hologram separately. Inspect later script commands and pending native targets before adding a persistence correction.
- Resolve retained creation versus applied generations before respawning an object or weakening ownership. Keep a proved exception scoped to its source, lease and component.
- Reuse shared population/object/destructible services and existing hook boundaries. Scan/plate logic still partly lives in Deep Storage-specific adapters; extract reusable contracts when needed rather than assuming a universal refactor already exists.
- Validate both plate orders, delayed admissions, surviving optional enemies, full dialogue, source lifetime and reset. Record same-room checkpoint recovery independently from a fresh activity run.
- Freeze source and documentation for each full build. Preserve exact pre-install files and the previous coherent validated DLL/script set, then distinguish installation, targeted live repairs and a complete native playthrough.

The recorded post-Deep Storage validation baseline is 20 suites in two configurations plus both DLL builds: 42 results, with five mission scripts packaged. Read the current tools before adding another mission. See [Deep Storage's implementation record](DEEP-STORAGE-IMPLEMENTATION.md) and [combined candidate acceptance](../../build/coo/deep-storage-door-waves-beam-20260909/acceptance.md) for evidence and remaining limits, not as a promise that a new mission needs no native investigation.
