# First platform activation failure — 13 September 2026

Installed Release DLL: `ceb6fc096d1bd59e13f276e7155bce5774906ff0ac140d8e55e92aa010c303bb`.
User-operated run 1, Destiny PID 62756. The user confirmed normal movement and
standing on the first platform, then performed a requested jump and return.
All diagnostic access was read-only. No game input or process memory writes were used.

## Observed failure

The log reached `reactor.path1.finished` and waited for ordered platform contacts.
All 13 first-path platform physics bindings were observed. Slot 38's raised pose
was acknowledged at tick 204078. At 204093, the adapter reported `supported=0`.
No support-present, platform activation, or player-alive event followed.

Two stable standing snapshots prove the authenticated player was at
`(10.010683, 211.595993, -774.639526)`, with full handle `5DFAA015`.
The player's body transform at `+1A0` had Z `-775.749512`; the exact slot-38
platform's body transform had Z `-775.75`. This is the correct first platform.
The initially logged component pointer had relocated; the current native bundle
resolver correctly located its replacement using the full entity identity.

## Contact-source mismatch

The adapter enumerated only the primary `80F42FA5 / 80808A0C / 378` physics body's
array. That body appeared in the player's collision rows but its contact atom had
zero points in both stable standing snapshots. Therefore the existing classifier
could never qualify this standing sample from that body.

Three other rigid bodies belonging to the same exact platform entity had nonempty
contact manifolds. Their configurations are `80C70467`, `81558185`, and `80F42FB5`.
The jump/return recording contains 2,370 samples. It also shows that upward-normal
classification of these child manifolds alone is insufficient: normals can be
horizontal near the platform edge while the player remains at platform height.
These records describe volume membership, not physical ground support. The source
fix selects only the platform-specific `80F42FB5 / 8080929E / 1B0` component,
its rigid body at `+88`, and its authenticated native cylinder shape. The generic
`80C70467` body is not an alternate input. All identities are resolved from the
current platform entity and rechecked after reading the contact manifold.

The selected volume body in this capture is `1A3A46AE9B0`. Of 2,370 recorded rows,
2,362 are valid and 2,262 contain nonempty contacts for that exact body. Membership
becomes absent at elapsed 24.9197882 seconds and returns at 29.9715342 seconds.
The movement includes lateral travel, so this is a jump/leave/return comparison,
not an isolated vertical-jump experiment. The source accepts stable finite native
contacts in any normal direction within the native collision tolerance. The
controller retains the separate 500 ms ordered solo dwell. This is explicit solo
occupancy policy; it does not claim recovery of the missing type-34 host rules.

Selected original records and the full recording's SHA-256 are preserved in
`evidence/platform-volume-live-input.json`. The original contact-normal proof is
retained as ABI evidence; it does not establish grounded support on this cylinder.

## Independent player-health failure

The reused health reader rejected every native interface reference with a nonzero
component offset. This player uses group handle `73F9E104` plus offset `E00`.
The resolved health component has kind `80804B8A`, self handle `4DF9E0AA`, full
player owner `5DFAA015`, and a clear dead flag. A component reference is not always
a zero-offset self handle.

The source fix resolves the group plus a bounded signed offset, verifies the health
component and its own self handle, repeats identity checks, and retains that self
handle for corpse tracking. The focused Release regression passes 89 checks.
The primary agent reviewed the change and independently ran that binary.
The corrected Release was installed at 17:27 EDT; details follow below.

## Review and candidate validation

The exact-volume reader has 37 passing focused Release checks, including the
recorded centre, edge and return points and rejection when an unrelated child has
contacts but the exact volume is empty. The embedded Eater regression passes
4,787 controller checks. The independent reviewer found no production blocker.
The primary agent checked the selected excerpts against the complete recording;
the byte hash and all selected original records match.

The captured platform bundle has 231 reflected rows in 20 active groups. The
reviewed successful binding path reads 28,657 bytes, below its unchanged 32,768-byte
limit. The repeated native source callback permits another binding attempt if a
component is not ready yet. This review does not weaken the read limit or invoke
game code from diagnostic tools.

