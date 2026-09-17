# Hijacked squad count reconstruction

The original retail script's per-source actor counts have not been recovered. The installed packages prove source definitions, category order, template alternatives, placement rules, and individual member definitions. They do not establish a count from the number of template variants or alternative spawn points. The supplied YouTube reference could not be fetched, so this change does not claim a frame-counted retail reconstruction.

The user requested groups of three while preserving individual enemies. The explicit, reviewed mapping is stored in `tools/coo/hijacked_squad_counts.json`:

- Ordinary Goblin and Fanatic infantry sources request three actors from their original category, except the nine Mists reinforcement sources below.
- Mists composite reinforcement sources7/8/9/10/12/19/23/24/25 request two actors each. Standing simple-point sources2/14/15 retain three. All singletons remain unchanged. This is the latest user-directed density policy; producer family is native evidence, while reinforcement classification is reconstruction.
- Harpy-category sources using authored composite producers request three. Individually placed simple-point Harpies remain single, including the three separate small-arena sources 4, 5, and 6.
- Snipers and elites remain single. The Hydra, Fallen dropship, and final Well Hydra (native source name `sq_final_minotaur`) each retain one actor; their corresponding authored individual members are cave 22, surface 19, and Well 36.
- Each of the four mixed-category sources requests one actor from the authored elite category and two from its escort category, preserving category order. These are surface sources 13, 16, 20, and 21.
- Well platform infantry 27/28 and floor/final infantry 33/34/37 request three. Well Harpy sources 29–32 each retain a three-actor capacity; the current Lua schedules only 29 and 30 alongside the final Well Hydra, for six Harpies instead of twelve. Sources 31 and 32 stay unrequested. Final Hydra 35 remains one.

Across the entire catalog, 19 sources remain single, nine have totals of two and 28 have totals of three, for a maximum of 121 authored-request entries if every source were scheduled. This is not an assertion that 121 enemies appear in one playthrough. The Lua graph still chooses which sources and phases run; this change does not enable dormant sources or add repeat waves.

`Spawn.requested` contains the explicit per-category counts; `Spawn.count` is their sum. The native source authority publishes those category counts only after authentic placement readiness, and publishes zero while inactive or unprepared. The existing native engine still selects templates, uses the original placement rules, queues requests, creates actors, and controls spawn effects. No actors are fabricated, teleported, or duplicated by the count projection.

The mission's receipt ledger derives its per-source total from the same `Spawn.count`. Its capacity is 16 actors per source; this mapping's maximum is three. The generator validates category identity and order against package extraction, rejects oversized totals, and preserves explicit singleton producers. Focused catalog tests cover mixed-category wire values, zero-count gating, singleton exceptions, ledger capacity, native readiness, and every required death before clearance.

Remaining uncertainty: these values are a user-directed density reconstruction, not verified retail encounter counts. Native placement of three infantry on the platform producers still requires gameplay validation. Spawn-effect multiplicity alone cannot establish missing actor counts or a failed native spawn.


The10September scheduling change preserves all native category counts and
placements. Well source35 is a Hydra despite its `sq_final_minotaur` name: its
selected template80C0D467 shares the Hydra character, biped, fullbody, motion
and selector-lookup resources with the Entangled Mind (50of52 resource matches).
The four actual Harpy sources29–32 select80C0D08A and share rule83, tactical
row26/9, and table80B427F3 anchor11. Activating two existing groups halves the
escort to six without rewriting native placement or population definitions.
