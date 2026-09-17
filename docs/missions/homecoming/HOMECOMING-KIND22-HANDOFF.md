# Homecoming mission — kind-22 activation handoff

Written 2026-08-18. For continuing the Red War opening (`mission_towerfall` / Homecoming) work in a
new chat. Read this first, then `project-notes/FAILED_EXPERIMENTS.md` (do-not-repeat ledger).

---

## TL;DR — where we are

The mission loads to **map + banner only** because the authored activity **manager never activates**.
We traced the entire activation chain and found the single missing trigger:

> **Activation is driven by a session-stream message of kind 22 ("host-reestablish"). Dawn's group
> host never sent it. We added a send, but with the WRONG body (8 bytes). The client needs a
> 136-byte body. Writing the correct 136-byte encoder is the next step.**

- **Right trigger, right timing, wrong body size** (8 vs 136 bytes).
- Everything else in the chain works: the mission launches, membership + parameters are delivered,
  the client's picker resolves the authored manager by session id every time.

---

## The breakthrough (confirmed)

The client's activity session-stream consumer switches on a message **kind**. Per launch only three
kinds arrive:

| kind | meaning | Dawn sends it? | picker/manager |
|------|---------|-------------------|----------------|
| 8  | connect-establish | (handshake) | no manager lookup |
| 30 | membership-update | ✅ `SessionMessageId::membershipUpdate = 30` | picker resolves mgr by **session id** → succeeds |
| 38 | parameters-update | ✅ `kParameterUpdateId = 38` | picker resolves mgr → succeeds |
| **22** | **host-reestablish** | ❌ **never** | picker → **activate fn** (drives mgr active) |

- The picker keys on the message's **64-bit session id** (NOT the FTID — an earlier FTID fix was the
  wrong field and did nothing). It resolves to **identity 2** (the authored manager `m4`) every time.
- Case 0x16 (kind 22) passes the *same* table + identity pointer to the *same* picker, then calls the
  **activate** function. Because kind 22 never arrives, the manager resolves but never activates →
  `definition_enabled` stays 0 → no scripts.

**Body format (from collaborator RE):** kind-22 body is **136 bytes**:
`sessionId, machineId, host NetAddr, 128-bit identity field, 144-bit identity field`.

> Note on addresses: the collaborator's function RVAs (`+16DF360`, `+16D56C0`, `+177A0B0`, `+177E940`,
> etc.) are from *their* build and **do not map to our dump** (e.g. `0x177E940` is an audio resampler in
> ours). But the **protocol kind numbers match our `SessionMessageId` exactly**, so the finding is
> build-independent and trustworthy at the protocol level. Do not hook those RVAs in our DLL.

---

## What we need to solve (the next step)

**Write the proper 136-byte kind-22 encoder and send it instead of the 8-byte stub.**

Current stub (build `67d58c1`) sends kind 22 with an 8-byte session-id-only body — the client drops it.

