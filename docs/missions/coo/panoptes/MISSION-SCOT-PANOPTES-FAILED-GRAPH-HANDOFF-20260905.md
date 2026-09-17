# Panoptes native graph failure — handoff, 2026-09-05

## Current user request and status

The user requested native animation graph implementation through the Panoptes intro, followed by a manual test before encounter/combat work. Revision 9 was implemented and installed. The user tested it and reported: **“the cutscene does not play nothing plays properly....”**

The latest request is **“just give me a handoff.”** Investigation stopped at that request. No repair, rollback, rebuild or installation was performed after this failed test. Do not describe revision 9 as working because its offline tests passed.

## Confirmed failure from the latest log

Preserved log: `C:/Destiny 2 Development/build/scot-panoptes-native-graph-20260905/dawn.revision9-failed.log`.

Relevant receipts in the original log:

- Line 85, `t=17718`: revision 9 installed its five hooks successfully.
- Line 12332, `t=165093`: doorway reached, run 1, bubble 14, route `crown_entrance`, position `-1490.08,467.60,-16.84`, `boss_door=1`.
- Line 12335, `t=165109`: `member_wait reason=awaiting_member_authority_actor`.
- Line 12336, same time: `character_bound actor=44F4200A character=27F9EA3F source=80F6690B/738`.
- Line 12337: boss entity definition `80F45253` constructed, result `51FAA2E0`.
- Line 12338: **`member_wait reason=character_animation_binding_not_ready`**.
- Line 12410, `t=173093`: player reaches route `reveal`, position `-1490.78,408.66,-27.23`.
- Later boundary receipts still show `boss_seen=0 intro_started=0`.
- No `graph_queue`, `graph_node`, or `summon_condition` receipt was found.

**Established:** member activation spawned Panoptes and the character initializer was observed. The bridge stopped before submitting the native graph command. The cutscene also remained blocked because it now requires a native fly-node receipt.

**Not established:** which individual check inside `character_owner(...)` or `named_pair(...)` failed. They share the same coarse `character_animation_binding_not_ready` message. Do not claim a specific field is wrong without additional evidence.

`boss_seen=0` does not prove absence of an actor: this flag is set only when the bridge claims its graph queue, and that never happened.

## Installed build and rollback

Workspace: `C:/Destiny 2 Development`.

Installed DLL SHA-256:

`B6FFE0CF875F022C01F4244300CC35B15C7913D828787A43561806F738D5715A`

Candidate: `build/scot-panoptes-native-graph-20260905/candidate-20260905-122048`.

Frozen source SHA-256:

`FF6009588FF8A1E66AFFB751B17580AA771C98674A5202C7316D8752143544D0`

Settings SHA-256, unchanged:

`B2C0AD3655561F85EC08D3D23D3BBBC53249B85F4BBE7A09D329D6BA6FEAA8D4`

Backup: `build/scot-panoptes-native-graph-20260905/install-backup-20260905-122252`.

The previous DLL is `steam_api64.before.dll` in that backup, SHA-256:

`D874D6B54601536F054D925ACD55B417CC39464EE471E212DD18E7C103B5E614`

Revision 8 had user-confirmed intro motion and working doorway timing. Summon/VFX remained incomplete. Its frozen candidate is `build/scot-panoptes-completion-20260905/candidate-20260905-055931`.

The user manually launches `C:/Destiny 2 Development/launch-scot-reveal-debug.cmd`. Do not launch the game automatically. Check that Destiny is closed before replacing its DLL. No Destiny process was listed during this handoff check.

## Current implementation

Primary files, relative to the workspace:

- `Dawn/src/client/hooks/bootflow/omega_reveal_native.cpp`
- `Dawn/src/client/hooks/bootflow/omega_boss_graph_runtime.inl`
- `Dawn/src/client/hooks/bootflow/omega_boss_graph.h`
- `Dawn/src/client/hooks/bootflow/omega_boss_graph_observation.h`
- `Dawn/src/client/hooks/bootflow/omega_reveal_bindings.h`
- `Dawn/src/state/activity/omega/omega_boss_authority.h`
- `Dawn/src/state/activity/omega/omega_progression.h`
- `Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp`
- `Dawn/src/middleware/bap/activity_message/sensor_auth_update.h`
- `Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`

Parent authority `95FB2E01/1/0` uses 641 bits, zero loose actors and authored spawn location `95FB2E01/66/57`. Member authority `95FB2E01/2/1` uses 42 bits and the matching generation. Activation follows the existing doorway latch.

Five hooks: parent `4EDC20` observation only, cinematic `106AB20`, member tick `AB6600`, character initializer `C70180`, graph update `F4E660`. The old raw render-layer hook/request and loose-actor spawn call were removed from production reveal code.

The intended native kind-9 command uses group `AFB11A12`, sequence `65D2379F`, graph asset `80F45178`, and a complete 0x808-byte queue submitted with `AB6C60`.

Expected nodes: 0 lead-in, 1 fly (`80F1FD8C`), 3 hover, 4 summon (`80F45188`), 2 idle. The condition `C0F9C866` is added through character opcode `5E`/`C620F0` only after the close approach and an observed native hover. The native graph should dispatch clip-authored effects. This has **not** been reached in the failed local test.

Camera `A74B2200` now requires a validated fly receipt. This explains the camera regression when owner validation blocks graph submission; it does not prove the camera implementation itself is defective.

