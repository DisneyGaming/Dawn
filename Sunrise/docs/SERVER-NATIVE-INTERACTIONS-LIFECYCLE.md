# Native mission interactions and retained lifecycle adapters

## Implemented interaction authority

The active Omega Arc-charge receptacles and A Deadly Trial revival pedestal now receive native `80804FB8` enable records inside their ordinary type-4 `8080992F` source authority. The server chooses source activation. Original native source creation and authority application deliver the interaction record to the entity. The client no longer calls `F33930` to enable these interactions.

The migrated Omega route is the archive authority path, which previously used `omega_arc_charge::write_authority` without an interaction record. The separate non-archive `omega::transit` path already emitted this record and remains unchanged. The three archive sink sources are `0040BF06/20`, `0040BF05/3`, and `0040BF03/2`; the Trial pedestal is `EB7AF01B/59` (all type 4).

Each active sink/pedestal body is 375 bits: the existing 252-bit source envelope plus a 123-bit present dynamic record. Inactive preparation, consumed/held Omega sources, and retired sources remain 252 bits with an empty list. The original source generation, deferred activation offsets, candidate index, authored transform, auxiliary integer -1, and native consumption behavior remain intact. Other object sources carry no interaction record. The common source writer supports capture, interaction, and generic-device records together within the native two-bit count; inactive sources omit all records.

`enable_sink`, `enable_trial_pedestal`, the direct `F33930` pointer/signature dependency, and the unused sink-address cache were removed from `omega_arc_charge_receipts.cpp`. Source binding, full identity checks, genuine carry/dunk/use observations, and the original `F32CD0` interaction callback remain. No input, dunk, or completion is synthesized.

## Exact native evidence

Read-only disassembly and a manifest are in `build/unit/server-migration-interactions/evidence/`. The mapped image SHA256 is `63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e`.

- `9EFFC0`: after creation, `9F0053` calls `9EF680`, so the dynamic record is delivered to the created entity. The existing source-authority evidence at `build/unit/server-migration-devices/evidence/9F19F0.txt` also shows repeat authority application dispatching `9EF680` without requiring a new placement generation.
- `9EF680`: resolves the source's weak entity, rejects removed entities, and retains the deferred generation match at `9EF74E..9EF757` before native component-interface dispatch. The migration bypasses none of these checks.
- `9FA4B0` and `9F9B30`: decode and select the dynamic schema. The reflected `80804FB8` fields are a biased two-bit mode, 55-bit scoped reference, biased 32-bit revision, and boolean. The shared native interaction writer already uses this layout.
- `F33930`: searches records for `80804FB8`; mode 2 clears `component+2C0` at `F3398D`. It copies the scoped override at `+2C4/+2C8/+2CA`, then marks the prompt cache dirty at `+280`. It never writes used/request/consumed state at `+2D0/+2D8/+2DC` or requester state at `+2E0`.
- The absent scoped override is `C59D1C81FF00FFFF`. It matches original `F32820` setup and the captured Trial controller in `build/coo/deadly-trial-pike-stall-20260907/sagira-native-interaction.json`. Authored player/item eligibility remains in the native definition and original interaction evaluation.

## Retained transport and ending handoffs

`sample_omega_portal_transport` remains on the world-frame owner. It waits for region-88 membership to apply, calls precheck `E2E720`, and invokes `E2E7E0(88, &4E5FD117, 0)`. It retires outgoing Omega publication before transition to avoid applying into a de-instantiating slice set. The exact wrapper disassembly shows `E2E7E0` obtains the native world controller through `E35170` and submits through `E1D400`; this is a local world-controller API, not a recovered server authority payload. `world_step.cpp` records the worker-context freeze that requires the current dispatch location.

The ending adapter still constructs and validates the native Mercury activity-29 descriptor, preserves the fireteam nonce, and calls `BF95D0`, `BFB1F0`, and `BF97D0` on the world thread. It also maintains the effective no-ship transition classification in the fireteam property and raises the native cleanup transition through `E2DEB0(28,309)`. The existing loading-cinematic suppression wrapper remains. No equivalent server message producer/consumer has been established for these selection, classification, and cleanup steps. Moving these calls to a server worker would retain the same adapter and violate the known thread constraint. No speculative replacement was applied.

## Retained placements, streaming unloads, and child retirement

The missing authored Hijacked placements still require the original `575690` constructor in the exact bubble/authority context, with the complete authenticated table row, GUID, and original table/record arguments. Type-1 source authority requests the population but does not instantiate those missing local table rows. A server payload that performs that table creation has not been established.

Source retirement already publishes a changed generation, zero population counts, and the recovered retirement policy. Fresh `4EC1A0` disassembly confirms native removal additionally requires the entity authority bitmap at `26BE0E0` (`4EC228..4EC23D`). It then calls `A93E40` at `4EC266` regardless of whether each actor passed that authority gate. Earlier recovered evidence in `build/coo/hijacked-retirement-research-20260910/owned-list-and-detachment-audit.json` shows that this call clears loose-actor ownership and compacts the source list. A generation change therefore cannot guarantee child removal; weakening identity or assigning authority would be an unproved replacement.

The area-unload adapter remains bounded to `424D10` and the boolean-only `A07CBE` return site for component `80807E3E`, where native code performs `56A8F0` itself. The recovered unload prepass only destroys matching table-owned entities; the dynamic surviving actors have table field `+88 == FFFFFFFF`. See `native-unload-discard-handoff.json` in the same research folder. No equivalent persistence or unload-selection field in source authority has been proved.

Native child cleanup still requires the allocator TLS service. The original `98F40` reads the TLS service and dispatches its free method at vtable+20; the recorded camera-thread failure dereferenced a missing service. Current retirement validates this service and runs at the native source boundary. These lifecycle hooks were retained unchanged for manual approval of a future verified replacement.

## Focused validation

`Sunrise/unit/native_mission_interaction_tests.cpp` executes original `F32820`, `9FA4B0`, `9F9B30`, and `F33930` instructions in its own process. Reflection scalar reads are modeled from the pinned descriptor; original source creation and live player input are not simulated. A full SHA256 check rejects any other image before execution.

Release `/W4 /WX` passed 4,002 checks, including SHA validation, all three archive sink cycles and lifecycle phases, Trial active/inactive pedestal, unrelated objects, exact decoded-command parity with the old native call, repeated publication, and pending/consumed use preservation. A deliberately wrong image was rejected before executing recovered code. Receipts are `native-results.json`, `native-test.log`, and `wrong-image-test.log` under `build/unit/server-migration-interactions/`.

Existing Release regressions also passed: A Deadly Trial 6,684 checks; Omega archive protocol 71,412 bodies across 12 packets; Omega archive encounter 939 checks. `regression-results.json` records binaries, arguments, and outputs. There is no `omega_first_mancannon_tests.vcxproj`; it was not counted as an executed test.

This establishes encoding and native-consumer behavior. The newly migrated prompt delivery still needs a fresh game run. The previously installed candidate's successful Omega playthrough does not validate these later producer changes. No game process writes or installation occurred in this scoped work.
