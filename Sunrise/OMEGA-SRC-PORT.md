# Omega archive integration

Implementation guide: [Ikora animation synchronization and the mission-ending cutscene](<C:/Destiny 2 Development/Sunrise/docs/IKORA-ANIMATION-AND-ENDING.md>).

Omega now uses the implementation from `C:/Users/gauta/Downloads/src.zip` (SHA-256 `e5abd612550d7f76d2a923b7d157775318ff8f7b92cc7d36012a2efd9d420acf`).

The port includes the native opening, Forest navigation, Lair cinematic and encounters, three Crown cycles, rescue Scenes, Arc charges, boss health, music, ending cinematic, and return handoff. All 67 new archive files are present. Omega-named source files match the archive byte for byte except the two shared dialogue/directive files, which also retain Tower Watch support.

The shared integration preserves Towerfall's bootstrap, opening preset, cue interpreter, publication scheduling, descriptor metadata, and native observations. `sensor_auth_update::Snapshot::archiveOmega` selects the archive encoder only for `mission_scot`. The existing codecs remain in the `*_other_missions.cpp` files. The activity-message route selects the archive sense parser for Omega and the previous parser for other destinations. Existing test projects include these compatibility units.

The old `omega_reveal_native.cpp`, `omega_progression.cpp`, and `omega_lair_start.cpp` are excluded from the DLL project; the archive's runtime files own Omega. Their source remains available alongside compatibility models and old fixtures. Cache version 52 rebuilds the catalog with Omega schema records while retaining the metadata required by other missions.

Validation:

- Release x64 DLL builds with warnings treated as errors.
- `omega_archive_encounter_tests`: 939 checks; four islands, 143 actors, three Crown cycles, native receipt ordering, stale/duplicate rejection, ending and handoff. The same test produces identical results against the untouched archive.
- `omega_archive_protocol_tests`: 71,412 nonempty authority-body cases and 12 complete roster packets match the archive; aggregate digest `A5BE474333FF4DDF`.
- `activity_sense_update_parser_tests`: all existing parser checks pass, including the complete Towerfall snapshot and descriptor mapping.
- `other_mission_protocol_tests`: Tower Watch dialogue, directive, Scene selector and isolation from Omega fields pass.

The built DLL is `C:/Destiny 2 Development/build/omega-src-port/Release/steam_api64.dll`. The initial port was deployed to `C:/Destiny 2 Development/steam_api64.dll` on September 6, 2026, with SHA-256 `2E2D52BBDAFB52AB36692C8B86A96AC33102AD8B1D6597C65E8EECBD4432566E`. The user's first playthrough exposed the Ikora startup failure described below.

The Ikora fix restores the existing catalog lookup by registry key in `build_data_roster_runtime.cpp`. The archive searches only indices 1006–1134, but this installation's 1,925-row catalog puts Forest generator `2763EC97` at index 1197. The initial publication consequently omitted that group; the native 13-entry acknowledgement failed the archive's readiness predicate, and the correctly received approach monitor was rejected as `not_ready`. The restored lookup publishes the same archive group on bubble 11 without depending on extraction order. Omega's readiness predicate, monitor predicate, encounter logic, codecs, and settings are unchanged. The reproducible port script now preserves this compatibility fix.

The fixed Release DLL builds without warnings or errors and has SHA-256 `7CB7EFF1E0C199286B6E9DE46352F9F9359C0BBCDF3000A79C9FA7A54FAA8CA1`. `omega_forest_roster_tests` passes shifted-catalog, boundary, replacement, missing-group, capacity and idempotency checks. It also replays the user's native startup/approach packets: the original roster fails readiness; restoring the exact generator acknowledgement makes it pass while retaining all six native object bodies. Invalid scope, inactive entry, and altered Scene body remain rejected. Evidence is preserved under `C:/Destiny 2 Development/build/omega-ikora-trigger-fix-20260906/`. The fixed build was installed and hash-verified on September 6, 2026 at 10:42 EDT after the user closed Destiny 2. The prior DLL and receipt are under `C:/Destiny 2 Development/.sunrise/backups/omega-20260906-104242-d7260f63/`. Live confirmation remains pending.

The previous installed DLL and deployment receipt are in `C:/Destiny 2 Development/.sunrise/backups/omega-20260906-103014-e3658cd1/`. To restore it, close Destiny 2 and copy that directory's `steam_api64.dll` to the game root.

The reusable deployment script is `C:/Destiny 2 Development/deploy-omega.ps1`. Run it in PowerShell with `& 'C:/Destiny 2 Development/deploy-omega.ps1'`. It checks the source hash, refuses to deploy while the game is running, verifies a backup, installs and verifies the DLL, and restores the previous DLL if installation fails. Its default expected hash identifies this verified build; deploying a newly validated build requires passing its SHA-256 with `-ExpectedSha256` (and `-SourcePath` if the build location changes).

The original source, unit tests, project, resources and settings are preserved in `C:/Destiny 2 Development/tools/omega-reference-20260906/before-port.zip`. The same directory contains the extracted reference, original comparison, reproducible port script, source hash manifest and build logs. `build_port.py` runs MSBuild with a normalized environment; it does not deploy the DLL.
