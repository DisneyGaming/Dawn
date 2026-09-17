# First-platform test after the load-crash fix

User-operated test, 13 September 2026. Loaded Release DLL SHA-256
`7e430774ee2685cb63b85a0b3086d0a23b70c03804adb1edde65457c87a024f2`.

The log reaches `activity:in_world` at tick 100344 in `raid_envy_v310`.
The previous startup null-read did not recur in this attempt. The controller
reports `reactor.path1.finished` pending at tick 127579, explicitly because
`native_platform_occupancy_and_completion` is not implemented.

The user confirms standing on the first platform with normal player movement.
They supplied two screenshots, initially deferred the fixes, then explicitly
requested both fixes together with the complete platform progression:

- The opening/dropdown structure did not open to allow the descent.
- The second round door did not open.

Preserved evidence: `build/coo/eater-first-platform-20260913/baseline.log`,
`dropdown-closed.png`, and `second-door-closed.png`. Exact door identities are
not established by the screenshots alone; the package joins below identify them.

The session reports region 56, then returns to region 16 at tick 145047;
the last resolved publication region in this baseline is 16. Reactor monitor
`686321C8 / 30 / 111` (`pm_encounter_start`) continues producing raw Sense
records, but the observer reports destination validation/reset. Consequently,
physical reactor position and current host region must be distinguished before
accepting occupancy observations. The user's route past the blocked doors is
not yet known. No game controls were operated.

## Read-only standing capture

`first-platform-standing.json` contains 64 stable samples. Raw evidence is in
`player-body-standing.bin` and `player-component-standing.bin` beside it. Full
salted local owner: `4EFAA152`; body point: `(8.122920, 210.311279, -774.640076)`;
velocity: zero. The first platform source is `686321C8/4/38`, definition
`80C43DAD`, with authored origin `(9.5, 210.5, -795.750061)`.

The source origin is 21.11 units below the standing body point. An arbitrary
distance check against that origin is not a recovered standing/contact predicate.
The capture is complete and the user was told they could move.

## Door and area-tracking correction

The missing commands target group `5654D7FD` in region 56:

- Dropdown: device `80C43A65`, type 23/0; monitor `8155C31B`, type 30/3;
  shared volume definition `8155C315`, type 60/6.
- Crossing iris: device `80C43A68`, type 23/1; monitor `8155C31E`, type 30/4;
  shared volume definition `8155C315`, type 60/5.

The Lua reactor phase now waits for each exact approach and requests its device
position 1 before path 1. The exact authored volume is also accepted from the
authenticated local-body position. Existing native power/lock defaults remain.

The live transition carries current/held region 56 (`45A97FF8`) while its other
region leg names outgoing entrance 16 (`8BA80878`). Eater previously discarded
the current leg. Exact `raid_envy_v310` now uses the existing held-region policy;
the regression replays these fields and keeps region 56 current. This corrects
event routing and the lifetime selector together, without inferring an area
from player coordinates.

## Implemented reactor policy

The four authored paths contain 13, 11, 13 and 19 platforms. The controller
requires an acknowledged native object and applied movement revision before an
ordered contact can count. Each activation needs continuous samples across the
Lua `platform_hold_ms` (500 ms); gaps over 250 ms interrupt dwell. A completed
activation raises the next platform and retains earlier platforms for solo play.
Device movement revisions are independent of object creation generations.

All required activations plus a fresh goal observation commit a path. An early
goal latch cannot satisfy the completion gate. Native health death resets the
unfinished path, allows respawn and keeps committed checkpoints. Missing body
samples alone are not death. Spawn sets use the recovered approach `AD98065D`
and path goals `1E8DBF89`, `C4FDE8CD`, `FD39D01C`, `10BB2205`, all in region 56.
Only the approach and first goal have prior live-arrival evidence; later goal
spawn sets are package/geometry verified and still need live retry acceptance.

The exact checkpoint light assets are enabled after paths 1–3 as an explicit
solo presentation policy inferred from their names/order. This does not claim
recovery of the original host timing graph.

The focused Release tests exercise the shipped Lua through all 56 platform
receipts into holdout, stale ownership/revision rejection, early goals and a
same-entity death/retry. The native standing/contact reader is now implemented;
an installed user-operated traversal still needs verification. Offline fixtures
are not a live playthrough.

## Earlier native standing detector (superseded)

The implementation in this section describes the installed `eater-final-dll-20260913-e`
candidate. Its live failure led to the authored-cylinder occupancy and health-reference
fix recorded in [PLATFORM-ACTIVATION-FAILURE-20260913.md](PLATFORM-ACTIVATION-FAILURE-20260913.md).
The upward-support predicate below is historical and is no longer the source policy.

The exact platform physics join is `80F42FCD` build ordinal 2, configuration
`80F42FA5 / 80808A0C / 378`. It uses the same component layout as the captured
player's `80C0C518 / 80808A0C / 660`. The adapter resolves the current platform's
body array and matches its bodies against the actual player's collision partners.
It then checks the exact contact manager, ordered body pair, live atom and contact
points. Array, component, source and player ownership are revalidated after reads.
The manager and both bodies must share the same world; contact separation is
bounded by that world's current native collision tolerance, read through the
constructor-proved collision-input chain. No guessed distance cutoff is used.

