# Arrival, doors, and enemy density

Build 86657. Requested scope ends at the barrier execution boundary; the barrier
fight itself remains deliberately unimplemented.

## Final solo behavior

- All twelve crossing sources start with the reactor darkness zone. Their authored
  categories request two actors each, totaling 30, and persist across checkpoints.
- The large final platform gets nine Loyalists: three each from finale sources
  18/19/20, using placement rules 311/293/309. The positions are center
  `(8.25,-2.75,-738.75)`, left `(-54.25,-10.75,-741.25)`, and right
  `(66.5,-13.75,-741.25)`. Nine real admitted deaths are required; eight cannot clear it.
- The reactor hatch stays instantiated. Clearing the Loyalists publishes
  **Delve deeper** and drives its native opening pose. Traversal waits for an
  applied opening receipt, not just the command.
- The underbelly exit gate binds to the current salted generic device of its
  authored door object. Suction volumes start only after the exit and ejection
  gates physically report open. Open gates persist and their revisions are
  reasserted after death; the solo entrance remains open.
- The launch stages the authored Argos boss source in direct creation mode,
  its energy shield, and six debris housing objects. The six housing pose commands
  wait for object creation. None of the 23 boss platform objects is requested.
- Movement is sampled from the authenticated player physics callback at most
  20 times/second. Passing any one of the seven measured hoop aperture midplanes
  must precede entering the authored quarantine arrival polygon. Stale attempts,
  missing player identity, out-of-radius crossings, and discontinuous hoop samples
  are rejected. Death discards the previous attempt's hoop proof.
- Entering that arrival trigger after a hoop updates **Break the barrier** /
  **Destroy the barrier protecting Argos, Planetary Core.**, enables restricted
  respawn, and enters phase 4 at mechanic 110. No timer completes that mechanic.
- The launcher describes the raid without development progress or playable limits.

## Trigger policy and evidence

The user approved using an arrival trigger instead of requiring physical floor
support. No authored volume contains their captured point
`(-72.63634,-1118.47107,-1810.67761)`. The actual destination is covered by the
overlapping `tv_quarantine_breakout` and `tv_quarantine_cooking` polygons; these
contain the earlier standing point `(-40.77986,-1307.53723,-1909.78943)`.
This is an area-entry policy and does not claim a native grounded-state signal.

- [Hoop cylinder proof](HOOP-CROSSING-20260913.md)
- [Arrival volume capture](evidence/current-arrival-volume-check.json)
- Live intro evidence: `build/coo/eater-argos-live-20260913/RESULT.md` and
  `intro-platforms-off-receipt.json`. The user confirmed the debris/shield appearance
  with the boss platforms hidden.
- Live door evidence: `build/coo/eater-second-door-live-20260913/binding-receipt.json`.

The rebuilt source needs a fresh natural playthrough to confirm the combined
spawn timing, door animations, source density, and hoop-to-arrival handoff.
Synthetic receipts and successful compilation do not establish gameplay acceptance.
