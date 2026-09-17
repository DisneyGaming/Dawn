# Dawn / Destiny 2 "Homecoming" — full technical briefing

A self-contained handoff for a new AI assistant (e.g. GPT / Codex) with zero prior context. Written
2026-08-18. Read this top to bottom before touching code. It explains the project, the goal, the
architecture, the fork situation, everything we've proven, every dead end, the current task, and the
strategic reframe that changes what "done" looks like.

---

## 1. What Dawn is

**Dawn is an offline / archival mod for Destiny 2.** Destiny 2 is an always-online game whose
servers for this content are gone. Dawn makes a specific archived build run **fully offline** by:

- Shipping as **`steam_api64.dll`** — a proxy DLL that sits where Steam's real API DLL goes. It emulates
  Steam (local SteamID, callbacks, interface tables) so the game boots, and it **injects itself into the
  game process**. The real Steam DLL is preserved and chain-loaded; never overwrite it.
- **Hooking the game client** in-process (Microsoft Detours-style detours) to steer boot flow, patch
  content loading, spawn entities, draw an ImGui overlay, etc.
- **Emulating the online backend locally** — Bungie's "BAP" services, Demonware, the activity/session
  matchmaking, the gameplay peer/host networking. The game thinks it's talking to real servers; it's
  talking to Dawn inside its own process / loopback.

The game is heavily anti-tamper: `destiny2.exe` on disk is **packed** (no useful static strings), and
**attaching a debugger instantly kills the game**. So all reverse engineering is done against an
**unpacked memory dump** (see §9), and all runtime instrumentation is done from **inside Dawn's own
hooks** (which are undetected).

## 2. The goal (what "success" means)

Play the **Red War campaign opening mission, "Homecoming,"** offline — with real **enemies, scripted
events, objectives, dialogue, and the opening cinematic** — not just an empty world.

Internal identifiers:
- The mission's package/activity is **`mission_towerfall`**, activity index **266**.
- It is reached from the **Farm** (`cine_farm_376`, activity 277) via the native **CHOSEN transition**
  that goes **activity 282 → 266**.
- **Current result: the mission loads to the map + "NEW MISSION — HOMECOMING" banner and nothing else.**
  No enemies, no scripts, no objectives, no cinematic. The same emptiness affects free-roam and strikes
  (combat/AI never wakes offline). This is the core unsolved problem.

## 3. Architecture — two sides

Dawn's source lives under `Dawn-src/Dawn/src/`. Two halves matter:

- **Client side** (`src/client/…`): in-process hooks on the game. Notable: `client/hooks/bootflow/`
  (steers the activity boot/launch and contains most of our *observe-only recorders/probes*),
  `client/hooks/…` (banner, bitmap, spawn, graphics, etc.), `client/hooks/hooking/` (the detour engine).
- **Server side** (`src/server/…` and `src/middleware/…`): the fake backend. `server/bap/encrypted/…`
  implements the encrypted BAP services (activity host manager, activity messages, matchmaking, push
  notifications, queuez). `server/gameplay/…` implements the peer/host gameplay networking (group host,
  membership, endpoints). `middleware/…` holds the wire encoders/decoders and protocol definitions.
- **State** (`src/state/…`): protocol-neutral runtime state (activity membership, forced destination,
  account, build data).

The mod's operator UI (ImGui overlay, opened with **Insert**) includes an **"Activity override"** panel
that forces the next activity load to a chosen destination — this is how we force `mission_towerfall`.

## 4. The fork / branch situation

The repo (`Dawn-src`, a git repo) has diverged lines of work:

- **`red-war-gameplay-host`** — the **current working branch**, focused on the Red War opening mission
  bootflow + the activity-host manager + gameplay/QoL (fly, infinite ammo, HUD). This is what's
  deployed. It was brought in from a collaborator snapshot and made to build locally.
- **`spawner`** — an older, **divergent** line (cannot be git-merged; different base commit). It holds a
  **working Entity Spawner** (spawn arbitrary entities into the world via the overlay), plus older
  "authored route/manager" force hooks (`client/hooks/homecoming/…`, incl. `activate.cpp`) and AI
  probes. **Preserved** on that branch and in a filesystem backup
  (`_Dawn-src_spawner_backup_20260818`). Important because the **Entity Spawner is very likely "the
  client-side executor already built"** referenced in §8's strategic reframe.

