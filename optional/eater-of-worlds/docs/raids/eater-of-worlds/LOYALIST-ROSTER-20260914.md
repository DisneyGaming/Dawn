# Reactor Loyalist reinforcements

The finale requests **2 Loyalist Colossi, 5 Loyalist Psions and 3 Loyalist Incendiors**. This ten-enemy roster supersedes the earlier nine-enemy and four-enemy test groups. Standard and Contest share the implementation; Contest changes only the player's effective Power. The twelve crossing sources still request thirty enemies.

Source 18 uses center delivery rule 311 and requests two Colossi. Source 19 uses left delivery rule 293 and requests three Incendiors. Source 20 uses right delivery rule 309 and requests five Psions. Native ships, insertion, release, category and source generation remain intact. All ten actual deaths are required before the existing hatch and objective sequence can complete.

## Permanent delivery fixes

`eater_reinforcements.cpp` owns a dedicated production detour at native `+4E34C0`. Normal bootflow installs it, quiesces it, and checks removal before teardown. Its call gate preserves exactly-once native forwarding, including the trampoline publication window. The retired broad spawner diagnostic bundle remains disabled and no longer owns this selection callback.

The adapter runs after native selection and before `+4E2E80` copies the complete member into its durable creation queue. Incendior `80BFAA20` comes from resource `8155C43B`, descriptor `0x920`, choice `AAF9AB71`; Psion `80C1A8E4` comes from `8155C3FF`, descriptor `0x8B0`, choice `36E6516A`. Colossi retain native choice `80C0FA98`. Only the relocatable reference and authored choice metadata change. The selected Eater run, exact source definition, category, generation and loaded donor are verified, with a final run/generation recheck. Unverified selections pass through unchanged. The adapter never moves an actor.

All three finale sources now use tactical objective **22 (`holdout_door_objective`)**, with its five authored tasks. Objective 3 previously sent them to the neighboring platform. Objective 22 contains the final platform and side landing decks. The generator owns this correction so catalog regeneration preserves it.

Death cleanup still retires native actors and waits for empty queues, zero owned actors, matching incoming/applied/request generations, settled entities, and two consecutive observations. Request word `source+0x638` retains the generation after native reset. Requiring that word to equal zero caused retries to wait forever; the predicate now requires the matching generation. Old-generation births and deaths cannot count toward the next attempt.

## Evidence and acceptance

`build/coo/eater-ship-memory-test-20260914/RESULTS.md` records the live investigation. A nine-enemy mixed delivery stayed on the correct landing decks until player death. A subsequent four-enemy attempt used the same type substitution and registered all four real kills. The user confirmed that test worked. Captured retirement generations 130 and 132 establish the corrected reset layout.

The new ten-enemy group uses that tested delivery path with revised counts. Its complete native playthrough remains to be performed after installation. Release validation, packaging and installation receipts are under `build/coo/eater-loyalist-delivery-20260914`. Automated checks cover serialized counts and objective 22, all five tactical rows, exact native choice replacement, rejected substitutions, ten-kill completion, and death/retry isolation. The original roster candidate was installed with SHA-256 `312777dc0f3f3f8b9a29393a4fd07c11ac49ef648e4423958454d11a0429ca9a`; its dormant hook is the reason this dedicated owner is necessary.