The candidate directory is `build/coo/eater-platform-volume-fix-20260913`.
It preserves `capture-review.json`, `health-review.json`, and the validation logs.
The first offline check found only the mutable cache fingerprint had changed after
the live session. Regeneration changed four evidence hash fields and no generated
header contents or semantic bindings. The old evidence and exact comparison are
preserved under the candidate's `evidence-refresh/` directory. The integrated
Release validation includes both the historical normal ABI proof and the new
authored-cylinder proof.

## Installed corrected Release

Installed on 2026-09-13 at 17:27 EDT after Destiny closed. All five targeted
Release jobs passed against a stable source manifest with zero compiler warnings:
4,787 Eater controller checks plus 37 native-volume checks, 12,282 roster checks,
86 player-position checks, the other-mission native protocol regression, and the
DLL build. The separate health regression passed 89 checks. These are targeted
checks, not a rerun of the full Release suite or a completed gameplay test.

- DLL SHA-256: `9624c46932739d57106b33236aac6a00cc117d1b64d9de2e1ac4419c386579b1`.
- Package SHA-256: `45db29d08decdd798eae4b2dc5c47308ebe3e66511f46f8cec5bb5c031a8fdfb`.
- Backup: `.sunrise/backups/lua-20260913-172703-a2cfcb34`.
- Receipt: `build/coo/eater-platform-volume-fix-20260913/installation.json`.

Installer validation passed and all 16 installed file hashes matched afterward.
Destiny remained closed. On the next user-operated run, check that the alive event
appears and first-platform occupancy remains present for 500 ms, followed by
`platform_activated` and the next platform's applied pose. Normal Release logs now
name the detector `platform_occupancy`, with policy
`authored_platform_volume_solo_dwell`. Continue the existing ordered-path and
checkpoint playtest after the first activation succeeds.

## Preserved evidence

`build/coo/eater-platform-failure-20260913/` contains:

- `baseline.log`: log saved before diagnosis.
- `player-symbols.json`: player-cache addresses recovered from the matching PDB.
- `standing-1/` and `standing-2/`: player/body/collision/manager/atom captures and
  identity stability checks.
- `platform/`: current exact platform entity, body, world, and collision input.
- `health-capture.json` and its collector: native signatures, player identity,
  complete bundle component catalog, and the health-reference evidence.
- `jump-and-land.jsonl` and its collector: bounded motion/contact comparison.

Physical platform progression remains unaccepted until a corrected Release is
installed and the user completes the live check.

## User-authorized temporary alive-gate test at 17:35 EDT

The next run, PID 11208, loaded DLL `9624c46932739d57106b33236aac6a00cc117d1b64d9de2e1ac4419c386579b1`.
Its log recorded real first-platform occupancy, but never an accepted player-alive
event. A read-only PDB-derived controller snapshot confirmed `aliveKnown=0`,
`dead=0`, and `healthPlayer=FFFFFFFF`, with the correct current player `50FAA390`.
The health fix therefore did not resolve the live health-reporting failure.

The user explicitly requested forcing the check in memory to test the next rise.
Only five controller-state bytes were temporarily changed: `healthPlayer` to the
authenticated current player and `aliveKnown` to true. No contact, platform-pose,
native player-health, executable-code or installed-file bytes were changed.
The test was bounded to ten seconds and restored both original field values after
2.62 seconds, once activation and the next platform pose were acknowledged.

- Tick 430641: `platform_activated`, path 1, activated 1, slot 38.
- Tick 432672: `platform_pose_applied`, slot 39.
- Final controller: `next=1`, raised bits `3`, activated bits `1`.
- Both temporary health fields were restored and verified.

This confirms that the current native occupancy, solo dwell and next-platform
movement path can work when the health gate passes. It does not validate the
automatic health reader or the full encounter. Evidence and the exact temporary
test are in `build/coo/eater-platform-recheck-20260913-1733/`, including
`controller-before.json`, `temporary-alive-test.json`, and the before/after logs.
