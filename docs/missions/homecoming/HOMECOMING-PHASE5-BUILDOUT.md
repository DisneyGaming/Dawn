# Homecoming — Phase 5 Build-Out: Managed-Session-Start / Peer Citizen-Join

**Status date:** 2026-08-18
**Milestone reached:** `world_controller: successfully changed world to: mission_towerfall` — first time ever, past the infinite loading screen.
**This document:** everything needed to build out **Phase 5**, the current wall: the server holds the activity membership with `no_host_session`, so the client's managed-session-start never completes, the peer citizen-join fails, and slice-set loading times out.

Authorized personal reverse engineering on the user's own machine.

Cross-refs (repo): `HOMECOMING-QOS-HANDOFF.md`, `HOMECOMING-MATCHMAKING-HANDOFF.md`, `HOMECOMING-FINDINGS.md`.
Cross-refs (memory): `homecoming-matchmaking-pivot.md`, `homecoming-authored-fork.md`, `homecoming-predicate-exonerated.md`, `homecoming-collaborator-map.md`, `dawn-project-state.md`.

---

## 0. TL;DR

We drove Homecoming end-to-end through boot → matchmaking search → NAT → QoS transport → **QoS suitability (defeated via the `qos_suitable` hook)** → **world transition into `mission_towerfall`**. The client now transitions into the mission and enters `activity:initial_slice_set_loading`, where it waits:

```
world_controller:state:setup:initial_slice_set_loading: Waiting for managed-session-start for all peers.
world_controller:slice_set_transition_manager: Region 'PUB48.48' session state 'disc' -> 'ctng' (starting ambassador matchmaking)
world_controller:state:activity:initial_slice_set_loading: Timeout entering the prologue-filler (given 5000ms).
```

and every ~5 s:
- **client:** `session_tracker: marking session #0 ... unsuitable. Reason=join-failed` (reason code 11)
- **server:** `ev=gameplay stage=membership result=held reason=no_host_session`

**The single Phase-5 blocker:** the region's **advertisement never reaches `ready`** — it stays `pending` because `group::activity_host_session()` for region 48's machine id returns `kAbsentSessionId` at the moment the membership push runs. Without a *present* host-session descriptor, the server publishes no join descriptor, the client's citizen-join has no target, membership is held, and the managed session never starts.

**Goal of Phase 5:** make the region-48 activity host session **`ready` and joinable** so membership publishes → the client's citizen-join to the embedded host at `127.0.0.1:30976` completes → peer session established → entity slots flow → mission finishes loading.

**Honest ceiling (unchanged):** a completed peer session gets us *into* a live instance (session + entity slots + script runners), NOT guaranteed content. Enemies/acts/cinematics are the **authored content layer** (Phase 8, separate) — isinternet still saw "0 acts" past the peer session. The authored machinery (currently observe-only) is the groundwork for that; do not remove it.

---

## 1. Where the line is now — the two-path architecture

There are **two distinct load paths**, and Phase 5 lives on the *public* one:

- **Public / peer path (ACTIVE, this is Phase 5):** `region_force_public` forces the mission's slice set "public" so the client joins the embedded host as a **peer** via the **citizen-join** (patrol-style), not a fireteam-join. This is the "get in" track. Everything below is this path.
- **Authored path (PARKED, observe-only):** the authored-launch machinery (gate spoof `0x1763B20`, fork-force `param6=1`, authored ctor `FUN_7FF6197C7C00`, manager populate). This is the **content** track (acts/cinematics/AI). It is *mutually exclusive* with `region_force_public` for missions — the authored fork routes to a fireteam-join and bypasses the public transition. Flags `kDoSpoof`, `kDoActivate`, `kForceAuthoredFork`, `kFillPayload`, `kDoRewrite` are all **false**. Leave them off for Phase 5.

