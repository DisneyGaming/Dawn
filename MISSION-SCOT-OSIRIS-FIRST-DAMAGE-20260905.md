# Current candidate: 5CEC908E � Crown health callback, 2026-09-06

Installed and hash verified after confirming D2 closed. Previous DLL backed up in candidate evidence/before-install. Await next live run on 5CEC908E; do not launch automatically.

Candidate: build/omega-full-20260905/candidate-20260906-005800
Build: 5CEC908EAB8141DB5E41905CB5299A68092430E714B99862267234BD3CC7F36D
DLL: 22E309FAF15E1FCA59FFB98B2FD53CAA9121F9CD5A6BAA2C636B45FC5E4D0425
Source: 8AE5438773855D0C81978775E3D540148F7471784ED2F6D20DA47F08325884C7
1460 frozen files; working source matches. Release zero warnings/errors; all18 Debug/Release regression runs passed. Native suite6603/config. Install using install_candidate.ps1 after checking D2 closed. No FPS changes, health writes, injected gameplay calls, effect replay, new hooks or forced movement.

Health monitoring moved from AB6600 member hook to validated observe_mission_crown after real graph_update. Three-cycle tests now enter graph_update to sample baseline, ignore foreign graph, process actual downward crossing and native body checkpoint. Existing phase/token/typed owner/finite health checks preserved. Original CD6C20 emulated from copied live pages returns body1 and eye1; original AB27C0 returns enabled authority with matching revision. See eye-damage-audit-20260906.md and health-emulation-56596-1788670357 / member-authority-emulation-56596-1788670625 for proof. No missing-health-log scheduling hypothesis should be reported as conclusively identified old guard failure.

Latest user missed the shot. Previous run proved three frame-zero effects reached native builders, then opening->eye loop and no recover. It did not prove a landed hit or physical vulnerability. Next acceptance: initial eye_health_sample, landed shot yielding eye decrease, one threshold crossing, native recovery/death, and checkpoint+animation completion. If still immune, inspect downstream damage-ping graph and regional damage eligibility separately; do not replay already accepted effects.

---

# Osiris rescue and first damage phase — 5 September 2026

User authorized fixing the visible Osiris rescue and first charge/damage phase, preserving the documented reconstruction. No new agents. The original retail mission controller remains unrecovered; the external D:/Sunrise-port source is unavailable.

## Live failure captured

Read-only PID 44932 capture: `build/omega-full-20260905/rescue-live-44932-1788652747/`.

- The first three Fallen waves completed. Native deletion event `1D3CF86F` was accepted at t366562.
- Osiris `99BD2FEB/1/0`, definition `80F4799D`: decoded generation 2, request count 1; live generation 0, request count 0. All 24 cataloged rescue sources use runtime class `8080948F` and **definition offset 878**, unlike the enemy spawners' 728.
- Intro Scene `99BD2FEB/43/9`, definition `80F479BF`: decoded generation 2 and five cast references; live generation/processed generation 0, no selector, completion byte 1. Runtime class is **80806266**; **80806382 is the source-reference metadata at +164**, not the runtime class.
- First ring cores BF06 slots 28–30 have generation-2 preparation in decoded storage, absent from live component state. Charge/sink devices have the same missed delivery.
- Dialogue row 14 reached native processed generation 1 in this run. Rows 15/16 remain unrequested because they require the accepted rescue-ready receipt. No direct audible-playback claim is made.

## Implementation

- `omega_rescue_delivery.h/.inl`: exact cataloged NPC source delivery through original 4E8FB0, using a stable current decoded 80807EC9 body. Verify complete native template, source/object identity, generation, registry, area, and Scene request. One delivery claim per cast source/run. No factory calls, synthesized placement, or additional actors.
- `omega_cannon_delivery.h/.inl`: extend existing bounded discovery/delivery to 40 transit sources, 18 gates, and 11 cataloged Scenes, alongside the existing 12 chase devices. Full member references and offsets are revalidated. Route activation uses the existing transit authority policy.
- Scene delivery uses original **B41330**, which reads the native decoded body, starts its authored selector, and deduplicates retained external events. Add its unhooked signature to the reveal bindings; shared hook ownership is unchanged.
- `omega_mission_rescue.inl`: correct the Scene runtime-class filter. Continue requiring the exact active blocking child and entrance event, not the completion byte alone.
- `omega_mission_arc.inl`: charge/sink source ownership now uses the typed member reference and native owner resolver, not source +24. Actual carry and consumed-sink callbacks still gate pickup and dunk.
- Native named damage sequence, eye downward crossing, and body checkpoint remain the existing reconstructed mission sequence. No health writes, timer-driven damage, or proximity-based dunk.

## Verification and live limit

Candidate: `build/omega-full-20260905/candidate-20260905-200551`, build **C8ACB7BE**. Consult its manifest for installation state, hashes, and full test results.

- Captured NPC/Scene/ring bodies are preserved in `Sunrise/unit/fixtures/omega_rescue`.
- Rescue/Arc tests: 3,345 checks each Debug/Release, including captured layout regressions, body mutation rejection, and publication-versus-delivery validation for all transit lifecycles.
- Native graph tests: 5,652 checks each Debug/Release. New fixture executes original B41330 instructions against captured Scene storage, verifying one start and one retained release event. Selector creation/graph consumption remain modeled boundaries.
- Broader candidate runner: `build/omega-full-20260905/build_rescue_phase.py`.

**Visible Osiris, audible rescue lines, physical charge route, pickup/dunk, and first live damage phase still require game acceptance.** The golden bubble alone is not evidence that Osiris's Scene started. Inspect `rescue_source_delivery`, `rescue_scene_delivery`, `rescue_ready`, `transit_prepared`, `arc_source`, `arc_pickup`, `arc_dunk`, and named-sequence/health receipts on the next run.


## Follow-up: exact authored rescue sequence (PID 52852)

C8ACB7BE visibly spawned Osiris (user screenshot). All five first-rescue NPC sources adopted current authority before Scene 9. Entrance event 63A4F800 and live E00 holding timeline were captured, but no rescue-ready receipt was accepted. Root runtime class is **80806384**, definition offset **8BB8**, state byte **2**. E00 is timeline runtime **808084E9**, definition offset **D68**, with no selector-state requirement. The previous observer incorrectly required both to be selector definition class 808063A7 at offset 90/state 1. Other root definition offsets are 1B28 (second) and 89B8 (final), proven by reciprocal packaged headers.

The deletion start at t314969 preceded Scene start at t319172: authority publication waited for the five-second keepalive. Ghost row 14 dispatched; rows 15/16 were never requested. Row 14's actual audibility remains unproven and was reported silent by the user.

Follow-up implementation:

