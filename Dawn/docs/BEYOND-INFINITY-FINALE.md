# Beyond Infinity: consolidated live repairs

The patch preserves the accepted Well and Precipice route and consolidates the live-tested Forest, Past, Future, and ending repairs into the normal Lua/native executor. The user completed both Forest visits and escaped through the Lighthouse in an intervened live run, then confirmed both ending exchanges finished. A fresh run of the consolidated installed build is still required to verify integration and the corrected portal and shield visuals.

## Reference and recorded acceptance

Reference: [FireCarnage Gaming, Beyond Infinity](https://www.youtube.com/watch?v=SX-qxvYWyhM&t=470s). The requested 7:50-to-end span was previously reviewed using 185 frames sampled every two seconds, 13 contact sheets, and the full caption tail. Native dialogue bank `80F1FDF7` supplies the recovered wording and durations. This cooperative reference does not establish exact actor counts or off-camera trigger times. The user deferred precise Past timing while the functional route was repaired.

Evidence is retained in `build/coo/beyond-infinity-finale-research`, `beyond-infinity-forest-research`, `beyond-infinity-past-live`, and `beyond-infinity-final-repair-research`. The live process was PID25996, creation time134333922710912444, mission run2, generation65. Each live mutation has a pinned before/after record; none is installed as a background memory writer.

The user confirmed native travel into Mercury's past with row32, the return portal's transport and row34, both Forest traversals, Panoptes and the Future enemies appearing, transport through the invisible escape portal, and the final Sagira/Ikora exchanges. Logs record row48 submitted at t4223437, mission completion at t4236422, and executor completion at t4236547. These are receipts from the repaired live run, not proof that the prior installed DLL could complete without intervention.

## Accepted opening and two Forest visits

The accepted plate clock, rising ring, permanent exposure after full charge, Gateway-derived native box damage, wall opening on actual box destruction, stair dialogue, upper pair with only the higher voice, and full Precipice conversation remain in place. The earlier high-ledge reflection was explicitly deferred by the user.

The existing Forest worker has a Beyond-specific adapter scoped to container `80F4D0F2`, `80804FED` definition17D8, sensor `8E70632B/37/3`, and descriptor `80F460FA` atD68. It uses the native solver and authority mechanism already used by Omega. Omega's separate Forest D contract remains unchanged.

The shared debug ignition is suppressed for Beyond. Native Forest generation stays disabled until the authenticated final Behold speech finishes and Lua requests pass1. The adapter confirms native race publication before generation. Pass1 selects Vex key67AF9045 and north-to-west endpoints; pass2 selects Fallen key89567586 and west-to-east endpoints. Native islands, enemy sources, Daemons, and gate interactions remain responsible for traversal.

The first pass preserves actual entry observations collected during the held reveal. A different later pass clears obsolete traversal latches; repeated publication of the same pass does not. Row30, the Daemon tutorial, requires the native `reveal.tv_if_entered` volume below the overlook. Merely approaching the overlook does not play it. Row37, the full Traveler/Lost Prophecies passage requested by the user, plays once on the second Forest entry. The competing row36 publication at that entry is removed. Row38 belongs to the second exit corridor.

## Native portal arrivals

The live runs established native portal travel and a premature host override at the Future load seam. Waiting for a redundant host teleport stalled the mission after the player had arrived. Lua now uses the actual receiving area:

- First Forest to Past: `past.past_quarantine_volume.occupied`.
- Past to Forest: `forest.tv_end_1.occupied`, then the native `forest.tv_begin_past` entry.
- Second Forest to Future: `future.tv_player_enters_space`, observed after actual native travel. The loading-corridor directive is not portal contact.
- Future to escape corridor: `escape.mercury_m_vod_future_060_filter.occupied`.
- Escape corridor to present Mercury: `lighthouse.mercury_m_vod_lighthouse_030_filter` only. The outbound intro010 corridor is not on this return path.

The `.occupied` observation samples current presence and cannot be satisfied by a previous visit's latch. The initial Lighthouse latches are retired during the second Forest pass. Actual final Lighthouse crossings are retained while row47 is awaiting submission or playing, so a player who runs ahead does not lose the final dialogue. No host routes1/2/3/4/5 are requested by the shipped mission, and no arrival bit is fabricated.

The consolidated patch initially retained host route3 because its handshake completed. The subsequent live run exposed a distinct error: the corridor load-zone directive requested that teleport before contact with the portal. At t382641 the client explicitly preempted its normal_z_leg transition with this host teleport. Successful delivery did not establish the correct trigger. The correction removes route3 from Lua as well and observes native Future arrival; the native transition and barrier retain control. The arrival crossing is latched for the current run so finishing the approach dialogue cannot lose an early crossing. The shared transport implementation remains unchanged but is not requested by the shipped mission.

## Past construction and return

Scene80EC0945 owns machine1, its two Reflections, placements, animation children, and authored clocks. After row32 finishes near the first Reflection, Lua supplies40C0AC42. Native1FEBC344 releases machines2-5 and row33; C0026857 advances their devices. Row34 follows the completed history speech. The native Scene's31.5-second child supplies the return actor.

The recorded Scene never emitted51EEA0A6 on the tested path. The return portal therefore depends on actual completion of row34 rather than that missing event. Input7AB76D9E remains tied to the completed history and vista, but cannot strand the player. Exact synchronization of the return actor, optional row35 and the portal-opening gesture remains a visual/timing acceptance item; the user deferred ordering refinement during the live repair.

## Portal presentation

The transport cores and visible frames are different objects. Future cores80F44A37 transported correctly even when their frames were invisible. Their effect devices43/46 select the static frame placements in container80F46016 by IDs1BA56A56910D5480 and11D83B99F7888842, respectively. Both placements instantiate80F3DB0A, also used by the Past return frame source80F461C1.

All three frame controls now publish native position0.1 for activation and0 for off. Graph80F3DB09 reads `device_position` and accepts activation in the range[0.090100005,0.100100003]; the old value1 selected no authored mode. Unrelated devices retain their previous channels. The actual Past frame animation and Future visibility must be checked on the consolidated run. No guessed transform overrides or replacement visual meshes are used.

## Future speech, Panoptes and shields

Future Scene80EC0901 retains its native entry and approach inputsFA39DB9E,05BBC301 and43E472AF, top-level rows39/40, internal relays, and Panoptes animation timing. Input05BBC301 releases action15, which starts action19 and native vignette entity80EC090E (graph80EC090D). This is an animation cue as well as a route toward row40. It now follows `tv_player_approaching_first_echo` while row39 can still be playing, instead of waiting for row39 completion and `tv_player_near_first_echo`. The same Scene generation and its authored cast remain active after leaving the approach volume; the higher scene cue still requires both completed speeches and its native volume. The revised appearance/teleport and actor persistence require a visual replay in a new process. Native3A26E9BE and4A0A18EB gate the escape presentation. Native46 remains Scene-owned.

The duplicate audio was traced to action17 launching child entity80EC0874 and graph80EC0872. That child already owns rows43,42,44 and45, while Lua also queued them globally. Their native start times explain repeats separated by approximately1-4 seconds. The redundant global publishers and their waits are removed; the child remains the sole owner. There is no evidence supporting removal of rows39/40.

The Future ambush uses the six packaged front sources and three back sources, including both authored categories where present. Their native invincibility effects are type26 slots15/16 and their target filters are type34 slots20/19 in registry0FF26BCC. Filters and effects are published before the enemy requests. The native source-set selector80809578 collects all actors from each exact source, covering multi-category spawns; the existing Gateway single-entity selector80809579 is unchanged. No player or broad world selector is used. The mission never requires killing Panoptes or the protected ambush to escape.

## Ending dialogue and completion

Fresh arrival in the escape corridor queues row47: Sagira's "This is bad. Really, really bad" and cooperation exchange. The actual present-Mercury Lighthouse exit queues row48, beginning "Ikora! You there?" The shared dialogue queue serializes these exchanges, records their real submission, and waits for the final row's playback duration before publishing objective10 and mission completion. Native transport and dialogue remain separate facts.

A subsequent fresh run (PID46492, creation134333982771570986, run1/generation1) exposed an incorrect prerequisite: the ending waited for intro010 at x−554..−536/y1951..2014, while native escape returned to present Mercury at x359.395/y241.075/z100.145 inside lighthouse030. Replacing that pending live requirement with the existing native lighthouse030 capability released row48 once and completed after its playback window. The user confirmed the full exchange. The shipped Lua removes the unrelated prerequisite entirely; no dialogue or arrival receipt is fabricated. Evidence and the reversible live mutation are in `build/coo/beyond-infinity-future-scene-fix`.

## Validation and delivery

The installed native candidate remains `build/coo/beyond-infinity-consolidated-patch`, validated by 18 suites in both Debug and Release plus both production DLL builds: 38 successful builds/tests with no compiler warnings. The subsequent Future load-zone correction is the compatible Lua-only package `build/coo/beyond-infinity-native-future-patch`. It retains that exact installed DLL and native source and records the updated script, previous script, native hash pin and two focused Debug/Release route suites. Its installation receipt distinguishes the file update from the running process, which keeps its already-loaded Lua document until restart. The subsequent compatible package `build/coo/beyond-infinity-future-scene-patch` includes that earlier barrier fix plus the early native reflection cue and actual Lighthouse exit correction, with two further focused Debug/Release passes. It preserves all 1544 native build inputs and the same installed DLL.

The Future correction additionally replays the former load-zone contact with no host request, rejects foreign arrival observations, and retains an actual arrival crossed before the warning finishes. Regressions exercise current versus stale native arrivals, foreign run rejection, entry-latch lifecycle, completed row34 gating the return portal, single row37 on re-entry, sole native ownership of the four Future lines, real scene cues before escape, early Lighthouse crossings during speech, final row48 completion, and reset isolation. Wire checks cover exact portal channels and native shield/filter target sets. They are not a substitute for native visual validation.

The final fresh playthrough must verify initial plate VFX, both generated paths and Daemon doors, portal appearance and floating pose, shield visuals/immunity, native Future speech without repeats, and uninterrupted completion without live edits. Checkpoint/wipe and cooperative advancement remain unaccepted; exact retail choreography and the deferred high-ledge actor are not claimed as complete.

## September9 final polish candidate

The completed Forest reveal now receives an explicit dormant Scene authority after native row23 completion. Its retained C7ECAA77 input cannot restart Behold when the area reloads on a later Forest pass. The first-pass physical-entry latch and Past travel sequence are unchanged; the reported Past failure was subsequently attributed to bypassing the Forest jump trigger.

Future ambush sources6–14 now receive type3 `future_ambush_objective` slot0, rows0–5 for the six front sources and row6 for all three rear sources. These native rows and provider bounds come from80F46039/80F46040. Nearest authored firing bounds, then center, then row is an explicit placement policy, matching the Gateway reconstruction method. The source rules, category counts and invincibility filters remain native. Full source-wire fixtures use the independent native reflection encoder.

The early escape event4A0A18EB still releases the ambush. Both visible portal frames and transport cores now wait for a separate speech-tail observation: root action23 must actually complete its native33.5-second delay, after which the entire4593ms bank duration of row45 plus250ms is retained. This is conservative reconstruction timing, not an audio-device completion receipt. Child80EC0872 starts row45 at33.34392166s on the same preceding completion edge. The earlier29.5-second escape event, root completion, foreign receipts, and elapsed host time without the cue cannot release the portals. Native row45 remains the only voice publisher.

Osiris actor disappearance remains under investigation. The candidate adds read-only, identity-checked logs for children27 and17, including their salted actor references. It does not claim actor persistence has been fixed. Fresh live validation of the three changes and that actor handoff is pending.

## Forest sensor runtime identity repair

The next live run exposed a pre-existing adapter rejection: `lookup_sensor` correctly returns definition class80804EF6, but the component at the resolved address has runtime class80804EF7. The second comparison incorrectly expected80804EF6 again. The native worker stayed idle even though Lua had reached Forest pass1 and row23 had finished. User confirmation established this predates the final-polish patch.

In PID13124 (creation134334037313695400, installed DLL3e329131c3f527dd16d6bb16afccc858803254f3b86312cebb5d10ef707a33a6), the sensor header was `{80F460FA,80804EF7,D68}`. A one-byte correction to the Beyond-only compiled comparison released the existing generator: at native tick567031 it reported ready1/pass1, then progressed through worker states6,2,4,5,0. Lookup class, package identity, scope8E70632B/type37/slot3, worker configuration80F4D0F1, native switches, generation checks and owner checks are retained. No teleport or entry receipt was fabricated. Evidence and the reversible live patch are in `build/coo/beyond-infinity-forest-live-20260909`.

The source now distinguishes lookup and runtime identities explicitly. The regression fixture uses the captured runtime header, and rejects the definition class, wrong definition offset and another generator. The user confirmed platforms appeared after this repair, then completed both rebuilt traversal connections as recorded below.

## Forest endpoint alignment repair

The live sensor repair exposed a separate route error. Generic Forest A prefab defaults had been used as mission endpoints. The first route ended at west row1, one 135m row north of the fixed Past entrance. Changing the native command to west row0 regenerated the path in PID13124, and the user cleared it and confirmed the connection works. The second route now starts at that same west row0 and targets east row0/height0, matching the authored Future corridor side and elevation. The user also cleared this rebuilt route and confirmed that it connects. Both endpoint fixes are live-accepted; the new installed combination still requires a fresh run. The earlier south-row2 target faced the wrong edge.

The native endpoint booleans remain entrance-only: they initially open a gate, not extend its geometry. Native encounters and gate clearing still control traversal. Evidence, captures and the reversible live row repair are in `build/coo/beyond-infinity-endpoint-live-20260909`.

## Combined Well queue and actor recovery patch

The user requested immediate Well animations on their native crossings while prior conversations continue. The shipped Well and reflection graphs now queue bank rows6,12,13,14,9,10,15,17,19 in that story order. Queue acceptance advances their dependencies; actual native submission and the bank duration still serialize playback. Repeated crossings cannot enqueue a row twice. The queue survives the Well-to-reflections phase handoff.

The six selected Well Scene instances retain their native cast, animation, effects, and event inputs. Their instance-local speech-start callbacks are detached using the same absent-handle operation already verified live for the lower upper-Well reflection. The package verifier follows completion edges transitively for all eight distinct speech nodes: downstream nodes contain only speech and delay actions, never animation or actor actions. No package graph or shared bank is modified. Lower and higher upper-Well actors both animate; row17 enters the queue once.

The Precipice run-up animations also follow the native volume without waiting for the Well backlog. The final circle conversation starts after row19 has finished, so its native row24 ("Where's the real Osiris?") cannot overlap the preceding exchange. Native row26 and the actual overlook crossing still release Behold; authenticated native row23 completion still gates the first Forest. Later Forest visits cannot replay the retired reveal.

A focused regression drives the shipped Well and reflections while withholding the first scene's voice acknowledgement. All six scene requests and the Precipice run-up become active; the ordered nine-row voice queue subsequently drains once, and the final reveal remains gated until then. These offline checks establish scheduling and ownership, not new in-game visual acceptance.

The user's Future disappearance report is now localized just before "But that was before you." Earlier actor diagnostics read the wrong parameter offset. The corrected main/hold parameter offset is270, with its salted actor reference at290. Native child27 has an Osiris fallback entity80EC1113; child17 has no fallback despite binding the same root cast. The combined patch adds guarded recovery for an absent actor on the first observed main-child handoff, using the native spawn, binding, ownership and cleanup path. Existing actors and later authored disappearance remain untouched. This recovery requires fresh live confirmation and does not claim the prior logs prove the missing-cast hypothesis.
