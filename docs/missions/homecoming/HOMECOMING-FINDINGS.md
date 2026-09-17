# Homecoming Reimplementation — Complete Findings

All findings from reverse-engineering Destiny 2's **Homecoming** (Red War opening) mission for
offline revival under Dawn. Written 2026-08-17. Reimplementing collaborator **George
Ratington's** pipeline (`Wow.pdf`) from scratch, module by module, measured at each step.

> **Direction as of this doc:** we are building our **own server-side** authored activity-selection
> injection. The critical blocker to solve is the **destination predicate at `0xC068F0`** (see the
> dedicated section). The client-side force approach is a confirmed dead-end (prunes the session).

---

## 1. The problem in one paragraph

Spawned/loaded combatants are bare prefabs (no name, no AI movement) and Homecoming stays a dead
shell because the activity manager for **identity 1 never enters "authored mode 1"** — it stays in
local **mode 6**, which skips authored receiver/component creation (mission director slot 35,
activity script slot 18, encounters, AI parameterization). Mode is gated by an authored **route**,
which is set from an authored **activity selection descriptor** the game never produces offline. The
world loads (as `mission_towerfall`) but every manager stays local mode 6.

---

## 2. Build & test loop

```
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release
```
- `-BuildOnly` compiles without deploying (game can stay open). Deploy requires the game closed.
- `-Restore` rolls the DLL back to the last backup (use if a change won't boot).
- Deploy overwrites `bin\x64\steam_api64.dll` (Dawn is a `steam_api64` proxy; the real Steam DLL
  is at `.dawn\original\steam_api64.dll` — never overwrite it).
- **Always confirm deploy:** `md5sum bin/x64/steam_api64.dll Dawn-src/build/x64/Release/steam_api64.dll`
  must match; if the game was open the copy silently fails and you run the old DLL.
- Repo: `C:\Destiny 2 Development\Dawn-src`, branch **`spawner`** @ `6aae441`.
  **George's Homecoming code is NOT in any branch** — we rebuild it.
- Log: `bin\x64\Dawn\logs\dawn.log`. Retail log piped in as `ev=retail`. Probes emit `ev=…`.

---

## 3. Reverse-engineering assets

- **Unpacked image**: `C:\Destiny 2 Development\destiny2_unpacked.bin` (145 MB). On-disk exe is
  packed — always analyse the dump. **Base `0x7FF618070000`.** All of George's PDF RVAs apply at
  `base + RVA` (our dump IS his Season of Arrivals build — verified).
- **Redacted share copy**: `destiny2_unpacked.shared.bin` (Steam ID / username / SOID zeroed, same
  size so offsets line up).
- **Ghidra**: project `C:\Users\gauta\Ghidra.gpr` (analysis complete). Headless (Ghidra must be
  closed):
  ```
  & "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" `
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis -readOnly `
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  Scripts in `C:\Users\gauta\ghidra_scripts\`; outputs to `C:\Destiny 2 Development\*_out.txt`
  (`homecoming_out`, `route_out`, `pump_out`, `launcher_out`, `predicate_out`).
- **Python RE helpers** (session scratchpad): `d2dis.py` (capstone disasm, VA-aware), `callers.py`,
  `siggen.py` (wildcarded unique sigs), `xref.py`, `dumpstr.py`, `window.py`. Base `0x7FF618070000`.

---

## 4. Verified execution chain (client side)

```
BAP server publishes activity selection            <-- via activity MESSAGE 1 (global state push),
   (the authored 0x1B0 descriptor)                      NOT service 6 (see dead-ends)
 -> client ingests, native producer builds the 0x1B0 runtime descriptor
 -> descriptor route byte +0x12  (0 = local, nonzero = authored)
 -> route_commit FUN_7FF6197C1F50 branches on +0x12
 -> route_update FUN_7FF61981C390 applies to manager
 -> selection pump FUN_7FF6197CE520 reads per-slot state[slot*0x1B + 2]
      == 0  -> local  init FUN_7FF6197E23C0 -> FUN_7FF6197E2440   (mode 6)  <-- ALWAYS taken now
      != 0  -> authored init FUN_7FF6197FFE00 / FUN_7FF6197E1AA0 -> FUN_7FF6197E3200 (mode 1)
 -> authored manager creates receiver/component path FUN_7FF6197B8C50 / dispatch FUN_7FF61766A30
      (mission director slot 35, activity script slot 18, encounters, AI parameterization)
```

### Key addresses (base `0x7FF618070000`)
| Function | VA | Role |
|---|---|---|
| **destination predicate** | `0x7FF618C768F0` | **REJECTS 282→266 (see §7)** |
| activity-selection launcher | `0x7FF618C6A5A0` | copies 0x1B0 from `accessor+0xA30`, publishes |
| selection-launch accessor | `0x7FF618C69FA0` | returns object whose `+0xA30` holds the 0x1B0 desc |
| current-selection publisher | `0x7FF61981DA60` | copies 0x1B0 into owner lane, vtable-publishes |
| route_commit | `0x7FF6197C1F50` | branches on descriptor `+0x12` route byte |
| route_update | `0x7FF61981C390` | applies route to manager |
| selection pump | `0x7FF6197CE520` | per-slot local-vs-authored decision (`state[slot*0x1B+2]`) |
| local mgr init | `0x7FF6197E2440` | mode 6 (shared init; dispatcher passes mode 4=local, 0xC=authored) |
| authored mgr init | `0x7FF6197E3200` | mode 1 |
| 0x1B0 descriptor init (state-0 producer) | `0x7FF618547940` | builds the runtime descriptor |
| datum registry | `+0x2439C70` | index=handle&0x1FFF; table=(handle>>13)&sel; +0x30 size, +0x34 mask, +0x08 data; record fixup `rec -= mask & *(u64*)(rec+8)` |
| identity index on a manager | `+0x854` | 0 = orbit, 1 = mission |
| activity-client slot state | `manager + 0x27D0 + identity*0x2D0` | -1 = unset |

---

## 5. The two descriptor representations

**(a) The 0x1B0 RUNTIME selection descriptor** (built by the client's native producer, lives at
`accessor+0xA30`, published by `FUN_7FF61981DA60`):
- dword0 packed: `reason[0:4] source[4:16] dest[16:28]` (confirmed).
- route byte at `+0x12`; package name ~`+0x50`.
- The **minimal/local** form (all we ever see offline): small source/dest, body of `FFFF0000`
  sentinels. George's "372-bit minimal, stays local mode 6".
- The **authored** form (George's "620-bit"): only built when a real CHOSEN selection is ingested.
  **Never happens offline** — the CHOSEN Director node does not exist to select.

**(b) The WIRE activity-selection descriptor** (BAP service-6 / message-1, parser
`middleware/bap/activity_host_manager/request/selection/activity_manager_descriptor_parser.cpp`).
From bit 0, each field wire-biased by +1:
```
reason[0:4]  source[4:16]  dest[16:28]
optional element index (presence + 9 bits)
skull count[5]  then skullCount × (group[2] + value[7])
unknown byte[8]  optional peer byte
optional arrival-bubble hash[32]  optional spawn-set hash[32]
package name (40 signed bytes, bias 128)
trailing boolean[1]
tail array 1 (1-bit count, 96-bit entries)   tail array 2 (6-bit count, 13-bit entries)
```
Minimal form ≈ 372 bits, authored CHOSEN ≈ 620 bits. **The ~248-bit delta is the "opaque authored
data" we CANNOT source offline** (not in content tags, not capturable). This is the fundamental
constraint.

---

## 6. Dead-ends (do not repeat)

1. **Client-side force of authored mode / route byte — PRUNES.** Forcing `route_byte=+0x12=1`, or the
   pump's `state[+2]`, or the manager mode bit 0x8 (via `FUN_7FF6197E2440`) produces incomplete
   route state and the session prunes (`Deleting session` + `Abort matchmaking`). George confirmed:
   *"the whole genuine authored descriptor must pass before route commit."* Force code is in
   `client/hooks/homecoming/authored_force.cpp` but **disabled** in activation.
2. **Service-6 descriptor rewrite — WRONG LEVER + TOO LATE.** The svc-6 handler
   (`activity_host_manager_route.cpp`) ECHOES the client's `descriptorBits`; it does not build them.
   Measured: managers/route commit at t≈71.3–71.5s, the single service-6 fires at t≈72.0s — the route
   is decided BEFORE service-6. Rewriting the echoed descriptor's source/dest/reason to 282/266/0
   (built, gated to towerfall in `activity_forced_destination.cpp`, logs `ev=bap svc=6
   stage=authroute`) had zero effect. The route lever is the **message-1 global activity-state push**,
   earlier. That rewrite is inert; leave or remove.
3. **Navigating to the CHOSEN node to capture the authored descriptor — impossible offline.** The Red
   War / CHOSEN Director node does not exist to select. The only thing that ever populates `+0xA30`
   is the minimal descriptor.

---

## 7. THE PREDICATE — EXONERATED (was the presumed blocker; runtime probe disproves it)

> **UPDATE 2026-08-17 (predprobe run):** The predicate is NOT the blocker. Runtime hook of
> `FUN_7FF618C768F0` during a forced-Homecoming load (`predprobe`, log below) shows: when asked
> "is the current destination activity **266**?" it returns **`0x1` = ACCEPT** (`t=157922 p1=266
> … ret=0x1 verdict=ACCEPT`). The `destbytes` dump proves the resolved destination datum is genuine
> Homecoming — `+0x24 = 0x80B500BC` (mission_towerfall.scenario), `+0x3C = 0x811C9DC5`. The only
> REJECT observed is `p1=8` (correct: the destination is 266, not activity 8). George's "why does the
> predicate reject 282→266" was a **misdiagnosis**.
>
> **The real blocker is ORDERING (measured):**
> | t (ms) | event | meaning |
> |---|---|---|
> | 146875 | `mgrprobe path=local(mode6) f854=1` | identity-1 mission manager created **LOCAL/mode6** |
> | 147079 | `mgrprobe route route_byte=0x00 source=1 dest=-1` | route commits **LOCAL**, dest **-1 (unset)** |
> | 147579 | `svc=6 authroute 282→266` (inert §6.2) | fires *after* commit |
> | 149829 | `modeprobe verdict=AUTHORED(mode1) gates=1111` | mode dispatcher flips authored — 3s too late |
> | 153516 | `bootflow region result=forced slice_set=48` | override's effects land |
> | 157922 | `predprobe p1=266 ret=0x1 ACCEPT`, dest=Homecoming | destination finally authored |
>
> Identity-1's manager is built LOCAL and its route commits LOCAL (dest=-1) **~3–11 s before** the
> authored destination exists. The manager is **never re-created** (no `authored(mode1)` init in the
> whole log; all 4 mgr hooks were installed, so one would have logged). `modeprobe` sitting at
> AUTHORED from 149829 is George's "green yet local": the gate conditions become true, but the mission
> manager was already built local and nothing rebuilds it. Same disease as the svc-6 dead-end (§6.2),
> now timed precisely: **the authored selection commits too late to beat the identity-1 manager pump
> at t≈146.9 s.** The pump (`FUN_7FF6197CE520`) marks a slot done after creating its manager
> (`*(plVar29+0x12)=1`), so re-running the pump alone will not re-create it. New frontier: either land
> the authored selection before t≈146.9 s (§8 message-1 push, now with a deadline), or force a
> re-pump *with the slot's created-flag reset* AND a nonzero per-slot route byte. **Next probe:
> `pumpprobe` — hook the pump, read per-slot selection route byte (`getter+0x12`) + identity
> (`ctx+0x854`) + mode (`ctx+0x1aef8`) at each invocation, to learn whether slot-1's route byte ever
> becomes nonzero and whether the pump re-runs after the override.**
>
> Probe bug (does not affect conclusions): `predprobe verdict` tests the full 64-bit return; the
> native returns a bool in AL, so early `why=dest_null` lines mislabel AL=0 as ACCEPT. Mask `ret&0xFF`
> next revision. The two decisive lines (266→0x1, 8→0x0) are clean.

### Historical framing (now disproved — kept for context)
George's exact frontier: **"282 (CHOSEN) to 266 (Homecoming) is already present before route
readiness, and I was determining why destination predicate `0xC068F0` rejects it."**

`FUN_7FF618C768F0(short activityIndex, char* name)` — validates whether the **current destination
datum matches a requested activity**. Decompiled (`predicate_out.txt`):

```c
undefined8 FUN_7ff618c768f0(short param_1, char *param_2) {  // param_1 = activity index, param_2 = name (usually null)
    puVar5 = 0;                                  // current destination record
    if (FUN_7ff618c91370() &&                    // GATE 1 (see below)
        (mgr = FUN_7ff618ea6c30()) != 0 &&       // manager singleton
        FUN_7ff618c91370()) {                    // GATE 2
        idx = *FUN_7ff618c91fe0();               // a datum index
        tbl = FUN_7ff618c91fe0();
        if (idx != -1 && *(int*)(tbl + 0x14 + idx*8) != -1)
            puVar5 = FUN_7ff618439450(idx);      // datum lookup: (idx&0x1FFF)*size + base
    }
    if (FUN_7ff61857bf00() == 0) return 1;        // readiness FALSE -> ACCEPT
    if (puVar5 != 0) {
        if (param_2 && *param_2) return FUN_7ff61843c1e0(puVar5, param_2);   // compare dest name to `name`
        if (param_1 != -1) {
            act = lookup_activity(param_1);       // FUN_7ff61857a210 vtable +0x460
            if (act) { name = act+0x68 field; return FUN_7ff61843c1e0(puVar5, name); }  // compare dest to activity param_1's name
        }
    }
    return 0;                                      // REJECT
}
```

**GATE `FUN_7FF618C91370`:** returns 0 (fail) when the activity-client slot
`*(int*)(manager + 0x27D0 + identity*0x2D0) == -1` (slot unset) AND mode != 4
(`FUN_7FF618BB8550(4)`) AND a fallback (`FUN_7FF61931BCA0` + `*(char*)(FUN_7FF61931B180()+0x5C18)`)
fails.

**Callers:** the launcher/selection cluster — `0x7FF618C6A0C2`, `0xC6A3D2`, `0xC6A452`, `0xC6B541`,
`0xC7EEEB` — all pass `edx=0` (no name) and `ecx=activity index`. So it's asked "does the current
destination == activity N (by index)?"

**Why it rejects 282→266:** returns 0 when readiness is TRUE, a destination is set, but the current
destination datum's name does NOT match the requested activity index. During the transition, when the
game validates "destination == 266 (Homecoming)?", the live destination datum is not yet Homecoming
(it is the CHOSEN 282 / edz / prior), so the name/index comparison fails → reject → route not
committed authored. **The task: make the current destination datum resolve to Homecoming (266) at the
moment this predicate runs, so the comparison passes** — OR understand the ordering so the authored
selection is committed to the destination datum before this validation.

**Open sub-questions for the new chat:**
- What exactly is the "current destination datum" (`puVar5`), and where is it set from the ingested
  selection? Trace `FUN_7FF618C91FE0` (the index source) and `FUN_7FF618439450` (the datum table).
- Decompile `FUN_7FF61843C1E0` (the name comparison) to see the exact match rule.
- Is the rejection at the GATE (slot -1) or the name compare? Instrument at runtime: hook
  `FUN_7FF618C768F0`, log `param_1`, the resolved `puVar5`, its name, and the return value, during a
  Homecoming load.

---

## 8. Server-side plan — TRACED AND CLOSED (option 1 hits the §5b wall, confirmed structurally)

> **UPDATE 2026-08-17 (option 1 traced end-to-end + tail-derivability decode):** The message-1 push
> and the 282→266 rewrite are **already built** and were the exact dead-end measured. The push is
> `server/bap/encrypted/push/activity/activity_global_state_push.cpp` — it sends
> `output.descriptorBits = selection.descriptorBits` (line 64). The override
> (`activity_forced_destination.cpp::apply_authored_route`) already rewrites those bits to
> reason=0/source=282/dest=266 but **preserves the opaque tail "as the client encoded it"** and needs
> a captured descriptor. Offline the capture is empty (`hcprobe stage=publish packed=0 head=00..FFFF`),
> so 282→266 is stamped on an empty body → client reads local (`route_byte=0 dest=-1`), exactly as
> `pumpprobe`/`mgrprobe` measured. **There is no push left to write; the only remaining work was
> synthesizing the opaque body — and that is impossible.**
>
> **Tail-derivability decode (game-free, from `activity_manager_descriptor_parser.cpp` +
> `definition.h`):** minimal form = **372 bits exactly** (reason 4 + source 12 + dest 12 + name 320 +
> presence/count bits, all optionals absent) — matches the code's own "common form is 372". The
> authored form ≈620 bits is a **~248-bit delta**. Derivable optionals (element index 9, three 32-bit
> hashes, peer byte) supply only ~120 bits; reaching 248 **structurally requires ≥128 bits from the
> two 64-bit sensitive scalars and/or the 96-bit reference-tail entries**, which the parser itself
> names **"identity-like scalar," "runtime nonce,"** and an unnamed 96-bit **"reference"** tail array
> that *"carries fields with no known name."* A runtime nonce is non-derivable by definition; identity
> and reference handles are per-session runtime state, in no content tag. Corroborated by measurement:
> writing 282→266 over an empty body still commits local, so the client keys authored-mode off the
> opaque body, not the front scalars. Routes around are all closed: capturing a real nonce needs
> online CHOSEN nav (§6.3); a faked nonce is session-validated; there is no front-scalar-only path.
>
> **Conclusion: authored mode 1 cannot be produced offline. The achievable ceiling is what the build
> already reaches — world loads as `mission_towerfall`, destination = Homecoming (`0x80B500BC`),
> predicate accepts 266 — i.e. George's documented ceiling (§11). This is the end of the authored-mode
> road.** Probes retained (predprobe/pumpprobe/mgrprobe/modeprobe/hcprobe) all diagnostic, forward
> calls unchanged. Minor open cleanup: `predprobe verdict` should mask `ret&0xFF` (bool in AL).

### 8f. WRITER IDENTIFIED BY OBSERVATION — the root holder is never written (2026-08-17)

`holdprobe` resolved the holder vtable from a **live pointer** and detoured both setter slots, so the
writer is now observed rather than inferred. Concrete addresses (base `0x7FF618070000`):

| slot | RVA | VA | identity |
|---|---|---|---|
| vtable | `0x1CA79D0` | `0x7FF619D179D0` | holder class vtable |
| get `+0x98` | `0x1778C70` | `0x7FF6197E8C70` | `return holder + 0x148` — the record lives at `holder+0x148` |
| set `+0xB0` | `0x17AD310` | `0x7FF61981D310` | writes the **per-slot** holder |
| set `+0xB8` | `0x17AB350` | `0x7FF61981B350` | writes the **root** holder |

**Results (all measured, cross-checked):**
1. **The root holder `ctx(0)+0x18DD0` is NEVER populated.** Census reported `root=[0,0,0]` for the
   entire session, and the `+0xB8` setter was hooked but recorded **zero calls**.
2. **The only writer is the pump itself.** All 14 observed writes were `+0xB0` into
   `ctx(0)+0x19068` (holder delta `0x298` = `0x19068 - 0x18DD0`, confirmed), from
   `caller_rva=0x175EF59` and `0x175FC0D` — both inside the pump `FUN_7FF6197CE520`
   (`0x175E520..0x175FE5C`, verified).
3. Every record it writes is **local**: `route=0x00`, `source=1`, `dest=-1`, cycling
   `kind=1,2,3,5,6,7,0`.
4. Slots 1 and 2 never receive a record at all (`slot=[1,0,0]`).

Independent corroboration: the `+0x98` accessor resolved to `FUN_7FF6197E8C70`, which was decompiled
separately (§launcher_out) as `return param_1 + 0x148` — matching the holder's internal record slot,
and `FUN_7FF6197FDAB0` is `*(byte*)(holder+0x140) & 1`, the populated flag the census reads.

**Conclusion:** the pump writes local records *because the root holder it would source an authored
record from is empty*. The route byte is not wrong — no authored record ever enters the system. The
missing write is a single call to `0x7FF61981B350(rootHolder, record)`.

**Known record contract** (0xA8 bytes): `kind` at `+0x00` (consumer `FUN_7FF6197C51A0` requires
`== 5`), `source` `+0x08`, `dest` `+0x0C`, **route byte `+0x12`** (nonzero = authored), id `+0x18`
(not 0 or 1), payload `+0x20..0x9F`, `+0xA0` kind word, `+0xA1` must be 0.

### 8e. CORRECTION — `FUN_7FF6197CB8F0` is a FIRETEAM JOIN, not the selection ingest (2026-08-17)

Calling it directly disproved the §8d identification. With `param_5` = an 0x80 block whose leading
64 bits were `0x5111C0DE5111C0DE` and `param_6` = `0x5111C0DE00000001`, the game logged:

```
networking:logic:join: [5111C0DE-00000001] queued new fireteam to fireteam join to
                       [{0}-DEC01151:DEC01151-0.0.0.0:0:0.0.0.0:0]
```

Both supplied values echo back — `[5111C0DE-00000001]` is `param_6`, and `DEC01151:DEC01151` is the
identifier printed in memory order (`DE C0 11 51`). **The call was accepted and used our data**, but
the 0x80 block is a **join/session target descriptor**, not an activity selection: the zero tail was
parsed as peer addresses (`0.0.0.0:0:0.0.0.0:0`), so the client queued a fireteam join to a null
peer. Effects: session stranded in a pending fireteam, and a flood of
`networking:simulation:entity: failed to create 'player_broadcast' entity`.

Outcome measured: **route unchanged** (`route_byte=0x00 LOCAL, source=1 dest=-1`), manager still
`local(mode6)`, and slot 1 still had no selection record in **102 of 102** pump samples. Host
readiness (`FUN_7FF6197C40F0`) *does* hold offline (`armed_ready value=1`), so that predicate is not
a blocker — but it gates a join, not a selection.

**Module disabled** in `client_hook_activation.cpp` (`hooks::homecoming::inject`), source retained.
The §8d chain diagram is wrong at its first box: whatever fills `context(0) + 0x18DD0` for the
*selection* is still unidentified, and it is not this function.

### 8d. THE LEVER IDENTIFIED — the root selection holder at `context(0) + 0x18DD0` (2026-08-17)

Traced `route_commit` backwards and confirmed every link against runtime measurement. The route byte
is never *written* by anything downstream; it is carried by a selection record that propagates from a
single root holder:

```
FUN_7FF6197CB8F0 (ingest)  --vtable +0xB8-->  ctx(0) + 0x18DD0        <-- ROOT holder
                                                   | pump reads via vtable +0x98,
                                                   | gated by FUN_7FF6197FDAB0 ("has value")
                                                   v
selection pump FUN_7FF6197CE520  --vtable +0xB0-->  ctx(slot) + 0x19068
                                                   |
                     FUN_7FF6197C1370 reads ctx+0x19068 via vtable +0x98
                                                   v
      FUN_7FF6197C51A0 (fetch+gate)  /  pump  -->  route_commit FUN_7FF6197C1F50
```

`FUN_7FF6197C51A0` is only 141 bytes and does no construction at all — it fetches and gates:
```c
piVar3 = FUN_7ff6197c1370(param_1, &local_res10);
if (piVar3 && *piVar3 == 5 && *(char*)(piVar3 + 0xa1) == 0) FUN_7ff6197c1f50(piVar3);
```

**Measured confirmation of the root being empty offline:** `pumpprobe` shows slot 1 with no selection
record in **58 of 58** samples (`s1[id=-1 rt=-2 sel=0]`), and `ingprobe` (hook on
`FUN_7FF6197CB8F0`) recorded **zero invocations** across a full session including a Homecoming load
(`stage=install result=ok`, then no calls). So nothing ever fills the root holder offline, nothing
propagates, and the route byte is 0 for want of a record — not because a value is wrong.

**This also explains the message-1 negative (§8c) cleanly:** message-1 never touches this holder, so
no amount of correct descriptor content delivered that way can move the route.

**Known constraints on the record** (from the consumers, all cheap to satisfy):
`*(int*)(rec + 0x00) == 5`, `*(char*)(rec + 0xA1) == 0`, `*(long long*)(rec + 0x18)` not 0 or 1,
route byte at `+0x12` nonzero for authored, source at `+0x08`, dest at `+0x0C`.

**Next action:** fill the root holder. Two routes, both needing the ~0x80-byte selection block:
(a) call `FUN_7FF6197CB8F0` from Dawn with a synthesized block — it is an ordinary function and
publishes into exactly `ctx(0) + 0x18DD0`; or (b) call the holder's vtable `+0xB8` setter directly.
Route (a) is preferred: it drives the game's own genuine publish path with complete data, which is
what George said was required, rather than forcing a bit as in the §6.1 dead-end.

### 8c. MESSAGE-1 IS NOT THE ROUTE LEVER — falsified by direct experiment (2026-08-17)

The §4/§6.2 premise that "the route is driven by the message-1 global activity-state selection push"
is **experimentally false**. Dawn now synthesizes a complete, structurally valid 620-bit authored
descriptor (source=282 dest=266, name `mission_towerfall`, real SOID, host-assigned nonce, all
unknown fields reproduced from measured captures) and ships it in **every** periodic message-1 push
via the existing `replay_descriptor` path.

**Delivery is proven, not assumed.** Push body size is an exact function of descriptor width
(`1161 - 372 + N` bits): a 372-bit descriptor gives `body=146`, 620-bit gives `body=177`, 716-bit
gives `body=189`. Observed pushes were `body=177` throughout — the client was genuinely being sent a
620-bit descriptor every ~5 s.

**Timing was correct.** Six authored pushes landed at t=91734…111750, the last **5.7 s before** the
Homecoming load's route commit:

| t (ms) | event |
|---|---|
| 91734 … 111750 | 6 × `stage=authored result=built bits=620 source=282 dest=266` pushed |
| 117312 | identity-1 manager built -> **local(mode6)** |
| 117515 | route commits -> **route_byte=0x00 LOCAL, dest=-1** |
| 117609 | client mints its **own** nonce `0779B1AC-3A171DC7` |

**Result: no effect.** Route still LOCAL with `dest=-1`, manager still mode 6, and the client did
**not** adopt the host-assigned nonce (`0x5111C0DE5111C0DE` never appears in its log) — so
host-assignment of the activity nonce is also disproved.

**What this means:** the client's runtime 0x1B0 descriptor — the one `route_commit` actually reads —
is **not** populated from message-1's embedded selection descriptor, at least not for a pending load.
Two candidate explanations remain untested: (a) message-1's descriptor is scoped to the activity
session it is pushed on (the *previous* activity's session id) and is simply not consulted for the
next load, or (b) message-1's descriptor is not the route source at all. Either way the remaining
search is for the **actual** producer of the runtime descriptor's route byte, not for more descriptor
content — the payload problem is solved (§8a) and delivery is proven; only the lever is wrong.

### 8a. §5b WALL BROKEN — the authored 620-bit descriptor IS produced offline (2026-08-17, MEASURED)

**The core premise of §5b is FALSE.** Instrumenting Dawn's svc-6 parse (`ev=bap svc=6
stage=descbits`, in `activity_host_manager_route.cpp::report_descriptor_bits`) captured the client
encoding a **complete 620-bit authored descriptor offline**, for `cine_farm_376` (activity 277):

```
bits=620 name_bit=257 kind=0 elem=1/-1 skulls=0 arr=1 spawn=1
hex=11161168033D54600200200201B52F3A230D4D88950015FF02393B8B811C9DC5F1F4F772EFF370F976EFD9DBDB40...
```

Full decode (620 of 620 bits consumed, zero leftover — decoder self-validated against a synthetic
372-bit minimal form):

| field | value | sourceable offline? |
|---|---|---|
| reason / source / dest | 0 / 277 / 277 | trivially |
| elementIndex | present, **-1** (absent sentinel) | trivially |
| **sensitive scalar #1** | **`0x9EAA300100100100`** = our configured `primary_soid` | **YES — it is our own SOID from settings.json** |
| **sensitive scalar #2** | **`0xB52F3A230D4D8895`** = the **fireteam/activity nonce** | **YES — capturable; see below** |
| skulls | 0 | trivially |
| peer byte | present | trivially |
| arrivalBubbleHash / spawnSetHash / unknown hash | all `0x811C9DC5` (FNV-1a offset basis = "none") | trivially |
| **refs tail** | **present, count = 0** | **no 96-bit references exist at all** |
| **pairs tail** | **present, count = 0** | none |

The "~248 bits of opaque authored data we CANNOT source offline" is therefore: our own SOID, three
FNV *none* sentinels, an absent-element sentinel, a peer byte, two empty tail arrays, and **one**
64-bit nonce. **There are no 96-bit opaque reference entries** — confirming empirically what §8b
could only hypothesise (and correctly flagged as not established by arithmetic alone).

**Scalar #2 identified by the game's own retail log** (`ev=retail site=139`):
`world_controller:state:activity_setup: snapshot, fireteam nonce=B52F3A23-0D4D8895, activity
nonce=B52F3A23-0D4D8895`. It is minted **client-side at activity setup**, not by Dawn — but that
is not a blocker, because the client mints it **for our own live session** and Dawn receives it on
the wire. We do not need to forge it; we capture it.

**What remains is the KNOWN timing/lever problem, not a data problem.** Same run: identity-1 manager
built local `t=85046`, route commits LOCAL `dest=-1` `t=85093`, and svc-6 `stage=authroute
result=applied source=282 dest=266` fires at `t=85312` — **219 ms after the route already
committed**, exactly the §6.2 dead-end. `apply_authored_route` is now doing its bit-surgery on a
*genuine* 620-bit descriptor, but on the wrong lever and too late.

**NONCE STABILITY — MEASURED 2026-08-17 (3 loads, one session). Two corrections to the above:**

**(1) The nonce is NOT stable — it is re-minted on every activity setup.** Three consecutive setups
in one process gave three different values, each cross-verified against the game's own retail
`fireteam nonce=` line and the decoded scalar #2 (two independent reads, all agreeing):

| load | activity | bits | scalar #2 (nonce) |
|---|---|---|---|
| 1 | `edz_freeroam` (8) | **716** | `0x481F457F5726D3F0` |
| 2 | `edz_freeroam` (8) | **716** | `0xD4B1B0F10B64863A` |
| 3 | `raid_gluttony_0` (565) | 620 | `0x0B9A3170F4F1A8C0` |

Scalar #1 was `0x9EAA300100100100` (our SOID) in all three — that half stays confirmed and constant.

**(2) 96-bit reference entries DO occur — the earlier "there are NO 96-bit references at all" was
over-generalised from a single `cine_farm_376` sample and is WRONG.** Both `edz_freeroam` captures
carry `refs tail count=1`, i.e. one 96-bit entry (716 = 620 + 96). `raid_gluttony_0` and
`cine_farm_376` carry count=0. So ref presence is **per-activity**, not universally absent. All
captures decoded to exactly their stated bit length with zero leftover.

**(3) THE HARD ORDERING PROBLEM: the nonce does not exist until AFTER the route has committed.**
Consistent across all three loads — the nonce is minted *after* identity-1's manager is already built
local and its route already committed:

| load | mgr local | route commit (LOCAL, dest=-1) | nonce minted | svc-6 descbits |
|---|---|---|---|---|
| 1 | t=74922 | t=75125 | **t=75219** (+94 ms) | t=75625 |
| 2 | t=137656 | t=137688 | **t=137703** (+15 ms) | t=137766 |
| 3 | t=195094 | t=195297 | **t=195391** (+94 ms) | t=195797 |

So a descriptor for load N **cannot** be assembled in time for load N's own route commit — its nonce
does not yet exist. Replaying a cached descriptor therefore necessarily means using a **stale** nonce
from load N-1. Whether the client accepts a stale/foreign activity nonce is UNTESTED and is now the
pivotal open question. Note the client sends its self-minted nonce to the host in the activity-host
startup request (`ev=retail site=151 … (nonce: …)`), which suggests the nonce is normally
**host-assigned** — Dawn *is* the activity host, so having Dawn assign it in the message-1 push
(rather than replaying a stale one) is the architecturally correct variant to try.

### 8b. WIRE-CODEC HUNT — verified results (2026-08-17, Ghidra headless campaign)

**Verified (high confidence):**
1. **The game's bit-stream reader library is identified and confirmed:**
   `read_bits(stream,n)->uint` @ `0x7FF6183C13B0` (RVA 0x3513B0), `read_wide(stream,dst,n)` @
   `0x7FF6183C1070`, `read_bool(stream)` @ `0x7FF6183C0EF0`, `stream_status` @ `0x7FF6183BE9B0`.
   MSB-first, 64-bit accumulator at `+0x28`, bit cursor `+0x24`, byte ptr `+0x38` — semantics match
   Dawn's `encoding::bits::Reader` exactly.
2. **The parser field model is CORRECT — it reproduces the minimal size exactly.** Modelling
   `activity_manager_descriptor_parser.cpp` field-by-field gives a fixed/presence skeleton of
   **52 bits**; with only the package name present the total is **372 bits exactly**, independently
   corroborated by Dawn's own comment ("the common form is 372"). The field model is therefore
   trustworthy ground truth.
3. **The descriptor codec is NOT hand-written with immediate widths.** Across the whole image:
   435 `read_bits` call sites, of which width 12 appears only **twice** and no site anywhere has the
   descriptor's opening `4,12,12` (reason/source/dest) sequence. Only **4** call sites in the entire
   binary read 96 bits. Scanner verified against ground truth (recovers `[2,6,96,96,bool]` for
   `FUN_7FF619773CD0`, matching its decompile). Conclusion: the codec is **schema/table-driven** —
   widths come from a data table, not immediates. This is why the encoder resists signature hunting.

**Suggestive but NOT established (self-refuted — recorded to prevent re-deriving it):**
Under "every optional present + both tail arrays present but EMPTY + 0 skulls" the total is
**620 bits exactly**, matching George's authored figure, which would mean **no 96-bit reference
entries are needed** and the hard unknowns reduce to the two 64-bit scalars. **However a brute-force
uniqueness check found 45 distinct field combinations that also total exactly 620, and 28 of them
still require 96-bit reference entries.** George's figure is documented as approximate ("≈620"), so
bit-count arithmetic alone **cannot** determine the tail composition. Do not treat "no refs needed"
as fact.

**Still unknown:** the encoder itself was not located (schema-driven, so the next step would be
finding the schema/width table in .rdata rather than signature-scanning code), and the origin of the
two 64-bit "sensitive scalars" remains unresolved. Earlier campaign (§8 above) established the
related ingest-path identity values are deterministic build/capability data with **no nonce
primitive anywhere in that cluster** (no rdtsc/RNG/crypto), which weakens but does not disprove the
"runtime nonce" label the Dawn author assigned by guess from a skip-parser.

### Historical plan (now closed — kept for context)
The route is driven by the **message-1 global activity-state selection push**, not service 6. Dawn
server code:
- `server/bap/encrypted/push/activity/` — the roster / global-state pushes (message 1 territory).
- `server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp` — service 6 (echoes
  client descriptor; NOT the route lever).
- `middleware/bap/activity_host_manager/request/selection/` — the wire selection descriptor parser
  and `definition.h` struct (`ActivityManagerSelection`, `descriptorBits`).
- `state/activity/forced/activity_forced_destination.cpp` — where the override rewrites the echoed
  descriptor (has `write_byte` / `write_bits` bit-surgery helpers already).

Goal: publish an authored activity selection (activity 266 + the authored refs — activity hash
`0x9ACCB518`, scenario `0x80B500BC`, launch descriptor `0x80FDB97F`) via the message-1 push so the
client's native producer builds the authored 0x1B0 descriptor, the destination datum becomes
Homecoming, the predicate `0xC068F0` accepts, `route_commit` commits authored, and `mgrprobe` shows
identity 1 → `authored(mode1)`. **The opaque-authored-data constraint (§5b) still looms** — this may
require deriving the authored descriptor body, which is the hardest open problem.

---

## 9. Authored content — CONFIRMED READABLE

Dawn reads any tag via `middleware/content/packages/reader.h :: read_tag`. Setup (from
`client/content/entity_names/entity_name_cache.cpp`): `packages::package_directory(buf)` +
`packages::collect_keys(keys)` + heap `reader::Scratch` + `reader::Source{ wstring_view(dir), &keys }`.
All 7 Homecoming tags present locally and decoded:
| Tag | Class | Size | Notes |
|---|---|---|---|
| `0x80FDB97F` | 0x80809BA3 | 0x30 | launch descriptor; children at +0x0C/+0x10/+0x18 |
| `0x80FEB810`/`0x80FDB980`/`0x80FDB981` | 0x80809BAB | 0x34 | launch children; ref leaves 0x80FC16CC… |
| `0x80B500AC` | 0x80808AAE | 0x48 | mission_towerfall.activity; +0x08 = **0x9ACCB518**, +0x40 = scenario, +0x44 = launch desc |
| `0x80B500BC` | 0x80809994 | 0x7D8 | mission_towerfall.scenario |
| `0x80B508A9` | 0x80809C36 | 0xBE2 | scenario_patch |

These are DEFINITION inputs, not the runtime 0x1B0 descriptor.

---

## 10. Probes built (Dawn-src/Dawn/src/client/hooks/)

All forward calls unchanged (diagnostic), except `authored_force` (disabled) and the svc-6 authroute
rewrite (inert). Grep stages in the log:
| File | Stage | What it proved |
|---|---|---|
| `ai/ai_probe.cpp` | `ev=aiprobe` | actors register and tick; firing-position selector runs |
| `ai/mode_probe.cpp` | `ev=modeprobe` | the 4-gate mode branch is exonerated (green yet local) |
| `ai/manager_probe.cpp` | `ev=mgrprobe path=` / `stage=route` | every manager local; route_byte=0; source=1 dest=-1 |
| `homecoming/homecoming_probe.cpp` | `ev=hcprobe` | launcher sees only trivial selections; +0xA30 minimal; content tags readable |
| `homecoming/authored_force.cpp` | `ev=authforce` | forcing mode bit does not flip path; prunes (DISABLED) |

---

## 11. The honest ceiling

Per George's PDF (p3): even his **entire** pipeline — including a working 282→266 rewrite (Destiny
logs `0x011A→0x010A`) — still yields **no cinematic, objectives, dialogue, AI, encounters, markers,
or progression**. Reimplementing everything reproduces "reaches the mission transition, world loads",
**not a playable mission**. Our data agrees: world loads as `mission_towerfall`, all managers local.

---

## 12. Key codes (verified)
- `282 / 0x11A` CHOSEN source · `266 / 0x10A` mission_towerfall (Homecoming)
- `48 / 0x30` Homecoming slice (PUB48.48) · `0x9ACCB518` activity-definition hash
- route field descriptor `+0x12` (0 local / nonzero authored) · mode 6 wrong / mode 1 required
- identity index at manager `+0x854` (0 orbit / 1 mission)
- activity-client slot `manager + 0x27D0 + identity*0x2D0` (-1 = unset)