- Validate reciprocal packaged runtime/definition headers, actual running root, typed live E00 timeline, retained entrance event and fresh weak ownership. Native Scene still owns entrance, echoes, holding, transforms and departure.
- Receipt `rescue_started` requests challenge row 15 (25/30 on subsequent rescues). Receipt `rescue_ready` independently requests holding row 16/26 and permits the existing charge route only with deletion-hold and transit readiness. Preserve authored exchange intervals and one native dispatch per cue.
- Before manually reconciling a Scene, verify every cataloged cast source has current adopted authority and a fresh typed owner reference. No actor factory, manual offset, duplicate body or animation replacement.
- Per-link mission revision/cue delivery wakes promptly, bounded to one update per 100ms; unchanged state sends no additional traffic. Failed staging does not acknowledge a revision. Dialogue interval expiry can wake publication without a new gameplay event.
- Fix the read-only dialogue diagnostic's missing table-directory indirection and log actual bank/selector on existing native dispatch. This is evidence of dispatch, not sound output, and never makes an additional audio call.

New replay fixture combines the exact live root and 41 node prefixes with the compiled topology for uncaptured intermediate parent nodes. Its provenance is recorded in `Sunrise/unit/fixtures/omega_rescue/README.md`. Small fake fixtures alone had concealed the original class/layout mistake.

Candidate/build/test/install status: see `build/omega-full-20260905/candidate-20260905-202422/candidate-manifest.json`. The first live charge/dunk/damage acceptance and audible dialogue still need a fresh run; never label offline tests as live validation.


## Live acceptance observation: 4AECEE48 / PID 58740

Read-only inspection requested by user. Captures: `rescue-live-58740-1788654741` and `rescue-graph-58740-1788654749` under build/omega-full-20260905.

- Correct installed build confirmed by log identity.
- Native deletion starts t304203. All five NPCs adopt by t304641. Scene starts t304688 (485 ms later).
- `rescue_started` at t304688; `rescue_ready` at t315860; deletion-hold at t316782.
- Side Scenes 81/82/83 start at t319750. Arc charge and sink sources bind live generation 3 at t319844. Native bodies match decoded authority. Charge objective 85A8F583 appears. No pickup/dunk receipt through t339782.
- Native audio dispatch now has exact bank80F1FD07/selector: row14 94E09524 (304625), row15 0294D229 (314688), row16 B8CE809F (324719), row18 CB7F171D (334750), row21 C645267E (339782). Audio heard remains user verification.
- **Timing fix NOT fully effective:** updates/voice dispatch still align with five-second keepalive. Challenge and hold are ~10 seconds apart instead of desired6.4. Publication change must not be described as proven resolved. opening_host_ready gating/caller cadence not yet diagnosed; no new code edits in this inspection.
- Foreground render samples ~137�154 FPS. An active-to-partial suspend log preceded temporary30FPS; then foreground samples recovered. No new performance fix applied.


## Current active task: Arc route missing after successful Osiris (PID58740, game subsequently closed)

User confirmed Osiris sequence and audible dialogue ALL WORKED. Now reports missing native platform/dunk location and cannot reach eye. User can physically carry the charge. No arc_pickup or arc_dunk log through game shutdown t612625. Thus host state never left route; documented bridge/portal policy waits for chargePickedUp. Do not bypass pickup or force geometry on speculation.

Read docs ARC-CHARGE.md and CROWN-TRANSIT.md; all native preparation, source adoption and Source-to-entity identities exist. BF06 bridge24/25 and portal17 have generation2 inactive while charge18/sink20 have generation3 active. Source18 entity39FAA363, Source20 entity41FAA24E. Live carry runtime80F66667/80804221/+598 at10F9E683 (nested in world+4C group45F9E6E9 at+1230). Sink80F6666E/80804FB2/+388 at4DF9E834 (group57F9E39E at+20). Header classes/offsets match production checks. Dynamic component handle bucket975 has stride32, unlike activity source bucket968 stride16. Existing source-only scanner excluded dynamic components; new capture_arc_live.py supports both and saves native holder components.

Carry was observed as state0 and state1 with holder context5661F58057408080C8040000000000005561F58000000000A0ECF97B633E80800000000000000000. No user-confirmed held-state snapshot obtained before game closed. DO NOT equate state1 with actual accepted pickup without verifying native inventory/holder ownership. Imported docs expect state3; D97B40 really checks3 but that alone does not establish handling of inventory-held state1. Need capture while user confirms holding, including all shared-item instances in case native inventory duplicates/transfers the pickup.

Read-only scripts in build/omega-full-20260905: capture_arc_live.py (fresh PID/base, scans current logged member/character buckets and saves matching exact transit/carry/sink/controller tags, now32-stride capable), capture_arc_members.py and read_arc_checks.py contain OLD hardcoded handles and must not run unchanged against a fresh process. Captures arc-live-58740-1788654900, arc-members-58740-1788654913 and arc-entities-58740-1788654885. No source implementation edits/build/install in this Arc investigation; only capture scripts and notes.

Asked user to relaunch, pick up charge and leave game open while holding. Need their readiness plus fresh PID/base. Earlier user said 'ok gonna do it; platform does not exist either', then game closed. Preserve their successful Osiris verification and keep agents off. Current installed build4AECEE48.


### User correction: pre-pickup platform geometry

User supplied current-build and retail screenshots: the retail golden platform/runway materializes during the cannon route while the player still holds a gun, BEFORE charge pickup. Requested reading the damage-phase MD. This corrects prior overly broad explanation that missing pickup alone explains all absent geometry. CROWN-TRANSIT identifies left runway/end BF06 type4 slots24/25, gates26/27; OMEGA-FULL-ENCOUNTER-STATE-CONTRACT line56 and transit doc line83 describe reconstructed pickup->bridge/portal policy, not verified retail ordering. Current omega_transit_authority.h couples Role::bridge and Role::portal to chargePickedUp. Need distinguish pre-pickup route geometry from post-pickup progression and verify which exact authored placement matches the screenshot; do not claim identified mesh purely from screenshot or that docs proved cannon-time trigger. CROWN-TRANSIT line85 explicitly left materialization unverified. No source edit in this documentation review.

## Arc runway and user-confirmed held-state fix (DA181324 candidate)

Latest user authorization: implement the documented route through Arc dunk and eye damage. Keep agents off. Osiris rescue visuals/animations/dialogue are user-accepted on 4AECEE48; preserve them.

Read-only held capture `build/omega-full-20260905/arc-live-48516-1788655743`, saved immediately after user said they had picked up the charge:
- Native item 46FAA21A, carry component18F9E57B, prefix80F66667/80804221/+598, state **1**.
- Holder context80F56156/80804057/+4C8, interface80803E63, full owner7FF9E41B, offset0.
- Resolved inventory header80F56156/80803E64/+468 has self7FF9E41B and player1DFAA259.
- Item world record+C matches46FAA21A; native attachment parent+3C is1DFAA259. Player world+C matches1DFAA259. This is independent native attachment corroboration of the user-confirmed pickup.
- Source/sink remained bound; sink01FAA211/controller19F9E613 had no requested or consumed interaction. This does not prove its prompt is broken independently of route accessibility.