Recent commits on `red-war-gameplay-host` (most recent last are the experiments this session):
- deploy the branch as base
- `795a4a0` — FTID/`field6` identity fallback (turned out to be the wrong field; harmless)
- `68f897a` — activity feature-flag recorder + poke (dead end; the flag was already set)
- `67d58c1` — **kind-22 "host-reestablish" send stub (8-byte body; needs the real 136-byte body)**

## 5. What is CONFIRMED WORKING (do not re-investigate)

- Steam emulation, local SteamID, callbacks, interface tables.
- Sign-in, character load, orbit.
- Local Demonware/BAP services and gameplay networking (many `ev=bap svc=… result=ok`).
- **Destination override + native 282→266 correction**: the override stages, then commits at the CHOSEN
  moment (`ev=activity_override … result=committed trigger=authored_chosen_prelaunch_282`), and svc-6
  selects `mission_towerfall` activity 266 from 282 with the full 620-bit descriptor.
- **Group join handshake**: membership snapshots `built` (×3), parameters (incl. `activity-host`)
  delivered and **decoded by the client**, player added, `establish ok`.
- **The client's activity-manager picker resolves the authored manager (identity 2) by session id every
  time** from the membership(30)/parameters(38) messages.
- Mission package, map, arrival bubble, and the mission banner load.
- DLL builds clean (0 warnings / 0 errors).

## 6. The failure, precisely

The authored activity **manager never activates**. Concretely, in the client's manager table:
- The authored manager is **identity 2 / activity slot 4** (`m4`). It reaches `mode 1→2→4→5`, gets its
  component **registered**, but its **`active` field (`manager+0x1AF00`) stays 0**, **`selected`
  (`+0x87C`) stays 0**, and its **definition never enables (`definition+0x94C` stays 0)** — for every
  manager, all run.
- Downstream: no `identity_enable`, no script events, and after ~189 s the activity-host BAP connections
  (`OUT FAH` / `OUT GAH0` to `127.0.0.1:30974`) hit `_connection_failure_suicide`.

We traced the activation chain to the trigger:
```
mission inert
 └ authored manager never ACTIVATES  (manager+0x1AF00 == 0 for every manager)
    └ the client only activates from a session-stream message of KIND 22 ("host-reestablish")
       └ Dawn never sent kind 22  →  and when triggered, it is host-migration and is REFUSED
```

## 7. Dead ends this session (do NOT repeat — each was disproven by logs/decompile)

1. **Host-session churn** — the 3 activity-host allocations were 3 different regions (fast-travel), not
   instability.
2. **FTID / `field6` fix** — the picker keys on **session id, not FTID**. The `make_identity` fallback
   (`795a4a0`) is harmless but irrelevant.
3. **Current→target transition / empty-identity target join** — not the blocker; the same host
   establishes fine as PUBLIC CURRENT with an empty character.
4. **`reliable_registry_decode result=rejected`** — benign idle polling (native returns false on an
   empty queue), not content failing.
5. **Feature flag `DAT_7ff61a6ed5c6`** (gates the solo activity-init `FUN_7ff6197a74f0`) — it is already
   **1** at runtime; never the gate. The solo-init is simply never reached.
6. **8-byte kind-22 body** — right message, wrong body size (client needs 136 bytes).
7. **Forcing individual gates** (route byte, manager mode, lifecycle, identity, `+0x1AF00`, the "authored
   launch" arm) → historically freeze/prune. See `project-notes/FAILED_EXPERIMENTS.md` (a 19-section
   do-not-repeat ledger — READ IT).

## 8. The strategic reframe (the most important section — this changes the goal)

Late-breaking collaborator RE (decoding the client's own XOR-obfuscated log strings with
`ghidra\scripts\logstr.py`) reframed the whole problem. **Three findings:**

**(a) The "authored launch" machinery is a red herring.** The `activator (+F17360)` / `authored path
(+F23A50)` / "gate-5 hold" cluster that the old `spawner` branch's `activate.cpp` tried to force is
actually **matchmaking rejoin bookkeeping** — it saves/restores `permanent_session_id` +
`session_join_key` on the **account profile** so you can rejoin an activity after disconnect. Its mode
byte (`activity def+0x98`) is **1 only for strikes** (patrols/missions are 0). Missions don't use it.
This is why forcing it always produced prune/boot. **Abandon that whole thread.**

