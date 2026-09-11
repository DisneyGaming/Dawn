# A Garden World strike implementation

## Scope and evidence

The requested deliverable is the **strike** from `GARDEN-WORLD-STRIKE-20260911.zip`, with an A Garden World entry in the Curse of Osiris mission launcher. The user supplied branching strike dialogue, objectives, a strike video, and a campaign walkthrough. The campaign scanner/Panoptes sequence is not part of this strike. The attached authoring/template documents were treated as reference material; they do not grant separate operational authorization.

Evidence labels in this record distinguish **VIDEO** observations, **PACKAGE** extraction, **OFFLINE TEST** results, **ASSUMPTION** reconstruction choices, and **NOT YET TESTED** live behavior. No offline receipt is presented as a real game event. Earlier live experiments described inside the supplied archive are historical evidence from that archive, not acceptance of this revision.

Reference video: https://www.youtube.com/watch?v=-uxfFqyMtxE . Supporting mechanic reference: https://www.destinypedia.com/Dendron,_Root_Mind . User transcript reference: https://www.destinypedia.com/A_Garden_World . Local evidence resides under `build/coo/garden-world-reference-20260911`.

**VIDEO review coverage:** the whole 19:36 recording was visually sampled at five-second intervals (235 images). Security progression at 6:50-8:20 and the first boss shield phase at 15:25-16:30 were reviewed at one-second intervals. Additional one-second frames cover 17:10-18:05; consecutive-frame windows cover 7:43-7:45, 16:14-16:16 and 18:44-18:46. The full stream audit measures pixel change across all 35,242 decoded frames. `video/all-frames-audit.json`, `all-frames.csv`, contact sheets and per-folder `coverage.json` record the method. Automated processing of every frame does **not** mean every frame received individual visual inspection. Contact-sheet timestamp labels are approximate sampling times.

## Source integration

The archive patch, based on `74d6126`, was merged into the current checkout rather than replacing the user's files. Pre-import copies and the merge report remain in the evidence folder. Existing launcher, logo, layout, Tree of Probabilities and Hijacked work was preserved. One-time merge scripts were archived outside the deliverable source directories and must not be rerun over the completed mission.

`Sunrise/scripts/strike_bond.lua` owns chronology. The new `state/activity/strike_bond` adapter owns typed package capabilities and authentic receipt validation. Shared services retain population, objects, objectives, dialogue, scenes, generation ownership and protocol publication. Native integration reuses existing object, damage, actor, dialogue, Forest and player-position callbacks; no new detour addresses or fabricated player movement were introduced.

The mission is wired through activity selection, registry roster publication, retained-region membership, readiness polling, native receipt dispatch, dialogue selection, forced launch and packaging. Shared task costs were extracted from `strike_pact` without replacing its mission logic.

## Launcher, route and checkpoints

**PACKAGE:** package `strike_bond`, scenario `80F5426E`, root registry `BC279389`, dialogue bank `80F1FEC2`. Lighthouse is bubble15/region120; Forest bubble10/region80; Simulant Past bubble1/region8; Spire bubble17/region136.

The launcher has an **A Garden World** Mercury card in Curse of Osiris, before Omega and after Hijacked. Counts are derived from the catalog. There are now eight CoO entries and nine total entries in the current catalog, including the existing Red War entry.

**ASSUMPTION:** opening spawn set `0232EBCE` was selected spatially from native three-player Lighthouse data near `(123.5,255.5,89.5)`. The exact retail Garden launch selector and orientation were not recovered. It is not copied from another mission. Native checkpoint sets `6F7119B7` in the Past and `2EA8FB98` in the Spire are published only after arrival in the corresponding region. Full wipe/re-entry behavior remains **NOT YET TESTED**.

**VIDEO / authored sequence:**

