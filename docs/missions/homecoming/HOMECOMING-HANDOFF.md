# Homecoming / Dawn — Session Handoff (2026-08-17)

Complete context for continuing the Destiny 2 **Homecoming** offline-revival work under **Dawn**.
Read this first, then `HOMECOMING-FINDINGS.md` (the long-form findings doc — this file summarises and
supersedes its older sections where they conflict).

**Authorized personal reverse engineering on the user's own machine.**

---

## 0. THE NEW DIRECTION (what the next session should do)

> **Get the fireteam/session machinery working first, then tie it into activity selection.**

This is the user's decision and it is well-founded. Every lever we located for the activity route
turned out to belong to the **fireteam/session subsystem**, not to activity selection:

- `FUN_7FF6197CB8F0` — looked like the selection ingest; is a **fireteam join** (proved by calling it).
- `context+0x18DD0` — looked like the root selection holder; writing it puts the client into a
  **pending fireteam join** it can never complete (`JOINING FIRETEAM` on screen forever).

The client's activity route is downstream of a *session*. Offline there is no real fireteam/session,
so the selection machinery never gets fed. Making a **self-contained, self-completing local fireteam
session** is therefore the prerequisite, not a detour.

**Concrete first questions for that work:**
1. What does a fireteam join need to *complete* rather than hang? The join is queued to a null peer
   (`{0}-DEC01151:DEC01151-0.0.0.0:0:0.0.0.0:0` when we supplied our own id). Find where the join
   target/peer address comes from and whether it can be pointed at ourselves (loopback / self-peer).
2. Dawn **is** the activity host (`ev=retail networking:activity_client:receive_message_from_host`
   shows the client talking to it). Which BAP message completes the fireteam handshake?
3. Once a session exists and completes, re-run `holdprobe` — if `context+0x18DD0` fills *naturally*,
   the pump should propagate and the route may go authored with no forcing at all.

**Do not** write `context+0x18DD0` directly again — see §5 dead-ends.

---

## 1. Goal and the honest ceiling

