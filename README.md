<div align="center">

<img src="assets/dawn-logo.svg" width="120" alt="Dawn">

# Dawn

**A mission executor for Destiny 2 build 86657.**

Authored missions — scenes, dialogue, encounters, devices, objectives — driven from Lua
against recovered native package data.

<sub>
<b>TESTERS AND DEVELOPERS ONLY.</b> This is a development build; a source-build installer is included.<br>
It assumes you can build a C++20 DLL, read a log, and recover from a broken install yourself.
</sub>

</div>

---

Dawn runs against an existing Destiny 2 build 86657 installation. The installer preserves the
previous runtime and copies its account, settings, and caches into the new `Dawn` folder.
The DLL can also migrate a unique sibling runtime on first launch. Existing generated caches
are reused when compatible; missing or outdated data is rebuilt from installed packages.

## Loadout studio

Open **Loadout** in the in-game menu. The native editor adapts Sundial's catalog, perk selection,
localization, and preview layouts to Dawn's account storage. Parhelion is not required.

- Edit character identity, progression, equipment, subclasses, and character/account inventories.
- All nine subclasses have prebuilt ability combinations, including every tree, jump, grenade,
  and class ability choice. Saving preserves the other combinations for later edits.
- Browse weapons by type, armor by slot and class, cosmetics, and the full discovered perk pool.
  Search names, descriptions, or hashes; filter rarity and sort type/name/rarity. Weapon and armor
  cards use their layered preview artwork from the installed game packages.
- Give weapons and armor, equip owned items, change power and quantities, lock items, and edit
  every ordinary socket. The perk picker offers Compatible, Socket + gear type, Socket type,
  Gear type, and All scopes. Expanded scopes intentionally allow unconventional combinations.
- Randomize selected equipment slots while preserving the previous items in inventory. Armor
  stat targets select the closest available native stat plugs and display the actual result.

Changes stay in a draft until **Save changes**. Saving creates an SQLite backup in
`Dawn/editor-backups`, checks inventory limits, and rejects a stale draft if the account changed
while editing. **Restart the game after saving** to load the edited account. Reload discards an
unsaved draft only after confirmation. Inventory capacity and one exotic per gear category are
preserved; no item is silently removed to make room.

The **Credits** tab thanks both upstream projects and links their original repositories.
Sundial attribution, source revision, and GPL license are in
[Dawn/vendor/sundial/NOTICE.md](Dawn/vendor/sundial/NOTICE.md).

For local validation, export installed fixtures with `tools/testing/editor_package_fixtures.py`,
then build `Dawn/unit/editor_visual_tests.vcxproj` and run its executable with fixture, screenshot,
and installed font paths as its three arguments. Fixtures and screenshots stay in
ignored local folders; game artwork is not bundled in source control.

---

## What this is

Dawn adds a **bounded mission executor**. A mission is authored as a Lua graph and executed against
native services — population, scenes, objects, destructibles, dialogue, objectives and lifecycle.
Lua owns story order and gating; C++ owns native identity, receipts and wire encoding.

Lua is evaluated once at load and its VM closes before gameplay. There is no live reload and no
scripting at runtime.

Eater of Worlds is excluded from this build. Its implementation and reconstruction notes are
preserved in [a separate archive](optional/eater-of-worlds/README.md).

```
scripts/<mission>.lua        story order, dependencies, gates, objectives
src/state/activity/<m>/      recovered identities, bindings, authority bodies, controller
src/state/activity/coo/      the shared executor and services
src/client/hooks/            authenticated native observations
src/middleware/, src/server/ wire encoding and publication
```

## Requirements

| | |
|---|---|
| Game | Destiny 2 **86657** (`86657.20.08.23.1800.d2_rc`) |
| Base | An existing build 86657 game installation |
| Toolchain | **MSBuild 18 Build Tools**, platform toolset **v145**, C++20 |
| Python | 3.11+, for `tools/coo/` validation and the binding generators |

VS2022 Community carries only v143 and will fail the build. The fix is to install Build Tools 18 —
**never** to downgrade `PlatformToolset` in the project.

