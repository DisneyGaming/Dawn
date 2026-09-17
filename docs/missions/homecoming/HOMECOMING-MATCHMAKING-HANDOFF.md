# Homecoming / Dawn — Handoff: The Public-Region Matchmaking Connect (2026-08-18)

Continuation handoff for the Destiny 2 **Homecoming** (`mission_towerfall`) offline-revival work under
**Dawn**. This supersedes all prior handoffs' *direction*. Everything here was **measured this session**
unless marked inferred. Read this first, then `HOMECOMING-AH-HANDSHAKE-HANDOFF.md` (facts only — its §0
direction is dead), `HOMECOMING-FINDINGS.md`, and George's notes for deep background.

**Authorized personal reverse engineering on the user's own machine.**

---

## 0. TL;DR — WHERE WE ARE (read this)

We went from *"the authored launch never runs offline"* (George's + every prior session's wall) all the way
to **the mission loading its world on the correct public path**, one protocol seam short of it becoming
playable. The chain that now works:

```
region_force_public  →  mission treated as PUBLIC (not a dead private/fireteam)
   →  world changes to mission_towerfall, prologue_intro_loading runs
   →  client enters the citizen-join / ambassador flow for region PUB48.48
   →  [CURRENT WALL] matchmaking session search returns EMPTY
        so the client searches forever ("PUBLIC but not yet connected") and never connects
```