Cause established: observer only accepted state3 (based on the imported ARC-CHARGE.md interpretation of D90A70/D97B40). This actual inventory-held charge is state1, so no pickup receipt opened the post-pickup portal. The native state3 branches are real but do not cover this observed inventory case.

Implementation:
- Bridge sources and their gates activate at chargeEnabled (rescue route), before pickup, and remain through the dunk until the next cycle. Portal eligibility still follows accepted pickup. Applies all3 authored cycles. This corrects the reconstructed docs' late bridge policy using the user's retail screenshot; geometry materialization still needs a new live test.
- Added state1 observation requiring exact shared item/source generation and salt, full inventory owner identity and typed context, inventory class/asset/offset, and agreement with independent native attachment player plus full live world identity. Retained state3 path with full player validation.
- Existing100ms bounded device pump reconciles committed carry state once after source adoption. It performs no pickup request, native carry transition, teleport, invented event, or memory write. Covers callbacks arriving before source/holder joins. Native drop and original consumed-request dunk remain authoritative.
- Existing native dunk -> shield graph -> named eye-refill sequence -> observed exposed eye chain retained. No forced health or damage changes.
- Added captured binary fixtures and tests for correct inventory pickup, false/stale owners, wrong attachment/player, rejected sources, deferred source readiness, drops, duplicates and native dunk consumption. Added all-cycle pre-pickup bridge lifetime tests and bounded observer cadence coverage.

Working source now frozen in `candidate-20260905-205517`, buildDA1813243867A1E121AD798F76BEFDCA893A30DE7C85F2997C9B1A0568D40ACD,1432 files, sourceSHA0CBA643EAE116415B248BD813FEDE1F3725857BE51481EE73CF4A1C8B811627F. Full verification/install in progress when this note was written. Earlier205305 candidate passed fullDLL compile but its boss-runtime test harness lacked the new observer stub; superseded.205428 snapshot superseded before building. Do not install either earlier candidate.

Remaining live acceptance: native runway visible/solid during launch route; arc_pickup state1; reachable native dunk prompt and arc_dunk; native shield break and exposed/damageable eye. The user closed PID48516 after capture; no automatic game close performed.

### DA181324 installed

Final frozen Release build succeeded with 0 warnings and 0 errors. All18 Debug/Release regression runs passed, including4254 rescue/Arc checks per configuration,5687 native boss/runtime checks,18392 mission-state checks, and the existing FPS/native-readable/roster/hook regression checks. Working-source canonical SHA exactly matches the frozen manifest.

Installed with D2 closed and previous4AECEE48 DLL backed up at candidate-20260905-205517/evidence/before-install/steam_api64.dll. Installed SHA256 BAB4F3FC58EA57E12CD1291E932BDC99C7FD9654F6FF300E7DD7E08C3B7B0E1D verified. BuildID DA1813243867A1E121AD798F76BEFDCA893A30DE7C85F2997C9B1A0568D40ACD. Game was not launched or closed by the agent. Live Arc runway/dunk/eye acceptance remains pending; next step is the user's replay using the existing launcher and fresh live log capture. Do not call eye damage or geometry live-confirmed from the regression tests alone.

## DA181324 live failure: platform still absent (PID38108)

User asked for live/log inspection only. Read-only capture `arc-live-38108-1788656675` and `platform-live-38108-1788656704`.

- Correct DA181324 DLL/build confirmed by current startup identity.
- rescue_ready t346672; platform source/gate native_apply adopted=1 at t350422.
- BF06 source24 handle79F900B5 and source25 handle03F900B6 are generation3, active1. Created native entities3AFAA1F9/2DFAA24C exist with exact world+C identities.
- Gates26/27 handles09F900B7/5CF900B8 have position0 revision1, power1 revision-1, lock0 revision-1; cached applied gate channels agree. Gate target associations point at exact created entities and native device components62F9E782/24F9E5D6.
- Both native80C22861/80803910/+A78 device instances have position current+370=0, target+37C=0, interpolation+374=0, changed+620=0, delay+628=0.
- Original DF7FF0 (DF81A0 onwards) compares current/target and skips movement when equal/no delay. DF1F70 runs direction-specific callbacks; documented decreasing path invokes phase_in. Thus sources and gate delivery are working, but the current0-to-0 policy leaves no position transition to trigger platform phase-in. Need identify authored initialization/activation transition instead of assuming spawning at0 materializes; no synthetic pulse or live memory write performed.
- Native bridge/end phase_in80BFD0FD and phase_out80BFD0FE components are instantiated and captured. This does not prove their effects played.
- Still NO arc_pickup/arc_dunk receipt in current log at inspection. Captured item0FFAA1FD component3DF9E49B state1, inventory14F9ECA0/player4AFAA3C4. Do not claim the new pickup observer is live accepted; its remaining failed qualification/poll path is not isolated yet.
- No production edits/build/install during this inspection. DA181324 remains installed. Earlier timing-only platform change was insufficient for visible materialization.

## Platform direction correction — native execution proof (2026-09-05)

User authorized fixing materialization first, leaving Arc deposit investigation until afterward. D2 is currently closed. Agents remain off.

**Correction to previous explanation and imported docs:** CROWN-TRANSIT.md reversed the platform channel directions. Original DF1F70 with actual float constants and captured 80C22861 position-channel links starts80BFD0FD phase_in when position INCREASES0->1. It starts80BFD0FE phase_out when position DECREASES1->0. Previous note claiming phase_in needed a decreasing transition was incorrect.

Evidence: position channel begins at device+350 (original DF7FF0). Positive delta target-current uses channel+68/70 callback, captured owner19F9E786/offset0 at device+3B8 -> phase_in80BFD0FD. Negative delta uses channel+B8/C0, captured owner44F9E833/offset0 at device+408 -> phase_out80BFD0FE. Static interpretation is now independently checked by executing original DF1F70 machine code with captured references in a private regression process. Actual effect rendering is stubbed; native direction, interpolation, request target and completion/deduplication execute unchanged. Debug/Release native tests passed before final build.

Package-reader verification: all7 Role::bridge source entities80F4AD97,80F4ADB5,80F4ADD3,80F4ADF1,80F4ACF1,80F4AD42,80F4AD29 include the same80C22861 device at entity offsetE4.

Production edits: omega_transit_authority.h writes position1 when bridge active,0 dormant/retired; omega_cannon_delivery.h accepts exactly the same corrected authority. Native interpolation and phase graph execution remain unchanged. No pulse, direct native effect invocation, runtime memory modification, new hooks, or pickup/deposit changes. Platforms still activate at rescue route readiness before pickup and persist through dunk until next cycle.

