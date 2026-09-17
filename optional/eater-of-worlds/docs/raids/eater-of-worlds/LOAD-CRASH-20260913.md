# Eater entrance load crash — 13 September 2026

## Observed failure

The first entrance test of DLL SHA-256
`2a89065b98fa905b93fd7900fe7fb53cf0a23b59f9d7d8e782ded073631fbcdb`
crashed before the player spawned. The selection was activity 536,
`raid_envy_v310`, scenario-local bubble 2, packed region/slice 16,
spawn set `8BA80878`.

Evidence is preserved under `build/coo/eater-load-crash-20260913`:
the current `Dawn/logs/dawn.log`, installed DLL hash, and the native crash
folder `crash_folder_20764_20260913_140101`. The older log under `bin/x64`
does not describe this attempt.

The log reaches successful slice instantiation at tick 104437, followed by a
10,123-byte full authority packet at tick 104453. The exception is a null read
at live address `00007FF707F5B1F1`, executable RVA `42B1F1` with image base
`00007FF707B30000`. The world remains in `activity:initial_slice_set_loading`.
This is a distinct failure from the earlier native object-ID allocation BIRD.
Passing that allocation stage once does not prove every ID-pool case is fixed.

## Cause in the startup packet

The recovered Eater global includes lifetime component
`24C67333 / type 17 / slot 3`, source `80FB52A8 + 718`, authority schema
`8080991A`. Its startup body falls through to the shared 520-bit lifetime
encoder. The lifetime's field `+C` defaults to scenario ordinal **0**.
The selected entrance is ordinal **2**, encoded elsewhere as packed region
**16**. These fields are not interchangeable.

The existing `lifetimeScenarioOrdinal` correction was admitted only for the
other mission global `4786C0E0 / 17 / 3`; Eater did not populate or consume it.
The inactive Eater gameplay frame during loading does not prevent publication
of this shared startup body.

The original native code explains why a wrong ordinal can crash even with no
active objective:

1. `ABC090` applies type-17 lifetime authority and dispatches the state change.
   The crash stack includes its return address `ABC1D1`.
2. The state-change route calls `10090A0` (stack return `BEAA1F`), which rebuilds
   type-68 directive presentation through `100A6E0`.
3. `4FFBB0` reads lifetime authority `+C`. If that ordinal differs from the
   current viewer's region and no other marker supplies a route, `100AD0E`
   calls `A262A0` to obtain an area-navigation position. The first crash-stack
   frame is exactly that return address, `100AD13`; another frame is
   `10091A1`, the return from `10090A0` calling `100A6E0`.
4. `4C8E60` maps the ordinal to `(ordinal & 63) << 3`. `438CA0` looks up that
   packed region's native record and returns null for an absent handle.
5. `42B1F1` reads the first word of that return value without a null check.

This identifies an incorrect scenario selector, rather than an unsupported
lifetime schema or evidence that lifetime publication must be removed.
Haunted Forest already uses the same native field with an explicit local
scenario ordinal.

## Correction and acceptance boundary

The source correction keeps the normal lifetime/startup body and populates
Eater's exact component with its admitted scenario ordinal, including packets
sent before the solo gameplay controller becomes active:

- `eater_of_worlds_roster.h` admits exactly packed regions 0, 8, ..., 56 and
  converts them to ordinals 0 through 7.
- `activity_roster_snapshot.cpp` sets the selector after exact Eater roster
  admission, from the transition-qualified membership region. A later region
  update therefore does not retain the original entrance's ordinal.
- `sensor_auth_update.h` carries a separate admission flag available before
  the Eater gameplay frame becomes enabled.
- `activity_sensor_auth_bodies_other_missions.cpp` consumes that selector only
  for selected `24C67333 / 17 / 3`. Missing or out-of-range Eater ordinals fail
  publication. The existing shared mission lifetime policy is preserved.

The regression test in `other_mission_protocol_tests.cpp` covers all eight
regions, malformed packed indices, missing/invalid ordinals, foreign keys and
slots, pre-arrival publication with a disabled gameplay frame, and transitions
through entrance/reactor/belly. It compares all 520 lifetime bits outside the
corrected 32-bit field, using the actual entrance spawn override, so lifecycle,
switches and spawning fields must remain unchanged.

`tools/coo/verify_eater_lifetime_native.py` validates the pinned executable and
preserved crash addresses and executes the original ordinal packer, native
lookup tail and faulting dereference. It reproduces the exact null read with
ordinal 0 and successfully copies a valid record with ordinal 2. The encrypted
table-pointer unwrap and runtime table contents are explicitly modeled; this
is a lookup-contract replay, not a live playthrough. Its independently rerun
output is in `build/coo/eater-lifetime-final-20260913/native-lifetime-replay.json`.

The full validation precheck detected a cache fingerprint change after the
user's game run. Fresh extraction differed only in `cacheSha256`; all recovered
content and descriptor matches were identical. Evidence was backed up to
`binding-evidence-before-cache-refresh.zip` in the incident directory, then
regenerated through the normal dependency chain. `cache-provenance-refresh.json`
records both cache hashes and confirms all generated source bytes stayed
identical. No content mismatch was bypassed.

Live acceptance still requires a fresh attempt that reaches
`activity:in_world` and produces a player after installation of the corrected
DLL. The full raid remains incomplete as recorded in [IMPLEMENTATION.md](IMPLEMENTATION.md).