**THE ONE REMAINING BLOCKER:** Dawn's embedded matchmaking (**BAP svc 42→43**) answers the client's
region **session search** with an **empty result set**, so the solo client never finds the local region
host session to connect to. Fix = make `sessionSearch` return the local region host session
(`0x9EAA300100200002`) so the client connects → peer session establishes → entity slots flow → mission
loads. This is the deepest, most protocol-specific layer; **isinternet has it working** ("As PEER, the
'PUBLIC TARGET' session is established"). Fastest path = port their search-result/descriptor approach.

**Honest ceiling (unchanged):** even a full connect gives session + entity slots + script-runners, but NOT
guaranteed enemies/acts — isinternet still sees "0 acts" (further gates past the session). That content
layer is scriptable later; the user's goal is "get the mission to load, script content after."

---

## 1. CURRENT STATE

- **Deployed DLL:** `6ea7d6b86a30` (steam_api64.dll). Verify:
  `md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"` vs
  `.../Dawn-src/build/x64/Release/steam_api64.dll`.
- **Clean baseline (revert target):** `efbab4c7f6d4` (all client hooks observe-only).
- **Current experiment config:**
  - Gate spoof (`activate.cpp kDoSpoof`) = **false**
  - Fork force (`authored_probe.cpp kForceAuthoredFork`) = **false**
  - Payload fill (`authored_probe.cpp kFillPayload`) = **false**
  - `region_force_public` = **ON**
  - `ambassador_slot` = **self** (server change, see §6)
  - Direct-arm (`activate.cpp kDoActivate`) = **false**
  - All probes (ev=gateprobe, consprobe, stagedprobe, authprobe, regionpub, activate) = observe-only ON

---

## 2. THE JOURNEY THIS SESSION (what was proven, in order)

Each step is measured. This is the narrative a new chat needs to not re-tread.

1. **§0 of the prior handoff ("AH handshake never completes") — REFUTED.** In the live log, Homecoming
   reaches `ready for instantiation! AH->9eaa300100200003` and `changed world to: mission_towerfall`, just
   like the farm. The symptoms the prior handoff blamed (empty `ah-sid`, roster `state=3`-forever, "no AH
   ids waiting") **all fire identically on the WORKING farm** → not the differentiator. Do NOT rework
   `activity_host_manager_route.cpp`.

2. **The real client-side wall = the arm gate `F94CF0`.** It calls the arm action `F907B0` (→ stages the
   authored launch) ONLY when the two activity-client slots are **NOT both committed**. Offline both commit
   LOCAL and stay committed, so it always returns without arming — regardless of `mgr+0x81`/payload. Verified
   from `armtrig_out.txt` lines 63–86.

3. **Direct-arm `F907B0` — SAFE but reset.** Calling it directly (bypassing the gate) stages `obj+0x3F0`,
   but F94CF0's reset branch zeroes the whole activation within ms. No crash. So arming from the manager
   side is futile while both slots committed (= George's "transition-32" reset).

4. **Suppress F94CF0 → obj+0x3F0 HELD** (`stagedprobe verdict=AUTHORED_REACHABLE`, mode 6 + staged flag both
   open, 3s). But the ctor still never fired.

5. **Consumer probe (F5E750/F93A50).** The consumer tick `F5E750` runs every frame but its gate-1 "pending"
   flag `tick_obj+0x4C` is **0 the entire session** → it never consults the held `obj+0x3F0`, never calls
   `F93A50`. So the whole manager-arm approach was blocked upstream by a pending flag nothing sets offline.

6. **Found the CHOSEN-launch TRIGGER (this was the breakthrough pivot).** The authored launch is dispatched
   by gate `0x1763B20` → dispatcher `0x134FDF0`. The gate fires ONLY when:
   `DAT_7FF61B24C431 == 0` AND `FUN_618E9D510() != -1` AND `FUN_618E9D510() == FUN_618E9D440(0)` AND
   `FUN_61853DEB0(0) != 0`.

7. **Gate probe — the phase match happens NATURALLY.** `phase_global == phase_lane0 == 29` for ~20s during
   the Homecoming load. The ONLY persistent blockers are two byte flags: `DAT_7FF61B24C431 = 1` (needs 0)
   and the "ready" byte read by `FUN_61853DEB0` (= 0, needs ≠0).

8. **SPOOF (flip the two bytes) → the REAL dispatch fired for the first time offline.** One-shot during the
   phase-29 window: set `DAT=0` + ready=1 for exactly one native gate call, then restore. The dispatcher ran
   with real args (`p4` = live pointer). No crash. BUT it took the **`fireteam_join` fork** because
   `param6 = 0` (the authored discriminator, from `manager+0x148`, still 0 offline).

9. **Force `param6 = 1` → the authored ctor `FUN_7FF6197C7C00` FIRED for the first time ever offline.**
   (George + every prior session measured it as "never runs.") **MILESTONE.** No crash. BUT it fired with an
   ALL-ZERO payload, so the authored launch immediately did an `outgoing_join` to a NULL session
   (`invitee player id=--:--`) → "joining fireteam" hang.

10. **Payload fill experiment** (fill the ctor's 14-qword block at `FUN_7FF6193C1450()+0xbc` with local SOIDs)
    → **"unable to join target player."** This CONFIRMED the block drives the join target (slot [2] = player),
    but joining a specific *player* is the wrong model.

11. **isinternet intel (verified at the instruction level — their RVAs match ours exactly).** A **private**
    activity (missions/raids/strikes/story/quest) never establishes a peer session with the embedded host →
    entity manager never binds → no slots. **Public** regions (patrol) work because the client joins the host
    as a PEER. Fix = **`region_force_public`**.

12. **`region_force_public` — WORKED.** Force the mission's slice-set "public" at the transition starter's
    reader call. The mission crossed off the dead fireteam path onto the **public citizen-join path**,
    world-loaded (`changed world to: mission_towerfall`, `initial_slice_set_loading`, entering citizen join).
    Server allocated + advertised the region host session (`0x9EAA300100200002`, region 48 ready).

13. **Citizen join failed: phantom ambassador.** The advertisement named **ambassador = peer 1**, but offline
    there's only peer **#0** (us). So `group_target` resolved to all-zeros → "session disappeared while we
    were joining" → infinite retry. Root: `gameplay_advertisement.cpp::ambassador_slot` returns not-self.

14. **`ambassador_slot = self` → moved to MATCHMAKING dead-end (current).** Now the client becomes the
    ambassador and does a **session search** for the region, which returns **NO RESULTS** →
    "PUBLIC but not yet connected" → aborts. The search-return-empty is the current wall (§5).

---

## 3. DLL LEDGER (md5, chronological this session)

| md5 | what it added | result |
|---|---|---|
| `efbab4c7f6d4` | **clean baseline** (observe-only) | revert target |
| `f63ec9e35de3` | placeholder-fill + activate (start of session) | did not arm |
| `06dfa361ec24` | option-B direct-arm F907B0 | safe, reset in ms |
| `442fd21cc420` | suppress-reset (skip F94CF0 forward) | obj+0x3F0 HELD, ctor still 0 |
| `bc9584f12c18` | consumer probe F5E750/F93A50 (fixed sig) | pending flag never set |
| `548600b181e8` | gate probe (0x1763B20) | phase match natural; 2 byte flags block |
| `8b1543c410ce` | SPOOF (flip DAT+ready) | real dispatch fired; fork=fireteam (param6=0) |
| `a98e2986750747bd` | force param6=1 | **authored ctor FIRED**; null-payload join hang |
| `0e8771a62958` | payload fill | "unable to join target player" |
| `2e0537d4909c` | region_force_public + spoof | conflict (spoof diverts) |
| `ec15a146968a` | region_force_public ALONE | **mission on public path, world-loaded**; citizen join phantom-peer |
| `6ea7d6b86a30` | **ambassador=self (CURRENT)** | matchmaking search → NO RESULTS |

---

## 4. THE CLIENT-SIDE MAP (RVAs, base `0x7FF618070000`)

**Base caution:** the unpacked dump uses base `0x7FF618070000` (file offset == this RVA). All RVAs below are
verified working in deployed code. George's notes are INCONSISTENT (some of his RVAs use PE ImageBase
`0x7FF618000000`, e.g. he writes the dispatcher as `0x13C0170` where our module base gives `0x1350170`; his
ctor `0x1757C00` matches ours). When in doubt, use the VA. Ghidra/on-disk file offset == RVA-from-`0x7FF618070000`.

### The authored-launch trigger chain (how we fire it)
- **Gate `0x1763B20`** (VA `0x7FF6197D3B20`) — the CHOSEN-launch trigger. Fires the dispatcher only when
  `DAT_7FF61B24C431==0 && E9D510()!=-1 && E9D510()==E9D440(0) && 53DEB0(0)!=0`. Called by periodic tick
  `FUN_7FF618BCF310`. Decompiled in `authctor_out.txt` (`upstream_gate_1763B20`).
- **`DAT_7FF61B24C431`** — RVA `0x31DC431`. The master flag (=1 offline, needs 0). Only used at this gate.
- **`FUN_618E9D510()`** — RVA `0xE2D510`. Session PHASE enum (climbs 2→…→29→…).
- **`FUN_618E9D440(lane)`** — RVA `0xE2D440`. Per-lane state. `E9D440(0)` reaches 29 (matches phase_global).
- **`FUN_61853DEB0(p)`** — RVA `0x4CDEB0`. "ready" reader: if p!=0 writes byteA; returns byteB. `53DEB0(0)`
  returns byteB (=0 offline, needs ≠0). byteB address is derived at runtime from its 2nd movzx displacement.
- **Dispatcher `FUN_7FF6193C0170`** (VA) — selection dispatcher, sig `void(u8,char,u32,i64,u64,int p6)`.
  `p6==1` → authored ctor; else → `FUN_7FF6197CB8F0` fireteam-join. Caller `FUN_7FF6193C17B0` sources
  `p6` from `lVar5+0x148` (`lVar5 = FUN_7FF6193C1450()`), the ctor payload from `lVar5+0xbc`.
- **Authored ctor `FUN_7FF6197C7C00`** (VA, RVA `0x1757C00`) — memsets a 0xA8 record, stamps route/kind
  `0x10204` (=AUTHORED), copies `param_2[0..0xd]` (14 qwords, the payload) into the record. Payload is
  **all-zero offline** → null session join. Decompiled in `authctor_out.txt`.
- **Payload source:** `FUN_7FF6193C1450() + 0xbc` (14 qwords / 0x70 bytes; `+0x148` on the same object is
  the fork discriminator). This is the "consumption gap" localized to one memory block.

### The manager-arm side (explored, then superseded by the trigger)
- **Arm tick `F94CF0`** VA `0x7FF618F94CF0` (sig-scanned) — arms via `F907B0` only when slots NOT both
  committed; else resets/disarms. `armtrig_out.txt`.
- **Arm action `F907B0`** RVA `0xF207B0` — `F971C0` stages `obj+0x3F0=mgr+0x84`, `obj+0x3D0=mgr+0x08`,
  `mgr+0x141=1`. Its telemetry branch (`FUN_7ff6183cdc50(6)`) has a Ghidra "noreturn" formatter — harmless.
- **Activator `F87360`** RVA `0xF17360` — sets `mgr+0x81/0x84/0x88/0x140`; flag=0 calls disarm `F83D30`.
- **Consumer tick `F5E750`** VA `0x7FF618F5E750`, RVA `0xEEE750` — gate1 `tick_obj+0x4C` (never set offline),
  gate2 `F7C5AB0()`, gate3 short, gate4 mode 6..9, gate5 `obj+0x3F0`. `nonnet_out.txt`.
- **Authored consumer `F93A50`** VA `0x7FF618F93A50`, RVA `0xF23A50` — fires ctor only if `mgr+0x8c!=0 &&
  mgr+0x13c!=2`; always disarms via `F83ED0` at the end.
- **Staging holder:** `obj = FUN_7FF618C517E0(FUN_7FF618C51E20())` — RVAs `0xBE17E0(0xC517E0)` /
  `0xBE1E20(0xC51E20)`. `obj+0x3F0` = staged flag, `obj+0x3D0` = payload.
- Ctx `C96490`=RVA `0xC26490`; slot `C963B0`=`0xC263B0`; slot-state `C730E0`=`0xC030E0`; mode enum at
  `ctx+0x1AEF8`.

### region_force_public (WORKS)
- **Reader `FUN_7FF618C910F0`** VA, RVA `0xC210F0` — the "is this slice-set public?" reader (a VM thunk;
  hook by RVA, don't disassemble). Arg = slice-set id (ECX, `<0x1ff`), returns bool in AL.
- **Transition starter `FUN_7FF618E9B120`** RVA `0xE2B120`; the reader is CALLed at `0xE2B3B8`, returning to
  **`0xE2B3BD`** — force public only when `_ReturnAddress()==+E2B3BD` (not the per-frame pollers) and the
  slice matches our mission slice (excludes orbit). Result stored to `[session+0x2BC]`.
- Other reader call sites (do NOT force): `+12F9FD3` (per-frame poller), `+B40868`/`+B408B5`, `+F138BC`,
  `+FA0D10`. Orbit = transition type 4 (table `.rdata +1C27960`) — **never force orbit → hangs**.

### isinternet's map (verified subset; RVAs are ours)
- `+EE5A80` post-MM launch "Exit Matchmaking" fork; reads authored-mode getters `+DDF800` / `+DDE1D0`
  (non-zero → authored branch `+F23A50`, runs activator `+F17360` → `+F13D30` "go": clears `session+0x3F2`,
  staged `+0x3F0`, payload `+0x3D0`). (Their cleaner authored-force is a getter override vs our gate spoof.)
- Entity bind: `+16EC250` → `+1703690` (copies the slot mask once a session is ESTABLISHED).
- Script-runner network facet at finalize: `+56C6B0` → `+403F70` → `+3F8580` → `+4D7110` (launch-order race).

---

## 5. THE CURRENT WALL — MATCHMAKING (the next chat's job)

**Symptom (client, retail):**
```
PAH assigned ambassadorship to us
Started matchmaking for activity-region 'PUB48.48'
'PUBLIC activity target' Starting search  (Search [ACTIVE], Gather [INACTIVE])
Sending session search request ... set id 0x800d0030
Online session search ... returned NO RESULTS      ← every ~5s
Region 'PUB48.48' is PUBLIC but not yet connected   ← then aborts ~t=111k
```

**Server (Dawn) is doing its job:** `ev=gameplay stage=activityhost result=allocated
session=0x9EAA300100200002 held=1`, `stage=advertise result=ok region=48`. The host session exists and is
advertised. The gap is only the **matchmaking search reply**.

**Root:** `matchmaking_response_encoder.cpp` encodes `RequestKind::sessionSearch → encode_empty_message(
kSearchResultsField=3)` — an **empty** result set. So the client's search always returns 0 sessions and it
never connects/hosts.

**Fix direction:** make svc-42 `sessionSearch` return the **local region host session** as a result so the
client finds and connects to the embedded host. To do it correctly you need (currently unknown, blind):
1. The **field-3 search-result entry wire format** (the repeated session-summary submessage; only ever
   encoded empty so far).
2. A valid **128-byte matchmaking session descriptor** for the local host (the client never advertised the
   region — it's searching *for* it — so there's nothing stored to echo; must construct one).
3. The **search → locate → join** handoff: does search return joinable descriptors, or session ids that the
   client then `locateSession`s (kind 7, which returns `latest_snapshot`'s descriptor — empty offline)?

**RECOMMENDATION:** this is the one layer where **isinternet already has working code** (patrol/public
regions connect for them). Getting their search-result shape / descriptor is the fastest, lowest-risk unlock.
Otherwise: instrument the exact `sessionSearch` request bytes (log the parsed request in
`matchmaking_route.cpp`) and capture a real advertised descriptor (from the `advertisementUpdate` path) to
derive the format, then implement — expect several iterations.

**Open question worth testing:** is `ambassador=self` (search path) the right lever, or should we revert to
`ambassador != self` (citizen-join-as-peer path, "As PEER") and instead make Dawn's embedded gameplay host
ANSWER the citizen join (be a valid ambassador/host peer)? isinternet's "As PEER" phrasing hints the peer
path may be theirs. Both currently dead-end offline; the matchmaking-search fix is more self-contained.

---

## 6. THE SERVER-SIDE MAP (Dawn-src, branch `spawner`)

Repo: `C:\Destiny 2 Development\Dawn-src`. Paths under `Dawn/src/`.

- **`server/gameplay/gameplay_advertisement.cpp`** — builds the CitizenAdvertisement. `build_candidate`:
  `machineId=region_machine_id(idx)`, `onlineSessionId=region_identity(...)`, `hostSession=
  group::activity_host_session(machineId)`, `ambassadorSlot=ambassador_slot(localSlot)`. State ready/pending/
  absent. **CHANGED THIS SESSION:** `ambassador_slot` now returns `localMemberSlot` (self) — revert to
  `localMemberSlot==0?1:0` if switching back to the peer path.
- **`server/gameplay/group/group_host_sessions.cpp`** — maps `groupSessionId → hostSessionId` (table cap 8,
  claim/allocate/evict). `activity_host_session` claims; `allocate_claimed_host_sessions` fills via
  `prepare_session`+`commit`. This is where `0x9EAA300100200002` comes from.
- **`server/gameplay/group/group_host.cpp`, `peer/peer_transport.cpp`, `dtls/dtls_host.cpp`,
  `association/association_host.cpp`, `endpoint/gameplay_endpoint.cpp`** (embedded port 30976) — the transport
  that would answer a peer/citizen join. NO server peer/dtls activity was seen at join time.
- **`server/bap/encrypted/matchmaking/matchmaking_route.cpp`** — **THE FIX POINT.** `encode_response`;
  `sessionSearch` returns empty; `locateSession` returns `latest_snapshot`.
- **`middleware/bap/matchmaking/definition.h`** — `RequestKind` (sessionSearch=1, advertisementUpdate=2,
  locateSession=7, …), `Response{kind, advertisementId, descriptor[128]}`, `kJoinDescriptorSize=128`.
- **`middleware/bap/matchmaking/response/matchmaking_response_encoder.cpp`** — `sessionSearch →
  encode_empty_message(field 3)`; `locateSession → encode_locate_result`; `advertisementUpdate →
  encode_advertisement_id`.
- **`middleware/bap/matchmaking/response/matchmaking_dynamic_response.h`** — `encode_advertisement_id`,
  `encode_locate_result` (id + 128-byte descriptor).
- **`server/bap/encrypted/push/activity/activity_keepalive_push.cpp`** — `kKeepaliveIntervalMs=5000`,
  `kRosterBurstIntervalMs=1000`; the citizen advertisement rides the keepalive; ambassador/region-change.
- **`server/bap/encrypted/push/activity/activity_roster_push.cpp`** — roster + bubble-authority grant
  (grant=6 for Homecoming; works, not the blocker).
- **`server/bap/encrypted/activity_message/activity_message_route.cpp`** — `prepare_join`/`prepare_grant` →
  entity_slots (leases; sent for private activities too, but the client never applied them without a peer
  session).
- **`server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp`** — svc-6 AH manager
  (assigns sessionId). `state/activity/forced/activity_forced_destination.cpp` — synthesizes the authored
  620-bit descriptor for towerfall (production done server-side).

---

## 7. CLIENT HOOK MODULES (Dawn-src/Dawn/src/client/hooks/homecoming/)

- **`activate.cpp`** — the experiment hub. Hooks arm tick `F94CF0`. Contains: direct-arm (`kDoActivate=false`),
  reset-suppression, **gate spoof** (`kDoSpoof=false`; flips `DAT_7FF61B24C431` + the 53DEB0 ready byte during
  the phase match, restores in-band — writes derived from the binary, not guessed), **gate probe**
  (`ev=gateprobe`, hooks 0x1763B20, reads the 4 conditions), **consumer probes** (`ev=consprobe`, hooks
  F5E750/F93A50 observe-only).
- **`authored_probe.cpp`** — hooks dispatcher `FUN_7FF6193C0170` + ctor `FUN_7FF6197C7C00` (`ev=authprobe`).
  **fork force** (`kForceAuthoredFork=false`; forces `param6=1` when towerfall override active). **payload
  fill** (`kFillPayload=false`; fills the 14-qword payload with local SOIDs — reverted, counterproductive).
- **`region_public.cpp` / `.h`** — **NEW this session** (added to `Dawn.vcxproj` ClCompile+ClInclude AND
  registered in `client/runtime/client_hook_activation.cpp`). `ev=regionpub`. Hooks reader `0xC210F0` by RVA;
  returns 1 (public) only when `_ReturnAddress()==+E2B3BD` && towerfall override active && `sliceSet==the
  override's slice`. `kForcePublic=true`. **This is a keeper — it works.**
- **`staged_probe.cpp`** — `ev=stagedprobe`, polls `obj+0x3F0`/`obj+0x3D0` + mode every 250ms.
- Older observe-only: `homecoming_probe`, `predicate_probe`, `ingest_probe`, `holder_probe`, `pub_rewrite`
  (kDoRewrite=false), `pub_probe` (disabled), `ai/{mode,manager,pump}_probe`. Disabled harmful:
  `authored_inject`, `root_publish`, `authored_force`, `armgate_probe` (double-hooks F94CF0).
- Registration: `client/runtime/client_hook_activation.cpp`.

---

## 8. BUILD / TEST LOOP

```bash
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- `-BuildOnly` compiles while the game runs; **full deploy requires the game closed**.
- Warnings are errors (MSVC `/WX`): cast bools in bit-ops, no unused locals, no arrays through the
  `safe_read` template (its `except` path does `value = {}`).
- **Always verify the deploy landed:**
  `md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"` vs
  `"/c/Destiny 2 Development/Dawn-src/build/x64/Release/steam_api64.dll"`.
- **NEW `.cpp`/`.h` must be added to `Dawn/Dawn.vcxproj`** (explicit 582-file `<ClCompile>`/`<ClInclude>`
  list — CMake globs but MSBuild uses the vcxproj) AND registered in `client_hook_activation.cpp`.
- Log: `bin\x64\Dawn\logs\dawn.log` (truncated per run; `.old` = previous). The retail engine log is
  piped in as `ev=retail` and names things directly. Server logs are `server ...`, client hooks `client ...`.

### In-game workflow (Homecoming is a runtime toggle, not saved)
1. Boot to **orbit**.
2. **Enable the override FROM ORBIT** (Insert → Activity override → `mission_towerfall` → bubble → slice
   auto-fills 48 → spawn `none`). Enabling from a loaded activity misfires; enable from orbit.
3. Load Homecoming.

---

## 9. RE ASSETS

- Unpacked image: `C:\Destiny 2 Development\destiny2_unpacked.bin`, **base `0x7FF618070000`**, file offset ==
  RVA. On-disk exe is packed — always use the dump. **VMProtect anti-debug checks Dr registers — no hardware
  breakpoints; the game kills debuggers.** Use PAGE_GUARD if runtime watch is ever needed. Some regions are
  VM-mutated (thunks to `0x7FF5D8...` addresses) — the reader `0xC910F0` and the ctor's telemetry are such.
- Ghidra headless (Ghidra must be closed), scripts in `C:\Users\gauta\ghidra_scripts\`:
  ```
  "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" \
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis -readOnly \
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  New this session: `DecompRegionPublic.java` (decompiles by RVA + dumps a call-site instruction).
- Existing decompile outputs in `C:\Destiny 2 Development\`: `authctor_out.txt` (gate 0x1763B20, dispatcher
  0x13C0170, ctor 0x1757C00), `armtrig_out.txt`/`armer_out.txt` (F94CF0/F907B0/F971C0), `nonnet_out.txt`
  (F5E750/F93A50 gates), `activator_out.txt` (F87360), `recuse_out.txt` (opaque-field proof), `staging_out.txt`,
  `regionpub_out.txt` (NEW: reader call site, E2B120 starter), plus `recuse/route/pump/lever/msg/mgrpop/…`.
- Bit-stream helpers (confirmed): `read_bits 0x3513B0`, `read_wide 0x351070`, `read_bool 0x350EF0`,
  `stream_status 0x34E9B0` (these RVAs are George's; verify base).

---

## 10. KEY CONSTANTS

- `282 / 0x11A` CHOSEN source · `266 / 0x10A` mission_towerfall · `48 / 0x30` slice · bubble `6`
- region **48**, slice-set **PUB48.48** (hash `0x1AD5E415`), package hash `0x62D85FB3`
- **region host session `0x9EAA300100200002`** (the citizen-join target AH `9EAA3001:00200002`)
- activity roster soid `0x9EAA300100200003` · farm AH `9EAA3001:00200001` · account SOID
  `0x9EAA300100100100` · player `0x9EAA300100100102` · character `0x9EAA300100100101`
- gate `0x1763B20` · `DAT_7FF61B24C431` (RVA `0x31DC431`, =1 needs 0) · `53DEB0(0)` ready (=0 needs ≠0)
- phase match `E9D510()==E9D440(0)==29` (natural, ~t=43–64s during load)
- `param6==1` = authored-ctor fork; else = fireteam_join · ctor payload = `FUN_7FF6193C1450()+0xbc` (14 qwords)
- route/kind const `0x10204` = AUTHORED · mode enum `ctx+0x1AEF8` (=6 offline)
- matchmaking: game mode `0x800D0030`, set id `0x800d0030`, silo pool `5C01`, join descriptor 128 bytes
- gameplay endpoint embedded **port 30976**

---

## 11. GEORGE'S BLOCKERS & HOW WE GOT PAST THEM

| George's blocker | His finding | How we got past it |
|---|---|---|
| Route 0 / local mode 6 vs route 1 / authored mode 1 | manager never went authored offline | it's gated on the CHOSEN trigger, not the manager; **fired the real trigger** (gate spoof) instead of forcing the manager |
| Authored ctor `0x1757C00` "does not execute offline" | never ran in his traces | **made it fire** — gate spoof + `param6=1` fork-force → `stage=ctor FIRED` |
| Peer/state-5 stall — "client treated as peer, waits for missing AH handoff" | client stalls as peer | confirmed it's a **session join**; `region_force_public` reframes it as the intended public citizen-join |
| transition-32 reset (`0xEE5B29`) wipes forced state | forced manager reverted ~94ms | diagnosed as F94CF0's both-committed reset; **stopped forcing**, used the real trigger (not reset) |
| provider `-1` for id 282 | suspected missing server value | ruled out (authored content-table data) — didn't chase |
| His ceiling: no cinematic/objectives/AI/encounters | reached authored publication + world-load, never mode 1 | now **past** his ceiling on launch/session; content layer (acts/enemies) remains (isinternet's "0 acts") |

---

## 12. DEAD ENDS — DO NOT REPEAT (each disproved by measurement)

1. Reworking `activity_host_manager_route.cpp` to "complete the AH handshake" — it already completes.
2. Forcing the route byte / mode bit / `route_commit 0x1751F50` — teardown/prune. NEVER force route_commit.
3. Writing the manager root/holder `context+0x18DD0` — `JOINING FIRETEAM` forever.
4. Calling selection-ingest `FUN_7FF6197CB8F0` — it's a fireteam JOIN; phantom fireteam.
5. Publisher rewrite (`0x17ADA60`) fabricating a 282→266 selection onto an EMPTY descriptor — crashes.
6. Forcing manager activation + placeholder payload — reverts (both-committed reset). Manager-arm side is a
   dead end while both slots committed; the real lever is the trigger gate `0x1763B20`.
7. Payload-fill (local SOIDs into the ctor block) — makes the join target a specific PLAYER → "unable to join
   target player." Wrong model; the right model is the public region peer join.
8. `region_force_public` + authored spoof together — the spoof diverts the load down the fireteam route and
   bypasses the public transition starter. For MISSIONS run region_force_public WITHOUT the spoof.
9. Provider `-1` for id 282 — authored content-table data, not missing server state.

**Meta-lesson (holds all session): instrument, don't force. Every forcing attempt that worked did so because
we'd first measured exactly what to force; every blind force crashed, reverted, or hung.**

---

## 13. IMMEDIATE NEXT STEP

Make Dawn's **matchmaking `sessionSearch` (svc 42→43)** return the local region host session
(`0x9EAA300100200002`) so the solo client stops searching and connects → peer session establishes → entity
slots → mission loads. Start in `server/bap/encrypted/matchmaking/matchmaking_route.cpp` +
`middleware/bap/matchmaking/response/`. **Fastest = port isinternet's search-result/descriptor approach** (they
have public regions connecting). If deriving it ourselves: first log the parsed `sessionSearch` request and a
real advertised 128-byte descriptor, work out the field-3 search-result entry format, then encode the local
session. Also worth A/B testing: revert `ambassador_slot` to not-self and instead make the embedded host
answer the citizen join (the "As PEER" path).

Clean baseline to revert to = `efbab4c7f6d4`. Keepers: `region_public` module (works), all observe probes.

---

*Cross-refs: memory `homecoming-authored-fork.md` (this session's blow-by-blow, most detailed),
`homecoming-collaborator-map.md`, `homecoming-predicate-exonerated.md`, `dawn-project-state.md`.
George's `Wow.pdf` RVA table (verify base per function). isinternet has the public-region matchmaking connect.*
