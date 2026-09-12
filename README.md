<div align="center">

<img src="assets/dawn-logo.svg" width="120" alt="Dawn">

# Dawn

**A mission executor for Destiny 2 build 86657.**

Authored missions — scenes, dialogue, encounters, devices, objectives — driven from Lua
against recovered native package data.

<sub>
<b>TESTERS AND DEVELOPERS ONLY.</b> This is not a mod release and there is no installer.<br>
It assumes you can build a C++20 DLL, read a log, and recover from a broken install yourself.
</sub>

</div>

---

> ### The current install path is a bootstrap, not the product
>
> Today Dawn is installed **on top of a working Sunrise build** — you stand Sunrise up first, then
> replace its DLL and drop in the mission scripts. That is how it works *right now*, because Dawn
> forks Sunrise and reuses its runtime tree, settings and generated caches.
>
> **This is not the intended way to install Dawn and it will not stay this way.** Do not build
> tooling, scripts, or documentation that assumes the Sunrise-first sequence is permanent. Treat
> everything in [Setup](#setup) as the current bootstrap, and expect the steps below to be replaced.

---

## What this is

Dawn adds a **bounded mission executor**. A mission is authored as a Lua graph and executed against
native services — population, scenes, objects, destructibles, dialogue, objectives and lifecycle.
Lua owns story order and gating; C++ owns native identity, receipts and wire encoding.

Lua is evaluated once at load and its VM closes before gameplay. There is no live reload and no
scripting at runtime.

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
| Base | **A working Sunrise install** — *current bootstrap only, see the note above* |
| Toolchain | **MSBuild 18 Build Tools**, platform toolset **v145**, C++20 |
| Python | 3.11+, for `tools/coo/` validation and the binding generators |

VS2022 Community carries only v143 and will fail the build. The fix is to install Build Tools 18 —
**never** to downgrade `PlatformToolset` in the project.

Builds are `/W4 /WX`. Warnings are errors.

---

## Quick install

If you installed Sunrise with the official installer and have never built from source, this does
the whole thing — builds Dawn, backs up what it touches, deploys to every location that could win,
ensures the mission arrival overrides exist, then launches and proves which DLL actually mapped:

```powershell
git clone <repo> dawn
cd dawn
.\tools\install\Install-Dawn.ps1
```

| flag | |
|---|---|
| `-GameRoot "D:\Sunrise"` | skip auto-detection |
| `-SkipBuild` | deploy the existing build output |
| `-NoLaunch` | deploy without starting the game |
| `-Restore` | roll back to the last backup it made |

Backups land in `<GAME_ROOT>\.dawn\backup\<timestamp>\` and cover both DLL locations, both
`settings.json` files and every mission script. The pristine Steam DLL the Sunrise installer saved
at `.sunrise\original\steam_api64.dll` is never touched.

Read [Setup](#setup) anyway — the script automates those steps but the reasoning behind them is
what you will need when something goes wrong.

---

## Setup

Seven steps, start to finish. Steps 1 and 2 are the parts that will change.

### 1. Stand up a working Sunrise install *(bootstrap)*

Dawn ships a DLL and mission scripts — not a runtime. Before Dawn can load anything you need a
Sunrise install that already:

- launches the game and reaches the world,
- has generated its caches (`build_data.bin`, `content_manifest.bin`),
- has a valid `settings.json`.

Confirm Sunrise boots on its own **before** you touch anything below. If Sunrise is broken, Dawn
will be broken in ways that look like Dawn's fault.

### 2. Find where your install actually loads from *(bootstrap)*

Dawn resolves its mission scripts **relative to the loaded DLL**, so this step decides everything
that follows. Get it wrong and every later step silently does nothing.

A Sunrise install looks like this:

```
<GAME_ROOT>/
  destiny2.exe                  <- the executable sits at the root
  bin/x64/
    steam_api64.dll             <- where the Sunrise installer places the mod
    Sunrise/                    <- runtime tree: scripts, settings, logs, cache
  .sunrise/
    original/steam_api64.dll    <- the pristine Steam DLL, kept for rollback
    install-state.json
```

**The exe sits at the root, so a `steam_api64.dll` placed next to it can shadow the one in
`bin/x64/`.** Installs differ, and only one copy is ever mapped into the process. Do not guess —
launch the game and ask it:

```powershell
Get-Process destiny2 | % { $_.Modules | ? { $_.ModuleName -like 'steam_api64*' } | select FileName }
```

Whatever path that prints is the one that matters. The `Sunrise/` runtime tree — `scripts/`,
`settings.json`, `logs/` — must sit **beside that DLL**, not beside the other one.

If you are unsure, deploy to both locations in step 4 and let this command arbitrate.

### 3. Clone and build

```bash
git clone <repo> dawn && cd dawn

"C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
  Sunrise/Sunrise.vcxproj -p:Configuration=Release -p:Platform=x64 \
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
New-Item -ItemType Directory -Force -Path "$Root/bin/x64/Sunrise/scripts", "$Root/Sunrise/scripts" | Out-Null
Copy-Item Sunrise/scripts/*.lua "$Root/bin/x64/Sunrise/scripts/" -Force
Copy-Item Sunrise/scripts/*.lua "$Root/Sunrise/scripts/"         -Force
```

Keeping both copies identical costs nothing and removes a whole class of "my change did nothing".

Deploying the DLL to both locations but the scripts to only one is the worst of both worlds: the new
DLL maps, reads the runtime tree beside *itself*, and finds the **old** scripts. That presents as a
change that did nothing, or as a mission whose C++ and Lua disagree.

**Rollback.** The Sunrise installer preserves the untouched Steam DLL at
`<GAME_ROOT>/.sunrise/original/steam_api64.dll`. Copy it back over both locations to return to a
clean game. Back up the Sunrise DLL you are replacing too, so you can get back to plain Sunrise
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
grep "ev=coo_script" <runtime-tree>/logs/sunrise.log
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

Everything worth knowing is in the runtime tree's `logs/sunrise.log`. It rotates to `.old` on every
launch, so copy it before relaunching if a run is worth keeping.

```bash
grep "ev=coo_script"    sunrise.log   # which graph loaded, and its fingerprint
grep "ev=coo_executor"  sunrise.log   # phase + step bitmask
grep "ev=coo_stall"     sunrise.log   # the stalled command and what it waits on
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
| [`Sunrise/docs/MISSION-IMPLEMENTATION-TEMPLATE.md`](Sunrise/docs/MISSION-IMPLEMENTATION-TEMPLATE.md) | the implementation contract — beat records, binding records, acceptance checklists |
| [`Sunrise/docs/LUA-MISSION-AUTHORING.md`](Sunrise/docs/LUA-MISSION-AUTHORING.md) | the authoring interface and the six per-beat contracts |
| [`Sunrise/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md`](Sunrise/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md) | recovering a mission from its package |
| [`Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md`](Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md) | what the shared services already do |

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
