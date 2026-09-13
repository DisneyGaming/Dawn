# Eater of Worlds reconstruction dossier

This folder is a research and authoring handoff for the Destiny 2 build-86657 Eater of Worlds raid lair. It contains no gameplay implementation and does not alter shared runtime files.

## Recovered identity

The installed client scenario is `0x80B49E7A`, class `0x80809994`, named `raid_envy_v310` in the version-54 Sunrise build-data cache. Its package-definition hash is `0x3B50D933`, its scenario blob is 1,462 bytes, and its SHA-256 is `eb43a19a565b833e71b51edbebed8669500fa7d17d207cd8c53718a9b5113d10`.

The installed public-activity table proves three Eater rows: ordinal 536 / hash `0xB8218A8C`, ordinal 537 / `0x81029D0A`, and ordinal 538 / `0x303AF7C6`. All three name `raid_envy_v310`, show “Leviathan, Eater of Worlds” / “In the belly of the beast.”, use power 750 and difficulty settings `0x813206EE`. Rows 536 and 537 share gameplay settings `0x69A905F7`; row 538 uses `0x8A1DF706`. No installed label recovered here identifies them as normal or prestige, so the handoff keeps the three native identities unlabeled.

This identity is stronger than a name match:

- The scenario owns eight bubbles. The two raid-lair-exclusive bubbles are `raid_wing_envy_belly` (`0x8CAD7143`, map-global index 25) and `raid_wing_envy_mouth` (`0x217C510E`, index 26).
- The belly package graph contains Argos, shield sections, oracles, elemental relic channels, missiles, damage buffs, immunity, alpha strike, detainment, and the final loot transition.
- The mouth graph contains the four falling-platform paths, the Loyalist holdout, traversal gates, checkpoints, encounter objectives, and combat squads.
- Six object-behavior roots link to paths containing `raids\envy_d2` or `sandbox_custom\raids\envy`. Each supplied archive blob was checked byte-for-byte against its installed package entry.

The six `raid_gluttony_0` through `raid_gluttony_5` scenario rows are the original Leviathan raid activities. They share six Leviathan bubbles with this scenario but do not carry the Eater-exclusive belly and mouth object graphs. Do not use a `raid_gluttony_*` scenario as the Eater launch identity.

## Evidence levels

The documents use three evidence levels:

1. **Installed build proof** means a tag, class, offset, array relation, name, hash, or serialized reference was read from build-86657 package bytes or the version-54 cache.
2. **Byte-verified archive proof** means a DECOMP_SHARE behavior row was joined to its installed class-`0x8080941E` blob and both SHA-256 values matched.
3. **Known gameplay behavior** means the player-visible flow is supported by contemporary guides or reference transcripts. It is useful for authoring, but it does not promote community timings or terminology into native identities.

Readable field names establish intent and topology. They do not by themselves prove trigger thresholds, wave counts, encounter order, or authoritative server conditions.

## Folder guide

- [PLAN.md](PLAN.md) is the comprehensive solo reconstruction plan: encounter-by-encounter recovery, native integration, solo rules, checkpoints, validation gates, and delivery.
- [LIVE-REACTOR-20260913.md](LIVE-REACTOR-20260913.md) identifies the observed first-path goal spawn and records native joins for all four path goals and the encounter-start volume.
- [LIVE-ARGOS-20260913.md](LIVE-ARGOS-20260913.md) records the completed barrier/Argos capture, including all 335 Argos, 301 barrier, and 22 belly-traversal descriptors; combined live availability coverage is 886/891 known client descriptors.
- [ENTRANCE-RECOVERY.md](ENTRANCE-RECOVERY.md) records the complete offline export of the five entrance definitions, two trigger volumes, and their wrapper/bubble metadata; the failed live launch remains a separate issue.
- [NATIVE-PACKAGE-MAP.md](NATIVE-PACKAGE-MAP.md) identifies the scenario, bubbles, packages, groups, and the current cache publication gap.
- [ENCOUNTER-MECHANICS.md](ENCOUNTER-MECHANICS.md) separates known player behavior from package-proven objects, triggers, squads, scenes, and boss rules.
- [BEHAVIOR-EVIDENCE.md](BEHAVIOR-EVIDENCE.md) records the six byte-verified behavior programs, their conditions and links, and the native interpreter limits.
- [AUTHORING-PLAN.md](AUTHORING-PLAN.md) gives an implementation order, ownership split, validation gates, and unresolved binding work.
- [SOURCES.md](SOURCES.md) records local and web provenance.
- [evidence/native-inventory.json](evidence/native-inventory.json) is the complete machine-readable extraction: every resolved client descriptor, its source tag and offset, schema classes, named squads and entity candidates, objectives, dialogue rows, package files, and behavior evidence.
- [tools/extract_eater_of_worlds.py](tools/extract_eater_of_worlds.py) rebuilds that evidence without writing package bytes.

## Highest-value conclusions for an author

- The route is represented by native content; this is not a web-only reconstruction. The platform encounter, traversal, barrier phase, Argos, objective strings, dialogue bank, two scene graphs, and specialist client behaviors are all in installed data.
- Lua should own phase order, requests, dependencies, objective choices, dialogue selection, and completion gates. Native bindings must own the recovered registry/type/slot identities, object state operations, population admission, real deaths, scene ownership, and authenticated receipts.
- The version-54 cache row currently has `authoredGroupCounts = 0` for all eight bubbles. It publishes the common player/lifetime group and the small shared Berth groups, but omits the cross-package Eater groups. An implementation must close that catalog/publication gap before a script can address the large encounters safely.
- Package declarations include host-only slots that have no client descriptor. The extraction preserves both counts. Do not synthesize the missing descriptors or renumber the resolved client slots.
