# Homecoming / Dawn — Handoff: The Activity-Host Handshake (2026-08-18)

Continuation handoff for the Destiny 2 **Homecoming** offline-revival work under **Dawn**.
This supersedes the prior handoff's *direction* (not its facts). Read this first, then
`HOMECOMING-FINDINGS.md` and George's notes for background. Everything here is **measured this session**
unless marked inferred.

**Authorized personal reverse engineering on the user's own machine.**

---

> ## ⛔ CORRECTION (2026-08-18, later) — §0's DIRECTION IS REFUTED. DO NOT FOLLOW IT.
>
> Re-reading the live `dawn.log` (farm vs Homecoming, side by side) plus the F94CF0 decompile
> overturned §0. **Homecoming's AH handshake DOES complete:** `received startup response from AH
> [id 9EAA3001:00200003]` → join `[result 0]` → **`ready for instantiation! AH->9eaa300100200003`** →
> `As HOST ... moving forward` → `successfully changed world to: mission_towerfall`. The symptoms §0
> called the blocker — empty `ah-sid`, roster `state=3`-forever, "no AH ids POSSE [WAITING]" — ALL fire
> identically on the WORKING farm, so none is the differentiator. **Do NOT rework
> `activity_host_manager_route.cpp` to "complete the handshake" — it already completes.**
>
> **The real, verified wall** is the client-side arm gate `FUN_618F94CF0`: it calls the arm action
> `F907B0` (which stages the authored launch → ctor → mode 1) ONLY when the two activity-client slots are
> **NOT both committed**. Offline both slots commit as LOCAL and stay committed the whole session, so the
> tick always returns without arming — regardless of `mgr+0x81`/payload. This makes the placeholder-fill
> AND the pending "flag=1" activation experiments **futile** (they don't touch slot-commit state). The
> client also never produces an authored 282→266 selection offline (`ev=pubrw rewrote=0 src=0 dest=0`
> throughout; only farm 277→277 sent); towerfall is 100% server-painted geometry on a farm selection.
>
> **Best path forward:** (A) gate/delay slot1's LOCAL commit until the manager is active+populated so the
> arm window opens, or (B) call `F907B0(mgr)` directly to bypass the both-committed gate (higher risk —
> get buy-in; observe-only re-enable of `armgate_probe` first to confirm the branch). Full detail in
> memory `homecoming-authored-fork.md` (bottom). Clean baseline DLL = `efbab4c7f6d4`.
>
> Everything in §§1–10 below is kept for its FACTS (RVAs, constants, server map); only §0's *direction*
> is dead.

---

## 0. THE DIRECTION (what this chat should do)

> **Find out why Homecoming's Activity-Host (AH) handshake never completes, then make Dawn's
> embedded AH complete it — like it already does for the farm.**

This is the single, well-diagnosed, *safe* (server-side) lever. Everything downstream of it is present
and reachable; the AH session simply never establishes for the authored mission.

### The smoking gun (measured in `bin\x64\Dawn\logs\dawn.log`)

- **The farm (`cine_farm_376`, activity `0x0115`) COMPLETES the AH handshake:**
  `world_controller:activity_manager: '... CURRENT' activity client requesting activity host startup`
  → `networking:activity_client:receive_message_from_host: [... AH->0 ...] received startup response
  from AH [id 9EAA3001:00200001]` → `Sent join request` → `Received join result ... [result 0]` →
  **`activity client '[AC PRIVATE CURRENT CON-Y EST-Y AH->9eaa300100200001 MEM-0]' is ready for
  instantiation!`**
- **Homecoming (`mission_towerfall`, `0x010A`/266) does NOT:** the server roster sits at **`state=3`
  forever** (`ev=activity stage=roster ... dest=mission_towerfall ... state=3`), and
  `networking:activity_client:activity_host_changed: instance=PRIVATE CURRENT, ah-sid=` fires with an
  **EMPTY ah-sid**. It never reaches "ready for instantiation."

**So Homecoming's authored activity-host session never establishes. That is why the authored-launch
manager never populates and the route stays local/mode 6.** Everything we tried before (forcing the
manager) was treating a symptom.

### The exact first questions

1. **Why does `cine_farm_376` get a real `ah-sid` and "ready for instantiation", but `mission_towerfall`
   gets an empty `ah-sid` and sticks at roster `state=3`?** Diff the two in the log side by side, then
   in Dawn's server code.
2. Trace Dawn's **Activity-Host manager path** (svc 6/16 — see §4) that produces the startup
   response + join result. What does it emit for the farm vs. for Homecoming? The empty ah-sid means
   the AH never assigns/returns a session id for the authored mission.
3. Cross-check George's new client-side stall (§2) — the client is suspended in a **fiber at
   `0xC21820`** (slot-2 connection-payload serializer) waiting for the peer-side AH handoff. Confirm our
   Homecoming load reaches the same suspended state (selection state 5), which is the client symptom of
   the same missing handoff.

---

## 1. Goal and honest ceiling

Target: **authored Homecoming — the manager comes up authored/mode 1** (the door to authored content:
activity script slot 18, mission director slot 35). This is *past* George's confirmed ceiling (he
reached authored publication but never confirmed mode 1).

**Honest ceiling:** even a full success here opens the *door*, not the *mission*. George's own notes:
even his full pipeline yielded no cinematic, objectives, dialogue, AI, encounters, or progression —
that authored-content layer is server-authoritative and may not exist offline. World-load Homecoming
(geometry, PUB48.48) already works today. Do not oversell mode 1 as "the mission playing."

---

## 2. THE NEW CRITICAL FINDING (George's 2026-08-18 notes, corroborated)

George independently reached the same wall from the client side and named it precisely:

- **Peer/host diagnosis:** on the Homecoming path the client is treated as a **peer**, **skips local
  activity-host startup**, reaches **selection state 5**, and **waits for a missing peer-side Activity
  Host handoff**.
- **Concrete stall (RVAs on our build, base `0x7FF618070000`):**
  - `0xC21820` — slot-2 connection-payload serializer; fiber suspended at `0xC219B3` calling helper
    `0x17797B0`, saved return `0xC219B8`.
  - `0xBFC970` — state-5 path function that builds the slot-2 connection payload.
  - `0xC0DA25` — confirmed upstream frame in the suspended state-5 fiber chain.
  - `0xC0CF30` — activity-client slot-**state-5** update.
- **The reset that erased our forced activation = "transition event 32"** (`0xEE5B29`: "resets lane 0
  while uninitialized"). This is exactly the ~94ms revert we measured when we forced `mgr+0x81`. Two
  independent traces, same mechanism.
- **The 282 route-object side WORKS** (rules out that being the blocker): predicate `0xD4D890` succeeds,
  dispatcher `0xEEDE90` selects lane 0 at `0xEF1D90`; object 282 advances session **31→38**; object 137
  (`0xEE510`) completes host setup → `prologue_intro_loading`.
- **Ruled out:** provider-table `-1` for id 282 (`0x507EE0` / `0x506130`) is authored **content-table
  data**, not a missing server/networking value. Don't chase it.

---

## 3. The full mechanical chain we mapped (consumer side) — all present, gated on the AH session

```
AH handshake completes (roster leaves state=3, real ah-sid, "ready for instantiation")   ← THE BLOCKER
        │  (offline: never happens for the authored mission)
        ▼
