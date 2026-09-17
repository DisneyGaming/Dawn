# Dawn — session handoff

Context for continuing work. Written 2026-08-16.

## Current state: WORKING

- Repo: `C:\Destiny 2 Development\Dawn-src`
- Branch: **`spawner`**, tracking **`reglitched/master`** (head `6aae441 Entity Spawner`)
- **Entity spawning works.** Overlay (**Insert**) → **Spawn** module → pick entity → origin
  player/crosshair → amount → spawn.
- Deployed DLL is `bin\x64\steam_api64.dll` (Dawn is a `steam_api64` proxy; the real Steam DLL
  is preserved at `.dawn\original\steam_api64.dll` and must never be overwritten).

### Goal now
Spawned entities appear but **have no AI — they don't shoot, move, or react.**

## Dev loop

```
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```

Flags: `-BuildOnly` (compiles while game runs), `-Restore` (roll back DLL from `.dawn\backup\`),
`-ClearCache` (force content re-extraction), `-ResetSettings`.

Toolchain: VS Build Tools 2026 (v145) + Windows SDK 10.0.26100. Builds clean, ~1 min.

Logging: `bin\x64\Dawn\settings.json` → `core.logging` is set to `debug` on all five channels
with `file_sink: true`. Log lands at `bin\x64\Dawn\logs\dawn.log`. **The game's own retail
log is piped into it** (`ev=retail`) — that is the single most useful diagnostic available.

## The AI problem — what is already known

This is the crux and it was mapped in detail. Do not re-derive:

1. **The combat/AI director never initializes offline.** Across many full sessions at debug level,
   the game's own retail log only ever showed `world_controller` and `networking` subsystems.
   **Nothing** AI / combat / encounter / squad / director ever appeared. The subsystem is dormant,
   not merely unsignalled.
2. **Content is present but inert.** EDZ freeroam declares 180+ unique placed objects (dumped and
   catalogued; see `edz_freeroam_objects.csv` in this folder). They stream and register fine.
3. Free-roam combat is **ambient** (director-driven); strike combat is **scripted**. Loading a
   strike (`strike_hymn` = Lake of Shadows) produced the **same empty shell** — roster reported
   `groups=1 objects=21`, identical to freeroam. Strikes do not wake it either.

### Server-side levers tried — all dead ends
- **Incident relay (msg 19)**: echoing the client's own incidents back as authoritative did
  nothing, and caused a resonance loop (client re-raises endlessly). Reverted.
- **Incident injection**: originating msg-19 incidents at chosen content rows, including sweeps,
  did nothing.
- **Activity lifetime state** (`sensor_auth_update` type-17, values 3/6/10 only): phase 6 produced
  a "mission end" popup and a new client incident (target 7500); phase 10 nothing visible. Proves
  the lever drives the **mission lifecycle**, not the ambient combat director.
- **Slice sets**: every EDZ bubble has 0 or 1 slice. No hidden combat variant. Theory falsified.

## Reverse-engineering assets

- **Unpacked image dump**: `C:\Destiny 2 Development\destiny2_unpacked.bin` (138 MB, complete —
  every page readable, `zeroed=0`).
  **`destiny2.exe` on disk is packed** (entropy 7.76–7.97; no runtime strings present). Static
  analysis of the .exe is worthless; always use the memory dump.
- **Ghidra 12.1.2** at `C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\`. Heap raised to
  16 GB in `support/launch.properties`. Import the dump as **Raw Binary**, `x86:LE:64` /
  Visual Studio, base address = the `base=` value from that dump's log line (was
  `0x7FF618070000`).
- **x64dbg does not work**: attaching terminates the game instantly (anti-debug). In-process
  instrumentation from inside Dawn is undetected and is the reliable channel.

### Verified structures (module-relative offsets, base 0x7FF618070000)
| Thing | Offset |
|---|---|
| Player spawn gate | `+0xDC3360` |
| Participation datum accessor | `+0xA55FF0` |
| Identity accessor | `+0xA560C0` |
| Entity record SSE copy | `~+0x4B0B50` |
| Participant table base ptr (data) | `+0x1F90E18` |
| Participant table element size | `+0x1F90E20` |
| Datum-table registry ptr (data) | `+0x2439C70` |

- Datum handle encoding: **index = `handle & 0x1FFF`**, **salt = `handle >> 13`**.
- Component handle also encodes its table: **table = `(handle >> 13) & 0x3F`** (≤64 tables),
  descriptors striding `0x40`, data at `+0x08`, element size at `+0x30`.
- **Participant table is only 32 slots**, 480-byte records, and holds exactly **1 live entry (the
  player)**. It is NOT the world-object table. Free slots carry `0xFEFE` at `+0x06` and the next
  free index at `+0x04`.
- Player record layout: `+0x08` membership key, `+0x18..0x3F` five SOIDs, `+0x48` own datum handle,
  `+0x50` component handle, `+0x80`+ float transform matrices.
- Attempting to walk the registry at `+0x2439C70` returned mostly zeros with stray strings —
  **the descriptor layout assumption is still unresolved.** Read the registry's *initialiser* in
  Ghidra (find the function that writes that global) rather than probing memory.

## Suggested next steps for AI

1. Read `spawn_runtime.cpp` (643 lines) in `client/hooks/spawn/` — it is the working spawn
   implementation and shows how an entity is actually created and placed. Whatever it does *not*
   set is a strong candidate for what AI needs.
2. `is_tag_resident()` gates spawning on the entity being streamed into the current zone.
3. The likely shape of the problem: a spawned combatant needs to be attached to an AI/squad
   controller and given a team/faction so it treats the player as hostile. Compare a spawned
   entity's record against the player's (known layout above) to find unset fields.
4. Ghidra auto-analysis on the dump had **not** been run to completion. Running it makes
   "References to address" complete and is the highest-leverage unblock.

## Caveats
- Re-running the official Dawn installer overwrites the custom build (`.dawn\install-state.json`
  still records release 0.2.1).
- Branch `research-instrumentation` holds this session's exploratory probes (spawn tracer, entity
  table dumps, incident injector). Superseded, kept only for reference.