New fixtures in Sunrise/unit/fixtures/omega_platform and original-instruction test omega_platform_native_fixture.inl. Production wire tests cover all7 bridge gates at dormant, pre-pickup route, holding, dunk, and retired states. Delivery tests accept corrected1 active/0 retired and reject old0 active bodies.

Final source frozen1437files under candidate-20260905-211138, sourceSHAD6D7C72AD20FC9BC18C113CBD0D39981CBCFBDEC59800FB2ACB35F8F4F91FC8A. Full build/regression/install pending at this note. Visible/solid native platform and usable native dunk remain live-unverified; do not claim these from instruction tests.

### 30CEFB66 installed — materialization fix only

Candidate211138 completed Release build with0warnings/0errors; all18Debug/Release regressions passed. Native runtime5754checks/config includes actualDF1F70 direction tests and7gate delivery checks; ending/transit7403checks/config includes production147-bit bridge lifecycle values. Working source exactly matched frozen manifest before installation.

Installed and hash-verified with D2 closed: build30CEFB66D27A50527BC4E56A3C778CF9B3461CEE399161EF6B45359014168987, DLL00AD403653D65392FEBDB97E3B00FA8E48E46FD6400431400DA0B89ECF816A39. PriorDA181324 DLL backed up at candidate211138/evidence/before-install/steam_api64.dll (full candidate directory name above). Game was not launched or closed by the agent. Next: user replay to confirm platform visible/solid. Pickup/deposit remains unchanged and requires separate live acceptance after platform access; no claim that materialization alone fixes deposit.

## Platform accepted; Arc pickup/portal lookup failure isolated (PID40148)

User confirmed platform worked on30CEFB66 and supplied screenshot carrying charge on the platform without a deposit prompt. Preserve the materialization fix as user-accepted.

Read-only live capture arc-live-40148-1788657758: source41F900AF -> item3EFAA242; source03F900B1 -> sink6AFAA234, bothgeneration3. Carry58F9E7EE state1, typed inventory7CF9ECA0/player71FAA3C4 agrees with world attachment. Sink controller51F9E662 active0, requested0, consumed0. No arc_pickup/arc_dunk. Native world strideE0.

PDB-backed read-only globals: missionArcSources exact matches both live sources, missionArcHeld empty, missionArcHoldingfalse, missionruntime state phase7/route. So the state1 fix alone had not repaired the independent component-lookup failure.

Root cause now proven: mission_arc_component passed declaration IDs80803E70 and80804FB0 to native557470. Original lookup resolves query type hash through5980A0/5911D0/9EBB80/9EBF80. Those hashes do not exist in these actual entity descriptor interface tables. Item registered interface80803F6A (hashC302A192) returns group+1230 and declaration80803E70 atreference+1C. All three sink entities register80809658(hash6A42C187), returninggroup+20 and declaration80804FB0 atreference+1C. Reading the returned metadata field as a query-interface identity was the mistake.

Production changes only: use80803F6A for polling shared carry component; use80809658 for exact sink lookup. All existing source/epoch/salt/runtime-prefix/world-player checks retained. No native predicate override, fake pickup, deposit activation, item consumption, teleport or HP mutation. Accepted pickup enables existing native portal route; actual deposit still requires original consumed-request receipt.

New fixture omega_arc_interface_native_fixture.inl executes the original557470 and full original11-routine dependency chain against captured live metadata/descriptors in an isolated process. Old IDs returnfalse; correct interfaces resolve the item and all3sink entries with expectedowner/offset/declaration. Only private memory layout/group linkage and cookie boundary modeled. Debug/Release5873native runtimechecks passed before final frozen build.

Source frozen1449files in candidate-20260905-213002, sourceSHA41C8F4C61AE8E294DE02BB14BD47C3BDA16FB9FCE8B5A7A5D01D60A150209D4D. Full verification/install pending at this note. Game may still be open; install only onceclosed. Nextliveacceptance: arc_pickup state1, native portal creation/transport, actual dunk prompt and arc_dunk, shield/eye.

### 7F8A2275 ready, waiting for game close

Frozen candidate213002 Release compile0warnings/0errors; all18regression runs passed. Working source canonical hash exactly matched frozen manifest. Build7F8A2275C6C6F3D7CB844EA8F8177096B1F2E85291F69B4B924E4DD29CAB4857; DLL56C285F97348A61B661FF74046E00D1313E456324AA75EC0BD646A7110C364BA. User asked via async tool to close D2 and report closed; process40148 was stillopen. NOT installed yet. Run install_candidate.ps1 after processgone; retain backup/hashverification. Candidate-path.txt points213002. User's prior install authorization persists; no additional permission needed beyond gameclose.

## 7F8A2275 installed after user closed D2

User authorized installation after the monitoring session showed the previous 30CEFB66 build. Verified Destiny 2 was closed, then ran install_candidate.ps1 successfully. Installed build ID: 7F8A2275C6C6F3D7CB844EA8F8177096B1F2E85291F69B4B924E4DD29CAB4857.
DLL SHA256: 56C285F97348A61B661FF74046E00D1313E456324AA75EC0BD646A7110C364BA.
Candidate: build/omega-full-20260905/candidate-20260905-213002. All 18 regression runs previously passed. Native pickup acknowledgment, portal, deposit, and eye damage still require live validation. Game was not launched automatically.

## 7F8A2275 live pickup accepted; missing endpoint presentation

Running PID51532 logged correct build 7F8A2275 and native_carried pickup at t383281, item05FAA246/player6AFAA3C4/sink74F9E518. Portal source BF06/4/17 received active native_apply at t383859. Player entered BF06/60/81 at t385437. This proves receiving-volume entry, not independently the transport method. No accepted native dunk.

User screenshot still shows no deposit chamber. Read-only capture arc-live-51532-1788659329: sink source80F47678 generation3 active1 created29FAA14B; controller80F6666E/80804FB2/+388 self74F9E518 exists, requested0/consumed0. Its active0 is the interaction state, NOT proof that source creation failed. Carry was state0 at capture (earlier pickup was accepted).

Separate endpoint source80F47675 (o_dunk_end_fx, BF06/4/19) remained generation2 active0, createdFFFFFFFF/FFFFFFFF. Other-cycle endpoint sources also dormant. Package80F47675 references entity80F58F0B at+580; this separate entity includes graph80F58F0A and the rendering/presentation component set. Sink80F44F89 has interaction80F6666E plus80C70CAE and80FEAB32 and no render component. Its interaction sequence80F44F87 references80F58EE0 and carries the path content/public_events/vex_jam/vex_jam_interactable/sequences/vex_jam_interactable.sequence. Predicate9C99BE55 remains unmodified and unproven as a prompt blocker.

