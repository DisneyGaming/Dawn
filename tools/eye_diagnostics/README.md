# Panoptes eye troubleshooting — discovery v1

This package adds six native observation hooks and a repeatable build/evidence process. **It does not fix eye immunity and is not yet ready for a conclusive replay.** `contract.json` records the three unresolved native mappings. The current install remains separate from the built candidate.

## What has been implemented

The native hooks live in `Dawn/src/client/hooks/bootflow/omega_eye_execution_trace.inl`. They observe original calls and preserve their arguments, output parameters and return values. They make no health writes, emit no progression receipts and invoke no extra damage or script execution calls. Logging adds bounded work; offline tests do not establish its live timing cost.

- `58E260`: damage-ping/refill runtime execution entry and return.
- `5873F0`: native action index and action-runner result.
- `1212AF0`: native behavior creation result, correlated with the action that called it. Creation success does not prove an eventual command effect.
- `B804E0`: incoming-damage pipeline entry/return with raw packet flags.
- `CDCB60`: native Boolean gate counts/results within that incoming call. Original instructions return the inverse of health byte `+338` bit 1. This is one gate, not a complete immunity verdict.
- `B7E3C0`: native damage-summary arguments correlated with the incoming call. The exit's `native_amount` is the last summary amount when `summaries>0`; use individual summary events for multiple calls.

All probes require the active mission token, post-dunk cycle, full current boss owner and matching typed component. Script resources are restricted to `80F4547D/808084E9/928` and `80F45563/808084E9/488`. Health is independently resolved from the typed boss character. Calls outside the scope still execute normally. Nested foreign calls clear the thread-local observation scope. Before/after validation reports `current=0` if native execution changes ownership or component identity; such returns cannot qualify as current evidence.

Each token permits 24 script tick scopes, 64 action scopes and 64 damage scopes. The two script resources share these budgets. Absence after budget exhaustion is inconclusive. A `trace` serial is process-local; pair it with `run` and `epoch`, never across logs. Native `t` timestamps provide the shared log timeline. The tool does not manufacture a video/player timestamp.

Atomic install, rollback, original publication and uninstall now include all 21 reveal hooks. The unchanged native gate runs in private fixture memory for all 256 possible flag bytes. Other new native paths use ABI forwarding fixtures plus pinned instructions and hook relocation tests; the full live damage pipeline is **not** emulated by these tests.

## Evidence and unresolved work

The original native image is pinned by SHA256 in `contract.json`. Local evidence under `build/omega-full-20260905` includes:

- `eye-script-execution.txt`: execution entry ends at `58E3E7`; bytes beyond that are separate code/data.
- `eye-action-call.txt`, `eye-script-command-handlers.txt`: action dispatch and the call from `11E63F1` to `1212AF0`.
- `eye-behavior-creation-full.txt`: creation continues through `12065D0`; this does not resolve compiled operation `3C`.
- `eye-incoming-damage.txt`, `eye-damage-entry.txt`, `eye-damage-prologue.txt`: callers and split/protected entry for `B804E0`.
- `eye-feedback-callers.txt`, `eye-feedback-decision.txt`: native feedback construction leads, not an established “Immune” reason decoder.

Function ranges in `pdata` may be unwind fragments. Raw relative-call searches can match data and require instruction-boundary validation. Never hook a fragment as though it were a complete function.

The remaining identification work is:

1. Follow the created behavior into actual compiled damage-ping command execution; establish the exact owner and region representation for `3C/720A5B5B`. Action-runner and creation results alone are insufficient.
2. Establish the native value or command that changes visible eye closure. Correlate it with animation playback. Glow, illumination and a node named “eye loop” do not prove eye aperture.
3. Map the complete incoming-hit rejection result and selected region. The flag gate and zero damage summary are insufficient to label a hit “Immune.”

For each mapping, save the complete function/caller proof, typed owner layout and native branch semantics; add a narrow observational hook and tests before changing readiness. Do not change `replay_ready` merely to permit installation.

## Build and verification

Run from `C:/Destiny 2 Development`:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/eye_diagnostics/build.ps1
```

This checks the pinned original image, tests the collector, freezes source, builds Release, runs all 18 Debug/Release native regression runs, verifies the working source still matches the frozen source, and records the actual diagnostic scope. It archives this troubleshooting package and its hashes beside the candidate. It does not launch, close or install into the game.

`-FinalizeOnly` finalizes an already fully verified candidate selected by `build/omega-full-20260905/candidate-path.txt`; it rejects a changed DLL or working source. Do not use the old rescue/Arc finalizer for this scope because it writes stale validation text.

While native mappings remain incomplete, the manifest status is `verified discovery candidate; native mapping incomplete`. The existing installer rejects that status. This is a technical readiness hold, not a request for more user permission.

When the mappings, new probes and full tests are complete, finalize with an accurate contract. Recheck that Destiny 2 is closed, then use the existing `build/omega-full-20260905/install_candidate.ps1`. Its prior-DLL/settings checks, backup and post-copy hash verification remain required. Start the game manually only after that qualified installation.

## Capture process after readiness

1. Confirm the log's build identity and `revision=26`/the eventual qualified revision. Advance manually through rescue, Arc dunk and eye arrival.
2. Capture one opening and several deliberate shots during the visible opening. Record the visible opening, each shot/“Immune” result and closure in the same recording when available. Do not infer their timing from graph labels.
3. After the run, collect the explicit log against the explicit tested candidate. Use a new output directory each time:

```powershell
python tools/eye_diagnostics/collect.py `
  --candidate build/omega-full-20260905/candidate-YYYYMMDD-HHMMSS `
  --log Dawn/logs/dawn.log `
  --output build/omega-full-20260905/eye-capture-RUNNAME
```

The collector is read-only toward the game. It saves the exact log bytes and hashes, candidate manifest, diagnostic contract, ordered `timeline.jsonl` and `capture-status.json`. It uses no historical PID, base address or cached handles. Additional memory snapshots require freshly discovered process/base/ownership through the existing read-only helpers.

Exit 0 means the captured log has matching build identity and structurally valid trace pairs. It does **not** mean enough events occurred, the diagnosis is complete or gameplay is fixed. Exit 3 archives the evidence but reports stale/mixed build identity, wrong diagnostic revision or incomplete trace pairs. Exit 2 means collection itself failed. A final partial line is preserved in the raw log but excluded from observations and reported as incomplete.

Review `observed`, missing mappings, trace budgets and individual events. Correlate, within the same run/token, the accepted frame-zero dispatch, damage-ping execution, actual compiled command, selected hit region/rejection, visible closure control and original regional health samples. Check `current=1` before using any return observation. Pair viewer observations using a measured recording/log anchor; do not guess an offset.

## Choosing and verifying the eventual fix

Select the smallest gameplay change supported by that causal evidence. Rebuild and repeat one controlled run. Acceptance requires a visibly open, damageable eye, a real native regional health decrease and qualified downward crossing, followed by the expected recovery/death path. Passing fixtures, a successful builder, a zero/nonzero summary and an animation node do not satisfy those criteria.

For a second version, retain the collector/build workflow and immutable evidence bundles. Requalify every version-dependent resource, RVA, ABI, signature and owner layout; extend the fixtures and contract. Never carry the native image pin or an assumed immunity/closure meaning to another version without proof.