authored-launch manager populates  mgr+0x08 / +0x48 / +0x148   (always 0 offline, in EVERY activity)
        ▼
F94CF0 arm-decision passes  →  F907B0 arms  →  obj+0x3F0/+0x3D0 set   (obj = FUN_618C517E0(FUN_618C51E20()))
        ▼
F93A50 authored path  →  FUN_7FF6197C7C00 authored ctor runs   (record +0x12 route byte = AUTHORED)
        ▼
pump FUN_7FF6197CE520 adopts it  →  route_commit FUN_7FF6197C1F50  →  manager mode 1
```

Key manager functions (singleton via `FUN_618F87C70()` → vtable `+0xE0`; instance seen as `…82E213A8`):
- `FUN_618F94CF0(mgr)` — arm-decision tick (vtable method, no static callers). Gate to arm:
  `mgr+0x81` active · slot 1 exists · `mgr+0x148`/`+0x08`/`+0x48 != 0` · `mgr+0x141==0` · slots not both
  committed. **Offline all payload fields are 0**, so it declines.
- `FUN_618F87360(mgr, armval, flag)` — activator: sets `mgr+0x84`, `+0x88`, `+0x140`, `+0x81=1`. Does
  NOT set the payload fields. Forcing it is **safe** but reverts (transition-32).
- `FUN_618F907B0(mgr)` → `FUN_618F971C0` — arm setter: `obj+0x3F0=mgr+0x84`, `obj+0x3D0=mgr+0x08`,
  `obj+0x3C8=mgr+0x148`.
- `mgr+0x08`/`+0x48`/`+0x148` are **opaque values, never dereferenced on the path to mode 1**
  (`route_commit` never reads `record+0xC0`). So non-zero placeholders would pass the arm gate — but the
  arm never runs because the AH session (upstream) never establishes.

---

## 4. Server-side map (Dawn) — where the fix lives

Repo `C:\Destiny 2 Development\Dawn-src`, branch **`spawner`**. Key files:
- `server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp` — **svc 6/16 Activity
  Host Manager** (activity selection + host). *Start here — this is where the AH startup/join response
  is produced.*
- `server/bap/encrypted/activity_message/activity_message_route.cpp` — svc-8 activity messages
  (`report_query_answer` handles 29/31/32 by parse+log only). Msg 31 (0x1F query authority) is answered
  by roster grant, not here.
- `server/bap/encrypted/push/activity/activity_roster_push.cpp` — roster push; **wires the bubble-
  authority grant** (`bubble_authority::select_grant`/`record_grant`). Grant already works (grant=6 for
  Homecoming's bubble) — **not** the blocker.
- `server/bap/encrypted/push/activity/activity_roster_snapshot.cpp` / `activity_roster_report.cpp` —
  the roster state machine (the `state=3` we see stuck).
- `state/activity/forced/activity_forced_destination.cpp` — `apply()` already **synthesizes the full
  authored 620-bit descriptor** for towerfall (`build_authored_descriptor`, logs `ev=bap stage=authored
  result=built`). Production is done; consumption is the gap.
- `state/activity/membership/…`, `state/activity/entity_slots/…` — session membership + slots.

BAP service IDs: **6→7 Activity Host Manager** (selection), **16→17 Activity Host**, 8→9 activity
messages, 42→43 matchmaking. Activity-host id in our runs: `9EAA3001:00200001`; roster/session soid
`0x9EAA300100200003`; join/player `0x9EAA300100100102`.

**The diagnostic:** compare, for the farm vs Homecoming, what the AH manager returns as the startup
response + join result. The farm gets `AH->9eaa300100200001` and a real session; Homecoming gets an
empty ah-sid. Find where Dawn decides/emits the ah-sid and why it's empty for the authored mission.

---

## 5. DEAD ENDS — do not repeat (each disproved by measurement this session)

1. **Forcing the route byte / mode bit / route_commit** — teardown/prune. `route_commit 0x1751F50`
   must NEVER be forced.
2. **Writing the manager root/holder `context+0x18DD0`** — `JOINING FIRETEAM` forever.
3. **Calling the selection-ingest `FUN_7FF6197CB8F0`** — it's a fireteam JOIN; phantom fireteam.
4. **Publisher rewrite (`0x17ADA60`)** — fabricating a 282→266 selection onto the EMPTY published
   descriptor crashes (claims a CHOSEN transition with no backing machinery). Crashed the farm twice.
   Module `pub_rewrite` set `kDoRewrite=false` (observer only).
5. **Forcing manager activation + placeholder payload fields** — `F87360(mgr,1,0)` + filling
   `mgr+0x08/+0x48/+0x148` = `0x9EAA300100200001`: activation is safe but **does NOT arm** (ctor never
   fires) because the whole flow is suspended upstream on the peer/state-5 AH handoff, and transition-32
   resets the lane. Confirms the root is the AH session, not the field values.
6. **The 0x1F→0x20 message path** — Dawn doesn't send 0x20, but bubble-authority grant already works
   (grant=6) and is insufficient. Not the lever.
7. **Provider `-1` for id 282** — authored content-table data, not missing server state.

**Meta-lesson (again): instrument, don't force. Every forcing attempt either crashed, reverted, or hung;
every measurement moved us forward.**

---

## 6. Build / test loop

```
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- `-BuildOnly` compiles while the game runs; **deploy requires the game closed**.
- **Always verify the deploy landed:**
  ```
  md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"
  ```
  compare vs `Dawn-src/build/x64/Release/steam_api64.dll`.