The endpoint presentation was suppressed until chargeDunked by the imported reconstruction. Candidate31762B60 changes ONLY Role::endFx source activation to chargeEnabled OR chargeDunked. This creates it with the route, retains native position0/revision1 and generation3 through pickup/drop/dunk, and retires it on the next cycle. This is a correction to the imported activation policy, not a claim the original retail controller was recovered. Identification of the absent visual as this presentation is inferred from the separate source/entity/graph; actual chamber appearance and interaction remain live checks. No forced use, predicate override, teleport, item removal, health change or new hooks.

Added independent production wire lifecycle checks for all three endpoint sources/gates; existing tests also independently decode source/gate publication and validate native-delivery acceptance. Candidate build/test is in progress at candidate-20260905-215231. Installed build remains7F8A2275 until explicit installation after D2 closes.

Candidate31762B60 Release compiled successfully; all18 Debug/Release runs passed (ending/transit7601, rescue4254, native runtime5873 per configuration). Source70225C0728BE8040C2ABB9F81682222BAFE031771CD4DB77EEB9F30F1CDDAF8D matches frozen manifest; DLLB559AFB663617AC1839077EBF871127F1B79F7F4F494905F94BC680832D3A0D6. Ready for installation once D2 closes. No visual/dunk acceptance claimed.

## 31762B60 installed
User closed D2 and explicitly requested installation. install_candidate.ps1 verified process closure and installed build31762B6048B46384BB73059A72814DC3F15CB694898218EC4B981398A7526030. Installed DLL hashB559AFB663617AC1839077EBF871127F1B79F7F4F494905F94BC680832D3A0D6 verified. Prior7F8A2275 backed up under candidate-20260905-215231/evidence/before-install. All18 regression runs passed before installation. Game not launched automatically. Endpoint chamber appearance, native prompt, accepted dunk, and eye damage remain live-unverified.

## 31762B60 live: endpoint visible, no native deposit request
User screenshot shows endpoint VFX now visible, still holding charge, no dunk. User reports walking through it teleported them while retaining the charge. PID7320 correct31762B60. Log pickup t360750, portal BF06/4/17 applied t365125, receiving BF06/60/81 t366734; no arc_dunk. Read-only capture arc-live-7320-1788660239 confirms endpoint80F47675 generation3 active1 created79FAA24A; sink80F47678 generation3 active1 created1FFAA227. Sink4EF9E33C requested0 consumed0 active0. Carry77F9E461 state1, player50FAA3C4 still owns it. No code/build changes this turn.
Current portal policy is chargePickedUp, so teleport eligibility precedes actual dunk; retained charge does not mean a successful deposit. Need investigate native sink prompt/eligibility and confirm whether this particular portal should be post-dunk rather than blindly treating all portals as pickup routes. Earlier receiving-volume observation alone never proved correct retail transport sequencing. Predicate9C99BE55 not yet traced. Do not force use or claim sink state0 means missing source.

## Disable premature contact teleport; native dunk eligibility investigation

User clarified retail order: dunk, automatic teleport (not walking through a portal), then eye DPS; explicitly requested removal of pickup activation and precise native dunk restoration. Working production change: Role::portal stays inactive for the current cycle, including after pickup/dunk. Existing other-cycle retirement remains. No replacement contact teleport is introduced after dunk without identifying its native producer. Platform, endpoint visual, charge and sink source policies remain unchanged.

Read-only investigation PID7320 identified predicate registry80804D75 evaluator100BF10 ->100C000. Registry pointer decoding was reproduced arithmetically in inspect_arc_predicate.py from original A5AC00; it does not call or write the game. Evaluator enumerates player80803B37 interfaces and calls method184FD70. Live player50FAA3C4 has charge property provider80F7A6E0 on group32F9E29E/entity definition80F58EA5, and other provider80FEF337 on group43F9E468/entity definition80C1934E. Both component offsets70, definition620, method-table815B887B. Original getterC994A0 compares definition+9C: charge9C99BE55 MATCHES the sink predicate; other9CBEE071 differs. Therefore lack of the carried property is not supported as the prompt blocker. Native F30540 checks request readiness; captured unused sink should return true. No forced property or interaction flags added.

Native world positions were independently decoded read-only from original field/key routines. At the later capture the player was(-1417.477,-102.611,-39.197), sink(-1441.707,-119.976,-37.816), endpoint(-1441.707,-119.976,-37.853). This is a sample about30 units away, not proof the player was in interaction range when attempting dunk. Sink readiness and item property alone do not prove prompt range/selection/actor filtering. Need next live attempt after premature contact teleport is disabled. Successful native F36640 receipt and actual consumption still required; precise post-dunk scripted transport remains unproven.

Added original C994A0 and F30540 isolated fixture with real package definitions and PID7320 sink capture; fixture limits documented in Sunrise/unit/fixtures/omega_arc/eligibility/README.md. Updated all-three-cycle wire/lifecycle checks to forbid contact portal activation after pickup or dunk while preserving geometry. Candidate frozen221831 build/test running. No claim that dunk prompt is fixed; no new hooks, fake input, use call, health change or teleport bypass.

BFCF71EE verified: all18 regression runs passed, including5909 original native checks and7691 ending/transit checks per configuration. Source9A762878C73B66FC442E4FA8AC7B7EB551A36AF8CFDF148658CFD8650E1A7939 matches frozen source. DLLF770E47A59FCD4D9A47949085A9A7A24CEBF14D2321ECEB3E1E61C0E9030B4E6. Installation awaits game close. Native property getter and unused-sink readiness passed original instructions. Prompt usability is unresolved pending live test, not reported fixed.

## BFCF71EE installed
User closed D2 and requested installation. install_candidate.ps1 verified closure and installed buildBFCF71EE49448FE854977EFE7BEAC34E0B6889F7B8EEED797D13950523FF3889, DLL SHA256F770E47A59FCD4D9A47949085A9A7A24CEBF14D2321ECEB3E1E61C0E9030B4E6. Prior31762B60 backed up under candidate-20260905-221831/evidence/before-install. All18 regression runs passed. Game not launched automatically. Pickup-triggered contact portal disabled; deposit prompt, actual native consumption, and post-dunk scripted transport/eye damage remain live-unverified.

## Native deposit lock found; interaction override candidate

User confirmed premature teleport is gone. PID59880 remains on BFCF71EE, carrying item68FAA209 as player3BFAA3C4 at sink7AFAA22D/controller0DF9E6FE. Capture arc-live-59880-1788661758 and arc-position JSON show distance0.1919, native radius1.0, no LOS requirement, requested0/consumed0/used0. The carried item property matches. Controller+2C0 is locked1, with absent additional condition reference at+2C4. Original prompt calculation F32CD0/F32E9D rejects this combination independently of item/range readiness.

Original F32820 defaults an interaction to locked when its optional80804FB1 initialization record is absent. Original F33930 accepts a tagged80804FB8 override: payload mode1 locks, mode2 unlocks, absent extra reference leaves item predicates intact, and +280 is dirtied to recalculate prompt. The other80804FB8 consumer F37800 changes use state only when its revision advances; candidate retains revision0 and valuefalse, not a synthetic use.