### Plan
1. **Define the wire body** for `hostReestablish` in `middleware/gameplay/group/` (new encoder,
   e.g. `host_reestablish.h/.cpp` or extend `session_messages`). Fields, in order (needs bit-exact
   layout confirmed against the client's reader — see "open questions"):
   - `sessionId` (u64)
   - `machineId` (u64) — the member/machine key
   - host `NetAddr` — same encoding as `descriptor::write_net_addr(host.address, host.port, ...)`
   - 128-bit identity field
   - 144-bit identity field
   - total **136 bytes** decoded size → set `kHostReestablishSize = 136` (currently 8).
2. **Fill it** in `publish_host_reestablish()` (already exists in `server/gameplay/group/group_host.cpp`)
   from data Dawn already has:
   - `sessionId` → `record.sessionId`
   - `machineId` → member key (`state::activity::membership::member_key(...)`, as `publish_snapshot`
     already computes `peerMachineId`)
   - host NetAddr → `endpoint::advertised()` (already used in `publish_snapshot` via `write_net_addr`)
   - 128/144-bit identity fields → from the membership `Identity` we already build
     (`accountSoid`, `opaqueSoid`/character, `memberKey`, `joinIdentity`, `secondaryOpaque`/FTID — map
     these to the two identity fields once the exact layout is known)
3. **Build + deploy + test** (loop below). Watch for `identity_enable` finally firing and the manager
   activating.

### Open questions to nail before/while encoding
- **Exact bit layout** of the 136-byte body (field order, widths, any presence bits). Confirm by
  decompiling the client's **kind-22 reader** in our own dump (find via Ghidra: the session-stream
  switch's case for kind 22, or the reader that consumes 136 bytes and calls the activate path).
- **Which session id** — we used `record.sessionId` (matches kinds 30/38). Confirm the activate path
  wants the same id (it should, since it's the same picker).
- **What the two identity fields (128/144-bit) actually contain** — likely the same identity lanes as
  the membership snapshot (SOID/character/member key). The client's reader tells us.

---

## What is CONFIRMED WORKING (do not re-investigate)

- Steam emulation, sign-in, character, orbit.
- Destination override + native 282→266 correction (override commits via
  `authored_chosen_prelaunch_282`, svc-6 selects `mission_towerfall` activity 266).
- BAP/Demonware services, gameplay networking.
- Group join handshake: membership `built` ×3, parameters (incl. activity-host) delivered and decoded
  by the client, player added, `establish ok`.
- The client's manager picker resolves the authored manager (identity 2) by session id **every time**.
- Map, bubble, and "NEW MISSION — HOMECOMING" banner load.
- DLL builds clean (0/0).

## What is RULED OUT this session (dead ends — do not repeat)

1. **Host-session churn** — the 3 host allocations were 3 different regions (fast-travel), not churn.
2. **FTID / field6 fix** (`make_identity` fallback, build `795a4a0`) — the picker keys on **session id,
   not FTID**. The fallback is harmless but not the key. (Kept in tree.)
3. **Current→target transition / empty-identity target join** — not the blocker; the same host
   establishes fine as PUBLIC CURRENT with an empty character.
4. **`reliable_registry_decode result=rejected`** — benign idle polling (native fn returns false on an
   empty queue), not authored content failing.
5. **Feature flag `DAT_7ff61a6ed5c6`** — it is already **1** at runtime (recorder confirmed); it was
   never the gate. The solo-init `FUN_7ff6197a74f0` is simply **never reached** (Path C dead).
6. **8-byte kind-22 body** — wrong size; client needs 136 bytes (this handoff's task).
7. Forcing individual gates (route byte, manager mode, lifecycle, identity, +0x1AF00) → historically
   freeze/prune (ledger §6/§8/§9). Don't force; supply the right server message instead.

---

## The full activation chain (for reference)

```
mission inert
 └ authored manager (identity 2 / activity 4) never ACTIVATES
    └ +0x1AF00 ("active") = 0 for every manager  (activation never runs)
       └ activate fn never called
          └ the client only calls activate from session-stream case 0x16 = KIND 22
             └ kind 22 (host-reestablish) NEVER SENT by Dawn   <-- THE GAP
```

Manager offsets (our dump, base `0x7FF618070000`): mode `+0x1AEF8`, component/active `+0x1AF00`,
identity `+0x1C7C0`, selected `+0x87C`, registered `+0xE93C`. Definition offsets: enabled `+0x94C`,
activity `+0x24` (default -1). These are verified in our build.

---

## Build / deploy / test loop

```bash
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- Builds `Dawn-src\Dawn.sln` and deploys to `bin\x64\steam_api64.dll` (backs up the old one).
- `-Restore` rolls back the DLL; `-BuildOnly` compiles without deploying.
- Toolchain: **VS 2026/18 BuildTools, PlatformToolset v145, Windows SDK 10.0.26100**. There is **no
  `A:` drive** on this machine (older docs reference it — ignore).
- **MAX_PATH gotcha:** keep the repo at a short path (`C:\Destiny 2 Development\Dawn-src`). A few
  `#include`s are backslash-continued and blow the 260-char limit under long parent dirs.
- Log: `bin\x64\Dawn\logs\dawn.log` (client=debug, file_sink on). The game's retail log is piped
  in as `ev=retail`.
- **Verify the deployed DLL hash matches the build** before trusting a run — `-Restore` between runs can
  leave a stale DLL live.

### In-game repro
1. Launch `destiny2.exe`, sign in, reach orbit.
2. **Insert** → **Activity override** → Activity = `mission_towerfall`, Enabled on.
3. Launch the Homecoming opening from the Farm (triggers CHOSEN 282→266). Reach map + banner, wait
   ~15–20s (so `service()` sends kind-22 after membership/parameters settle).
4. Leave game open, read the log, then close.

### What to grep for
```
ev=gameplay stage=host_reestablish result=queued        # kind-22 was sent
ev=bootflow stage=activity_script_identity_enable n=     # THE payoff: manager activated (was always 0)
activity_script_manager_table ... definition_enabled=1   # or selected=1 / active != 0
_connection_failure_suicide                              # FAH/GAH death (should stop if it works)
```

---

## Key files

- **`server/gameplay/group/group_host.cpp`** — the group host. `publish_host_reestablish()` (the stub
  to fix), `service()` retry loop (sends it after membership+parameters+player), `Admitted` struct
  (`hostReestablishPublished` flag), `publish_snapshot()` (shows how machineId/NetAddr/identity are
  already encoded — copy that for the 136-byte body).
- **`middleware/gameplay/group/session_messages.h`** — `SessionMessageId::hostReestablish = 22`,
  `kHostReestablishSize` (currently 8 → make 136), `MemberState::established = 10`,
  `write_session_only` (the 8-byte stub encoder — replace with a real 136-byte encoder).
- **`middleware/gameplay/group/parameter_messages.h`** — `kParameterUpdateId = 38`.
- **`server/bap/encrypted/activity_message/membership/activity_membership_route.cpp`** — `make_identity`
  (identity lanes + the FTID fallback from `795a4a0`).
- **`client/hooks/bootflow/activity_feature_flag_probe.cpp`** — the recorder/poke we added (Path C, now
  known dead — can be removed later; harmless).
- **`client/hooks/bootflow/activity_script_upstream_probe.cpp`** — the big client-side recorder that
  already hooks `identity_enable`, `manager_ensure`, `manager_table`, etc. (our own "recorder").

## Git state

- Branch **`red-war-gameplay-host`** (deployed base; divergent fork from `spawner`, which holds the old
  authored/AI/spawner work — preserved, plus filesystem backup `_Dawn-src_spawner_backup_20260818`).
- Recent commits:
  - `67d58c1` — kind-22 host-reestablish stub (8-byte body) ← **latest; fix the body here**
  - `68f897a` — activity feature-flag recorder + poke (Path C; dead end)
  - `795a4a0` — FTID/field6 fallback (wrong field; harmless)
  - `f573a70` — deploy red-war-gameplay-host snapshot as base

## RE tooling (we are self-sufficient; no collaborator needed)

- **Ghidra project** `C:\Users\gauta\Ghidra.gpr`, program **`destiny2_unpacked.bin`** (145 MB memory
  dump, base `0x7FF618070000`, **file offset == RVA**). `destiny2.exe` on disk is packed — always use
  the dump. Debuggers get killed (anti-debug); in-process Dawn hooks are the reliable channel.
- **Ghidra headless** works (GUI must be closed first):
  ```
  "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" ^
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -readOnly -noanalysis ^
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  Decompile-to-file scripts live in `C:\Users\gauta\ghidra_scripts\Decomp*.java` (copy the pattern).
- **Capstone helpers** (scratchpad, less reliable than Ghidra — validate results): `d2dis.py` (disasm at
  RVA), `xref.py` (naive E8 direct callers — CORRECT), `align.py` (aligned disasm), `fieldwr.py`
  (`[reg+disp]` refs — prone to misalignment false hits), `vptr.py`. **When capstone and Ghidra
  disagree, trust Ghidra.**
- To confirm the 136-byte layout: decompile the client's **kind-22 reader** (the session-stream switch
  case for kind 22) in our own dump and read the field order/widths directly.

---

## One-paragraph summary to paste into a new chat

> Continuing Destiny 2 Dawn (offline mod) Homecoming mission work. We proved the authored activity
> manager never activates because the client only activates from a session-stream message of **kind 22
> ("host-reestablish")** which Dawn never sent. The manager is otherwise resolved fine by session id
> from the membership(30)/parameters(38) messages we already send. We added a kind-22 send in
> `group_host.cpp` (`publish_host_reestablish`, commit `67d58c1`) but with an **8-byte** body; the client
> needs a **136-byte** body: `sessionId, machineId, host NetAddr, 128-bit + 144-bit identity`. **Next
> step: write the real 136-byte encoder** (all fields are data Dawn already has — see
> `publish_snapshot`), confirm the exact bit layout against the client's kind-22 reader in the Ghidra
> dump, build/deploy, and check whether `activity_script_identity_enable` finally fires. Branch
> `red-war-gameplay-host`; see `HOMECOMING-KIND22-HANDOFF.md`.
