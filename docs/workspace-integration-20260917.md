# Workspace integration — 2026-09-17

`D2 Dev` now uses `vendor-fixes-for-prod`, tracking `origin/vendor-fixes-for-prod`.
The remote branch was checked again at completion: `c6da4d0a897cdea26f5a1903ed101266eb40a0e6`.
The selected integration is a local, uncommitted diff against that branch.

## Preserved work

- Original workspace: `../D2 Dev-backup-20260917-105431`. Its manifest contains 12,238 files; all exist with their recorded sizes.
- Parked adventures: `../D2 Dev-adventures-parked-20260917`. Its README explains the snapshot and recovery boundaries. All 327 files in `integration-snapshot` match their SHA-256 manifest.
- The parked snapshot includes 21 adventure implementations, tests, evidence, and shared dependencies from the integration checkpoint. Compare it with the active branch before restoring anything; applying the full patch would also replace shared Lost Sector/runtime code.

## Active scope

- 42 Lost Sectors, including EDZ and Moon; full entry populations, boss completion gates, repeat-entry handling, completion chests and rewards, and Moon crystal/shield/traversal behavior.
- Shared population lifetime, category decoding, streaming, admission and wire-capacity fixes. Vendor-owned actors retain the vendor branch's separate ownership path.
- All 22 retained adventure routes reject launch requests and publish disabled banners. Previously saved selections cannot start the legacy adventure opening runtime.
- The new adventure implementations and their 626 catalog sources are outside the active build. The active member catalog contains 1,692 sources and 16,512 choices and reproduces exactly from its stored pins.
- New Light, Tower, and Haunted Forest additions from the old workspace were excluded. Content already present in the vendor branch remains its baseline.
- Cache format 62 forces extraction after switching from either the vendor branch's old cache or the parked workspace's format 61.

## Validation

Release x64 production build passed with MSVC v145 and warnings treated as errors. Output: `build/x64/Release/steam_api64.dll`.

Twelve C++ suites passed: Lost Sector runtime (555 checks), Lost Sector registry catalog, adventure disabling (all 22 routes), population cycle, population merge, native population bridge, open-world source capacity, member observations, roster lifetime, sense parser, native activity policy (58,351 checks, including saved-adventure selections), and Mercury streaming.

Logs and the consolidated results are in `.codex-tools/test-results.json`, `.codex-tools/test-*.log`, and `.codex-tools/lost-production-final.log`. The local runner is `.codex-tools/run_integration_tests.py`; it supplies separate output/intermediate directories to MSBuild. `git diff --check` passes when treating CRLF line endings as such.

At the initial integration checkpoint, installed-game regeneration and live gameplay had not been validated: this workspace and the original backup lack `destiny2_unpacked.bin` and the required installed package/Oodle inputs. The member-catalog check above renders the stored pins; it does not claim fresh extraction from game packages.

## Installed save compatibility repair

The installed game was subsequently located at `C:/Destiny 2 Development`. The first integrated DLL failed at `initialize stage=state`, producing Destiny 2's generic game-content error. The old local branch and the vendor branch both used SQLite schema version 2 but had incompatible vendor table layouts (`scope`/`numeric` versus implicit owner scope/`kind`). The old writer also persisted empty vendor slots as vendor 65535.

Persistence now upgrades both layouts to version 3 inside the existing load transaction. It validates owner scopes, preserves vendor progression and unlocks, removes only canonical empty slots, and normalizes vendor 324 to its shared vendor 11 progression. Any failed load rolls back the schema conversion. The original v1 upgrade remains supported.

The production persistence suite passed with v1, both v2 layouts, populated vendor state, sentinel validation, owner validation, rollback after conversion, and reopen coverage. A SQLite backup of the real save was loaded through production persistence in an isolated executable directory; all 20 tables were compared. All character, item, quest, and settings rows were preserved. Only empty vendor records and the normal startup reward epoch changed. Evidence: `.codex-tools/save-migration-verification.json` and `.codex-tools/persistence-migration-tests.log`.

The corrected Release x64 DLL was installed in the game root and `bin/x64`, with all 23 scripts verified. SHA-256: `03BE17B4FD9951DED8ED9CEFAF776F9B20A5FB22F7C55D3D0F99E4254848A44C`. Install receipt: `C:/Destiny 2 Development/.dawn/installations/20260917-121817.json`; pre-install DLL/runtime/save backup: `C:/Destiny 2 Development/.dawn/backup/20260917-121817`. An additional untouched save backup is in `.codex-tools/save-backups/20260917-121511/player-state.db`.

The requested character flag overrides remain `acquiredFlags[20] = 2` and `acquiredFlags[59] = 0`; adventures remain disabled.

Live startup verification succeeded: the game mapped the corrected root DLL and logged `ev=initialize phase=complete ms=13406 result=ok`. Package extraction reported all build-data domains available; BAP account snapshot and entitlement refresh succeeded. The live database reports version 3, passes `quick_check`, and retains 3 characters, 187 character-item records, and 11 profile-item records. Destiny 2 remained open and responsive. This verifies startup, not a complete Lost Sector playthrough.
