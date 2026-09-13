# Byte-verified object behavior evidence

The supplied DECOMP_SHARE corpus contains exactly six class-`0x8080941E` roots whose linked asset path names Eater's `envy` content. For each root, the installed package blob, the archive raw blob, and the manifest SHA-256 are identical. This proves native identity and serialized graph structure. It does not make partially understood leaves safe to reproduce on the server.

## Recovered roots

### `0x80C75E63` — detain sphere

- Installed package: `w64_activities_023a_5.pkg`; 308 bytes; SHA-256 `b5017b741632ee2e758d70b5a9c462bb93640867065d96d8dd9bfbf3448dfed0`.
- Owner path: config `0x80C6D06F` at offset `0x278`, object class `0x80C75F82`, build row 27, subtype `0x80804DD1`.
- Links to `0x80F44468`, path ending `envy_titan_detain_sphere\projectiles\hopon\detain_sphere.pattern.tft`.
- The archive renderer labels one linked node `asset_link`; RE/54 maps class `0x80804E19` to engine node `spawn`. The graph therefore proves an object spawn linked to the detain-sphere asset, though the complete gameplay effect remains partial.

### `0x80F42E5C` — thunder-wall safe zone

- Installed package: `w64_raids_03a1_5.pkg`; 448 bytes; SHA-256 `78b566a87599c1c0d68e5d246381c1fb20b796dd4251571b973d3ba2d30f4aed`.
- Owner config `0x80F42E5E`, object class `0x80F42E5F`, subtype `0x80804DA6`, at offsets `0x318` and `0x410`.
- Structure: one player filter, one pass/fail branch, and an `add_hop_on` link to `0x80F42E61`, path ending `envy_thunder_wall_safe_zone_hopon.pattern.tft`.
- RE/54 identifies subtype `0x80804DA6` as a time-threshold submitter in the owner's per-frame update. The serialized threshold and exact activation context still need a binding-level decode.

### `0x80F42E67` — alpha-strike test and wipe timer

- Installed package: `w64_raids_03a1_5.pkg`; 1,502 bytes; SHA-256 `9616be87586ff40177bc9bcc99181fc5cbd4fed104dae7e79f0d6b037634bf3a`.
- Owner config `0x80F42E69`, object class `0x80F42E6C`, subtype `0x80804DBA`, at offsets `0x118` and `0x1A0`.
- Condition: target-object channel `0x9908C83F`, source debug text `is_reapplying == 1`, compared with `float4(1,0,0,1)`.
- Actions: two assignments write target hash `0x537D810A` from context-primary channel `0x80F01208`, source debug text `source.wipe_timer_seconds`.
- Link: `0x80F42D59`, path ending `envy_vex_boss_alpha_strike_test.pattern.tft`.
- The graph proves the condition and assignment expressions. It does not prove the human meaning of either channel hash independently of the debug strings.

### `0x80F42E68` — wipe damage

- Installed package: `w64_raids_03a1_5.pkg`; 816 bytes; SHA-256 `3404b7aecefb710119041e5ff164e7f26aa22eb3afa11095568e5ac5a586eb4c`.
- Same owner config/object/subtype as the alpha-strike test, offset `0x1B8`.
- Condition: target-object channel `0xE50EF818`, source debug text `is_wiping == 1`, compared with `float4(1,0,0,1)`.
- Link: `0x80F42E71`, path ending `alpha_strike\wipe_damage\wipe_damage.pattern.tft`.

### `0x80F44153` — production alpha strike

- Installed package: `w64_activities_03a2_5.pkg`; 373 bytes; SHA-256 `efc39b4abb5c1451f4d86ca7822fbb5bab1560265bfbef418276d7d1012c56b3`.
- Owner config `0x80F44154` at offset `0x2DE8`, object class `0x80F44155`, subtypes `0x808072CB` and `0x808084D7`.
- Link: `0x80C75D0A`, path ending `envy_raid_boss_alpha_strike.pattern.tft`.
- The structural renderer calls the containing node `behavior_container`; RE/54 maps class `0x80804E3C` to `global`, which rebuilds its target list from global player/actor sources before dispatch.

### `0x80F44464` — detain finale damage-over-time

- Installed package: `w64_activities_03a2_5.pkg`; 274 bytes; SHA-256 `ecf5b979c032e0985f3efede9bd33bfb9a9f2327a55dd9c841a9eac5dcb0f676`.
- Owner config `0x80F44465` at offset `0x138`, object class `0x80F44468`, subtype `0x80804DAC`.
- Link: `0x80F44405`, path ending `detain_finale_dot.pattern.tft`.
- The corpus marks this root `structure_complete_known_nodes`; it contains one `add_hop_on` link.

## Interpreter and activation boundary

RE/54 establishes the client execution chain after submission: class-`0x8080941E` root executor, element dispatch, registered native node callback, optional filter branch, compiled float4 expression VM, then selected child or native action. The owner's own per-frame update submits behavior roots; the subtype-specific slot table in RE/54 identifies the submitting callback family.

The archive root rows still carry legacy text saying the first submission producer is unresolved. The newer RE/54 evidence supersedes that corpus field at the general owner/subtype level. A root's concrete lifecycle prerequisites and serialized thresholds can still be unresolved, as with the thunder-wall safe-zone root.

## Required reading rule

Never treat the readable decompiler's structural labels as engine actions without joining the node class:

- `flag_leaf` can mean `kill`, `navpoint`, `exit_seat`, or `town_presence`.
- `pattern_link` can mean `add_hop_on` or `spawn_projectile`.
- `asset_link` can mean `spawn`.
- `behavior_container` can mean `defer` or `global`.
- `pass_fail` maps to `filter`.

The authoritative class-name mapping and native handler mechanisms are in `DECOMP_SHARE/RE/54_object_behavior_self_activation.md`, especially “Each node class id has its engine name,” “What each node does,” and “The corpus decompiler's labels are structural, and several mislead.”

Keep these programs client-native. Package registration and a valid root do not prove that its owner is resident or that the root is active in the current activity. The server must construct the ordinary scenario, roster, entity, and ownership path that lets the client reach its native lifecycle.

