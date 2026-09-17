# Festival of the Lost integration

Integrated `origin/fotl-haunted-forest-eater` at `027498b` into our checkpoint `ec1ad25` on `codex/integrate-fotl-haunted-forest`. This supersedes the earlier instruction to exclude Haunted Forest. Adventures remain disabled and parked.

The pre-merge workspace is preserved at `C:/Users/gauta/Documents/ChatGPT/D2 Dev-before-fotl-20260917-154241`: source.zip, a verified SHA-256 manifest of 4,382 files, a binary working-tree patch, and Git state.

## Included behavior

- Haunted Forest entry/capture, repeated timed branches, authored travel/arrival and respawn, boss encounters, objectives, music, environment, completion, and rewards.
- Festival Tower/Eva, native Director selection, equipped-mask requirement, quest stages, masks, bounties, candy pickup support, package rewards, and persisted weapon rolls.
- Event presets, vendor rule resources, and installation of those resources beside both runtime DLL locations.
- Existing New Light mission, unarmed start, Ghost door, tank objective, jumpship ending, per-character completion flag, vendor services, Lost Sectors, and Tower revisit recovery are retained.

## Merge decisions

Preserved eight-category population support and recurring patrol recovery; used named field initializers for the incoming Forest population definitions. Native round publications remain scheduled during phase transitions. Generic Forest travel is composed with the specialized New Light travel projections.

Eva uses the Festival transaction handler. Existing vendors retain their service transactions and New Light introductions. Both paths share item acquisition, with explicit mission reward handling and the incoming vendor/package after-image path. Postmaster insertion and item eviction remain supported. The two opcode-904 request types now have separate namespaces.

Family-4 keeps its exact native layout. Ten turn-in flags, two service flags, four New Light flags, and four Festival flags fit the twenty-entry override bank. Class predicates use the character's native identity rather than redundant overrides. Regression tests cover every New Light step and character class with and without an equipped mask.

Build-data cache format is 64. Save schema is 4, adding persistent item-roll storage after the existing vendor migrations. A copy of the installed schema-3 save upgraded successfully: all three characters, eleven profile items, and all pre-existing character, inventory, vendor, quest, and mission rows were unchanged. Only the normal runtime metadata epoch advanced. The live save was not reset.

## Verification

Release x64 builds with MSVC v145. Twenty-one targeted suites pass, covering Forest launch/lifetime against installed package fixtures; entry/capture and twelve repeated rounds; generators; 7,328 combined Festival/New Light/vendor flag checks; New Light; Lost Sectors; disabled adventures; population/streaming; save migration and roll persistence; activity policies; campaign rosters; and native wire lifetimes. Four pinned-native Tower recovery replay tests also pass.

Evidence: `.codex-tools/fotl-test-results.json`, `.codex-tools/fotl-final-build.log`, `.codex-tools/fotl-existing-save.log`, `.codex-tools/fotl-tower-native-tests.log`, and `.codex-tools/fotl-package-fixtures/manifest.json`.

Deployment selects the Festival-only Tower preset and enables the branch's candy-drop hook. The five experimental extra reward coffers retain the incoming branch's disabled default because their live roster authority is not established. The ordinary end-of-run chest remains part of the Forest implementation. A full in-game Festival playthrough remains to be verified.