This is supported by the existing native source path:9F19F0 copies the decoded8080992F body then invokes9EF680; source creation9EFFC0 also invokes9EF680. That function enumerates80809AE3 interfaces and sends body+40 throughB31960. Read-only live lookup found sink entity definition80F44F89, group3CF9E546, native methodsF37800 andF33930 in precisely that interface. No direct live call or byte patch was made.

Reflected dynamic list80809AEA contains tagged80809AE8 entries (stride60 hex). Original decoder9FA4B0 reads schema80800046 via9F9B30 (presence1 plus tag32) then reads the selected payload.80804FB8 fields are signed enum2 with bias1, reference55, signed counter32 and bool1. Thus each sink source uses375 bits instead of252; all other source widths remain252. Sink override locks dormant/retired/other cycles and unlocks the current active route/carrying cycle. Native decoded authority validation checks exact override type, mode, absent reference, zero use revision and false use state before delivery. Contact portals remain inactive.

The imported ARC-CHARGE.md explicitly specified no dynamic overrides and marked live validation pending. This change corrects that reconstruction using the recovered code and captured lock state; it is not a claim that the original retail mission controller was recovered.

New original-instruction fixture executes F32820,9FA4B0,9F9B30,F33930 against production wire for all three sinks. Reflection scalar leaf reads are modeled from the pinned descriptors. It proves default lock, tag framing, mode application, cache invalidation, idempotence and untouched use/player counters; it does not simulate native player input or the complete prompt selector. Independent full-body decode tests cover all route lifecycles, malformed override rejection and following-record alignment. Targeted Debug runs passed6234 rescue checks and6128 native checks. First candidate224110 failed a test-only shadowed-local warning; corrected before candidate224328 was frozen. Candidate E10D118E is building full Release and all18 regression runs. Installed build remainsBFCF71EE. Actual prompt, dunk, charge consumption, scripted post-dunk transport and eye DPS still require live validation. No FPS change or new hook.

E10D118E verified: Release build and all18 Debug/Release regression runs passed. Canonical working source matches D01364F06F5FF664B086ED193D3301641621F4D216EB7B6474A87FE05B216488. Build ID E10D118E0CEAC90498D2B3379DAC858376634CACF6C4F3671D54FC935F302C36; DLL SHA256 7C672AB4DFBDF7ED155DE692FFFBDA55D57C536422F10CE2E59E1B2AE05366F3. Candidate224328 is ready, NOT installed: PID59880 is still open. User must close D2 before install_candidate.ps1. No additional installation authorization is needed once the game is closed. Next live boundary is actual prompt/use and arc_dunk; do not claim eye transport is solved from lock tests alone.

## E10D118E installed
User explicitly requested installation. install_candidate.ps1 confirmed D2 was closed, backed up BFCF71EE under candidate-20260905-224328/evidence/before-install, and installed E10D118E0CEAC90498D2B3379DAC858376634CACF6C4F3671D54FC935F302C36. Installed DLL SHA256 verified as 7C672AB4DFBDF7ED155DE692FFFBDA55D57C536422F10CE2E59E1B2AE05366F3. All 18 regression runs passed before installation. Game was not launched automatically. Native deposit prompt, actual dunk/charge consumption, scripted transport, and eye DPS remain live-unverified.

## E10D118E live dunk; player-record receipt mismatch

User confirmed dunk works and requested teleport into eye DPS. PID59920/base140697712656384 is E10D118E. Native capture build/omega-full-20260905/arc-live-59920-1788663457 proves sink controller2AF9E10C/entity63FAA223 has used1/requested1/consumed1. Pickup was item63FAA1F5/player68FAA3C4, epoch13/cycle1. No arc_dunk receipt or shield command was logged.

The sink's actor weak reference is 5784AF7B/3DFEC000, a PLAYER RECORD, not world entity68FAA3C4. Native F33A90 stores this record; original4B2260 resolves the player pool at image+1F90E18, stride+1F90E20, and reads controlled entity+54. Live player record has stride1E0, identity+44=3DFEC000, controlled+54=68FAA3C4. The old observer compared the two different identity domains directly and rejected the completed use.

Working fix mission_arc_user validates original salted weak reference, exact player-pool row and full identity, controlled world entity and its full world-row identity, then rechecks the weak reference. Both pre/post-original F36640 checks use this mapping, retain exact raw player association, and still demand a newly consumed native request. Tests use distinct player/world handles and reject stale salt, reused player identity, wrong controlled entity, reused world identity and a world handle substituted for the player record.

Transport staging changes: the authored source for the current cycle becomes active only after the accepted dunk while awaiting its receiving volume; it retires on actual receiving-platform arrival. Pickup remains insufficient. route_arrival now accepts eye arrival only for the same dunker in the new post-dunk epoch. Shield action claim waits for that arrival. Position observation admits this pending, unclaimed shield stage so the arrival gate cannot deadlock itself. All three cycle lifecycles and premature/stale/wrong-player arrivals are tested.

IMPORTANT TRANSPORT LIMIT: package root placements of80F4766F/80F47715/80F478A5 are respectively(-1455.567,-93.174,-34.591),(-1525.217,-89.003,-32.736),(-1494.210,-398.155,-14.798), offset from their deposit points. All instantiate80F44FA3 and its nested chain. Root placement does not by itself establish final nested-controller position/contact coverage. Source activation alone is NOT proven to automatically teleport the player at the deposit. The next live run must inspect the newly staged source/nested controller and actual receiving-volume receipt before claiming the teleport fixed. User was explicitly told this limitation. Do not blindly force a transform or invent a successful arrival.

Native research for the remaining explicit transport request if required: DE16D0 forwards source/actor toDE17D0, which checks source role and DE2010 eligibility then queues DE1600. Queue processing nearDEC58B checks eligibility, selects destination throughDE2180 (callDEC787), then invokesDEA5C0(source,actor,destination). DEA5C0 builds native transport arguments viaDE53A0 then callsDEA690. These routines have only been disassembled, not invoked or fully validated here. DE1C40/FBF9F0 are related effect registration, not a proven standalone teleport method. Need exact live source-to-nested-controller ownership and qualified dispatch/acknowledgment before introducing a native request call. Existing automated destination-selection fixtures model actor/collision eligibility and do not prove full automatic travel.

Targeted Debug tests passed6439 rescue/Arc,8156 ending/transit and6143 native checks. Frozen candidate230733 source7805B71B0A05125EFF822948374F44F7641A9CCCCEF27EB961DB6C74874D00B2 is building Release and full18 runs. Installed E10D118E remains unchanged. No FPS fix, agents, forced use, direct item removal or player-coordinate write.