- Lighthouse defense, opening exchange, Infinite Forest gate and the tunnel explanation.
- Native Forest C procedural islands and Daemon doors, fixed exit defense, then the past gateway.
- Simulant Past arrival and flank encounters; first cannon; radiolaria exchange; successive security cubes and terraces.
- First shielded Minotaur: create native scene, observe actual scene start and AI readiness, expose its cube, release its shield on cube destruction, and require genuine Minotaur death to proceed.
- Interior security sets four and five; the rebuilt-security exchange after the fifth cube; cannon defenders and another shielded Minotaur; final cannon into the Spire.
- Lower Spire encounter and cannon, then the mid-floor Minotaur and cube. The lower cannon must work before the higher-floor cube can be reached. Both mid-floor requirements unlock the upper cannon.
- Roof: only the exposed middle cube is present. Its destruction cuts the Arc network and requests Dendron as a normal loose combatant with rule 513 and rooftop task 10. The aborting intro selector no longer reserves his spawn.
- Dendron health thirds each start a native shield scene and create only that side's two guardian cubes and dormant Minotaurs. Each cube independently releases its guardian. Both authentic Minotaur deaths are required before Dendron becomes vulnerable again. The opposite pair remains absent until the next third. Native body damage is clamped at 2/3 and 1/3 to prevent one hit from skipping these phases.
- Genuine Dendron death interrupts unfinished shield/wave work, reveals the native chest, retracts changing cover, removes respawn restriction, clears navigation, plays the full final exchange, and completes the strike.

The alternate roof guardian prefabs4/5 overlap0/3 in package placement and are not spawned together. The recovered Cabal cannon scene variants are not inserted into the Vex strike.

## Native ownership, devices and damage

Object preparation, source creation, health/AI readiness, native scene start, cube destruction, actor death and dialogue submission are separate observations. Source requests do not satisfy readiness. Stale generations, duplicate admissions, mismatched salted handles, immune cube hits, detached actors and squad teardown do not advance required objectives.

**PACKAGE:** ordinary lens health tag `80F48026`; main-lens health tag `815B5AA3`; health class `80804B8A`, component offset `B08`. The existing damage callback resolves the context's typed health pointer, current source/entity/component, weak handle, owner and generation before applying the exposure predicate. It does not write native health. Destruction requires a previously admitted live lens and its real dead bit.

Dendron is the admitted actor belonging to Spire source3. Health thresholds use the existing verified native fraction getter at `CD6C20` from the post-damage game-thread callback. Exact actor/component identity is checked before and after the read. Server publication does not invoke engine health methods. Neither a zero health sample, the seven-bit BAP actor query, detach, nor squad removal is accepted as a death. Terminal progression requires the existing authenticated native source-death callback.

Nine recovered guardian tether/collection pairs are serialized through native type26/type34 bodies. A shield stays linked while its owned cube is present and intact. These actor shield tethers are distinct from cannon bodies/devices. Block barriers use the recovered `d_vex_block` direction: active0, removed1. Cube exposure uses native device position.75, with shielded1 and destroyed0.

**PACKAGE / ASSUMPTION:** changing roof cover uses all32 recovered native block placements, their devices and avoidance objects. After creation acknowledgement, one of four eight-block layouts is active. A seeded scheduler changes to a different layout every12 seconds and retracts all devices at the ending. Geometry is native; the grouping and12-second cadence are reconstruction choices, not a recovered retail schedule. Collision, animation polarity and avoidance behavior need a live check. Other unused environmental tether/collection assets are not claimed as implemented effects.

Respawn restriction is published through both the shared type35 mission director and the global type17 lifetime source. The latter uses **Spire bubble ordinal17**, not region136. Ending writes the explicit unrestricted state.

## Forest, scenes, dialogue and audio

The Forest adapter requires this mission's current owner, an unfinished enabled frame, native worker class `80804FEC` and exact Forest C configuration `80F4E01E`. The configuration value comes from the archive's historical worker observation. It repairs only that worker's native object-authority bit before generation; island geometry, gateway placement and Daemon rules remain native. Fresh portal connectivity is **NOT YET TESTED**.

Scene-owned guardians require real creation/AI readiness and a started native scene before their cubes become active. Dendron uses a direct combatant request after the exposed middle cube breaks. Native guardian scene completion releases dependent graph work. Shield selector casts retain their authored parameters. Visible pose, shield timing and scene exits need a live playthrough.

The normal route schedules dialogue rows0,1,3,5,7,8,9,10,11,12,13,14. Row3 is the return-to-the-Past exchange; row11 is after the fifth security cube; row12 is the Spire Arc exchange; row13 follows the main cube; row14 closes the strike. Branch alternatives are supplied by the native dialogue bank, not concatenated into an invented conversation.