- Warnings are errors (MSVC `/WX`): cast bools in bit-ops, no unused symbols.
- Log: `bin\x64\Dawn\logs\dawn.log` (truncated per run; `.old` = previous). Retail engine log is
  piped in as `ev=retail` and **names things directly** — the AH handshake lines above are `ev=retail`.

### In-game workflow (Homecoming is a runtime toggle, not saved)
1. Boot to orbit.
2. **Enable the override from ORBIT** (Insert → Activity override → `mission_towerfall` → bubble → slice
   auto-fills 48 → spawn `none`). Enabling it while a social/activity is loaded can misfire client
   experiments — enable from orbit.
3. Load Homecoming. (If a session warm-up is needed: load farm → return to orbit → enable override →
   load Homecoming.)

---

## 7. Reverse-engineering assets

- Unpacked image: `C:\Destiny 2 Development\destiny2_unpacked.bin`, **base `0x7FF618070000`**, file
  offset == RVA. On-disk exe is packed — always use the dump. **VMProtect anti-debug checks Dr
  registers — do NOT use hardware breakpoints; the game kills debuggers.** Some regions are VM-mutated
  so on-disk signatures miss even when the runtime scan finds them.
- Ghidra headless (Ghidra must be closed), scripts in `C:\Users\gauta\ghidra_scripts\`:
  ```
  & "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" `
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis -readOnly `
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  Write output paths with forward slashes. VA = `0x7FF618070000 + RVA` (George's RVAs are our build).
  Many vtable methods have **0 static xrefs** (dispatched) and the dump's vtables are ASLR-rebased, so
  absolute-VA vtable scans return nothing. Prefer signature scans and live vtable resolution.
- Python + capstone available. Linear disassembly desyncs — use byte-pattern/vectorized scans.
- Existing decompile outputs in `C:\Destiny 2 Development\`: `recuse_out`, `armer_out`, `armtrig_out`,
  `nonnet_out`, `mgrpop_out`, `staging_out`, `activator_out`, `commit_out`, `slot_out`, `pub_out`,
  `msg_out`, plus older `route_out`/`pump_out`/`lever_out`/etc.

---

## 8. Dawn probe modules written this session (client hooks, all in `Dawn-src/Dawn/src/client/hooks/`)

Observation modules (safe, keep): `ai/{mode,manager,pump}_probe`, `homecoming/{homecoming,predicate,
ingest,holder,staged,armgate,pub}_probe`, `homecoming/authored_probe`.
- `homecoming/staged_probe` (`ev=stagedprobe`) — polls the staging holder + mode.
- `homecoming/armgate_probe` (`ev=armgate`) — DISABLED (double-hooks F94CF0 with activate).
- `homecoming/pub_rewrite` (`ev=pubrw`) — owns publisher `0x17ADA60`; `kDoRewrite=false` (observer).
- `homecoming/activate` (`ev=activate`) — hooks F94CF0; currently `kDoActivate=true` (the placeholder
  experiment — **turn off / revert for the AH-handshake work**). Also dumps a full manager snapshot
  (`stage=SNAP`) the instant any payload field is non-zero (never fired offline).

Activation of modules: `Dawn-src/Dawn/src/client/runtime/client_hook_activation.cpp`.
DISABLED harmful modules: `homecoming/{authored_inject,root_publish,authored_force}`, `pub_probe`.

### DLL states (md5, most-recent last)
- `efbab4c7f6d4` — **clean baseline** (all probes observe-only, nothing forced). Revert target.
- `f63ec9e35de3` — **currently deployed**: placeholder-fill + activation experiment (did NOT arm).
  For the AH-handshake trace this can stay (its client experiment is irrelevant server-side) or be
  reverted to `efbab4c7f6d4` for a totally clean client.

---

## 9. Key constants

- `282 / 0x11A` CHOSEN source · `266 / 0x10A` mission_towerfall · `48 / 0x30` slice set · bubble `6`
- Activity-host id `9EAA3001:00200001` · roster/session soid `0x9EAA300100200003` · join/player
  `0x9EAA300100100102` · account SOID `0x9EAA300100100100`
- farm activity `0x0115` (`cine_farm_376`) — the WORKING AH-handshake reference
- Homecoming hashes: activity-def `0x9ACCB518` · scenario `0x80B500BC` · launch desc `0x80FDB97F`
- manager mode enum `ctx+0x1AEF8` (=6 offline) · manager active flag `mgr+0x81` · payload
  `mgr+0x08`/`+0x48`/`+0x148`
- AH stall RVAs: `0xC21820`/`0xBFC970`/`0xC0DA25`/`0xC0CF30` · transition-32 reset `0xEE5B29`

---

## 10. State at handoff

- World-load Homecoming works (geometry, PUB48.48, slice 48). Authored mode 1 does not.
- The blocker is now precisely located and **server-side**: Homecoming's Activity-Host handshake never
  completes (roster stuck `state=3`, empty ah-sid), while the farm's does. Fixing that is the job.
- Start in `activity_host_manager_route.cpp`; diff farm vs Homecoming AH startup/join emission; find why
  the ah-sid is empty for the authored mission.
- If stuck, cross-check the client stall (fiber `0xC21820`, state 5) to confirm the client symptom of the
  same missing handoff.