```
Boot─Orbit─Override ─ Search ─ NAT ─ QoS xport ─ QoS suitability ─ World transition ─┐
     DONE             DONE     DONE   DONE          DONE (qos_suitable) DONE (t=78828) │
                                                                                        ▼
   YOU ARE HERE ────────────────────▶  Managed-session-start / activity membership   ✗ BLOCKED
                                        server: result=held reason=no_host_session
                                                          │
                                Peer citizen-join / DTLS connect to 30976   ✗ never completes
                                                          │
                                Peer session established → entity slots → playable    ✗ not reached
                                                          │
                                Content: acts / objectives / AI / enemies (authored)  ✗ Phase 8
```

---

## 2. The exact failure, measured (build `13681439`, session 2026-08-18)

Client-side transition succeeds:
```
t=78672 setup:activity_world_transition: Setting activity-name from override 'mission_towerfall'
t=78828 world_controller: successfully changed world to: mission_towerfall
t=78828 Entering state 'activity:initial_slice_set_loading'
t=78922 initial_slice_set_loading: Waiting for managed-session-start for all peers.
t=83172 slice_set_transition_manager: Region 'PUB48.48' 'disc' -> 'ctng' (starting ambassador matchmaking)
t=83906 initial_slice_set_loading: Timeout entering the prologue-filler (given 5000ms).
```

Then, repeating every ~5 s until it gives up (~t=148313, world → empty):
```
client  session_tracker: marking session #0, name=UNKNOWN, s_id=5FA9CCE3:D8DFA32B unsuitable. Reason=join-failed
server  ev=gameplay stage=membership result=held reason=no_host_session
server  ev=gameplay stage=activityhost result=allocated session=0x9EAA300100200002 group=0x2BA3DFD8E3CCA95F held=1
```

Key observations:
- The client's own **fireteam** session establishes fine (peer #0 = self, `_established`). The problem is the **activity/managed** session (the join to the host).
- The server **does** allocate an activity host session (`result=allocated session=0x9EAA300100200002`), yet the membership push still reports `no_host_session`. That is the crux: **allocation and query are out of phase / keyed differently** (see §4).
- Reason code 11 = `join-failed` (reason table base `0x7ff61a0b98a0`, `[11]="join-failed"`).

---

## 3. The client side of Phase 5 (what the retail client is doing)

From prior runs on this path (memory `homecoming-matchmaking-pivot.md`), the citizen-join sequence is:
```
slice_set_transition_manager: Entering citizen join task ...
Starting citizen join for region 'PUB48.48' to session id '<SEARCH-SESSION>', AH ID '9EAA3001:00200002'.
Ambassador is activity peer '<N>'.
join-remote: [group_target:<...>] attempting join ... class=system-link-demonware type=group join_type=citizen
  → failure pending (result '1') ... FAILED because the session disappeared while we were joining. Will retry.
```

Two historical failure shapes on this path, both now understood:
1. **`ambassador != self` (slot 1):** `group_target` resolves to all-zeros because peer #1 does not exist offline → "session disappeared." **Fixed** by making `ambassador_slot` return self (see §4, already applied).
2. **`ambassador == self` (slot 0):** the solo client becomes the ambassador, *searches* for an existing region session to join, and finds none → `Search [ACTIVE], Gather [INACTIVE]` forever. **Fixed** by returning the local host as a search result (the whole matchmaking-search work, `HOMECOMING-MATCHMAKING-HANDOFF.md` / `HOMECOMING-QOS-HANDOFF.md`).

With both of those solved and QoS suitability forced, the client now *reaches* the citizen-join and issues the managed-session-start — and the server holds it with `no_host_session`. **That hold is the entire remaining Phase-5 problem.**

---

## 4. The server side — where `no_host_session` comes from (the heart of Phase 5)

### 4.1 The advertisement readiness state machine

`server/gameplay/gameplay_advertisement.h` defines three states:
```cpp
enum class AdvertisementState : std::uint8_t {
    ready,    // builds → push carries the descriptor + host session
    pending,  // only the region's activity host session is missing; next service slice fills it
    absent,   // channel advertises nothing; published body unchanged either way
};
```

