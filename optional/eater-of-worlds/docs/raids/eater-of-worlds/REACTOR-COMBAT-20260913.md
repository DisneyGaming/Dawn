# Reactor combat and natural route to Argos

13 September 2026. Current scope ends at arrival in the barrier arena. It does
not start or complete the barrier or Argos fights. The regular Release candidate
was installed at 19:44 local time after all five required build/test jobs passed.

## Requested solo behavior

- Load all twelve authored crossing enemy sources when the reactor activates its
  darkness zone. Request all fifteen categories represented by those sources and
  preserve their ownership across the four platform paths and checkpoints.
- At the final platform area, request six Loyalist infantry: the left and right
  primary fodder sources each supply two actors, and the two secondary fodder
  sources each supply one. This replaces the earlier three-wave proposal.
- Require six distinct native deaths. Then release respawn restriction, remove
  the reactor exit grate, request the authored chest and effect, and publish
  **Delve deeper** / **Venture deeper into the Leviathan.**
- Traverse the full underbelly route. Sequence its airlock, then request the
  ejection control after the exterior passage. The final underbelly-to-Argos door
  is separate from the reactor exit grate. Reach the actual arena volume to end
  this playable slice; loading the shared belly area is insufficient.

The twelve crossing sources are slots 23, 24, 25, 27, 28, 29, 30, 31, 32, 34,
35 and 36 in registry `686321C8`. Fifteen is the selected category population,
not a recovered original host-script total. Spawn-region point counts are not
enemy counts. Native authored templates, spawn regions, AI and damage remain
responsible for the actual actors.

## Spawn placement correction

The previous live log admitted four holdout actors from sources 4 and 10, then
reported readiness without death or a phase transition. The user observed them
disappearing. Their package definitions contain absent spawn-rule references,
and the previous sparse authority preserved that absence.

The new Eater-only binding supplies the matching named native placement rules:

- Primary/secondary left holdout, sources 4 and 5: rule 291.
- Primary/secondary right holdout, sources 10 and 11: rule 301.
- Island sources 23, 24 and 25: rules 295, 297 and 299.
- Dash source pairs 27/28, 29/30 and 31/32: rules 321, 323 and 325.
- Cluster sources 34, 35 and 36: rules 184, 186 and 188.

These joins reconstruct solo orchestration from matching authored families.
They are not a recovered original server script. Left and right holdout rules
contain five and four placement GUIDs respectively; the chosen category requests
still total six infantry. No reactor dropship scene or transport cast was found
in the recovered descriptors. This implementation uses authored infantry spawn
regions; it does not claim a recovered ship animation or passenger manifest.

The missing rule is a concrete source defect and a suspected cause of the visible
disappearance. Only the next live run can establish that the disappearance is
resolved. Offline serialization and controller checks cannot establish it.

## Combat, retry and scope

Readiness requires the actual actor, health, AI and a selected reachable tactical
task. A cost-evaluation row of minus one is not a combat assignment. Death must
match a current admitted actor and source generation. Duplicate deaths, removal,
unrelated actors and elapsed time cannot advance the objective.

Holdout death or player replacement cancels that attempt, retires the old holdout
sources, and retains all completed platform paths and the last checkpoint.
Rearming uses a fresh per-source generation after native cleanup acknowledgement.
Old births and deaths cannot satisfy the new attempt. Source cleanup is separate
from a kill; clearing a ledger does not prove that native actors were removed.

The existing `4EC1A0` retirement hook captures owned actors before forwarding the
native call once. A pending cleanup requires exact incoming/applied generations,
an empty owned list, cleared native scheduler/request queues, and the removal or
replacement of every captured entity. An actor awaiting entity attachment keeps
its native weak lease until that lease is released. Unreadable metadata does not
prove release; changing an entity's resource bundle does not prove removal.
Two consecutive qualified world observations produce the acknowledgement. The
poll runs only while cleanup is pending and is limited to 20 Hz.

The player-health reader retains an authenticated health lease and uses bounded,
throttled component discovery if the normal interface lookup fails. Unknown
health continues to permit authenticated solo platform contact. Only confirmed
death of the same current player vetoes progression. Health is never overwritten.

Traversal uses the recovered volume/device joins in
[TRAVERSAL-WIRING-20260913.md](TRAVERSAL-WIRING-20260913.md). The ejection device's
identity as the user's final physical door is inferred from its name and the
other recovered controls; opening it visually still needs confirmation. Piston
timing/damage and hoop collection/reward behavior remain unrecovered. Creating
their objects does not establish those mechanics.

## Acceptance after installation

Play normally from entrance through all platforms, fight the final six, then
follow **Delve deeper** through the underbelly into the arena. Check that path
enemies remain present, move, attack and take damage; the sixth final kill opens
the exit; and the final underbelly door allows natural passage. Try one holdout
death/retry. Observe FPS while advancing checkpoints. Do not use forced checks,
teleports or debugger completion for acceptance.

Focused tests cover this sequence using synthetic native receipts, including
five-versus-six kills, stale-generation rejection, retirement-gated retry,
natural passage observations and stopping before the barrier fight. Native
gameplay, physical door motion and FPS remain live acceptance items.

## Installed candidate

- Candidate: `build/coo/eater-reactor-combat-20260913`.
- Release DLL SHA-256: `f2c4c5ddf0ee6809370c3a171d4d1c3a2abab2f9c4413ab9989e6e3f6cb236af`.
- Package SHA-256: `86a7574bc076441d20e1ef53d30b0c02c4ad8348b9070f51b7126ac8e4e2a5a5`.
- Installation receipt: `build/coo/eater-reactor-combat-20260913/installation.json`.
- Installer backup: `.dawn/backups/lua-20260913-194402-ee3eebb7`.
- Previous coherent runtime package remains in
  `build/coo/eater-platform-performance-20260913/payload`.

Validation passed 6,309 Eater checks (including 29 retirement policy checks),
184 native platform checks, 12,282 roster checks, 86 player-position checks,
the other-mission protocol suite, and the Release DLL build. The installer
verified all sixteen copied files. Destiny was closed with the user's explicit
authorization and was not relaunched. No new in-game playthrough is claimed.