**(b) There is no client-hosted launch to flip — definitively.** The client chooses its "world
controller" once at boot and it is always **OWC (online)**. The other controllers in the table
(`SWC/DWC/AHWC/BHWC`) are null (saved-film / dedicated-host-process only), and `EWC` is just the bootflow
base of OWC. **Nothing in the client's launch code is waiting to be toggled.** The client is hard-wired
to expect authored content (encounter machines, spawners, scripts) to arrive **from a remote activity
host**.

**(c) "Manager activation" is host migration (kind 22), and it is being *refused*.** Kind 22 isn't a
"go" button — it's a **host-migration** message telling the client to move to a (new) activity host.
When it's triggered it is **refused by the current host** (observed live). Since Dawn is *both* the
sender and the host, we control both sides of that refusal.

Also: the credit for missions reaching the playable **shell** (map + banner) actually belongs to a
Dawn keeper called **`region_force_public`**, not to the authored-launch machinery.

### What this means for the goal

There is **no small "flip this and the mission plays" fix.** The client will not generate authored
content on its own; it expects a host to serve it. So real enemies/scripts/objectives require **one of
two host-side routes**, and picking between them is the key strategic decision:

1. **Dawn-as-activity-host simulates the authored content** — i.e. Dawn's server side sends the
   encounter/spawn/script/roster data a real Bungie activity host would, and the host-migration (kind
   22) handshake is made to **succeed** (Dawn, as the host, must accept it — currently it refuses).
   This is the "faithful" route but is a large host-simulation effort.