`advertisement_state(regionIndex)` (`gameplay_advertisement.cpp:223`) is just `build_candidate(...)` mapped:
```cpp
switch (build_candidate(regionIndex, kQueriedMemberSlot, candidate)) {
case Skip::none:          return AdvertisementState::ready;
case Skip::noHostSession: return AdvertisementState::pending;   // ← our hold
default:                  return AdvertisementState::absent;
}
```

`build_candidate` (`gameplay_advertisement.cpp:151`) returns `Skip::noHostSession` here:
```cpp
join.machineId      = region_machine_id(regionIndex);
join.onlineSessionId = region_identity(identity.onlineSessionId, regionIndex);
bool claimedSlot = false;
const std::uint64_t hostSession = group::activity_host_session(join.machineId, claimedSlot);
if (hostSession == state::activity::kAbsentSessionId) {
    return claimedSlot ? Skip::noHostSession   // slot claimed, not yet filled  → pending
                       : Skip::hostSessionFull; // no slot at all               → absent
}
```

So `pending` ⇔ **a slot is claimed for region 48's machine id but no session id has been allocated into it yet.**

### 4.2 Where `no_host_session` is logged (the hold)

`server/bap/encrypted/push/activity/activity_keepalive_push.cpp:171`:
```cpp
const AdvertisementState advertisement =
    publishesMembership ? advertisement_state(effective_region(...).index)
                        : AdvertisementState::absent;
if (publishesMembership && advertisement == AdvertisementState::pending) {
    // A push made while the advertisement is still being allocated spends that revision on a region
    // record with no join descriptor. The allocation lands in the next service slice.
    log("ev=gameplay stage=membership result=held reason=no_host_session");
} else if (publishesMembership) {
    // ... append_membership_notification(...) — the good path
}
```
Same hold also in `server/bap/encrypted/activity_transaction/activity_transaction_notifications.cpp:32`.

**So membership is deliberately held while the advertisement is `pending`, expecting the next service slice to fill the slot.** Our bug: it never leaves `pending`.

### 4.3 The two-phase host-session allocation (`group/group_host_sessions.cpp`)

This is a **claim-now / allocate-next-slice** design, because allocation advances the State revision that the calling push is committing against (they cannot nest):

- `activity_host_session(machineId, claimedSlot)` → `claim_locked()`: finds or claims a table slot (cap 8), returns `held = kAbsentSessionId` + `claimedSlot=true` on first claim. **Never allocates.**
- `allocate_claimed_host_sessions()` (the **service slice**, called from `group/group_host.cpp:624`) → for every claimed-but-empty slot, `state::activity::prepare_session()` + `commit()`, then stores the real session id into the slot. Logs `stage=activityhost result=allocated`.

So the intended cycle is: push claims slot → `pending` hold → service slice allocates → next push finds `ready`.

### 4.4 The suspected root cause (the diagnostic target)

The log shows **both** `result=allocated session=0x9EAA300100200002` **and** `result=held reason=no_host_session`, repeatedly, at the same time. That means the slot the **allocator fills** and the slot the **advertisement queries** are not the same slot. Candidate causes, in priority order:

1. **Key mismatch.** `build_candidate` claims/queries keyed by `join.machineId = region_machine_id(48)`. The allocation log prints `group=0x2BA3DFD8E3CCA95F` in one run and `group=0x9EAA...` (a machine id shape) in another — inconsistent keys. If the *membership-push* path and the *advertisement_state* path pass different group/machine ids into `activity_host_session()`, the claim they wait on and the slot the allocator fills never coincide. **First thing to verify:** log the exact `groupSessionId` argument at every `activity_host_session()` call site and confirm they match `region_machine_id(48)`.
2. **Service slice ordering / cadence.** `allocate_claimed_host_sessions()` runs from `group_host.cpp` `service` (~line 624). If the push that holds runs on a cadence that never interleaves with the service slice for that region, the slot stays empty. Confirm the service slice actually runs for region 48 between pushes (it clearly ran at least once — `result=allocated` fires — but maybe for a *different* claimed key, per cause 1).
3. **`prepare_session`/`commit` transient failure.** `allocate_claimed_host_sessions` returns early if "the account half is not loaded yet on an early slice." If that keeps failing for region 48's key, the slot never fills. Check for absence of `result=allocated` for the *specific* key the advertisement waits on.
4. **Eviction churn.** Table cap is 8 with LRU eviction. If more than 8 distinct keys cycle (e.g. a changing `groupSessionId` per poll), region 48's slot could be evicted before it's filled. Watch for `result=evicted`.