Target (user's words): **"world load and transition reached"** — this is George Ratington's documented
ceiling from `Wow.pdf`, and **it already works today**:
- world loads as `mission_towerfall`
- destination datum resolves to genuine Homecoming (`+0x24 = 0x80B500BC` = `mission_towerfall.scenario`)
- destination predicate accepts activity 266
- region forced to Homecoming's `slice_set=48`

**Authored mode 1 is BEYOND George's ceiling** — he never reached it either. Per `HOMECOMING-FINDINGS.md`
§11, even his full pipeline yielded no cinematic, objectives, dialogue, AI, encounters, markers or
progression. Do not oversell authored mode as "the mission working".

---

## 2. Build / test loop

```
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- `-BuildOnly` compiles while the game runs; **deploy requires the game closed**.
- `-Restore` rolls back to the previous DLL.
- **Always verify the deploy landed** (a silent no-op if the game was open):
  ```
  md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"
  ```
  Compare against `Dawn-src/build/x64/Release/steam_api64.dll`.
- Repo `C:\Destiny 2 Development\Dawn-src`, branch **`spawner`** @ `6aae441` (not a git repo at the
  top level — `git` commands from the repo root return the branch fine).
- Log: `bin\x64\Dawn\logs\dawn.log` (truncated per run). Retail engine log is piped in as
  `ev=retail` — **extremely valuable, it names things directly**.
- **Current clean DLL: `8e050583deb8`** (all harmful modules disabled).

### In-game workflow
The Homecoming redirect is **not** a saved setting — it is a runtime toggle:
1. Press **Insert** → Dawn overlay.
2. **"Activity override"** section → Enabled on → Activity `mission_towerfall` → pick a Bubble
   (auto-fills slice set 48) → Spawn set `none`.
3. **Load order matters**: the first activity after boot has no prior session. Load *something* first,
   then enable the override, then load Homecoming.

---

## 3. Reverse-engineering assets

- Unpacked image: `C:\Destiny 2 Development\destiny2_unpacked.bin`, **base `0x7FF618070000`**.
  File offset == RVA (memory-aligned dump). The on-disk exe is packed — always use the dump.
- Ghidra headless (Ghidra must be closed), scripts in `C:\Users\gauta\ghidra_scripts\`:
  ```
  & "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" `
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis -readOnly `
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  **Gotcha:** write output paths with forward slashes (`"C:/Destiny 2 Development/x_out.txt"`) —
  backslashes get mangled and produce `illegal escape character`.
  Existing outputs in `C:\Destiny 2 Development\`: `predicate_out`, `route_out`, `pump_out`,
  `launcher_out`, `homecoming_out`, `session_out`, `session_out2`, `wire_out`, `bitlib_out`,
  `bitmap_out`, `ingest_out`, `ingest2_out`, `payload_out`, `payload2_out`, `payload3_out`,
  `lever_out`, `ctx_out`, `idgen_out`.
- Python + capstone + numpy available. Linear disassembly **desyncs** on this image — use
  byte-pattern/vectorized scans, not naive linear sweeps.

---

## 4. VERIFIED FACTS (measured, trust these)

### 4a. The predicate is exonerated
`FUN_7FF618C768F0` (RVA `0xC068F0`) **ACCEPTS** activity 266 (`ret=0x1`). The only REJECT observed was
for `p1=8`, which is correct. George's "the predicate rejects 282→266" was a **misdiagnosis**.

### 4b. The §5b "opaque authored data" wall is BROKEN
The client **does** encode a complete authored descriptor offline. Captured via a hook on Dawn's
svc-6 parse (`ev=bap svc=6 stage=descbits`) and fully decoded (620/620 bits, zero leftover):

| field | value |
|---|---|
| sensitive scalar #1 | `0x9EAA300100100100` = **our own configured `primary_soid`** |
| sensitive scalar #2 | the **fireteam/activity nonce** (game logs it as `fireteam nonce=XXXXXXXX-XXXXXXXX`) |
| three 32-bit hashes | `0x811C9DC5` (FNV-1a offset basis = "none" sentinel) |
| element index | present, `-1` sentinel |
| tails | present, count 0 (but see below) |

- Wire descriptor **field model is proven**: modelling `activity_manager_descriptor_parser.cpp`
  field-by-field yields **372 bits exactly** for the minimal form (corroborated by Dawn's own code
  comment "the common form is 372") and **620** for all-optionals.
- **96-bit reference entries DO occur** (`edz_freeroam` carries count=1 → 716 bits). An earlier claim
  that they never appear was over-generalised from one sample — corrected.
- The nonce is **re-minted every activity setup** (3/3 different) **and is minted AFTER the route has
  already committed** (route t=75125 → nonce t=75219, etc.). So load N's descriptor cannot be built in
  time for load N.
- Dawn can **synthesize** a byte-exact valid 620-bit authored descriptor — code exists and works
  (see §6).

### 4c. The route's dependency chain (observed, not inferred)
`route_commit` **never writes** the route byte. It reads a record that propagates:

```
(writer unknown / never fires offline) --vt +0xB8--> context(0) + 0x18DD0      [ROOT holder]
                                                          | pump reads via vt +0x98
                                                          v
        selection pump FUN_7FF6197CE520 --vt +0xB0--> context(slot) + 0x19068  [SLOT holder]
                                                          | FUN_7FF6197C1370 reads
                                                          v
                        FUN_7FF6197C51A0 / pump  -->  route_commit FUN_7FF6197C1F50
```

Resolved from a **live vtable** (`holdprobe`):

| slot | RVA | VA | role |
|---|---|---|---|
| vtable | `0x1CA79D0` | `0x7FF619D179D0` | holder class |
| get `+0x98` | `0x1778C70` | `0x7FF6197E8C70` | `return holder + 0x148` (record lives there) |
| set `+0xB0` | `0x17AD310` | `0x7FF61981D310` | writes per-slot holder |
| set `+0xB8` | `0x17AB350` | `0x7FF61981B350` | writes root holder |

Measured: **root holder never populated offline** (`census root=[0,0,0]` all session, **zero `+0xB8`
calls**). All writes are the **pump itself** (`caller_rva` `0x175EF59` / `0x175FC0D`, both inside
`FUN_7FF6197CE520` @ `0x175E520..0x175FE5C`) writing **local** records (`route=0x00, source=1,
dest=-1`, kind cycling `1,2,3,5,6,7,0`). Slots 1 and 2 never receive a record at all.

**Interpretation (corrected):** `context+0x18DD0` is the **fireteam/session** holder. The pump reads it
because the session determines the activity. It is not a place to inject a selection.

### 4d. Record layout (0xA8 bytes)
| offset | meaning |
|---|---|
| `+0x00` | kind (`FUN_7FF6197C51A0` forwards to route_commit only when `== 5`) |
| `+0x08` | source activity index |
| `+0x0C` | dest activity index (`-1` = none) |
| `+0x12` | **route byte** (0 = local, nonzero = authored) |
| `+0x18` | identifier — must be neither `0` nor `1`; generator is `FUN_7FF61844DB90(out)` @ RVA `0x3DDB90` |
| `+0x20..0x9F` | 0x80-byte payload (in the join path this is a **join/session target**, zeros parse as peer addresses) |
| `+0xA0` | kind word (genuine call sites pass 4) |
| `+0xA1` | must be 0 |

### 4e. Pump gates (from its decompile, both now measured)
```c
if (ctx0 && (mode - 4U < 6) && (mode - 6U < 4)) {            // mode = *(int*)(ctx0+0x1AEF8), needs 6..9
    if (populated(root) && *(long long*)(rec+0x18) not in {0,1})
        adopt(rec);                                           // the propagation we want
}
```
Measured: **`mode=6` — the mode gate PASSES offline.** The cloned record's `+0x18` was `0x0` (sentinel)
— that part of the diagnosis was correct.

### 4f. Other measured facts
- Host readiness `FUN_7FF6197C40F0` (needs `ctx+0xE93C == ctx+0x87C`) **holds offline**.
- `FUN_7FF618C96490(index)` is a plain pointer accessor — safe to call from any thread.
- `FUN_7FF6197FDAB0(holder)` is just `*(byte*)(holder+0x140) & 1` — the populated flag, readable directly.
- Game bit-stream library: `read_bits(stream,n)` `0x7FF6183C13B0`, `read_wide` `0x7FF6183C1070`,
  `read_bool` `0x7FF6183C0EF0`, `stream_status` `0x7FF6183BE9B0`. MSB-first, 64-bit accumulator at
  `+0x28`, cursor `+0x24`. Matches Dawn's `encoding::bits::Reader`.
- The descriptor codec is **schema/table-driven**, not hand-coded: across 435 `read_bits` call sites,
  width 12 appears only twice and the `4,12,12` opening appears nowhere. Only 4 sites read 96 bits.
- `player_broadcast` entity-creation failures and `Abort matchmaking` are **baseline offline noise**,
  not caused by any of our modules (verified: present with all modules disabled).

---

## 5. DEAD ENDS — do not repeat (each disproved by measurement)

1. **The destination predicate** — exonerated, it accepts 266 (§4a).
2. **Service-6 descriptor rewrite** — wrong lever and ~200ms too late; svc-6 fires *after* the route
   commits. Still present as `apply_authored_route`, harmless.
3. **Message-1 global-state push** — **falsified as the route lever.** A valid 620-bit authored
   descriptor (282→266) was delivered in every periodic push, **5.7 s before** the route commit
   (delivery proven by body-size arithmetic: `1161−372+N` bits → 620 gives `body=177`, observed).
   Result: route still LOCAL, manager still mode 6. Message-1 never touches the holder that matters.
4. **Host-assigned activity nonce** — the client ignored our nonce (`0x5111C0DE5111C0DE` never appears
   in its log) and minted its own.
5. **Calling `FUN_7FF6197CB8F0`** ("selection ingest") — it is a **fireteam join**. It echoed our data
   back: `networking:logic:join: [5111C0DE-00000001] queued new fireteam to fireteam join to
   [{0}-DEC01151:DEC01151-0.0.0.0:0:0.0.0.0:0]`. Caused a pending fireteam. Module
   `hooks::homecoming::inject` **DISABLED**.
6. **Writing `context+0x18DD0` directly via the `+0xB8` setter** (`hooks::homecoming::root`) — the write
   **succeeds and persists** (`root_populated=1`, census `root=[1,0,0]`, first time ever offline, no
   pruning), and with a valid `+0x18` identifier it puts the client into **`JOINING FIRETEAM` forever**.
   Route stayed LOCAL. Module **DISABLED**.
7. **Client-side forcing of the route byte / mode bit** (`authored_force.cpp`) — prunes the session.
   Long-standing dead end, still disabled.

**Meta-lesson, stated plainly:** four hypotheses this session were derived from reading decompiled
structure and each was wrong; every one was caught by runtime measurement. **Instrument first, and
trust the retail log's own words over structural inference.**

---

## 6. Dawn code written this session

### Server-side (still ENABLED — harmless, arguably useful)
- `server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp`
  → `report_descriptor_bits()` logs `ev=bap svc=6 stage=descbits` with bit length, `name_bit`, and full
  raw hex of the client's descriptor. **This is how the §5b wall was broken — keep it.**
- `state/activity/forced/activity_forced_destination.cpp`
  → `build_authored_descriptor()` synthesizes a byte-exact 620-bit authored descriptor
  (`kind/source=282/dest=266`, real SOID from `state::account_snapshot()`, host nonce
  `kHostAssignedNonce`, all unknown fields reproduced from measured captures). Verified correct by
  round-tripping through an independent decoder. Logs `ev=bap stage=authored result=built`.
- `server/bap/encrypted/push/activity/activity_global_state_push.cpp`
  → `resolve_state()` applies the forced override so every periodic message-1 push carries the authored
  descriptor. Delivery works; it is simply not the route lever.

### Client-side probes (ENABLED, observation only, all forward calls unchanged)
| module | log tag | what it gives |
|---|---|---|
| `ai/manager_probe` | `ev=mgrprobe` | manager path local/authored + `stage=route` route byte/source/dest |
| `ai/pump_probe` | `ev=pumpprobe` | per-slot `id/mode/route` each pump invocation |
| `ai/mode_probe` | `ev=modeprobe` | the 4-gate mode dispatcher |
| `homecoming/homecoming_probe` | `ev=hcprobe` | launcher/publisher + `accessor+0xA30` + content tags |
| `homecoming/predicate_probe` | `ev=predprobe` | destination predicate verdicts (**note: `verdict` tests the full 64-bit return; the native returns a bool in AL — mask `ret & 0xFF`; cosmetic only**) |
| `homecoming/ingest_probe` | `ev=ingprobe` | hooks `FUN_7FF6197CB8F0`; logs args + 0x80 block |
| `homecoming/holder_probe` | `ev=holdprobe` | **resolves holder vtable from a live pointer**, detours both setters, logs every write with `caller_rva`, plus a census of all six holders' populated flags |

### Client-side actions (DISABLED — commented out in `client_hook_activation.cpp`)
- `homecoming/authored_inject` — calls `FUN_7FF6197CB8F0`. Causes a phantom fireteam join.
- `homecoming/root_publish` — writes the root holder. Causes `JOINING FIRETEAM` hang.
- `homecoming/authored_force` — long-standing pruning dead end.

**Install-order gotcha:** two modules must not signature-scan the same function — a detour rewrites the
prologue and the second scan fails (`reason=target`). Resolve addresses from a **live vtable** where
possible, or order installs so the scanner runs before the detour.

---

## 7. Useful scratch tooling (recreate as needed)

- **Descriptor decoder** — walks the wire descriptor per the parser's field model, self-tests against a
  synthetic 372-bit minimal form. Given `bits` + `hex` from a `descbits` log line it prints every field.
  Reproduce from `activity_manager_descriptor_parser.cpp`; the field list in §4b is enough.
- **Bit budget / uniqueness checker** — proved 372 and 620 exactly, and also proved the 620 composition
  is **not unique** (45 field combinations total 620; 28 need 96-bit refs). Use it to avoid over-reading
  bit-count arithmetic.
- **Vectorized xref scanner** — finds rip-relative references and `E8` call sites without linear
  disassembly (which desyncs). Needed for anything image-wide.

---

## 8. Key constants

- `282 / 0x11A` CHOSEN source · `266 / 0x10A` mission_towerfall (Homecoming) · `48 / 0x30` slice set
- `0x9ACCB518` activity-definition hash · `0x80B500BC` scenario · `0x80B500AC` activity ·
  `0x80FDB97F` launch descriptor
- `0x811C9DC5` FNV-1a offset basis = the "names nothing" sentinel
- `primary_soid` `0x9EAA300100100100` (from `bin/x64/Dawn/settings.json`)
- manager identity `+0x854` (0 = orbit, 1 = mission) · activity mode enum `ctx+0x1AEF8`
- root holder `ctx+0x18DD0` (**fireteam/session**) · slot holder `ctx+0x19068` · record `holder+0x148`
  · populated flag `holder+0x140` bit 0

---

## 9. State at handoff

- Deployed DLL should be **`8e050583deb8`** — all harmful modules disabled, probes active, override
  works, world loads Homecoming.
- If the user is stuck on `JOINING FIRETEAM`: deploy that DLL and restart the game. Nothing is
  persisted; the holder is runtime-only.
- `HOMECOMING-FINDINGS.md` contains the long-form record including §8a–§8f written this session. Where
  it conflicts with this file, **this file is newer**.
