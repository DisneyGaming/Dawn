# Eater of Worlds implementation checkpoint

13 September 2026. Build 86657. Sunrise implementation `eater_of_worlds`.

Current follow-up: [arrival, door and density changes](ARRIVAL-DOORS-DENSITY-20260913.md).
The new source stages the debris/shield intro, 30 crossing actors, nine final
Loyalists, applied door receipts, and a hoop-plus-arrival-trigger barrier handoff.
The barrier fight remains pending. The following earlier records are historical.
The user completed all four platform paths in the installed performance candidate.
This follow-up adds every crossing source at reactor activation, a six-Loyalist
holdout, per-source combat retry, and the full underbelly route ending at the
barrier arena. Its native combat and final physical door still require live
acceptance. Earlier failure and installation records below are historical.

The user's first entrance test of the installed candidate crashed during loading.
The native stack identifies a lifetime scenario-selector mismatch: the packet named
local area 0 while entrance region 16 requires local area 2. See
[LOAD-CRASH-20260913.md](LOAD-CRASH-20260913.md) for preserved evidence, the scoped
correction, and the required live recheck. The installation/testing record below
describes the original candidate and is retained as historical evidence.

**The complete solo raid is not implemented or gameplay accepted.** The current source
builds and contains the launch, recovered asset bindings, entrance ID mitigation and
mission infrastructure. It does not yet provide an end-to-end playable raid. In particular,
the barrier and Argos cycles remain incomplete. The installed reactor test exposed
a contact-source mismatch and an independent player-health reference bug; see
[the activation failure record](PLATFORM-ACTIVATION-FAILURE-20260913.md).
The GUI uses the basic Eater of Worlds — Solo raid description.

After a temporary alive-gate test successfully raised the next platform, the user
requested that authenticated platform contact suffice for solo progression. The
current controller permits unknown health and vetoes only confirmed death of the
same current player, including at goals and the holdout transition. See
[CONTACT-ALIVE-POLICY-20260913.md](CONTACT-ALIVE-POLICY-20260913.md). The native health
lookup's live failure remains unresolved; it is no longer a positive requirement
for platform progression.

The current platform follow-up adds both missing reactor-entry door commands,
fixes the held/current area tracking that reverted reactor 56 to entrance 16,
implements ordered dwell/retention for all 56 platforms, and binds native movement
acknowledgements, player-health death/retry and recovered checkpoint spawn sets.
The shipped Lua reaches holdout in the complete synthetic receipt regression.
The corrected occupancy producer resolves each expected platform's exact
`80F42FB5` cylinder component and matches its body against the real player's native
contact manifold. Standing, edge and jump/leave/return captures exposed the earlier
primary-body mistake. This is authored-volume occupancy with a separate solo dwell,
not grounded-support detection. The health reader also resolves native group-plus-
offset references. Activation with the corrected Release still requires a live test.
See [PLATFORM-ACTIVATION-FAILURE-20260913.md](PLATFORM-ACTIVATION-FAILURE-20260913.md).

## Implemented source

- `Sunrise/scripts/eater_of_worlds.lua` ships five active phases: arrival, reactor,
  Loyalist holdout, traversal, and the pending barrier execution boundary. Argos and
  ending definitions remain inactive for later implementation. Its bounded solo policy
  parameters compile through the existing Lua loader. The parameter declarations do not
  establish that the corresponding platform or shield policy is implemented.
- `Sunrise/src/state/activity/eater_of_worlds/` contains the controller, runtime,
  generated catalog and capability profile, native authority serialization and scene casts.
  Existing services handle object preparation, enemy admission and readiness, native
  clocks, dialogue submission, objective publication and completion ownership.
- The GUI has a **Raids** tab and **Eater of Worlds — Solo** entry. It selects the actual
  Eater activity, ordinal 536 / `B8218A8C`, package `raid_envy_v310`, entrance bubble 2 /
  slice 16 / spawn `8BA80878`. The direct activity selection does not borrow another
  mission's activity definition. Ordinary strike modifiers are excluded.
- Supplemental roster publication preserves the exact group, type and slot identities
  of all recovered client descriptors, including holes occupied by host-only declarations.
  The native private-area selection used to avoid waiting for another activity host is
  scoped to the exact direct Eater selection and all eight of its recovered slices.
