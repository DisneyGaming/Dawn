# Panoptes eye immunity and abrupt closing — handoff, 6 September 2026

## Start here

The installed build is **5CEC908E**. The player can complete Osiris's rescue, carry and dunk the Arc charge, and teleport to the eye platform. **The eye briefly opens, shots during that opening display Immune, and the eye visibly closes abruptly.** Eye damage and subsequent progression remain unverified.

The latest change made native health monitoring produce its initial reading. It did **not** fix vulnerability. The next task is targeted reverse engineering and instrumentation of actual damage-script execution, shot rejection, and the native state responsible for visible eye closure.

**Those three traces have not been captured, and their instrumentation is not ready.** We know the damage-ping resource and creation path; we have not identified all downstream execution handlers, the exact shot-rejection function/reason fields, or the native control for visible closure. Do not describe this as a finished capture design or request another identical run expecting new evidence.

D2 is closed at handoff. Process status was checked again while writing this document. Recheck before any live operation; all process IDs, handles, and addresses below are historical.

## User preferences and scope

- Work solo. The user explicitly asked to conserve tokens and stop spawning agents.
- Follow the supplied documentation's reconstruction while preserving its stated limits. The retail mission controller was not recovered; the referenced `D:/Sunrise-port` tree is unavailable.
- Keep explanations concise and distinguish observations, hypotheses, implemented changes, and unverified behavior.
- Current focus is eye damage and progression. Do not add unrelated FPS changes.
- Do not force damage, write arbitrary health/immunity values, manufacture a threshold receipt, replay an already accepted effect speculatively, or replace the native sequence with a timer.
- Do not launch or close the game automatically. Install only when it is closed. Previous work authorized building/installing the scoped fix; avoid redundant permission questions.

## Workspace and installed build

Workspace: `C:/Destiny 2 Development` (PowerShell).

Documentation: `C:/Users/gauta/Downloads/docs/docs`.

Installed DLL: `C:/Destiny 2 Development/steam_api64.dll`.

Build identity:

```text
Build ID: 5CEC908EAB8141DB5E41905CB5299A68092430E714B99862267234BD3CC7F36D
DLL SHA256: 22E309FAF15E1FCA59FFB98B2FD53CAA9121F9CD5A6BAA2C636B45FC5E4D0425
Source SHA256: 8AE5438773855D0C81978775E3D540148F7471784ED2F6D20DA47F08325884C7
Candidate: build/omega-full-20260905/candidate-20260906-005800
```

The installed DLL hash was reverified when writing this handoff. The candidate contains 1,460 frozen source files. Release built with zero warnings/errors; all 18 Debug/Release regression runs passed. The native boss runtime suite passed 6,603 checks per configuration. These tests do not establish physical eye vulnerability in the game.

Previous DLL backup: candidate `evidence/before-install/steam_api64.dll`.

Launcher: `launch-scot-reveal-debug.cmd` in the workspace.

No newer production edits, candidate, or installation were made during the most recent capture investigation. The newly added investigation scripts are under `build/omega-full-20260905`.

## Latest run: observed facts and the user's correction

Historical PID **51632**, game image base **0x7FF6BD2F0000** (decimal 140697712656384), build **5CEC908E**.

Log times below are the run's `t=` values in milliseconds:

1. `408391`: accepted native Arc dunk, cycle 1, epoch 13.
2. `412406`: eye-platform arrival allowed shield claim, epoch 14. Eye-refill resource `80F45564` returned native dispatch success; shield event `E9D4F854` was confirmed.
3. `412422`: first `eye_health_sample`: body **1.0**, eye **1.0**, `crossed=0`, health handle `7CF9EDCD`.
4. `412422`: all three frame-zero opening resources reached `104DA00`; each had `builder_calls=1`, `builder_accepted=1`, `current=1`.
5. `412422`: opening node 3, clip `80F45179`. Cursor consumed entries 0–3, then remained 3–3.
6. `417094`: node 6, clip `80F4518E`, classified by the host as the eye loop. No changed health sample, threshold crossing, host recovery event, or recovery node was recorded in the examined run.

**User observation takes precedence over an interpretation of node names:** the user shot during the brief visible opening, saw Immune, then saw the eye close abruptly. The log's “eye loop” classification does not prove that the eye remains visibly open or damageable. The logged opening-to-loop interval is about 4.67 seconds; the visible opening was described as about one second. Do not equate these durations.

An earlier run's user missed the shot. That is historical and must not be repeated as the latest result.

## Saved evidence

All paths in this section are relative to the workspace.

- **Latest live snapshot:** `build/omega-full-20260905/eye-events-51632-1788671669/`
  - `capture.json`: captured component identities and addresses.
  - `capture-status.json`: explicitly lists captured and missing evidence.
  - `sunrise.log`: log at snapshot time.
  - `closed-run.log`: log after process exit.
  - `character.bin`, `animation.bin`, `biped.bin`, and individual boss-owned component snapshots.