Builds are `/W4 /WX`. Warnings are errors.

---

## Quick install

For an existing build 86657 game installation, this — builds Dawn, backs up what it touches, deploys to every location that could win,
ensures the mission arrival overrides exist, then launches and proves which DLL actually mapped:

```powershell
git clone --branch codex/production https://github.com/isinternets/Dawn.git dawn
cd dawn
.\tools\install\Install-Dawn.ps1
```

| flag | |
|---|---|
| `-GameRoot "D:\Dawn"` | skip auto-detection |
| `-SkipBuild` | deploy the existing build output |
| `-NoLaunch` | deploy without starting the game |
| `-Restore` | roll back to the last backup it made |

Backups land in `<GAME_ROOT>\.dawn\backup\<timestamp>\` and cover both DLL locations, local runtime
settings, player databases, and every mission script. The pristine Steam DLL the Dawn installer
saved at `.dawn\original\steam_api64.dll` is never touched.

Read [Setup](#setup) anyway — the script automates those steps but the reasoning behind them is
what you will need when something goes wrong.

---

## Setup

Use the installer above, or follow these steps for a manual deployment.

### 1. Locate the installed game

Use the existing build 86657 installation that contains `destiny2.exe` and the complete `packages`
folder. The project builds the replacement DLL and mission scripts; it does not include the game.
The installer copies a unique previous runtime into `Dawn` before writing defaults, preserving
existing character saves. Keep the original runtime as a rollback copy.

### 2. Find where your install actually loads from *(bootstrap)*

Dawn resolves its mission scripts **relative to the loaded DLL**, so this step decides everything
that follows. Get it wrong and every later step silently does nothing.

A Dawn install looks like this:

```
<GAME_ROOT>/
  destiny2.exe                  <- the executable sits at the root
  bin/x64/
    steam_api64.dll             <- where the Dawn installer places the mod
    Dawn/                    <- runtime tree: scripts, settings, logs, cache
  .dawn/
    original/steam_api64.dll    <- the pristine Steam DLL, kept for rollback
    install-state.json
```

**The exe sits at the root, so a `steam_api64.dll` placed next to it can shadow the one in
`bin/x64/`.** Installs differ, and only one copy is ever mapped into the process. Do not guess —
launch the game and ask it:

```powershell
Get-Process destiny2 | % { $_.Modules | ? { $_.ModuleName -like 'steam_api64*' } | select FileName }
```

Whatever path that prints is the one that matters. The `Dawn/` runtime tree must be a child of
that DLL's directory. Its `scripts/`, `settings.json`, `player-state.db`, and `logs/` entries live
inside that tree.

See [Player persistence](Dawn/docs/PERSISTENCE.md) for first-run JSON migration, backups, and
the current mission-resume limits.

If you are unsure, deploy to both locations in step 4 and let this command arbitrate.

### 3. Clone and build

```bash
git clone --branch codex/production https://github.com/isinternets/Dawn.git dawn && cd dawn

"C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
  Dawn/Dawn.vcxproj -p:Configuration=Release -p:Platform=x64 \
  -p:PreferredToolArchitecture=x64 -m -v:minimal -nologo
```

Output: `build/x64/Release/steam_api64.dll`.

There is no `.sln` — build the `.vcxproj` directly.

**`-p:PreferredToolArchitecture=x64` is not optional on a first build.** The project sets
`MultiProcessorCompilation`, so `-m` on a many-core machine runs many compilers at once. Without
that flag they are the 32-bit `cl.exe`, and the heavier package translation units exhaust its
address space:

```
error C1060: compiler is out of heap space
  (compiling package_ability_build.cpp / package_subclass_build.cpp / package_build_report.cpp)
```

This only happens on a **from-scratch** build. Incremental builds recompile a handful of files and
never hit it, which is why it is invisible to anyone who already has a build tree and reliably
breaks a tester's first one. If the flag alone is not enough on a smaller machine, reduce the job
count as well: `-m:4` instead of `-m`.

### 4. Deploy

The DLL and the scripts must go together. A new DLL with old scripts, or the reverse, produces
behaviour that matches neither.

```powershell
$Root = "<GAME_ROOT>"     # from step 2