- `Sunrise/src/client/hooks/bootflow/eater_entity_id_startup.cpp` addresses the measured
  entrance ID starvation through the native owned-ID lease machinery. For the exact
  pending and committed entrance route and current simulation manager, it temporarily
  uses an existing native cache profile with a 400-ID midpoint instead of 150. It restores
  the original profile after each maintenance call. It does not manufacture IDs or change
  the fixed native object-record limit. This is a source implementation awaiting live
  validation, not a demonstrated BIRD fix.
- Existing guarded callbacks now deliver Eater object creation, squad admission/death,
  enemy readiness, task costs, player position, triggers, monitors, scenes and dialogue
  submission. Exact identities and generation checks reject unrelated or stale callbacks.

## Reproducible native links

The original [native inventory](evidence/native-inventory.json) remains the identity source.
These additional exports join it to the installed packages:

- [Runtime bindings](evidence/runtime-bindings.json): 19 groups, 891 client descriptors,
  75 squad sources, 73 volume definitions and 32 monitor/trigger-to-volume links.
  `tools/coo/extract_eater_bindings.py --check` re-extracts and checks the JSON, generated
  catalog, capability profile and each overlapping cached descriptor.
- [Reactor platform bindings](evidence/reactor-platform-bindings.json): the 56 exact
  platform placements, their authored path/index order, paired type-34 declarations and
  generic device component. `tools/coo/verify_eater_reactor_platform_bindings.py --check`
  checks package and pinned executable bytes. The generated `mechanisms.h` records the
  exact output binding. A positive device-position value satisfies the three recovered
  platform behavior gates. It does not establish standing on the platform, retention,
  sinking, motion extent or completion.
- [Channel target bindings](evidence/channel-target-bindings.json): 142 type-24
  source-to-object edges, including 70 barrier and 72 Argos channels, plus exact target
  entity/resource metadata. `docs/raids/eater-of-worlds/tools/extract_eater_channels.py
  --check` verifies them. Channel row counts and value meanings are deliberately null
  because the target edge alone does not prove either.
- [Scene bindings](evidence/scene-bindings.json): the two type-43 descriptors, exact
  selectors, native graph references and ordered casts. The alpha-strike scene needs its
  host-only type-48 target before the boss squad; the intro scene casts only the boss.
  `tools/coo/extract_eater_scenes.py` reproduces the evidence.
- [Health bindings](evidence/health-bindings.json): 201 exact type-4 source/entity/config
  joins for native `80804B8A` health components on mines, missiles and craniums.
  `tools/coo/extract_eater_health_bindings.py --check` verifies the package records and
  generated header. The existing native source callback now authenticates the exact
  health config/offset, source generation, salted object and resolved health handle.
  Registered `.destroyed` observations require a sampled live-to-dead transition of that
  same object. A missile death cannot satisfy a mine observation. This does not map
  Argos's six weakpoints or implement the charging/barrier cycle.
- [Carry bindings](evidence/carry-bindings.json): 120 exact cranium source/entity/config
  joins for the native Carry component. `tools/coo/extract_eater_carry_bindings.py --check`
  verifies those package records and the generated header. `.carried` and `.dropped`
  observations authenticate the real native holder and require ordered transitions on
  the same current source, entity and component. An initial ground state is not a drop;
  destruction clears held state without creating a drop. This does not establish a
  cooking duration, elemental charge, ammunition or target damage.
- [Station bindings](evidence/station-bindings.json): 18 exact fire-interaction sources,
  their `80804FB2` components and their authored fire/channel siblings.
  `tools/coo/extract_eater_station_bindings.py --check` verifies the joins. All nine
  entity/config pairs also expose the same native `80809AE3` interaction authority
  interface used by Omega. Requested stations receive its enable command only after
  creation is acknowledged; inactive, unacknowledged and unrelated objects retain
  their canonical source records. A `.used`
  observation requires a real requester, the exact cranium held before native use,
  and an original-backed consumed-request edge. The station event preserves native
  carry state. Prompt visibility alone cannot satisfy it, and it does not mark a cranium
  charged or write an unproved fire-channel value.