## Exact unresolved check

In `omega_boss_graph_runtime.inl`, `member_tick` emits the blocking message for:

```cpp
if (!character_owner(member, run, character, owner) || !named_pair(owner)) {
    member_wait(run, 8, "character_animation_binding_not_ready");
    return;
}
```

`character_owner` currently requires all of the following:

1. Freshly resolved character handle; prefix `80F6690B/80806832/+738`; self `+24`; actor `+C0`.
2. Animation handle from character `+5C0`; resolved animation config `+4 == 80F4519A`; character backlink `+30`; actor `+C4`.
3. Parent returned by `A8CB20(context24, actor)` matches animation `+C0` and resolves.
4. Biped handle from `DCBF30(outHandle, actor)` resolves; prefix `80F66907/808036CF/+1B48`; self `+24`; bank `+D0 == 80F45190`.
5. `graph::valid_owner(...)` passes, including distinct parent/character/animation, valid handles, run and generation, bounded member offset.
6. `named_pair` resolves `80F4519A`, calls `A889E0(config, groupHash*, sequenceHash*, outGroup*, outSequence*)`, and requires group ordinal 0, sequence ordinal 1.

The current log cannot distinguish these failures. The next useful diagnostic is a bounded receipt for each failed stage, with copied actual values and expected identities. Preserve validation; do not simply remove all checks or restart the cutscene independently to hide the blocked graph.

Check both native handle resolution and actual runtime configuration. Resource tags versus runtime handles, generic datum resolution versus component pointers, and the precise lookup result should be established from evidence rather than assumed. These are investigation candidates, **not confirmed causes**.

## Native evidence already available

Pinned image `destiny2_unpacked.bin` SHA-256:

`63D128F1C759B92D32B0F226BCBEC828BC58CEF193EE0DF6FDD582BD0290ED1E`

Image base: `0x7FF618070000`.

Relevant disassembly:

- `build/scot-omega-native-research-20260905/native-C70180.txt`
- `build/scot-omega-native-research-20260905/native-A8CB20.txt`
- `build/scot-panoptes-native-graph-20260905/observation/native-DCBF30.txt`
- `build/scot-panoptes-native-graph-20260905/authority/native-AB2000.txt`
- Other proofs and extracted assets under the same revision-9 `authority`, `command`, and `observation` folders.

`C70180` stores actor at character `+C0`, allocates animation at `+5C0`, copies actor record `+2C` to animation `+4`, actor record `+28` to animation `+0`, stores character self at animation `+30`, and initializes animation context `+C0` through `A8C380`.

`A8CB20` stores actor at output `+4`, obtains generic parent from actor record `+50`, writes that parent at output `+0`, and appends a cached action-context reference at `+8`. Its output is not the character.

`DCBF30` performs a native actor component lookup, resolves the returned datum plus relative component offset, and returns the component's `+24` value. The previous revision-8 animation callback did record prefix `80F66907/808036CF/1B48` and successfully resolved the intended bank and rows. Those earlier receipts are in `build/scot-panoptes-completion-20260905/dawn.revision8-observed.log`.

Important established distinctions:

- Activity member identity is owning datum handle plus relative member offset; use `4E5C60`, not member `+24` as a self handle.
- Member generation latch `+180` must match authority `+0` before adopting its actor. `AB71E0` performs generation reconciliation; `AB7350` does not.
- Animation-state `+24` is not an established self handle. That assumption was already removed.
- Character initialization is one-shot. The full actor/character pair is stored atomically before optional locking/logging, so a busy mutex cannot lose the receipt.

The supplied documents are under `C:/Users/gauta/Downloads/docs/docs/`, especially `OMEGA-LAIR-CINEMATIC.md` and `omega/reference/LAIR-INTRO-TIMING.md`. They refer to another host and later source snapshots. Their live-success claims are not local test results. The user does not have the missing source snapshot; reconstruct from the local code/image and supplied documentation.

## Build and test evidence — limits

Revision 9 passed Release compilation with zero warnings/errors and sixteen frozen-source regression runs: eight suites in Debug and Release. Authority bits match an independently reconstructed native-schema oracle. The hook harness tests 29 support hooks plus 5 reveal hooks, 27 reveal prefixes, and rollback for all five reveal attach positions. Graph observation tests include the actual extracted bank.

These checks did **not** exercise the live `character_owner`/`named_pair` chain and did not catch this runtime failure. Do not repeat broad offline tests without addressing that missing evidence.

Build helpers: `build/scot-panoptes-native-graph-20260905/freeze_candidate.ps1`, `build_and_verify.py`, `finalize_candidate.py`, and `install-candidate.ps1`. The finalizer targets the original revision-9 change set and pre-install DLL; adapt its baseline for any new candidate rather than forcing its old assertions to pass. Source snapshots are immutable; create a new frozen candidate after edits.

Python: `C:/Users/gauta/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`.

MSBuild: `C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe`.

Use explicit UTF-8 for Python text reads/writes. The build wrapper normalizes case-insensitive environment names and uses `/p:TrackFileAccess=false` to avoid duplicate `Path`/`PATH` tracking problems.

Earlier implementation notes: `C:/Destiny 2 Development/MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md`. This handoff supersedes its expected test outcome with the actual failed result.
