# Launchpad integration — 2026-09-17

The working branch `codex/integrate-mission-launchpad` combines the existing vendor, Lost Sector and runtime work with `origin/mission_launchpad` at `39b9c498c9e3f2f604a23d9202b32153b01a98d8`.

The previous workspace shipped `launchpad.lua` without the native Launchpad controller. Setting character flags 753/1041 launched the activity, but could not supply its cinematic, encounter or transition state. This integration adds the branch's complete mission controller, native observations, resurrection cinematic, gameplay transition, reward progression and cinematic handoff to the standard Tower.

## Compatibility decisions

- Keep existing vendor transactions, durable character unlocks, Lost Sector/runtime fixes and adventure disabling. Add New Light's quest conditions to the existing vendor evaluator.
- Store New Light Pursuit changes, item rewards and currencies in the existing SQLite account transaction. Do not compile the incoming branch's older separate vendor inventory save implementation or use its quest progress sidecar.
- Preserve shared population admission receipts, streaming leases, source variants, Eater/Garden callbacks and patrol respawn checks while adding Launchpad observations and fly-in completion.
- Append New Light's native overrides without clearing the vendor overrides already in those banks.
- Let the starting Pursuit project `acquiredFlags[20] = 2` and `acquiredFlags[59] = 0`. Quest completion changes these flags through the upstream progression rules, avoiding an unconditional restart of New Light after completion.
- Seed Escape the Cosmodrome in each default test Guardian's inventory. Per the follow-up request, remove all three weapon slots, ship, Sparrow and spare character inventory. Retain armor, Ghost, subclass, emblem and the other identity rows required by character selection. These remain the repository's three authored identities; mission pickups supply the weapons.
- Cache format 63 forces extraction of the merged Launchpad and existing Lost Sector catalogs.
- Give Launchpad and Gateway distinct object filenames in MSBuild; otherwise their `runtime.cpp` and `controller.cpp` objects overwrite one another.

## Backups and reset

The prior source, uncommitted changes, diagnostics and last working DLL/PDB are preserved in `../D2 Dev-before-launchpad-20260917-122536`. Its manifest verifies 4,337 source files. Local checkpoint commit: `cf53ed70c03692b9e831f7bab46c6bb153e5dd13`.

The user requested a save wipe. The old database and its WAL/SHM files were moved with SHA-256 verification to `C:/Destiny 2 Development/.dawn/backup/save-reset-20260917-124123`; both runtime settings were backed up there too. Both installed runtime directories received fresh authored state with the starting Pursuit. HUD, movement and other external runtime settings remain separate.

## Validation and installation

Release x64 production build succeeded with MSVC v145 and warnings treated as errors. The expanded Launchpad integration suite passed 105,516 checks, including cinematic-before-gameplay sequencing, foreign/stale event rejection, complete roster packets, all active/retired dialogue rows, 30 seconds of repeated gameplay publication, quest flag progression, and preservation of vendor overrides.

Nine regression suites passed: Lost Sector runtime, adventure disabling, native population bridge, native activity policy (58,351 checks), spawn lifecycle, persistence, activity sense parser, population cycle, and Mercury streaming. Results: `.codex-tools/launchpad-regression-results.json`; new suite: `.codex-tools/launchpad-baboon-wire-test.log`.

Installed both DLL locations and verified all 23 scripts. DLL SHA-256: `7B998E0DBFF72ED928FDD6E47F57A035A7C2DC33AC5FE12A835EC224D90A2189`. Receipt: `C:/Destiny 2 Development/.dawn/installations/20260917-125354.json`.

## Baboon diagnosis and correction

The first live run confirmed resurrection playback at t=98734, the completed landing transit at t=146781, and gameplay phase 6 at t=146859. The next mission publication failed at t=146984 (`clock=1 sensor=0`). The client reported loss of the activity host 20,502 ms after that last successful update, then exhausted its repair attempts.

Launchpad's imported authority advertised dialogue size as a constant plus one active timestamp. The retained runtime's dialogue writer also sends explicit zero timestamps for retired rows with consumed generations. As soon as the opening line retired, the declared body became 64 bits short, rejecting every complete mission update. The new regression reproduced that rejection before the fix. Launchpad now derives the declared size from the shared dialogue serializer's `dialogue_bits`, preserving its retirement semantics. Full mission packets encode for every row before and after consumption.

The failing log is preserved at `.codex-tools/launchpad-baboon-before-fix.log`. Before the empty-loadout retest, the database and settings were backed up again to `C:/Destiny 2 Development/.dawn/backup/launchpad-empty-start-20260917-125335`, then regenerated from the requested empty starting inventory. Production build log: `.codex-tools/launchpad-baboon-production-build.log`.

Live verification of the corrected build is recorded below when observed. Automated tests do not establish a complete in-game playthrough.

Corrected-build startup succeeded (`ev=initialize phase=complete ms=218 result=ok`), and the running process mapped `C:/Destiny 2 Development/steam_api64.dll`.

An initial attempt to make the account completely empty was too aggressive. Selecting a Guardian failed at `family4 stage=prepare result=fail step=move_character_object`, leaving the client on a black screen because the character object could not be moved into the selected slot. New Light requires an unarmed Guardian, but character selection still requires its authored armor, Ghost, subclass and identity equipment. The corrected reset removes kinetic, energy and heavy weapons, ship, Sparrow and all spare character inventory while preserving those required rows.

The black-screen save and settings are backed up at `C:/Destiny 2 Development/.dawn/backup/black-screen-empty-loadout-20260917-125946`. The replacement DLL SHA-256 is `5D18168582059F03663101C2B4830196111C95548E0FFCC49E45B79FDA6D30D0`; installation receipt: `C:/Destiny 2 Development/.dawn/installations/20260917-125953.json`. Live SQLite validation passed: 11 required equipped identity rows per Guardian, no equipment rows 0/1/2/9/10, one starting Pursuit per Guardian, and no mission checkpoints.

The corrected live retest selected Guardian `0x9EAA300100100102` successfully (`queuez stage=select result=ok`), launched `mission_launchpad`, played the resurrection cinematic, entered gameplay phase 6, and remained connected beyond the prior 20-second timeout. Mission execution advanced from section 0 into section 1 and reached the Ghost/lights sequence with `failure=0` and `fault=0`. No `publication result=blocked`, activity-host loss or fatal connection error occurred after the handoff.