# the DLL is locked while the game runs
Stop-Process -Name destiny2 -Force -ErrorAction SilentlyContinue

# deploy to both candidate locations; step 2 decides which one wins
Copy-Item build/x64/Release/steam_api64.dll "$Root/bin/x64/steam_api64.dll" -Force
Copy-Item build/x64/Release/steam_api64.dll "$Root/steam_api64.dll"         -Force

# the runtime tree must sit beside the DLL that actually maps - deploy to both trees for the
# same reason the DLL goes to both, and let step 7 arbitrate
New-Item -ItemType Directory -Force -Path "$Root/bin/x64/Dawn/scripts", "$Root/Dawn/scripts" | Out-Null
Copy-Item Dawn/scripts/*.lua "$Root/bin/x64/Dawn/scripts/" -Force
Copy-Item Dawn/scripts/*.lua "$Root/Dawn/scripts/"         -Force
```

Keeping both copies identical costs nothing and removes a whole class of "my change did nothing".

Deploying the DLL to both locations but the scripts to only one is the worst of both worlds: the new
DLL maps, reads the runtime tree beside *itself*, and finds the **old** scripts. That presents as a
change that did nothing, or as a mission whose C++ and Lua disagree.

**Rollback.** The Dawn installer preserves the untouched Steam DLL at
`<GAME_ROOT>/.dawn/original/steam_api64.dll`. Copy it back over both locations to return to a
clean game. Back up the Dawn DLL you are replacing too, so you can get back to plain Dawn
without reinstalling.

### 5. Choose a mission

Missions are reached through arrival overrides in the runtime tree's `settings.json`, keyed by
package name:

```json
"state": { "activity": { "arrival_overrides": [
  { "package_name": "strike_pact", "bubble": 15, "slice_set": 120, "spawn_set_hash": "0x0E1523FE" }
] } }
```

`bubble` and `slice_set` choose where you land; `spawn_set_hash` chooses the spawn point within it.

`settings.json` is capped at **1 MiB** — `kConfigCapacity` in
`core/settings/settings_runtime.cpp`, which rejects anything larger with `fail("too_large")`. A
file over the cap fails during load, before the log sinks exist, so it presents as a silent boot
failure rather than an error.

Patch the field you need rather than regenerating the file. Not because of the cap — there is
plenty of room — but because a pretty-printed rewrite balloons it: a 44 KB document re-rendered
with indentation reached 578 KB, and the shipped defaults are 72 KB on disk against 45 KB
compressed. Write compact if you write it at all.

### 6. Launch

```powershell
Start-Process -FilePath "<GAME_ROOT>/destiny2.exe" -WorkingDirectory "<GAME_ROOT>"
```

Boot to in-world takes roughly 60–110 seconds.

### 7. Verify you are running what you think you are

Before judging anything in game, confirm the build and the script:

```bash
grep "ev=coo_script" <runtime-tree>/logs/dawn.log
```

```
ev=coo_script mission=strike_pact result=loaded format=lua fnv1a64=341BE9305F2A1C9F
```

`result=loaded` means the DLL found and parsed the script. If `fnv1a64` does not match the file you
just deployed, **you are testing an old build** — go back to step 2 and check the mapped module.

---

## Missions

| script | mission |
|---|---|
| `omega.lua` | Omega |
| `beyond_infinity.lua` | Beyond Infinity |
| `gateway.lua` | The Gateway |
| `deadly_trial.lua` | A Deadly Trial |
| `deep_storage.lua` | Deep Storage |
| `strike_pact.lua` | Tree of Probabilities |
| `hijacked.lua` | Hijacked |

## Reading a run

Everything worth knowing is in the runtime tree's `logs/dawn.log`. It rotates to `.old` on every
launch, so copy it before relaunching if a run is worth keeping.

```bash
grep "ev=coo_script"    dawn.log   # which graph loaded, and its fingerprint
grep "ev=coo_executor"  dawn.log   # phase + step bitmask
grep "ev=coo_stall"     dawn.log   # the stalled command and what it waits on
```

`ev=coo_executor` prints `active=` and `complete=` as bitmasks — decode them against the graph's
step order to see exactly where a mission is sitting.

**Score on what renders in game, with the window focused.** A log line saying a thing was published
is not evidence the player saw or heard it.

## Validation and packaging

```powershell
python tools/coo/verify_lua.py --out build/coo/<mission>-<candidate>
python tools/coo/package_lua.py --validation build/coo/<mission>-<candidate>
./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/<mission>-<candidate> -ValidateOnly
```

`verify_lua.py` runs the suites in Debug and Release and builds both DLL configurations. It does not
install or launch the game. Packaging rejects changed source or binaries; use a fresh candidate
directory per change. Documentation is part of the source manifest, so finish doc edits before
freezing a candidate.

## Authoring a mission

Read these first, in order:

| doc | what it settles |
|---|---|
| [`Dawn/docs/MISSION-IMPLEMENTATION-TEMPLATE.md`](Dawn/docs/MISSION-IMPLEMENTATION-TEMPLATE.md) | the implementation contract — beat records, binding records, acceptance checklists |
| [`Dawn/docs/LUA-MISSION-AUTHORING.md`](Dawn/docs/LUA-MISSION-AUTHORING.md) | the authoring interface and the six per-beat contracts |
| [`Dawn/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md`](Dawn/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md) | recovering a mission from its package |
| [`Dawn/docs/UNIVERSAL-MISSION-SERVICES.md`](Dawn/docs/UNIVERSAL-MISSION-SERVICES.md) | what the shared services already do |

The six contracts are separate, and collapsing them is the most common authoring mistake:
**preload → arm → request → ready → advance → retire.**

Executor limits: 8 phases, 32 steps per graph, 8 commands per step, 64 conditions, 1 MiB of Lua.

A command's completion policy comes from its registered capability. Lua cannot invent a `.request`
alias or change a `Wait` policy by renaming something — that needs bindings and a rebuild.

## Troubleshooting

<table>
<tr><th align="left">Symptom</th><th align="left">Cause</th></tr>
<tr><td>Script edits do nothing</td><td>Lua is read once per process. Restart the game. If it still differs, check the <code>fnv1a64</code> in <code>ev=coo_script</code> against the file on disk.</td></tr>
<tr><td>C++ changes do nothing</td><td>You deployed to a DLL the process does not map. Re-check step 2.</td></tr>
<tr><td>Build fails on the toolset</td><td>Wrong MSBuild. Use Build Tools 18; do not edit <code>PlatformToolset</code>.</td></tr>
<tr><td>A new extractor produces nothing</td><td>A warm cache skips the package pass. Bump <code>kCacheFormatVersion</code> in <code>src/state/build_data/cache/records/format.h</code>.</td></tr>
<tr><td>Mission loads but nothing happens</td><td>Read <code>ev=coo_stall</code>. It names the command and what it is missing.</td></tr>
<tr><td>A phase never completes</td><td>A graph completes only when <b>every</b> step joins. One forgotten branch freezes the mission silently.</td></tr>
<tr><td>Game boots vanilla</td><td>The DLL was rejected or replaced. Verify the mapped module and that the game was closed when you copied.</td></tr>
<tr><td>Mission never loads at all</td><td>No <code>ev=coo_script</code> line means the activity host never started — you have not entered the activity, or the arrival override did not match.</td></tr>
</table>

## House rules

- **Never commit recovered key material.** Keys and tokens stay out of the tree; vendored copies
  carry redaction placeholders and must be re-applied on any sync.
- **Measure predicates, do not guess them.** If a rule has been wrong once, log the fields and
  derive it.
- **Log every call and its inputs**, not only the branch you expect to take. A probe that logs only
  on success cannot tell "never ran" from "ran and declined".
- **Do not delete a diagnostic** because the question looks closed.

---

<div align="center">
<sub>Offline research project. Not affiliated with or endorsed by Bungie.</sub>
</div>
