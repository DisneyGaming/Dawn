# Sources and provenance

## Installed build-86657 evidence

- `Dawn/cache/build_data.bin`: version 54, 65,979,287 bytes, SHA-256 `4f0b3ceb3a63d59a970ec835a4645fbb096ab23eaa47b8753a9701e1976fe9c1`.
- `packages/*.pkg`: read through `tools/coo/package_read.py`, which borrows key material from the pinned mapped image and never prints or saves it.
- Public activity table `0x81327CF0` and display table `0x81327D35`: installed activity indices, identity hashes, names, power/settings fields, and localized title/description.
- `build/coo/native-r13-installed-fixtures/metadata/globals.bin`: installed metadata-bank index used only to resolve display-table text references; referenced containers and language records are then read through the package reader.
- `tools/coo/cache_layout.vcxproj`: compiled locally to confirm the current version-54 record sizes and offsets. Generated compiler outputs are not part of this dossier.
- `Dawn/src/middleware/content/packages/tables/scenario_reader.*` and `slot_descriptor_reader.*`: field offsets and descriptor validation predicates used by the extractor.
- `Dawn/docs/MISSION-IMPLEMENTATION-TEMPLATE.md`, `LUA-MISSION-AUTHORING.md`, `NEW-MISSION-RECONSTRUCTION-GUIDE.md`, and `UNIVERSAL-MISSION-SERVICES.md`: current authoring, service ownership, evidence, and validation guidance.

The generated [native inventory](evidence/native-inventory.json) records tag classes, source offsets, schema IDs, hashes, package files, and declared/resolved counts so each conclusion can be reproduced.

## Supplied decompilation archive

Source root: `build/decomp-share-20260913/DECOMP_SHARE`.

- `README.md`: archive layout and reproducible pipeline.
- `SCRIPT/BEHAVIOR_TRIGGER_MAP.md`: corpus contract, owner/build paths, execution boundary, and audit checks.
- `SCRIPT/extracted_package_programs/behavior_trigger_map/roots.jsonl`: one indexed row per class-`0x8080941E` behavior root.
- `SCRIPT/extracted_package_programs/behavior_trigger_map/owner_edges.jsonl` and `build_paths.jsonl`: installed config-to-root and config-to-object construction paths.
- `SCRIPT/extracted_package_programs/object_behaviors_v7/*.behavior.json` and `.txt`: lossless IR and readable structural rendering.
- `RE/54_object_behavior_self_activation.md`: binary-verified node names, filter names, handler mechanisms, target validation, self-activation, and structural-label corrections.

The six Eater roots are `0x80C75E63`, `0x80F42E5C`, `0x80F42E67`, `0x80F42E68`, `0x80F44153`, and `0x80F44464`. The extractor requires archive raw SHA-256, manifest SHA-256, installed package SHA-256, and class ID to agree.

## Primary historical source

- [Bungie, “Curse of Osiris is Live!”](https://www.bungie.net/7/en/News/article/46526), December 8, 2017. Bungie identifies Eater of Worlds as the Curse of Osiris Raid Lair and its setting inside the Leviathan. This supports release/context only, not encounter constants.

## Gameplay references

- [Shacknews, “Destiny 2 - Eater of Worlds Raid Lair Complete Guide”](https://www.shacknews.com/article/102444/destiny-2---eater-of-worlds-raid-lair-complete-guide), December 9, 2017. Used for platform, holdout, traversal, cranium, and three-side player flow.
- [Shacknews, “Destiny 2 - Eater of Worlds - Argos Guide”](https://www.shacknews.com/article/102512/destiny-2---eater-of-worlds---argos-guide), December 15, 2017. Used for shield, beam convergence, damage, and final boss flow.
- [Destiny 2 Wiki, “Leviathan, Eater of Worlds”](https://d2.destinygamewiki.com/wiki/Leviathan,_Eater_of_Worlds). Used for detailed community timings, platform patterns, cannon/ring sequence, mine/cranium values, and wipe-phase descriptions.
- [Destinypedia, “Leviathan, Eater of Worlds”](https://www.destinypedia.com/Leviathan,_Eater_of_Worlds). Used to cross-check objective order and player-visible walkthrough/transcript structure.

The community sources establish known or reported player behavior. They do not prove build-86657 tag identities, exact native timers, authoritative trigger conditions, server ownership, or spawn counts. Those claims remain separate throughout the dossier.