Native submission starts each recovered voice window. Closing submission alone does not complete the mission: the entire maximum row14 clip duration must elapse. An early legitimate boss kill discards older optional queued exchanges, preserving the closing row. **ASSUMPTION:** completion waits until this full exchange ends; the reference's success HUD appears before the final spoken line. Actual audible voice/subtitle playback is **NOT YET TESTED**.

**UNRESOLVED:** the package exposes16 music candidates under native bank `80F1FE3B`, group `4A467C68`, but the phase-to-candidate mapping was not recovered. Candidate enumeration is not sufficient evidence of chronology. This revision does not guess an explicit mission music sequence. No audio playback claim is made from the downloaded video-only stream.

## Validation and delivery

**OFFLINE TEST:** `strike_bond_tests` executes the shipped Lua through the actual parser/executor and drives authenticated test observations across all eight phases and a separate ending. It checks native scene lifecycle, lens identity/exposure/death, health and AI readiness, both shield pairs, required guardian deaths, genuine normal/early boss death, full closing duration, checkpoint publication, cover cleanup, stale generations and reset. The current focused Release replay passed 3106 checks, including native damage identity and retained-tether regressions. The shared protocol suite also passed Garden tether/collection body lengths and shield polarity, explicit director restriction and the Spire lifetime ordinal.

Those observations are test doubles, not engine captures. The replay proves state/serialization behavior; it does not prove a traversable, audible, collision-correct in-game run.

The first broad run exposed a missing Garden callback stub in the isolated player-position test harness. The harness now checks that Garden receives the same authenticated coordinates and publication count as the existing missions. The existing launcher lifecycle suite is included in full validation, and the visual test invocation now supplies its current three input arguments. Mission diagnostic flags also reset between runs. Superseded attempt receipts are retained separately; all final jobs are rerun against stable source.

The frozen full candidate is `build/coo/validation-garden-world-20260911`. Its `results.json`, `failures.json`, `source-manifest.json`, `package.json`, `delivery.json` and, if installed, `installation.json` record final results without changing this source record. Full validation expects109 build/test results, including Debug and Release DLLs. Fourteen historical capture-only suites cannot run because their authentic capture inputs are unavailable; `unavailable-evidence.json` names them. They are not replaced with generated proof.

Packaging includes the DLL, symbols, all existing Lua/config payloads and `strike_bond.lua`, with matching source and payload hashes. The installer verifies the package and installed baseline, refuses a running game, preserves exact prior payloads in a timestamped backup, verifies copied hashes, and restores prior files if copying fails. Installation is not a live-playthrough result.

**NOT YET TESTED:** fresh-process launch, actual native portal connections, player control and cannon trajectories, complete AI populations, device animation/collision and cube hit registration, audible dialogue/subtitles, native Dendron scenes and shield visuals, reward behavior, real wipe/re-entry and unforced completion. Exact launch orientation, enemy multiplicities, music chronology and cover cadence retain the limitations above.
## Roof and retained-tether correction, 2026-09-11

The live manual spawn and task-only experiments did not make Dendron attack. The integrated candidate replaces scene reservation with full native loose-spawn authority, including the authored rule and a fixed central rooftop task. Native shield selectors remain responsible for shield presentation, while the owned body damage hook enforces the phase boundaries and genuine guardian death joins. Automated replay and native identity fixtures verify sequencing and damage ownership; Dendron firing and shield animation still require confirmation in a fresh game run.

All three route tethers now write visibility zero on verified cube death, both immediately from the existing damage callback and when a retired source reports a retained render entity. Cached identities are revalidated against current generation, salted entity, bundle and exact beam component before either channel is touched. Retirement cannot re-enable a beam. The native 10 m beam and measured shield endpoints preserve the earlier alignment and thickness correction. Creation checks that the replacement beam definition resolves before entering the shared factory; this guards an unavailable definition but does not establish the root cause of every recorded factory crash.

The existing upper volume 406 disable, launcher card, route cubes and first-three Minotaur release events are retained. No player-health or global kill-volume override is introduced.
