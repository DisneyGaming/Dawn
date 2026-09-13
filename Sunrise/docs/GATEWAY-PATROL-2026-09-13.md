# Gateway shield patrol and entrance dialogue

Local changes only. The user requires explicit permission before any push.

The initial patrol hook at native RVA `A08660` never ran for these actors. A live capture of the common locomotion builder `A8FE10` recorded their normal mode-1 movement. The corrected hook follows that builder, including its false-return/retained-command path at context `+18`, and republishes only changed destinations. Its live bridge has produced hundreds of reversals on the same actors. Navigation remains native; no respawning, teleportation, or shield replacement is used. Evidence: `build/coo/gateway-marchers-20260913/live-bridge-14256.json` and the `patrol_reversed` log receipts.

Only the 44 authored shield formation sources in registry `85742F3E` qualify: slots `16 + 18*group + {0,2,4,6}`, groups 0 through 10. Both patrol and sensory overrides require a living admission for the current run and revalidate the salted actor/source/entity/AI identities and source generation. Ordinary encounter enemies are excluded.

Native control predicates `A81ED0` and `A82070` are forced true for those owned marchers. The first skips visual/attack evaluation; the second rejects `A060E0` before all six sensory-event handlers. The override is queried rather than persisted in actor flags, so recycled slots inherit no suppression. Offline native instruction checks are in `verify_sensory_controls.py` and `sensory-control-proof.json` in the evidence directory. An in-game shooting/proximity check of the assembled DLL remains necessary.

Dialogue row 4 (`Here it is...`) is now requested once at X >= 266, independent of Y/Z, native trigger volumes and defender deaths. It uses the normal voice queue if another line is playing. First contact with barrier volume `4B946B28/60/448` requests row 5, the Ghost/Vance exchange, once. Native speech submission still starts the 8,960 ms Vance cue. Only the owned Hydra at `4B946B28/1/49` must die; the other entrance defenders may remain alive. The first return wave starts with the return cohorts after that cue, without another proximity or kill-count gate. These newer changes are validated separately under `build/coo/population-23-20260913/validation`.

Validation and installation receipts belong in `build/coo/gateway-patrol-passive-20260913`. The live bridge in PID 14256 fixes movement only; that process retains the previously loaded DLL. The combined sensory and dialogue changes require the assembled DLL and script on the next game launch.