2. **Use the client-side executor already built** — inject content client-side instead of waiting for a
   host. This is almost certainly the **Entity Spawner on the `spawner` branch** (it already places
   entities into the world). The open sub-problem there (from earlier work) was that spawned entities
   have **no AI** (they don't shoot/path). See the memory notes / `SESSION-NOTES.md`.

## 9. Reverse-engineering environment & tools

- **Unpacked dump:** `C:\Destiny 2 Development\destiny2_unpacked.bin` (~145 MB full memory image; every
  page readable). **Base `0x7FF618070000`; file offset == RVA.** `destiny2.exe` on disk is packed —
  always analyze the dump, never the file. Debuggers get killed (anti-debug); the reliable runtime
  channel is Dawn's own in-process hooks.
- **Ghidra 12.1.2** project at `C:\Users\gauta\Ghidra.gpr`, program `destiny2_unpacked.bin`. Headless
  works (close the GUI first — it locks the project):
  ```
  "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" ^
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -readOnly -noanalysis ^
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  Decompile-to-file scripts: `C:\Users\gauta\ghidra_scripts\Decomp*.java` (copy the pattern; they write
  decompiled C + xrefs to a `*_out.txt` under `C:\Destiny 2 Development\`).
- **`ghidra\scripts\logstr.py`** — decodes the client's **XOR-obfuscated log strings** into the game's
  own names ("Saving rejoin parameters to account profile", "Exit Matchmaking", etc.). **Use it before
  naming anything** — nearly every wrong-name detour this project took came from guessing from RVAs
  instead of reading the game's real words.
- **Capstone helpers** (a scratchpad dir, less reliable than Ghidra — cross-check): `d2dis.py` (disasm
  at RVA), `xref.py` (naive `E8` direct callers — correct), `align.py` (aligned disasm), `fieldwr.py`
  (`[reg+disp]` refs — prone to misalignment false hits). **When capstone and Ghidra disagree, trust
  Ghidra.**
- **Address-mapping caveat:** collaborators' function RVAs may be from a *slightly different build* and
  not map to our dump (we hit a case where a named "activate" fn was an audio resampler in ours). But
  **protocol-level facts (message kind numbers, struct field layouts) are build-independent** and match
  our `SessionMessageId`. Verify any borrowed RVA in our own dump before hooking it.

## 10. Build / deploy / test

```bash
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- Builds `Dawn-src\Dawn.sln`, deploys to `bin\x64\steam_api64.dll` (backs up the old one).
  `-Restore` rolls back; `-BuildOnly` compiles without deploying.
- Toolchain: **VS 2026/18 Build Tools, PlatformToolset v145, Windows SDK 10.0.26100.** There is **no
  `A:` drive** on this machine (older docs reference one — ignore). The `.vcxproj` lists files
  explicitly (no glob) — **add new `.cpp` files to it**.
- **MAX_PATH gotcha:** keep the repo at a short path. A few `#include`s are backslash-continued and blow
  the 260-char limit under long parent dirs (this bit us when the tree lived under a long `Downloads\…`).
- **Log:** `bin\x64\Dawn\logs\dawn.log` (client/core at `debug`, `file_sink` on). The game's own
  retail log is piped in as `ev=retail` — the single most useful diagnostic. Verify the deployed DLL
  hash matches the build before trusting a run (`-Restore` can leave a stale DLL live).
- **In-game repro:** launch → sign in → orbit → **Insert** → **Activity override** → Activity =
  `mission_towerfall`, Enabled → launch the opening from the Farm (CHOSEN 282→266) → reach map + banner,
  wait ~15–20 s → read the log → close.
- **Payoff greps:** `ev=gameplay stage=host_reestablish` (kind-22 sent),
  `ev=bootflow stage=activity_script_identity_enable n=` (manager activated — has ALWAYS been 0),
  `manager_table … definition_enabled=1` / `selected=1` / `active != 0`, `_connection_failure_suicide`.

## 11. The concrete current task (and how the reframe changes it)

**Immediate, small task (already scoped):** replace the 8-byte kind-22 stub with the real **136-byte
`host-reestablish` body**: `sessionId, machineId, host NetAddr, 128-bit identity field, 144-bit identity
field`. All fields are data Dawn already encodes in `publish_snapshot()` in
`server/gameplay/group/group_host.cpp` (machineId = member key, NetAddr = `endpoint::advertised()` via
`write_net_addr`, identity = the membership `Identity`). Confirm the exact bit layout by decompiling the
client's kind-22 **reader** in our dump. Files: `middleware/gameplay/group/session_messages.h`
(`SessionMessageId::hostReestablish = 22`, `kHostReestablishSize` 8 → 136, add a real encoder) and
`group_host.cpp` (`publish_host_reestablish`).

**But per §8, expect that alone to be insufficient:** kind 22 is a host-*migration* handshake that is
currently *refused*. So the fuller task is: (i) send a well-formed kind-22, **and (ii) make Dawn (the
host) accept the migration** so the manager actually activates — then (iii) confront the real content
problem, which is host-side simulation vs. the client-side executor (spawner). Decide route 1 vs route 2
from §8 before sinking large effort.

## 12. Manager / definition struct offsets (verified in our dump)

Manager: mode `+0x1AEF8`, active/component `+0x1AF00`, identity `+0x1C7C0`, selected `+0x87C`, registered
`+0xE93C`, member table `+0x860` (`0x120`-byte records; `MemberState::established = 10`).
Definition (via the identity-definition accessor): flags `+0x4`, state `+0xA`, activity `+0x24`
(default -1), request `+0x2C`, **enabled `+0x94C`**, pending `+0x94D`.

## 13. Where the durable notes live

- `HOMECOMING-KIND22-HANDOFF.md` — the narrow "fix the 136-byte body" handoff (this briefing supersedes
  its framing per §8).
- `project-notes/FAILED_EXPERIMENTS.md` — the 19-section do-not-repeat ledger (READ FIRST).
- `SESSION-NOTES.md` — older AI/spawner-focused notes (entity spawner works; spawned enemies have no AI).
- `dawn.log` — the live diagnostic.

---

### One-paragraph orientation

Dawn is an offline Destiny 2 mod (a `steam_api64.dll` proxy that hooks the client and fakes the
Bungie backend). We're trying to make the Red War opening mission "Homecoming" (`mission_towerfall`,
act 266) actually play — right now it loads to map+banner with no enemies/scripts. We proved the
authored manager never activates because the client only activates via a host-migration message
(session-stream **kind 22**, "host-reestablish") that Dawn never sent and that is **refused** when
triggered. We added a kind-22 send (commit `67d58c1`) but with the wrong body (8 vs the required 136
bytes). Crucially, late RE showed there is **no client-side switch** for this — the client is a pure
online client that expects a host to serve authored content — so the real fix is host-side: either make
Dawn **simulate the activity host** (and accept the kind-22 migration), or use the **client-side
executor (the Entity Spawner on the `spawner` branch)** to inject content directly. The immediate coding
task is the 136-byte kind-22 encoder; the strategic decision is server-simulation vs. spawner-injection.
