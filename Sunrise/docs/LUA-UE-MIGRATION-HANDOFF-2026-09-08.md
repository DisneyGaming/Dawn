# Lua mission scripting migration — UE handoff

Historical planning handoff. The migration and subsequent ownership work are implemented. Current files, Lua-only loading, and remaining Omega constraints are documented in [Lua mission authoring](LUA-MISSION-AUTHORING.md). JSON paths below identify the former baseline.


## Goal

Implement a Lua authoring front end for the existing universal executor (UE), then port **A Deadly Trial** from JSON to Lua without changing its gameplay behavior.

The user wants substantially easier mission scripting and would like the implementation completed in one focused pass where practical. A working implementation, build, and automated checks can be attempted in that pass; native equivalence still needs a fresh in-game playthrough.

Keep C++ responsible for the executor, native integration, ownership, and authenticated gameplay observations. This is a scripting-language migration, not a replacement of the UE or a new reverse-engineering project.

This document was prepared in a side conversation from the recorded parent-task history. It does not establish the current on-disk build or installation status. Verify the workspace before editing.

## First steps and current baseline

1. Read applicable `AGENTS.md` instructions and inspect the current working tree. There are extensive pre-existing changes; do not reset, clean, or overwrite unrelated work.
2. Inspect the current mission loader, document validation, executor, and Deadly Trial adapter before choosing the Lua interface.
3. Establish which DLL and mission files are installed, and which source revision they represent. Preserve a reproducible baseline.
4. Check whether the main task is still finalizing the lifetime patch described below. Avoid overlapping edits or overwriting its build artifacts. Use a separate validation output directory for Lua work.

### Important completion fix: verify before treating the baseline as final

The latest successful live test established:

- The mission controller finished correctly and sent state 6/result 1.
- The native global mission group had lost its activity owner, and the lifetime runtime had lost its sync-record handle.
- Restoring those two existing associations let the normal incoming packet apply state 6/result 1. No completion byte or end-function call was forced.
- The user confirmed the mission-complete banner/countdown and then a successful return to orbit.
- Character switching was not separately retested.

A permanent repair was written in `deadly_trial_lifetime.h/.cpp` and integrated with the bootflow lifecycle and DLL project. Debug and Release mission tests passed 6,678 checks before a final ownership-race guard was added. The subsequent rebuild was interrupted in the visible history. **Do not assume that the latest guarded version was compiled or installed.** Check current artifacts rather than continuing from that historical assumption.

Relevant evidence and build directories:

- `build/coo/deadly-trial-completion-live-20260908/`
- `build/coo/validation-deadly-trial-lifetime-20260908/`
- `build/coo/validation-deadly-trial-skiff-20260908/`

The previously installed Skiff build, before the lifetime repair, had DLL SHA-256 `b092e8459ccd8968d1fc664532ff185d325946fea31fc03562d39f765a5065ea`.

The accepted JSON script had SHA-256 `49a1d3108d6652abd6aa7f67f41f5239e1e5d62435ad0a4230c4d4201327cddc`.

These are historical comparison points, not assertions about the current installation. Do not reuse process IDs, addresses, or live patch commands from the diagnostic directory.

## Implementation scope

### First milestone

- Embed an appropriate Lua runtime after inspecting the Windows/MSVC build and dependency conventions. Record its version and license.
- Add Lua loading and validation beside the existing JSON loader.
- Expose a concise mission DSL backed by existing UE commands, conditions, sequencing, parallelism, and receipts.
- Port A Deadly Trial into a real `.lua` mission file and select it explicitly for testing.
- Retain JSON support for other missions and a deliberate baseline comparison path.
- Produce a build, tests, migration notes, and a testable patch.

Do not convert every mission in the first pass. Prove the interface with Deadly Trial, then use that result to estimate Gateway/Omega conversion.

### Recommended architecture

Prefer Lua that constructs the existing validated mission document/graph or an equivalent existing UE representation. Evaluate the authoring script at load time, then let the UE execute its commands and waits. This preserves the current scheduler and ownership behavior while removing much of JSON's verbosity.

Inspect what the current intermediate representation can express before committing to syntax. If direct imperative `await(...)` requires a new continuation or coroutine scheduler, explain that architectural cost rather than quietly introducing a second executor. A load-time builder DSL is an acceptable first milestone.

The earlier conversation's Lua example was illustrative, not an existing API:

```lua
await(walker:dead())
barrier:open()
tower:activate()

await(sagira:interacted())
await(revival:finished())
await(dialogue:finished())
await(activity:complete(6))
```

Design the actual API from existing capabilities. Keep mission code readable; do not merely embed a JSON string in Lua. Large native catalogs, tag mappings, geometry, and binary authority layouts should stay in their current data/native layers unless there is a concrete authoring reason to expose them.

### Runtime boundaries

- Keep native pointers, memory writes, hook installation, and raw authority encoding out of Lua.
- Bind commands and observations to the current mission run/generation. Stale callbacks must not affect a restarted mission.
- Preserve cancellation on leave, restart, character change, and shutdown through the existing lifecycle.
- Use actual native death, interaction, animation, and dialogue receipts where currently required. Timers cannot replace those conditions.
- Use a restricted Lua environment and bounded execution so an authoring mistake cannot hang the game. Expose only the libraries needed for mission definitions.
- Report load/validation failures with the script path, useful line information, and the offending command or reference where available. Invalid scripts must fail before partial mission activation.
- Prefer the established load-on-selection / reload-next-process behavior for the first pass. Hot reload is outside this milestone.
- A request to complete the mission is distinct from proof that native state 6 was applied. Preserve that distinction in diagnostics and any proposed API.

## Source map

Workspace root: `C:/Destiny 2 Development`.

