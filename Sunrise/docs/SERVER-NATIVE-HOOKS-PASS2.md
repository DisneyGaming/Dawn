# Native hook migration, second pass

For the latest active migration, see [the third-pass report](SERVER-NATIVE-HOOKS-PASS3.md).

This pass fixes the plate preparation regression and replaces verified client setters with native server authority. The user reported that the preceding installed candidate completed Omega end to end; that report applies to the preceding build, not this new candidate.

## Implemented

- Beyond Infinity, Deep Storage and Hijacked plate preparation now omits dynamic component records while the source is inactive. The captured Beyond source had desired=1, prepared=0, active=0 and no entity because its one-record timer body failed the readiness validator's empty-list requirement. The validator remains strict. All five plate encoders now pass the inactive handshake.
- Active plates publish both native timer80804FCA and generic-device pose80805063 records. The server owns desired pose, independent signed revisions and correction after authenticated native drift. Direct plateDF6BD0 setter calls are removed. Completed Deep Storage plates continue receiving normal authority updates.
- Omega's three archive arc-charge receptacles and A Deadly Trial's revival pedestal receive native80804FB8 interaction enable records. Direct enable_sink/enable_trial_pedestal calls and theirF33930 dependency are removed. Native player eligibility, actual input and successful-use observations remain.
- Correction: the arm changes in this pass targeted the unused legacy mission path. The installed active Omega owner still used its local arm setter. The third pass targets the compiled `omega_lair_cinematic.cpp` owner and active archive projection; pass-two tests do not prove that migration.

## Behavior that still requires hooks

This historical pass retained damage floors, boss/cube/lens protection, active Panoptes arms and other boss controls, selective model/VFX corrections, ending activity handoffs, placement and cleanup adapters. A later reachability audit found the listed experimental first-Forest portal adapter had no request caller; current portal movement uses the native host tuple protocol. The user subsequently authorized continuing all feasible native replacements.

The remaining hooks need contracts beyond a function address: synchronous damage handling; complete native program ownership and completion acknowledgement; selective model/effect lifetime; world-controller transition context; or table, authority-bitmap and allocator ownership. This pass does not call those hooks server native or move their engine calls onto a server worker.

Details and original-image evidence:

- [Plate pose and model/VFX findings](NATIVE-PLATE-POSE-MIGRATION.md).
- [Interactions, transport and cleanup findings](SERVER-NATIVE-INTERACTIONS-LIFECYCLE.md).
- [Panoptes arms and retained combat hooks](NATIVE-OMEGA-ARM-MIGRATION.md).

## Validation

The new plate regression decodes actual mission source bodies and passes them to the production inactive readiness validator. It tests all five plates, inactive startup/retirement, active timer-plus-pose delivery and stopped capture commands on departure. Full Beyond Infinity, Deep Storage and Hijacked route suites include stale run, generation, source, entity salt, timer and device rejection.

Native pose tests execute the pinned original producer/consumer in isolated emulation, checking repeated messages, stale/high signed revisions, snap independence and the native authority gate. Native interaction tests execute original setup, decoder and apply instructions, compare the decoded command with the previous setter command, and preserve pending/consumed use state. These tests do not replace a live playthrough.

Final focused results include 598 plate preparation/capture checks, 10,033 pose-service checks, 4,002 native interaction checks in each configuration, 1,049 boss-authority checks in each configuration, 19,656 Omega state checks in each configuration, and 10,024 ending/transport checks in each configuration. Beyond Infinity, Deep Storage, Hijacked and A Deadly Trial route suites also pass. The original arm proof passes 16 float-decoder cases and four complete control applications, including reset.

Combined build/test results, source hashes and installation receipt are recorded in build/coo/validation-native-hooks-pass2. The source manifest must match both before and after the Release build and again during installation. Previous installed DLLs/PDBs are backed up and both game load locations are hash-verified.

## Next playtests

1. Beyond Infinity: the plate appears, charges, resets on departure, completes and exposes the lens.
2. Deep Storage and Hijacked: normal/contested plate charging, retained completed pose, scans and mission completion.
3. Omega: initial left/right arms, chase islands, all three crown cycles, relic pickup/dunk prompts and ending.
4. A Deadly Trial: the revival pedestal prompt and completed hold.

## Installed candidate

Installed at 2026-09-12T16:47:41.6776057Z in both game load locations. Both installed DLL hashes match the verified Release build:

`e6af4de80d338776487d926c03506e1708c4b9c77d1a6b647f99e7f482b9d13d`

The prior version is preserved at `C:\Destiny 2 Development\.sunrise\backups\native-hooks-pass2-20260912-124740`. Settings and the original Steam DLL were hash-checked and preserved. The production input manifest covers 1,898 files. The arm packet and state checks exercised the unused legacy path; they were not evidence of active native arm delivery.

`build/coo/validation-native-hooks-pass2/validation-summary.json` contains the final 16 test/configuration results, original native proofs, retained-hook boundary and installation receipt. This candidate has not yet completed a live playthrough. Launch a fresh game process for the playtests above.