If the runtime catalog changes, regeneration order is: base extractor with
`--catalog-only`, channel/health/carry/station extractors, then base extractor with
`--cached` to update the capability profile. Run all applicable `--check` commands afterward.

## Known execution gaps

1. **Entrance acceptance:** the ID mitigation and door movement need a real successful
   load and player-confirmed traversal. Raising the native startup target does not prove
   the entrance's final demand fits it. The much larger belly roster also needs its own
   instantiation-demand check; the entrance mitigation does not cover that case.
2. **Reactor occupancy and retention:** all 56 platforms have ordered activation,
   physical pose acknowledgement, solo retention and checkpoint reset logic. The installed
   first-platform test fails because its primary physics body has no qualifying contact
   points. The source now binds the exact authored child cylinder using captured
   occupancy evidence; the corrected Release still requires a live activation test.
   See the activation failure record above. Time or a goal-volume crossing alone
   cannot advance it.
3. **Combat and traversal acceptance:** the current six-infantry holdout and all crossing
   sources use native admission, readiness and death receipts. Counts and spawn-rule
   joins are explicit solo policy requiring playtesting. Airlock and ejection commands
   are sequenced through natural passage; final physical door motion, piston hazards
   and hoop reward behavior are not yet accepted. See the current combat record.
4. **Craniums and barrier:** charging, dropped-cranium lifetime, elemental target selection,
   native channel semantics and the complete target cycle are unimplemented. Exact mine
   destruction, native pickup/drop and completed station-use observations are implemented
   but have not been observed in Eater gameplay.
   `barrier.cycle.finished` consequently has no producer. It never auto-completes.
5. **Argos:** exact boss source and scene identities are bound. Shield convergence,
   immunity/damage windows, detainment, weakpoint interruption and repeat cycles remain
   unimplemented. A genuine admitted boss death is required for the currently defined
   victory observation; merely detaching the actor cannot satisfy it.
6. **Retry/checkpoint behavior:** reactor checkpoints and holdout generation-based
   retirement/rearm are implemented. The fallback health reader and native holdout
   cleanup still require live death/retry acceptance. Later boss encounter retry and
   general retired-object recreation remain incomplete.
7. **Ending:** final dialogue submission and its native duration gate completion. Loot,
   complete scene teardown, return flow and a full raid run have not been accepted.

The six previously recovered Eater behavior roots are client-side behavior, not the missing
host orchestration. The decomp share includes `RE/54`; its referenced `RE/29` and `RE/66`
are absent from the supplied archive and the searched workspace. A request for their
location is pending. The game was not running during this implementation pass, so there
are no new live observations to close these gaps.

[Native control gaps](NATIVE-CONTROL-GAPS.md) records the independently reviewed physics,
type-34 collection and ID-allocation boundaries. In particular, native ID grants arrive
asynchronously. Calling maintenance after the allocator has already run dry cannot save
that same allocation. The proposed larger entrance cache is a prefill mitigation; it is
not a synchronous refill or a proved solution for every later group burst.

## Validation boundary

Focused tests exercise the shipped Lua graph, policy bounds, exact roster identities and
native serializer widths; they also reject stale or multi-player monitor inputs, unadmitted
deaths, unrequested final dialogue and reused object identities. A long elapsed-time
test confirms that the unresolved reactor gate remains pending. That test passing is
evidence of the guard, not evidence that the reactor encounter works.

A separate health fixture runs real compiled `.on`/`.destroyed` capabilities with
authenticated synthetic input records for the mine and missile adapters. It verifies
source identity, current region, alive-before-dead, deduplication, reset and isolation of
the required mine gate from a different destroyed object. Those records are supplied by
the test harness; they are not a captured Eater playthrough.

A separate carry fixture checks exact-holder pickup/drop, wrong-object isolation, invalid
component and entity identities, cross-area rejection, duplicate events, reset, and
destruction while held. The station fixture checks consumed-request counters, actual
holder identity, a unique held cranium, exact station ownership, request deduplication,
and preservation of carry state until the independent native drop. These are synthetic
native input records, not game interaction or a solo raid completion.

