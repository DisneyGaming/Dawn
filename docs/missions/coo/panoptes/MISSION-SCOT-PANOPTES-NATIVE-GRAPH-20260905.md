# Panoptes native intro graph — implemented 2026-09-05

The native graph candidate is built, verified offline, and installed. In-game animation, camera timing and VFX still require the user's manual test. This completes the implementation stage requested before working on the encounter; it does not claim the combat encounter is complete.

## Expected test

Launch `C:/Destiny 2 Development/launch-scot-reveal-debug.cmd` and start a fresh mission run.

1. Leave the triangular doorway into Panoptes's area. The existing authored door trigger activates his activity member. His native graph starts its lead-in and fly-in. The camera starts after the game reports that the fly clip is loaded and playing.
2. Stay near the doorway after the camera finishes. Panoptes should continue hovering while you remain outside the close approach trigger.
3. Walk closer along the path. The accepted close trigger plus a native hover receipt publishes the summon condition. The graph selects the full summon clip, dispatches its authored events, and then returns to its looping idle.

Watch both the body animation and the illumination/summon effects. A successful offline graph decode cannot prove that the effects rendered on screen. Combat and enemy waves are intentionally the next stage after this test.

The game was not launched during implementation or installation.

## What changed

The earlier build created a loose actor through the parent spawner and appended animation bank rows directly to the visible animator. That produced motion without establishing the activity member's named graph and its native event lifecycle.

The replacement uses the actual authored parent/member:

- Registry `95FB2E01`, parent slot `1/0`: exact 641-bit authority body, zero loose actors, generation and spawn rule `95FB2E01/66/57`.
- Same registry, member slot `2/1`: exact 42-bit authority body, matching generation and enabled flag, native command defaults preserved.
- Parent/member activation starts at the already accepted doorway trigger. Generation stays stable during region reloads and changes between mission runs.
- Native member callback `AB6600` owns the command submission. Parent callback `4EDC20` is observation only.

The command is native kind 9, group `AFB11A12`, sequence `65D2379F`, installed through `AB6C60`. Its complete 0x808-byte queue and absent-target defaults were reconstructed from local native constructors and reflection. The code confirms the copied queue, command head, actor and generation after submission. It does not retry an uncertain submission.

Graph asset `80F45178`, record 0, provides the authored sequence:

- Node 0: lead-in, bank row 2, clip `80F4517D`.
- Node 1: fly-in, bank row 5, clip `80F1FD8C`.
- Node 3: hover, bank row 1, clip `80F4517C`.
- Node 4: summon, bank row 13, clip `80F45188`.
- Node 2: idle, bank row 1, clip `80F4517C`.

The game advances these nodes. There is no replacement host timer advancing animation phases. The camera still uses the existing native cinematic `A74B2200`, now triggered by a validated loaded fly-node receipt from `F4E660`.

The summon condition is `C0F9C866`. It is added through the character's native opcode `5E` request, `C620F0`, only after both the close approach and a native hover receipt. Its exact group/sequence/event reference count is checked before and after the call. A retained native graph can hover or idle indefinitely. Cleanup removes only the owned reference when the command is replaced or disabled while the same character binding survives; actor destruction owns its own teardown.

## Why this should restore the effect path

The full native graph's clip initialization and update enter the original animation event dispatcher. The summon asset contains the summon and illumination resources `80F455AD`, `80F45566`, and `80F45568`. The earlier direct render-layer requests did not establish this member-owned graph path.

This candidate lets the native graph dispatch those clip-authored events with its actor context. It does not synthesize separate effect entities or infer VFX success from animation motion. The manual run must establish visible results.

## Ownership and runtime checks

The implementation retains full handles instead of native object pointers between callbacks. It checks the exact member, character and biped source definitions, the character/animation backlinks, native actor context, actual config `80F4519A`, and bank `80F45190`.

Two distinctions mattered during reconstruction:

- An activity member's identity is its owning datum handle plus relative component offset. Member `+24` is not a verified self handle. Native `4E5C60` supplies the datum handle, and the code reconstructs the same relative reference used by `AB5080`/`AB6600`.
- Animation state is specialized storage. Its `+24` is not a verified self handle either. The implementation checks character `+5C0`, animation `+30`, config `+4` and actor context `+C0/+C4` instead.

Member generation `+180` must match authority generation `+0`; member command revision must match authority revision. This prevents a newly published generation from adopting an older actor while native retirement is still pending.

The one-shot character initializer records its full actor/character pair atomically before optional logging. A busy logging lock cannot lose the only binding receipt.

Graph observations preserve the original float/context/state/output-pointer ABI and original return value. They distinguish requested node from loaded node and verify the actual bank row and clip. Exact owner, issued queue and graph identity are checked; unique containment within a native motion-arena allocation has not been independently proved.

## Verification and installed build

- Release DLL: zero warnings and errors.
- Eight regression suites pass in both Debug and Release: 16 frozen-source runs.
- Authority serialization matches an independent oracle derived from the pinned native reflection and default programs.
- Graph observation tests include the actual extracted animation bank.
- All 27 reveal bindings match the local pinned image.
- Hook harness verifies the 29 support hooks and 5 reveal hooks together, including rollback at each reveal attach position. It does not execute original game code.
- 1,261 baseline source files remain unchanged. Eight existing files changed and ten files were added.
- Working portal implementation, doorway geometry and active settings remain unchanged.

Installed DLL SHA-256:

`B6FFE0CF875F022C01F4244300CC35B15C7913D828787A43561806F738D5715A`

Frozen source SHA-256:

`FF6009588FF8A1E66AFFB751B17580AA771C98674A5202C7316D8752143544D0`

Candidate: `C:/Destiny 2 Development/build/scot-panoptes-native-graph-20260905/candidate-20260905-122048`.

Previous DLL, settings, log and the two cleared direct cache files are preserved in `C:/Destiny 2 Development/build/scot-panoptes-native-graph-20260905/install-backup-20260905-122252`.

The previous DLL was `D874D6B54601536F054D925ACD55B417CC39464EE471E212DD18E7C103B5E614`. Restore the saved `steam_api64.before.dll` to the workspace's `steam_api64.dll` only while Destiny is closed if rollback is needed.

## Logs to inspect after testing

Read `C:/Destiny 2 Development/Dawn/logs/dawn.log` for these receipts:

- `stage=install ... revision=9 ... animation=native_member_graph`
- `stage=character_bound`
- `stage=graph_queue ... confirmed=1`
- `stage=graph_node ... node=1` followed by `stage=intro_request`
- `stage=graph_node ... node=3` while waiting
- `stage=summon_condition ... confirmed=1` after walking closer
- `stage=graph_node ... node=4`, then `node=2`

If any stage stops, the bounded `member_wait` and `graph_observation_rejected` receipts identify the failed gate. Do not treat a queue receipt alone as proof of animation, or a node receipt alone as proof of visible VFX.

Implementation and evidence are under `C:/Destiny 2 Development/build/scot-panoptes-native-graph-20260905/`, including `changes.patch`, `verification.json`, `installed.json`, and the `authority`, `command`, and `observation` folders. The supplied documents were references; every added native binding and the critical data layouts were checked against the local executable image.