- Earlier log from this same run: `build/omega-full-20260905/eye-immune-51632-1788671434.log`.
- Investigation audit: `build/omega-full-20260905/eye-damage-audit-20260906.md`. Read its corrections and latest appended entries; earlier hypotheses are retained for history.
- **Original health getter proof, previous PID56596:** `build/omega-full-20260905/health-emulation-56596-1788670357/`. Original `CD6C20` executed in Unicorn using copied live pages, with no stubs or game calls. Body and eye returned 1.0. No writes outside the private emulated stack occurred.
- **Original authority lookup proof, previous PID56596:** `build/omega-full-20260905/member-authority-emulation-56596-1788670625/`. Original `AB27C0` returned authority enabled=1, member disabled=0, matching revision 0. Saved authority/member bytes accompany the result.

Latest snapshot identities:

```text
Actor       19F4200A
Character   08F9E936
Animation   20F3A00A
Biped       4AF9E93D
World       18FAA149
Member      41F90034, offset 0, generation 2, revision 0
Health      7CF9EDCD
Visual script 21F9E91F (resource 80F6695E)
Refill graph  7EF9E2D2 (resource 80F45563)
```

Damage-ping graph `80F4547D` was absent from the **late** component enumeration. This is not proof it never executed: it may be transient. The refill graph's continued presence likewise does not establish stalled execution or repeated refilling.

D2 exited during further investigation. A subsequent `OpenProcess` returned error 87, and process enumeration confirmed closure. The main snapshot above was saved before exit. The newer `capture_eye_native_state.py` attempt failed before acquiring a process handle; it did **not** capture its proposed additional vectors/region state.

## What the installed change actually does

Production files under `Sunrise/src/client/hooks/bootflow`:

- `omega_boss_graph_runtime.inl`: health sampling removed from the `AB6600` member hook. `graph_update` wraps original `F4E660` and calls the Crown observer.
- `omega_mission_crown.inl`: calls `observe_mission_health` after validating the native Crown graph, full boss ownership, current cycle, queue, bank, and node.
- `omega_mission_health.inl`: original regional getter, typed/full-handle guards, retained threshold crossing, and body checkpoint observation.
- `omega_reveal_native.cpp`: install revision 25, `health_monitor=native_crown_callback`. Still 15 reveal hooks; this change added no hook.
- `Sunrise/unit/omega_full_mission_native_fixture.inl`: three cycles exercise health sampling through production `graph_update`, including opening baseline, foreign-graph rejection, downward crossing, and body checkpoints.

The older member-only path produced no initial health sample. The new path does. Do not assert that a particular old native scheduler guard was conclusively identified: the exact cause of the missing old invocation was not established.

Existing validation remains: claimed shield/eye/recovery command, current immutable token and full owner, typed health component and matching entity, before/after identity checks, finite fractions in [0,1]. A fresh crossing is `previous > .90 && current <= .90`. A first low sample is insufficient. Body checkpoints are exactly .55 and .10. No health writes are made.

## Known native eye-event path

Opening clip `80F45179` contains three frame-zero kind-3 events:

- `80F453BF`: cast-end, event at file offset `7580`.
- `80F45568`: illumination-stop, at `75A8`.
- `80F4547E`: damage-ping, at `75D0`.

Resource is at event `+18`. Eye loop `80F4518E` has no such events.

Proven dispatch chain:

```text
F50280 -> F4E190 (cursor advancement)
F49BC0 -> 1050570 -> 104D2D0
kind 3 -> 104DA00(event, context, biped, policy)
entity branch -> 4B25F0 builder validation -> construction path
```

`104DA00` continues through native construction and iterated component-interface dispatch via `B31960`. Original `B31960` is an interface trampoline: it reads descriptor from pair+0 and instance from pair+8, then jumps through descriptor-relative method slot +30. This is a known route to investigate, **not yet a qualified installed trace of damage-ping script execution**.

Existing instrumentation:

- `omega_eye_clip_trace.inl`: hooks `104DA00` and `4B25F0`, with a scoped thread-local observation. Filters the three resources and qualified boss/dunk/cycle ownership. Reports dispatch and builder acceptance only. Also observes bounded opening/loop cursor samples.
- `omega_eye_resource_trace.inl`: hooks named-resource dispatcher `C71C30`, preserving all five arguments and its return. Refill uses this path.

**Important correction:** absence of cast-end/illumination-stop/damage-ping from `C71C30` logs does not mean those clip events failed. Raw kind-3 clip events use the path above. This mistaken premise was explicitly withdrawn.

Damage-ping entity `80F4547E` references graph `80F4547D`. Its compiled descriptors at `1E60/1E70` carry operation `3C` and region `720A5B5B`; later descriptors carry `5F/3F`. Their exact runtime semantics and effect on vulnerability remain unproven. Do not equate these bytes with a particular character request ABI without tracing the actual decoder.

Refill `80F45564` references graph `80F45563`; authored state action `80804AB1/80804AB2`, command `1F`, eye region `720A5B5B`, full checkpoint `95A36FFE`.