The station authority test executes the pinned original `F32820`, `9FA4B0`, `9F9B30`
and `F33930` instructions in a separate test process. Across all 18 stations it checks
active/inactive and acknowledged/unacknowledged states, exact wire width, prompt
unlocking, unchanged requester/use state and idempotent retransmission. All 120 cranium
sources remain outside that enable path. Debug and Release each passed 8,814 native
interaction checks. These tests never call into a running game.

The controller fixture's Debug stack overflow was caused by retaining the shipped-route
snapshots on the main stack while invoking the separate health/carry/station fixtures.
Giving those fixtures separate stack lifetimes resolved it without changing production
code or the process stack limit. Debug and Release each passed all 3,465 Eater checks.

Six regression fixture failures were reproduced at baseline commit
`02fc2c30fa728ab500db633d9e063aabea057631`. Their fixes derive roster limits from actual
capacity, set test-local numeric mission parameters independently of saved playtest
settings, and retain both an explicit rejection of a 20-group archive packet and the
valid 15-group archive compatibility case. Production mission settings were unchanged.
The Eater packet fixture separately covers its actual 17-group non-archive roster,
full outer packets in slices 16/56/48, registration-only publication and insufficient
output storage.

Final validation and installation are complete for this **entrance test candidate**:

- [Validation results](../../../build/coo/eater-final-validation-20260913/results.json):
  119 successful Debug/Release build/test jobs, zero failures, zero compiler warnings.
  The run checked a stable source manifest and both DLLs. Fourteen older capture-dependent
  proof suites remain explicitly unavailable; they are not counted as executed tests.
- Release DLL SHA-256:
  `2a89065b98fa905b93fd7900fe7fb53cf0a23b59f9d7d8e782ded073631fbcdb`.
- [Package](../../../build/coo/eater-final-validation-20260913/lua-missions.zip) and
  [package manifest](../../../build/coo/eater-final-validation-20260913/package.json)
  retain the binaries, scripts, source archive, exact validation job identities and hashes.
- [Installation receipt](../../../build/coo/eater-final-validation-20260913/installation.json)
  records verified installation with Destiny closed. Previous files are backed up at
  `C:\Destiny 2 Development\.sunrise\backups\lua-20260913-113012-c73d31b1`.
- The production Raids screen was rendered offscreen at wide and narrow sizes and visually
  inspected. The actual Raids tab and Eater launch row pass with saved Grandmaster settings;
  Eater still selects standard activity 536. This was an isolated UI test, not desktop control.
- The initial full pass had 119 successful jobs but correctly failed its source-stability
  audit because the final source-offset check and Raids visual test changed during it.
  Its records are preserved in `pre-final-review-validation.zip`; a complete subsequent
  119-job pass succeeded against the final source before packaging.

No game launch, game input, desktop takeover or Eater gameplay acceptance was performed.
Scripts load on the next game process. The installed candidate still stops at the unresolved
reactor gate; passing the checks above does not implement the missing raid mechanics.

## Continuing the implementation

The first user-operated test is the entrance:

1. Start the installed candidate and open the mission launcher from orbit.
2. Select **Raids**, then **Eater of Worlds — Solo**. The visible in-progress notice
   is intentional; this candidate cannot complete the raid.
3. Confirm that the original outer entrance loads, controls work, and approaching the
   entrance opens the door. If BIRD returns, retain the current log and do not classify
   the cache mitigation as accepted.
4. If the entrance works, stop acceptance at the first reactor path. Its completion
   currently has no producer. Remaining in that phase is a known implementation gap.

Log review should correlate the current mission run with `ev=eater_entity_ids` request
and grant-merge records, profile restoration, prerequisite 35 and arrival. Eater mission
records use `ev=eater_of_worlds`; an unresolved path reports `stage=mechanic_wait`.
The user retains all game and desktop controls. No operator automation is required.

Start at the unresolved entrance/reactor boundary in [PLAN.md](PLAN.md). Preserve the
accepted native identities and the independent health, device-application, scene and
dialogue receipts learned from earlier boss implementations. Close an actual control
or observation edge before changing a pending mechanic into a success condition. Capture
the current build/run and exact object ownership when testing it. Retain real failure and
clean retry behavior when adapting simultaneous player requirements for solo play.
