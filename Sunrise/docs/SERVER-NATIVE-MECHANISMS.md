# Server-owned mission mechanisms

This document records the first migration. [The second-pass report](SERVER-NATIVE-HOOKS-PASS2.md) supersedes its plate-pose, interaction-enable and Panoptes arm-delivery status. The user completed Omega end to end on the first installed build; the second build still needs a fresh playthrough.

## Migrated behavior

Beyond Infinity, Deep Storage and Hijacked now publish plate timer commands through each plate's ordinary type4 source authority. The source contains one existing `80804FCA` capture-controller record: 252 source bits plus 388 record bits. The server chooses the duration, activity-clock anchor, start/stop state and occupancy revision. Repeated publication keeps the same anchor. Departure or contest resets an incomplete attempt; confirmed completion retains the native timer. The existing reconstructed durations remain seven seconds for Beyond Infinity, five/ten/ten seconds for Deep Storage and five seconds for Hijacked. These are reconstruction estimates, not newly recovered retail constants.

The server also selects each plate's desired appearance. The client adapter receives the published 0/.1/.2 position; it does not choose a pose or start a timer. The old direct native timer apply and native clock/duration function calls have been removed from these adapters and their install dependencies.

Authenticated position, plate binding/progress, scan binding/raw playback and Beyond Infinity/Deep Storage lens binding/destruction are copied into a bounded observation queue. Only the server snapshot consumes the queue and changes those mission decisions. Intake preserves receipt order, validates the mission owner again and stops publication on overflow until a new owner is selected. No elapsed host time creates a completion observation.

Deep Storage and Hijacked contest samplers now return authenticated enemy positions. The server applies the authored plate geometry against the current living-enemy ledger. A missing, invalid or newly admitted actor cannot prove that the plate is clear. Scan observers return native mode, revision, active state, elapsed time, effective duration and participant evidence. The server decides start and completion from those values; native player input, Ghost binding and animation remain in the game.

Doors, barriers, portal frames and platforms already publish through the existing mission type23 authority encoders. Their authored state values and revisions remain in the mission controllers. This migration does not invent additional device bindings.

## Native evidence

The mapped image SHA256 is `63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e`. This change rechecked that hash and generated read-only disassembly under `build/unit/server-migration-devices/evidence/` with `manifest.json`. Earlier recovered evidence is in `build/coo/beyond-infinity-live-well/plate-scalar-research/findings.json` and the adjacent disassembly files.

- `9F19F0.txt`: source authority copies the decoded 0x170-byte state, including the dynamic list. The tail at `9F1CDE` unconditionally dispatches `9EF680`; dynamic state application is not gated on increasing the placement generation. `9EF680.txt` and `B31960.txt` show the native component-interface dispatch.
- `1003900.txt`: native entity authority remains checked at `100392A..1003932`; the accepted path calls `FEE1C0` with destination `component+30`. No authority check is bypassed.
- `FEE1C0.txt`: the matching typed record copies exactly 0x48 bytes, ending at `component+77`. It does not overwrite progress at `+1B8`, remaining output at `+1BC`, activation tracking at `+78` or completion latch at `+79`. It does not choose a new timestamp. Reapplying the same anchor therefore does not restart the timer.
- `1006F20.txt`: `1007125..1007144` evaluate the authoritative clock through a stack wrapper. `4C8E00.txt` retains a pointer to `component+38`. `4C8F80.txt`, `4C9120.txt`, `4C8E90.txt` and `4C8FF0.txt` show calculations writing to caller stack outputs while reading the authority tuple. The original tick writes progress/remaining at `10071A9`/`10071B6`, sets the completion latch at `10071FC`, and clears it at `1007269`. It does not mutate the requested authority tuple when completing.

The plate observer checks the exact published tuple before and after the original tick, together with source generation, committed generation, salted entity, timer/device owners and the exact authored timer definition. An old occupancy command cannot qualify a new attempt. Existing progress-before-completion guards remain enabled.

## Retained behavioral adapters and limits

The plate appearance setter (`DF6BD0`) is retained. These `pf_sync_plate.o_altar` profiles have a type4 object source and type3 AI objective, but no authenticated type23 device source for that altar. The generic-device dynamic state schema is not sufficiently recovered to replace the setter safely. The original timer graph can reset its pose, so the adapter must still reconcile current position, target position and native revision. Moving desired position selection to the server does not eliminate this behavioral adapter.

The authenticated damage/protection hooks are retained. The recovered `CDCB60` path controls the lethal-health branch, while `B804E0` performs synchronous damage side effects. No verified server authority payload currently replaces cube/lens immunity or a boss's per-hit phase floor. Removing these gates would expose protected objects or allow burst damage to bypass encounter phases. Server-owned vulnerability/phase choices remain authoritative; the hook enforces them at the native damage boundary. Binding/destruction receipt intake is distinct from that immediate protection.

Native source/controller discovery, position sampling, original timer completion and scan/interaction playback still require authenticated observation hooks. They are not replaced by fabricated occupancy, damage or completion events. Fresh in-game testing must confirm timing, visible plate state and interactions for the installed build. Offline regression and disassembly establish ownership and encoding behavior, not a new live-play result.

## Focused verification

Release builds use C++20, `/W4` and `/WX`, with isolated outputs under `build/unit/server-migration-devices/`.

- `deep/deep_storage_tests.exe`: full Lua route, interrupted/contested plates, missing enemy sample refusal, all-current-living coverage, server raw-scan classification, lens and ending guards, reset isolation: passed.
- `beyond/beyond_infinity_tests.exe`: full Lua route, runtime, plate/lens ownership and completion guards, authority widths and reset isolation: passed.
- `hijacked/hijacked_tests.exe`: normal and early-death routes, native ownership, charge, scan, movement and ending gates: passed.
- `capture/native_capture_authority_tests.exe`: 108 checks, zero failures. Includes parity with the previously verified native timer command, repeated-publication anchor stability, departure/reentry, stale state/revision refusal, completed hold, source-record width, ordered intake and overflow/reset behavior.

Deep Storage and Hijacked catalog tests now expect the additional 388 bits only on the authenticated plate sources. Other type4 objects remain 252 bits and scan links remain 65 bits.