`verify_eater_platform_contact_native.py` executes the original sphere collision
math offline, proving the B-to-A normal direction and preserving body order into
the contact manager. Both body orders pass. This establishes the ABI; it does
not substitute for observing the real player/platform collision in game.

The solo standing rule accepts upward support with a slope below 45 degrees and
requires 500 ms of continuous samples. This is an explicit solo policy over real
native contacts. It does not infer occupancy from the platform's authored origin.
The movement revisions, ordered path state and health/checkpoint retries remain
owned by the controller. The GUI describes this build as a solo reactor test.

## Earlier Release candidate before the contact adapter

`build/coo/eater-platform-work-20260913` contains the current candidate. All five
targeted Release jobs pass: Eater controller (4,786 checks), Eater roster (12,282),
player position (55), other-mission native protocol, and the DLL build. The final
source manifest is stable and offline binding checks pass. Candidate DLL SHA-256:
`59a411da765eaee8119ddc9f0f8e29eab907da843595eed9f6625073edd96771`.

`lua-missions.zip` SHA-256:
`4b934765b3bf4f943a7ff8f746eebaf0c09540bd8f0d07c4b3cb5cfd961d99b2`.
The package uses the explicit five-job `eater-reactor-release` scope, not a claim
that the full 78-job Release suite was rerun. Installer `-ValidateOnly` passed.
It is **not installed**: the current game is still using the earlier load-crash
fix and the second collision capture has not happened. No installation or native
platform playthrough is claimed for this candidate.

## Installed Release candidate with native contact production (before the occupancy fix)

`build/coo/eater-final-dll-20260913-e` supersedes the earlier candidate. All five
targeted Release jobs pass against a stable source manifest: 4,787 Eater controller
checks plus 24 native contact-reader checks, 12,282 roster checks, 86 player-position
checks, other-mission native protocol, and the DLL build. Eater offline checks
include the original collision-code normal and tolerance fixtures. The compiler
reported no warnings. This is the explicit five-job `eater-reactor-release` scope;
the full 78-job Release suite was not rerun for this update.

- DLL SHA-256: `ceb6fc096d1bd59e13f276e7155bce5774906ff0ac140d8e55e92aa010c303bb`
- Package SHA-256: `cd5c60d3b389c31e269cdb86e77698ef7c1056f735ec85e5e7310054675c042c`
- Package size: 22,559,698 bytes.
- Installer `-ValidateOnly`: passed against the existing installed baseline.

Review corrected two real ownership mistakes before packaging: an object's fresh
creation generation differs from the activity generation, and the collision input
must match the dispatcher pointer stored at world `+C8`, not that field's address.
Fixtures include the dispatcher-address rejection and mutation case. Player
callback removal is synchronous before hook teardown. All native observations
remain bounded reads; the ordinary authority writer owns movement commands.

Installed on 2026-09-13 at 16:48 EDT after the user closed Destiny. All 16 installed
files were checked against the package hashes after installation; the installed
DLL now matches `ceb6fc096d1bd59e13f276e7155bce5774906ff0ac140d8e55e92aa010c303bb`.
The prior installed files were preserved in
`.dawn/backups/lua-20260913-164829-590a00ac`; the installation receipt is
`build/coo/eater-final-dll-20260913-e/installation.json`. The game remained closed.
Door opening and live platform activation still require an in-game playthrough.

After installation, launch **Raids → Eater of Worlds — Solo** from orbit. Verify
the dropdown and crossing iris open on approach, then stand normally on the first
platform for at least half a second. Each activation should raise the next platform
and retain earlier ones. Continue through the four paths; test a death before and
after a checkpoint. Regular Release logs distinguish `platform_pose_applied`,
`platform_physics_bound`, support changes, `platform_activated`, and health edges.
Barrier/Argos completion remains outside this implemented reactor checkpoint.

## Corrected volume-occupancy Release installed at 17:27 EDT

The user-operated first-platform run exposed two blockers in the previous build:
the primary physics body had no manifold points, and the player-health reference
required a nonzero group offset. Both are fixed in
`build/coo/eater-platform-volume-fix-20260913`, installed after Destiny closed.
The five targeted Release jobs passed with zero warnings; the native reader now
has 37 checks, and the separate health regression has 89. All 16 installed file
hashes matched after installation. DLL SHA-256:
`9624c46932739d57106b33236aac6a00cc117d1b64d9de2e1ac4419c386579b1`.

The corrected policy uses the exact authored platform cylinder, followed by the
existing 500 ms solo dwell. The previous upward-support predicate is superseded.
See [the failure, evidence and installation record](PLATFORM-ACTIVATION-FAILURE-20260913.md).
Live activation of this corrected build remains unverified until the next
user-operated first-platform test.