Inspect these existing paths; verify their current contents and any renamed equivalents:

| Area | Paths relative to workspace root |
| --- | --- |
| Mission definition | `Sunrise/scripts/deadly_trial.json` |
| Loader / document code | `Sunrise/src/state/activity/coo/mission_script.cpp`, `script_views.h` |
| Executor and shared services | `Sunrise/src/state/activity/coo/executor.h` and neighboring service files |
| Mission adapter | `Sunrise/src/state/activity/deadly_trial/controller.h`, `deadly_trial_controller.cpp`, `deadly_trial_runtime.cpp` |
| Mission native data / authority | `Sunrise/src/state/activity/deadly_trial/catalog.h`, `authority.h`, `skiff_authority.h`, `navigation.h`, `ending_audio.h` |
| Accepted revival integration | `Sunrise/src/client/hooks/bootflow/deadly_trial_revival.h/.cpp` |
| Lifetime association repair | `Sunrise/src/client/hooks/bootflow/deadly_trial_lifetime.h/.cpp` |
| Hook lifecycle | `Sunrise/src/client/hooks/bootflow/bootflow_hook_lifecycle.cpp` |
| Mission tests | `Sunrise/unit/deadly_trial_tests.cpp`, `deadly_trial_tests.vcxproj`, and `fixtures/deadly_trial_lifetime_fixture.h` |
| Build and other regression suites | `Sunrise/Sunrise.vcxproj`, `Sunrise/unit/coo_script_tests.vcxproj`, `coo_mission_script_tests.vcxproj`, `coo_universal_services_tests.vcxproj` |
| Build helper | `tools/coo/verify.py` |
| Mission history | `Sunrise/docs/DEADLY-TRIAL-RECONSTRUCTION.md` |

The recorded loader used a format-2 JSON document, a mission-specific profile, and DLL-relative script discovery. Preserve its validation and deployment guarantees when adding Lua.

## Gameplay contracts to preserve

1. **Opening and Square:** opening objective/dialogue and Fallen spawn normally. Square clearance gates the Pike objective. An authenticated local-player Pike mount or the retained authored traversal alternative releases the route progression.
2. **Navigation:** retain the small persistent mission diamond and the accepted Radio Tower entrance height. Do not reintroduce the large Ghost-menu marker or underground exterior target.
3. **Walker:** entering the overpass activates the Walker, support, barrier, and Skiff. Only the admitted Walker actor's authenticated death clears the roadblock. Support deaths, travel, or elapsed time cannot substitute.
4. **Skiff:** preserve the accepted authored sequence: `dropship / enter_45_stop`, a native six-second hold, then `dropship / exit_45`. Keep its stable generation and accepted hover position. Do not replace it with a timer-driven respawn loop or a different entrance clip.
5. **Tower population:** Walker death activates all required Radio Tower sources independently of downstream travel/dialogue observations. Arrival and dialogue are not additional spawn gates or substitutes for clearing enemies.
6. **Travel dialogue:** retain the post-gate trial exchange and the Radio Tower arrival exchange, each once. Keep the established objective progression through the exterior fight and temple search.
7. **Sagira interaction:** preserve the pedestal hold, native Ghost summon/binding, hand release, authored animation, Sagira shell, VFX, and Ghost retirement.
8. **Dialogue ownership:** the accepted native revival animation owns the scan/revival audio. The host must not separately replay rows 9/10 and duplicate those lines. Inspect the latest code/documentation rather than copying superseded historical notes.
9. **Ending:** require the authenticated native animation end and the full remaining dialogue window before requesting state 6/result 1. Keep the lifetime association repair in C++; Lua should request completion through the supported service.
10. **Exit:** preserve run cancellation and native teardown. The successful live return-to-orbit test is part of the baseline to repeat after migration.

## Validation and acceptance

Use the same mission observations to drive both JSON and Lua definitions, and compare their normalized representation or observable command trace. Comparison must include command identities, payloads, dependencies, parallel behavior, and completion gates, not just step counts.

Required checks:

- Lua load and validation failures: syntax errors, unknown commands, invalid references, and unsupported values.
- Bounded script evaluation and restricted environment.
- Existing mission progression tests still pass using the Lua-authored mission.
- Missing/stale death, interaction, animation, and dialogue receipts do not advance their gates.
- Cancel/restart cannot deliver actions into the next generation.
- Native dialogue remains single-owner and is not submitted twice.
- Completion remains published after the mission graph finishes; native acceptance is separately observable.
- Other missions continue to load through JSON unchanged.
- Build the affected configurations with the repository's warning policy, and run relevant shared-loader/executor tests.

Then perform one normal in-game rerun: opening → Square/Pike → road encounters → Walker/barrier/Skiff → tower → Sagira → final dialogue → mission-complete banner/countdown → return to orbit. Test character switching separately if feasible. Log that the Lua file was actually loaded so a successful JSON fallback cannot masquerade as Lua validation.

Do not overwrite an installed DLL while Destiny is running. Prepare the candidate and evidence first, verify the game is closed, then install within the user's authorized test scope. Retain the previous DLL/script and record hashes. Do not launch a live mission or reuse historical memory patches without a current test request and current process verification.

## Deliverable

A readable Lua version of A Deadly Trial that runs through the existing UE, with the Lua front end, relevant tests, a compiled candidate, and clear installation/test status. Document the API actually implemented and the remaining limitations. Leave unrelated mission behavior and native choreography intact.

Suggested prompt to accompany this file:

> Implement the first milestone in this handoff: add Lua mission authoring on top of the existing UE and port A Deadly Trial while preserving its accepted behavior. Inspect the current workspace and completion-patch status first, avoid overlapping work, then implement and validate the migration. Prepare a testable build and tell me when an in-game rerun is needed.