## 2D0DB518 ready for next live transport check
Candidate230733 passed Release and all18 Debug/Release regression runs. A final comment-only clarification removed an unproven claim about contact-area coverage. Candidate230930 was then frozen and Release rebuilt. The finalizer verified that the only manifest source change was comments in omega_transit_authority.h, with all non-comment lines identical; it retained the18 test results with an explicit test_evidence_candidate and test_reuse_proof in the final manifest. No additional semantic changes occurred.
Final build2D0DB518F0601347736D6EBB539A28013CA6243C7EF5835F367ADEE5E4EA4EB1, DLL77F0D55F5C3EC46C65285A2416EFBE8BC05799BE7671BC13D1D39685E12DC127. Canonical working source verified590D6EED80626D8DBF83B7E7AF06FFB8CE9B0455DB84258F4AD442FB9ADCEC14. NOT installed: D2 PID59920 remains open. Request game closure, then install_candidate.ps1. Automatic teleport is still a live check; do not call it fixed from packet/state tests. After installation, ask user to dunk and remain there if no automatic move; capture native source/controller ownership, eligibility, queue, and receiving-platform arrival. No extra walking should be required for the intended retail behavior.

## 2D0DB518 installed
User confirmed closure and requested installation. Guarded install_candidate.ps1 completed successfully and verified installed build 2D0DB518F0601347736D6EBB539A28013CA6243C7EF5835F367ADEE5E4EA4EB1 and DLL SHA256 77F0D55F5C3EC46C65285A2416EFBE8BC05799BE7671BC13D1D39685E12DC127. Game was not launched automatically. Next step remains live dunk-to-teleport verification; automatic travel and eye DPS are not yet proven.

## 64CBA6AA: corrected dunk player validation and transition diagnostics, installed
User authorized correcting the unsupported +44 check and preparing the next dunk-to-DPS capture. Latest saved run on 2D0DB518 registered pickup at t350609 but no arc_dunk. User supplied an external live finding that native requested=1/consumed=1/used=1 and player +44=0; this turn did not independently recapture it because D2 was closed. Original 4B2260 disassembly independently confirms player-pool +54 supplies the controlled world entity and has no +44 identity comparison. Docs ARC-CHARGE.md require qualified actor ownership and real consumption, not +44 equality.

mission_arc_user removes only the unsupported +44 identity test. Original 352310 salted weak checks before/after, exact resolved player-pool membership, controlled entity +54, full world identity and before/after sink/player/request association remain required. No forced interaction, item removal, player transform or teleport dispatch was added. Tests now set +44=0 for valid dunks; also accept an unrelated nonzero +44, reject a wrong pool pointer, stale player weak, wrong controlled entity, recycled world identity and a world entity substituted for a player record. Actual native request consumption and nested drop behavior remain required in all three cycles.

New event diagnostics: arc_use_rejected records failed pre-use qualification for the current held sink; arc_use_result records post-use reason, request/consumed counters, used flag and actor association; arc_dunk remains the accepted host receipt. Diagnostics are emitted from use callbacks, not a new scan. Calls without a current held receipt are still forwarded without rejection diagnostics. eye_transport_source reports exact source authority adoption and its salted created entity at most twice per cycle/epoch (source_only; does NOT prove nested controller eligibility or a teleport request). Existing arrival milestone6 remains the native receiving-volume receipt. eye_shield_claim distinguishes accepted shield command from later named-sequence/event confirmation. Source diagnostics reuse the existing bounded device pump. No new native transport hook was added; queue/dispatch still need live inspection.

First frozen candidate233149 built Release but failed the native fixture compile because its reduced include environment omitted mission_weak. Corrected the diagnostic helper to use the existing native352310 reader directly; froze new candidate233337 and rebuilt/tested all18 configurations successfully. Rescue6496, state18632, ending8156, native6143, frame961, readable2022, roster99698 per configuration; hook/progression suites passed. Release zero warnings/errors. Working canonical source matches frozen C3CF4E8ACF69B4B477BF97659D7CA15AD09F6EC3E9EB5EEA1179A0E016C8BDA7.

Guarded installation succeeded with D2 closed. Build64CBA6AA5C8F0997FCCEB26AA1B471F0FDC00B5FAC23504D363B01B8E60E19BA, DLL SHA256 FC5D633E29B48B03AF4EBCED35FE56807B54178FC087148EE9CF64383BECCFCA. Prior2D0DB518 backed up in candidate233337/evidence/before-install. Game not launched automatically. Next: user launches same debug CMD, dunks, and leaves D2 open. Check arc_use_result/arc_dunk, eye_transport_source, arrival milestone6, eye_shield_claim, named_sequence and cycle_event. If source exists but no movement, inspect its nested native controller, actor eligibility and native request queue. Automatic dunk-to-eye travel is still unproven; do not declare solved from these tests or source adoption alone.

## 64CBA6AA live: dunk and teleport accepted; eye damage remains immune
PID37804, image140697712656384. User confirmed teleport and provided screenshot showing Immune hits at eye. Log: t395516 arc_use_result accepted=1, requested1 consumed0->1 used1, player record50FEC000 -> world21FAA242; arc_dunk epoch13. t399985 source6CF900AE adopted then t400094 entity5DFAA1F0 created. t401719 epoch14 receiving volume BF06/60/81 milestone6 accepted. t401750 eye_shield_claim arrival1; eye_refill_health C57C61EB/80F45564 confirmed native slot; shield event E9D4F854 confirmed. t401813 node3 clip80F45179; t406360 node6 clip80F4518E (eye loop). Thus no longer a missing dunk/arrival/shield-dispatch blocker. The native resource slot confirms queuing, not every downstream script/health effect. Physical eye damage remains unaccepted; exact immunity check has NOT been identified.
Read-only snapshot saved build/omega-full-20260905/eye-live-37804-1788666352: character1BF9E931, animation55F3A005, health01F9E95B, script controller79F9E913, saved log/capture.json/bins. Health typed prefix815B5A40/80804B8A/F98 and entity34FAA268 match; flags+338=00FF0000 (do not infer immunity-bit semantics without analysis). No game writes, source changes or installation in this diagnostic turn. Next investigate native damage eligibility, eye region binding and authored health-script effects after shield event. Docs BOSS-HEALTH and COMBAT-ACTIONS explicitly leave physical immunity/refill effects as live validation; health outputs BE57CE2E/DD46E308 are not immunity controls.

## 26906BA2 diagnostic candidate ready, 6 September 2026; NOT installed
User authorized work on eye immunity and asked whether game could close. Told yes after capture. Native investigation did not prove a safe immunity fix. Saved detailed evidence in build/omega-full-20260905/eye-damage-audit-20260906.md. While PID37804 remained live, captured all boss-owned bucket975 components and actual scalar array in eye-live-37804-1788666352. Body/eye/alive1, visual inputs21..24=0. Seven named-resource slots free at capture, only refill retained. No live writes or injected calls.

