# Eater entrance entity-ID startup fix

13 September 2026, build 86657. This note records the narrowly scoped implementation that
addresses the entity-ID starvation measured in [ENTRANCE-BIRD-DIAGNOSIS.md](ENTRANCE-BIRD-DIAGNOSIS.md).
It describes source and offline validation. The backed-up entrance candidate is now installed;
[IMPLEMENTATION.md](IMPLEMENTATION.md) records its exact build, validation and installation
receipt. No gameplay acceptance is claimed.

## Implemented scope

The policy applies only while Sunrise has an active `queued` or `preparing` launch for the exact
recovered entrance route:

- package `raid_envy_v310`
- bubble 2
- slice 16
- spawn `0x8BA80878`
- a joined activity session whose committed destination still matches that route
- the current simulation-domain manager returned by the checked native root getter
- the native 100/200 low/high profile measured on the failing local-client path

For that lifetime, the maintenance callback temporarily replaces the adjacent 100/200 pair with
the build's existing 300/500 profile. Native maintenance therefore targets its 400 midpoint. At
the captured 150 available IDs this asks for the exact 250-ID deficit. The pair is restored as the
native call returns. Restoration uses compare-exchange and never overwrites a concurrent native
lifecycle change.

The maintenance and grant-merge entry points are exact-build signature checked. The maintenance
call still sends native type 20, the Sunrise server still selects actual free IDs through
`prepare_grant`, and the native merge still places the resulting owned lease into the client
mask. The fix does not manufacture IDs, set allocator bits directly, change entity-record
capacity, change role selection, or bypass the reserve/session/revision checks.

The launch snapshot stays pending during `queued` and `preparing`. It becomes `arrived` only after
native bootflow step 38 reports `inMission`. The captured failure occurred earlier, during
`initial_slice_set_instantion`, so the exact launch scope remains eligible through the measured
failure boundary. Failure, cancellation, route replacement, session replacement, manager
replacement, or arrival removes eligibility.

Installation is independent from the rest of the bootflow hooks: an Eater-specific signature
mismatch emits a warning but does not disable unrelated mission bootflow behavior. Shutdown first
quiesces both callbacks, waits for active calls, removes both detours as one protected set, and
only then clears their trampolines.

## Diagnostics and offline validation

Receipts use `ev=eater_entity_ids`. They are bounded to 48 per load and record the selected route,
session, manager, native profile, before/source/after mask counts, requested deficit, and profile
restoration result. Unrelated routes never enter this receipt path.

`eater_entity_id_startup_tests` has 25 checks covering the measured 150-ID state, the 250-ID
deficit, nonlocal native profiles, stale and mismatched route/session/manager states, atomic
profile replacement, normal and exceptional restoration, preservation of concurrent native
changes, real free-ID selection, server-reserve separation, commit, replay rejection, and stale
revision rejection. The Release test passes. The full Release Sunrise build also passes with zero
warnings and zero errors.

## Remaining runtime boundary

The 400 target is the smallest larger profile already used by this native build. It exceeds the
163 object attempts visible around the failed capture, but the failed run could not reveal the
complete successful entrance population. A user-operated retry must still establish that IDs
remain available for the whole burst, prerequisite 35 completes, and the launch reaches
`activity:in_world` with working controls.

This is an entrance fix, not a general Eater allocation claim. The newly published complete roster
contains 688 client descriptors for bubble 6: 23 top-level descriptors plus 665 Argos/barrier
descriptors. The earlier successful bubble-6 capture did not instantiate that newly completed
roster. A 400-ID target does not cover even one ID per published descriptor if they instantiate in
one uninterrupted burst, and the entrance evidence shows that native object demand can exceed the
descriptor count. Bubble 7 publishes 221 client descriptors, but its demand after the completed
roster also has not been measured.

Do not extend the temporary profile to every Eater slice from descriptor counts alone. A bounded
per-transition extension needs either a successful-demand trace or a proved native pending-object
count and the maintenance/grant-merge boundary for that transition. It must retain the same exact
route, session, current-manager, free-ID transaction, reserve, and compare-exchange restoration
rules used here.

## Source map

- `Sunrise/src/client/hooks/bootflow/eater_entity_id_startup_policy.h`: pure route/profile policy
  and atomic temporary-profile guard.
- `Sunrise/src/client/hooks/bootflow/eater_entity_id_startup.cpp`: exact-build qualification,
  detours, diagnostics, and safe lifecycle.
- `Sunrise/src/client/hooks/bootflow/internal.h`: lifecycle declarations.
- `Sunrise/src/client/hooks/bootflow/bootflow_hook_lifecycle.cpp`: installation, quiescence, and
  protected teardown integration.
- `Sunrise/unit/eater_entity_id_startup_tests.cpp`: policy and real lease-transaction regressions.
- `Sunrise/unit/eater_entity_id_startup_tests.vcxproj`: isolated Release test target.