**Most likely:** cause 1 (key mismatch) — the allocation is happening for a *group session id* while the advertisement waits on a *machine id* (or vice-versa). Resolve the key, and `pending → ready` should follow within one service slice.

---

## 5. Server code map (Phase 5 surface)

All paths under `Dawn/src/`.

**Advertisement / host session (the hold):**
- `server/gameplay/gameplay_advertisement.{h,cpp}` — `build_candidate`, `advertisement_state`, `build_advertisement`, `search_target`, `ambassador_slot` (now returns self), `region_machine_id`, `region_identity`.
- `server/gameplay/group/group_host_sessions.{h,cpp}` — the 8-slot host-session table: `activity_host_session(key,&claimed)`, `claim_locked`, `allocate_claimed_host_sessions` (service slice), `held_host_session`, `free_evicted_host_sessions`, `reset_host_sessions`.
- `server/gameplay/group/group_host.cpp` — `service` (calls `allocate_claimed_host_sessions()` @ ~624; `reset_host_sessions()` @ ~710), membership build (`stage=membership result=built`), group→host mapping.

**Membership push / roster (who holds):**
- `server/bap/encrypted/push/activity/activity_keepalive_push.cpp` — `kKeepaliveIntervalMs=5000`, `kRosterBurstIntervalMs=1000`; citizen advertisement rides the keepalive; the `no_host_session` hold (@171); membership seed/token.
- `server/bap/encrypted/activity_transaction/activity_transaction_notifications.cpp` — the other `no_host_session` hold (@32).
- `server/bap/encrypted/push/activity/activity_roster_push.cpp` — roster + bubble authority grant (`select_grant`/`record_grant`; grant=6 for Homecoming's bubble).
- `server/bap/encrypted/push/activity/activity_membership_push.cpp`, `activity_global_state_push.cpp`, `activity_arrival.{cpp,h}`, `activity_notification_frame.cpp`, `activity_message_push.cpp`.
- `state/activity/membership/...::prepare_refresh` — builds the membership snapshot the push carries.

**Citizen-join / activity messages (the join the client issues):**
- `server/bap/encrypted/activity_message/activity_message_route.{h,cpp}` — `prepare_join` / `prepare_grant` → entity slots; `report_query_answer` (msg 29/31/32; currently parse+log only).
- `server/bap/encrypted/activity_message/membership/activity_membership_route.{h,cpp}` — region/member membership fields.
- `middleware/bap/activity_message/replicate_membership.{h}` — `CitizenAdvertisement` (`descriptor[128]`, `onlineSessionId`, `regionIndex`, `ambassadorSlot`, `present`).

**Transport to the embedded host (30976) — fires AFTER membership publishes:**
- `server/gameplay/endpoint/gameplay_endpoint.{h,cpp}` — the UDP endpoint on port **30976**; NAT intro reply (type 13→12), QoS responder, dispatch to association/dtls/peer.
- `server/gameplay/association/association_host.{h,cpp}` — `route()` (marker 1 = connection control, 0 = established packet), `send_payload()`.
- `server/gameplay/dtls/dtls_host.{h,cpp}` — `route()` (DTLS handshake datagram), `send_payload()` (seal record).
- `server/gameplay/peer/peer_transport.{h,cpp}` — `deliver()` (first bit picks out-of-band vs established), `enqueue_reliable(sessionId,id,size,body,bits)`. Reliable-message layer over the association.

**Matchmaking search (already returns the host):**
- `middleware/bap/matchmaking/response/matchmaking_dynamic_response.cpp` — `encode_search_result` (`f3{f1{f1{f1:128B desc}}}`, schema-correct).
- `middleware/bap/matchmaking/matchmaking_route.cpp` — fills `response.descriptor` from `search_target`.

---

## 6. Client-side hooks currently active (config for Phase 5)

Registered in `client/runtime/client_hook_activation.cpp`. On for the public/peer path:
- `homecoming::region_public` — forces the mission's slice set public at the transition starter's reader (`0xC210F0`, gated to `_ReturnAddress()==+E2B3BD`, towerfall override, mission slice 48). Never forces orbit.
- `homecoming::qos_suitable` — **NEW this session.** Hooks `markUnsuitable` (RVA `0x17DAE30`); converts reasons `{5,10,13}` into the success/suitable marking so the session passes QoS suitability + desirability. (Reasons: 5=payload-failed-to-decode, 10=previously-marked, 13=undesirable.)
- `homecoming::qos_probe` — observe-only (delivery discovery; can be retired).
- `retail_log` — the `ev=retail site=N` pipe + the one-shot `ev=qosverdict` backtrace probe.
- `homecoming::ingest`, `predicate`, `authored`, `staged`, `activate`, `pub_rewrite`, `holder` — **observe-only** (authored-path groundwork; all force flags false). Keep off for Phase 5.

`ambassador_slot` returns **self** (server change in `gameplay_advertisement.cpp`).

---

## 7. Build-out plan — concrete Phase 5 tasks, in order

### Task A — Instrument the host-session key (diagnose the pending stall)
Add logging (server side) to every `activity_host_session()` call and to `claim_locked` / `allocate_claimed_host_sessions`:
- At each call site: the exact `groupSessionId`/`machineId` argument, and the caller (advertisement vs keepalive vs membership).
- In `claim_locked`: which slot index was claimed/found for that key.
- In `allocate_claimed_host_sessions`: which key it filled.

**Confirm/deny cause 1:** does the key the keepalive push waits on (`region_machine_id(48)`) equal the key the allocator fills? If not, that mismatch is the bug — normalize both sides to the same key (`region_machine_id(48)`).

### Task B — Make region 48's advertisement reach `ready`
Depending on Task A:
- **If key mismatch:** route both the query and the allocation through the same `region_machine_id(48)` key so the claimed slot is the one that gets filled.
- **If cadence/ordering:** ensure `group_host.cpp` `service` runs `allocate_claimed_host_sessions()` between the keepalive pushes for region 48 (or eagerly allocate on first claim for the homecoming region).
- **If `prepare_session`/`commit` fails:** ensure the account/state half needed by `prepare_session` is loaded before the first membership push for region 48; retry until `result=allocated` for that key.

**Success signal:** `advertisement_state(48)` returns `ready`; the keepalive path takes the `else` branch (`append_membership_notification`) instead of the `no_host_session` hold; the membership push carries the 128-byte descriptor + host session.

### Task C — Verify the client's citizen-join now finds a target
With membership published, the client's citizen-join should resolve a real `group_target` (not all-zeros) and stop "session disappeared." Watch retail:
- `Starting citizen join for region 'PUB48.48' to session id '...' AH ID '9EAA3001:00200002'`
- absence of `failure pending (result '1')` / `session disappeared`.

### Task D — Complete the peer transport handshake to 30976
Once the join targets the embedded host, the client opens the connection: NAT intro → DTLS handshake → association → reliable peer messages. Server side is `gameplay_endpoint` → `association_host` → `dtls_host` → `peer_transport`. This code exists but has never been exercised by a real join; validate it against the **Demonware SDK reference** (`C:\Users\gauta\Downloads\DemonWare`: `bdSocket/bdDTLS`, `bdConnection`, `bdSocket/bdNAT`). Watch for server `ev=gameplay stage=connect` / `stage=join` / `stage=establish` on 30976.

### Task E — Managed-session-start completes → entity slots → load
When the association establishes and membership is acknowledged, `managed-session-start` fires, `initial_slice_set_loading` proceeds past the prologue-filler, entity slots lease, and the world finishes loading. `prepare_join`/`prepare_grant` (`activity_message_route.cpp`) hand out entity slots.

---

## 8. Success signals (whole of Phase 5)

In order of depth:
1. Server: `stage=advertise result=ok region=48` **and** advertisement query returns `ready` (no `no_host_session` hold on the keepalive).
2. Server: membership notification actually appended (not held) — `stage=membership result=built`.
3. Client: citizen-join resolves a non-zero `group_target`; no "session disappeared."
4. Server: `ev=gameplay stage=connect` / `stage=join` / `stage=establish` on **30976** (association/dtls/peer_transport).
5. Client: `managed-session-start` completes; `initial_slice_set_loading` leaves for the prologue-filler; entity slots lease; no `player_broadcast` entity failure storm.
6. Mission stays loaded (not "world → empty" at ~t=148k).

Reaching #4 is new ground; #5–6 is Phase 5 complete.

---

## 9. Key constants / RVAs

- Region **48**, slice set **PUB48.48** (hash `0x1AD5E415`), game mode `0x800D0030`, silo `0x5C01`.
- Region host session id `0x9EAA300100200002` (activity host allocated); embedded gameplay port **30976**; BAP primary `1.0.0.127:30974`.
- Reason table base `0x7ff61a0b98a0` (`[0]=none`): `4=payload-empty 5=payload-failed-to-decode 6=wrong-type 8=incompatible 10=previously-marked-unsuitable 11=join-failed 13=undesirable`.
- Client `markUnsuitable` = `FUN_7ff61984ae30` (RVA `0x17DAE30`, `void(ctx, byte* session, int reason)`); session flags at `session[0]` (bit `0x02`=unsuitable, `0x20`=suitable/valid); reason stored at `session+0xd0`.
- Client region-public reader `FUN_7FF618C910F0` (RVA `0xC210F0`), starter call site `+E2B3BD`.
- QoS responder + NAT intro in `gameplay_endpoint.cpp`; QoS reply format fully documented in `HOMECOMING-QOS-HANDOFF.md` §3.

---

## 10. Build / test loop

```bash
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- Full deploy needs the game closed (`-BuildOnly` compiles while it runs). Warnings are errors (`/WX`).
- Verify: `md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"` vs `.../Dawn-src/build/x64/Release/steam_api64.dll`.
- New `.cpp`/`.h` → add to `Dawn/Dawn.vcxproj` (ClCompile+ClInclude) AND (client hooks only) register in `client/runtime/client_hook_activation.cpp`.
- Log: `bin\x64\Dawn\logs\dawn.log` (truncated per run; `.old` = previous). `ev=retail` = piped engine log; `server ...` = Dawn server; `client ev=...` = hooks.
- **In-game:** boot to ORBIT → Insert → Activity override → `mission_towerfall` → bubble → slice auto-fills 48 → spawn `none` → load. (Enable override FROM ORBIT, not from a loaded activity.)

## 11. RE assets

- Unpacked image `C:\Destiny 2 Development\destiny2_unpacked.bin`, base `0x7FF618070000`, file offset == RVA. On-disk exe is packed — always use the dump. VMProtect anti-debug (no hardware breakpoints; use PAGE_GUARD if runtime is needed).
- Ghidra headless (Ghidra closed), project `C:\Users\gauta\Ghidra.gpr`, program `destiny2_unpacked.bin`:
  ```
  "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" \
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis [-readOnly] \
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
- **Demonware SDK reference:** `C:\Users\gauta\Downloads\DemonWare` — real bdQoS/bdConnection/bdDTLS/bdNAT/bdLobby/bdMatchMaking source. Use for Task D (peer transport / DTLS) and to validate wire formats.
- **Authored content layer (Phase 8):** `D2-Server-Infrastructure.pdf` is the full reference for the authored activity/route/mode pipeline, the authored-launch dormancy chain, and every function/RVA involved — the "everything else loads" endgame that sits on top of the peer session Phase 5 establishes.

---

*Everything in §2, §4, §5 is measured/read from the current tree this session. The Phase-5 blocker is narrow and diagnosed: the region-48 activity host session never becomes `ready` (stays `pending`/`no_host_session`), most likely a claim-vs-allocate key mismatch. Fix that, and the citizen-join + peer transport (already implemented) should carry the load through to entity slots.*