## What the supplied docs do and do not establish

Read `README.md` first, but treat its older installed-build and stopping-point claims as historical relative to this handoff.

- `omega/reference/BOSS-HEALTH.md`: separate body/eye regions; eye threshold .90; full refill 1.0; body .55/.10/death. Properties `BE57CE2E` and `DD46E308` are health outputs, not writable immunity controls. Its final paragraph explicitly leaves physical eye vulnerability and script effects for live validation.
- `omega/reference/OMEGA-PANOPTES-COMBAT-ACTIONS.md`: three native graphs and explicit shield/eye-end events; native observations, not host durations, advance phases. Its last paragraph says the private event proof does not execute the health script or prove gameplay effects; immunity remained unresolved.
- `omega/reference/OMEGA-FULL-ENCOUNTER-STATE-CONTRACT.md` and `OMEGA-CROWN-FULL-FIGHT.md`: reconstruction and receipt ordering; later stages were not live accepted in that source handoff.

No documented one-second eye damage window or instruction to close after an immune hit was found. The .90 checkpoint eligibility proof is **not** proof that an incoming bullet is eligible to damage the eye.

## Next work — identify before instrumenting

1. **Damage-ping execution:** resolve the actual created component's native execution interface and the handlers for its authored commands. Identify a narrow point that proves command execution and its target owner/region. Builder success alone is already measured.
2. **Visible closing:** identify the native animation/provider/region value or command that controls closure. Capture its changes alongside opening clip, graph node, and health. A node label cannot substitute for visual-state evidence.
3. **Immune hit:** identify the native incoming-damage decision function, verify its ABI, and establish how the target health/region and rejection result are represented. These have not been identified. Do not hook an arbitrary health function and name its result “immunity.”
4. Verify the proposed entry points against original instructions and local fixtures. Add bounded, observational traces filtered to the exact boss and current cycle/token. Preserve original arguments, return values, native calls, and timing behavior. Extend atomic hook install/rollback/uninstall and call-gate tests for any added hooks.
5. Build and verify a concrete diagnostic candidate before asking for a replay. Explain what each new trace will prove and what remains unknown. After install, capture one opening and the player's shots on a shared timeline. Only then select a gameplay fix.

Additional static disassembly was saved in `build/scot-progression-20260905/native-*.txt` for health functions including `CD8320`, `CD9370`, `CD9590`, `CDE440`, and `CDB440`, and in `build/omega-full-20260905/health-static-offsets.txt`. These are investigation leads only. None was established as the relevant shot-rejection or eye-closing function. Some functions are protected or split into unwind fragments; do not treat a partial disassembly as the complete function or interpret bytes after a return as meaningful continuation.

## Build and capture tools

Use PowerShell and keep dependent steps sequential; wait for a running process to complete before starting the next step.

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File build/omega-full-20260905/freeze_candidate.ps1
python build/omega-full-20260905/build_rescue_phase.py
pwsh -NoProfile -ExecutionPolicy Bypass -File build/omega-full-20260905/finalize_arc_receipt_candidate.ps1
# Only after game closure and candidate verification:
pwsh -NoProfile -ExecutionPolicy Bypass -File build/omega-full-20260905/install_candidate.ps1
```

The reused build/finalize scripts write stale rescue/Arc text into `live_validation`. Replace that manifest field with the actual candidate scope and limitations after verification. `candidate-path.txt` selects the frozen candidate. Installation verifies previous DLL/settings hashes, saves a backup, copies the candidate, and checks its hash.

Read-only helpers:

- `build/omega-full-20260905/capture_eye_events.py PID BASE_DECIMAL`.
- `build/omega-full-20260905/capture_motion.py PID BASE_DECIMAL`.
- `build/omega-full-20260905/emulate_health_read.py PID BASE_DECIMAL`.
- `build/omega-full-20260905/emulate_member_authority_read.py PID BASE_DECIMAL`.

These require a freshly discovered process/base and live matching ownership. The emulators execute copied instructions in Unicorn, never inject a call into D2, and reject writes outside their private emulated stack. Dependencies under `build/native-emulation-deps` and `.codex-tools/minidump` may require an escalated read because of existing directory permissions.

Datum resolution uses the native **row+8 signed masked displacement and wrapping subtraction**. Row+0 is not the allocation pointer. The older empty snapshot `eye-events-50740-1788668695` used the wrong interpretation and is invalid evidence.

Do not globally scan frozen candidate trees unnecessarily. On Windows, use `rg PATTERN directory -g 'omega*.inl'`; shell path wildcards are not expanded as on Unix.

## Completion criteria

A successful fix must show a genuinely damageable visible eye, an actual native health decrease and qualified crossing, followed by the appropriate recovery/death path. It must not merely show a builder success, animation node, synthetic health receipt, or passing offline tests.

This document supersedes the latest-status paragraphs in `MISSION-SCOT-OSIRIS-FIRST-DAMAGE-20260905.md`, which contain older claims such as awaiting the first 5CEC908E run or the user having missed the shot.