Opening clip80F45179 has three frame0 kind3 resources: cast-end80F453BF at7580, illum-stop80F45568 at75A8, damage-ping80F4547E at75D0. Damage-ping graph80F4547D includes region720A5B5B in compiled descriptors, but precise downstream eligibility operation not established. Native dispatcher C71C30 is shared by named C66590->C6AF40 and animation-event caller1030603. Verified ABI bool(channel,const descriptor12,bool retain,int context,int secondary). Native E98B30 resource admission must succeed for a true return; downstream graph execution is not thereby proven.

New omega_eye_resource_trace.inl detours original C71C30 as an observer. For five exact resources after real dunk in cycles1..3, validates typed character/actor/world/animation channel, records at most4/resource/action epoch, calls original once with all5 arguments untouched, preserves bool return, rechecks token/channel. Logs stage=eye_resource_dispatch with resource/name/kind/context/secondary/native_result/current/ordinal, receipt=dispatch_only. Never invokes a resource itself, changes immunity/HP or supplies encounter receipts. Install revision23 has13 reveal hooks,53 prefix bindings; existing support29+reveal13+nav1=43 hooks. Atomic rollback tests cover13 positions. Runtime fixture covers original success/failure, full argument preservation, unrelated-resource/wrong-owner filtering, cap without suppressing original calls, call gate and unchanged mission state.

Candidate235931 built but fixture failed because a preceding unrelated native fixture rebound the original character handle to another allocation. Moved observer test before that destructive fixture; preserved production ownership checks. Fresh candidate20260906-000306 built Release and passed all18 Debug/Release runs. Native6347 checks/config; rescue6496,state18632,ending8156,frame961,readable2022,roster99698 plus hook/progression. Canonical source0646B3F7CEBD5FEBE959A29275541B5AAAADE5224C5F19CBEC1DC95F8A705915 matches frozen1458 files.

Candidate26906BA22EF1305ADEFB9A03515E92B6F62D75DA2C9FC4DF0F0F4D3311F9359B, DLL940054C3B7A03C6658CB81986F2C7CB4F0A48BE44D99E904117A8003C639AE9C. Manifest finalized with observation-only live limits. NOT INSTALLED: PID37804 remains open on64CBA6AA. Async question asks user to close and confirm; installation already authorized, only process closure required. Next install_candidate.ps1 once closed, then same debug CMD to dunk/teleport/eye opening. Inspect new eye_resource_dispatch records alongside accepted arrival/shield/event/node receipts. No frame0-missing or immunity cause claim yet. If resource dispatch absent inspect clip event production; false inspect resource admission; true inspect graph execution and health-region eligibility. Do not replay possibly successful effects speculatively.
## 26906BA2 installed after user closure confirmation

User confirmed D2 closed. Guarded install_candidate.ps1 succeeded: build26906BA22EF1305ADEFB9A03515E92B6F62D75DA2C9FC4DF0F0F4D3311F9359B; installed DLL SHA256940054C3B7A03C6658CB81986F2C7CB4F0A48BE44D99E904117A8003C639AE9C verified. Prior64CBA6AA backed up in candidate20260906-000306/evidence/before-install. Game not launched automatically. This remains a diagnostic build, not an immunity fix. Next run: launch the same debug CMD, dunk, teleport, shoot the eye, and inspect eye_resource_dispatch alongside arrival/shield receipts.
## 1A341CD5 ready: correct clip-event diagnostic, not installed

User authorized investigating/fixing the eye after reporting it opens then quickly closes. Read newest26906BA2 log: real dunk518234, eye arrival520891, shield/refill520906, opening node3/80F45179 at520922, eye loop node6/80F4518E at525578. No later recovery event/node. Saved read-only boss/component snapshot and log under build/omega-full-20260905/eye-events-50740-1788668732. Current live PID50740, image base140697712656384; refresh both before any future capture.

IMPORTANT CORRECTION: prior missing-C71C30 records do NOT prove missing clip events. Actual raw clip kind3 path is F50280 -> F4E190 event cursor -> F49BC0 ->1050570 ->104D2D0 ->104DA00. Original104D2D0 switch table104D9E8 maps kind3 to104D7C4. Handler104DA00 uses event+18 resource and component builder4B25F0/seed4B4640/construction56DE00, or effect creation for the other native resource class. 10304F0 uses C71C30 but is not the raw clip-event producer. Told user explicitly the earlier diagnostic watched another route. No native event replay or immunity change is justified yet.

Added omega_eye_clip_trace.inl: typed boss biped/world/bank guard, three exact resource filters after qualified dunk, max4 dispatch records/resource/action token. Original104DA00 forwarded exactly once with four arguments unchanged. Thread-local scoped original4B25F0 observer forwards arguments and bool unchanged and records only matching resource builder calls/results. Builder acceptance does NOT prove completed construction/graph execution. Added existing-F4E660 cursor/context samples, max3 opening+3 eye per token; existing native health sampler logs max8 initial/changed eye values. No extra getter calls, no health writes, no new gameplay receipts. Existing C71C30 diagnostic retained for named refill. Install revision24 is15 reveal hooks;55 pinned signatures, support29+reveal15+nav1=45 hooks;15 rollback positions.

Frozen candidate20260906-003226,1460 canonical files, source7FE2FC2D0FFB7807813D0DD28FB8030F455E8D2BC4BDF53B14378F261A433CFA. Build1A341CD58026925DB4717BBDDD9F105FD0068ACE74A211FC16585AAC3EA4CF7E; DLL9C597CA229DB7E205F98FF259C23C5F58F4C38C41CD3B54C8E85E4625A14566D. Release zero warnings/errors; all18 Debug/Release regression runs passed. Native6594, rescue6496,state18632,ending8156,frame961,readable2022,roster99698 per config; hook/progression passed. Working source equals frozen; manifest explicitly corrected to diagnostic-only scope after finalize helper's outdated default text.

NOT INSTALLED at this checkpoint: current installed26906BA2 remains live. Asked user to close D2 for installation via async question. Installation authorized; process closure required. Then same debug CMD, dunk/teleport/shoot eye and leave game open. Inspect eye_clip_cursor (events consumed and context), eye_clip_dispatch (three effects plus builder acceptance), eye_health_sample, cycle_event/node and existing refill. Do not claim a gameplay fix from these diagnostics. Detailed corrected route disassembly and evidence are in build/omega-full-20260905/eye-damage-audit-20260906.md.

## 1A341CD5 installed
User authorized proceeding. D2 was closed. Guarded install succeeded; installed build1A341CD58026925DB4717BBDDD9F105FD0068ACE74A211FC16585AAC3EA4CF7E, DLL SHA2569C597CA229DB7E205F98FF259C23C5F58F4C38C41CD3B54C8E85E4625A14566D verified. Previous26906BA2 backed up by installer. Game not launched. Next run must capture actual clip dispatch, builder acceptance, event cursor/context and eye health. Diagnostic only; immunity fix remains unproven.
