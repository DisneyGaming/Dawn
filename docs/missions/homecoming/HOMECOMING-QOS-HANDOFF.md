# Destiny 2 Homecoming — Matchmaking Connect + QoS Handoff (2026-08-18)

Continuation handoff for the **Homecoming** (`mission_towerfall`) offline-revival under **Sunrise**.
Supersedes the *direction* of all prior handoffs. Read this first, then `HOMECOMING-MATCHMAKING-HANDOFF.md`
(the pivot that got us here), then the older `HOMECOMING-AH-HANDSHAKE-HANDOFF.md` / `HOMECOMING-FINDINGS.md`
for deep background. **Authorized personal reverse engineering on the user's own machine.**

Cross-refs: memory files `homecoming-matchmaking-pivot.md` (most detailed blow-by-blow), `sunrise-project-state.md`,
`homecoming-authored-fork.md`, `homecoming-collaborator-map.md`.

---

## 0. TL;DR — WHERE WE ARE

We drove Homecoming through the **entire public-region matchmaking connect**, byte by byte, and are **one gate
from the peer join**. The working chain:

```
region_force_public (mission treated PUBLIC)
  → client SEARCHES the region  → Sunrise sessionSearch returns the local host  ✓
  → client parses the result (schema-correct)  → gets session id + host address  ✓
  → NAT traversal to embedded host 127.0.0.1:30976  ✓
  → QoS reachability probe to 30976  → Sunrise answers a valid bdQoSReplyPacket  ✓ (reachable, accepted, 20ms)
  → [CURRENT WALL] client decodes the QoS reply PAYLOAD as a structured session blob;
        ours is zeros → "qos-payload-failed-to-decode" → session marked unsuitable → loops
```

**The one remaining blocker:** the client's `session_tracker` decodes the QoS reply's data payload as a
structured message and judges suitability. It, and the reason it emits, live in **VMProtect-obscured** code with
no string anchors and a dynamically-dispatched result callback — so neither a static client-bypass hook nor the
server payload format is reachable by static RE (which cracked everything up to here).

**Chosen path forward:** a **runtime bypass**. Stage-1 observe hook is DEPLOYED (build `ec059d241812`, module
`qos_probe`) to discover the session_tracker QoS handler at runtime. See §5.

**Honest ceiling (unchanged):** even a full connect gives session + entity slots + script-runners, NOT
guaranteed enemies/acts (isinternet still saw "0 acts" past the session). The user's goal is "get the mission to
load"; the connect gets us to world-load, content is a later scripting layer.

---

## 1. CURRENT STATE

- **Deployed DLL:** `ec059d241812` (steam_api64.dll). Verify with
  `md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"` vs the build output.
- **Clean revert baseline:** `efbab4c7f6d4` (all client hooks observe-only). Last stable pre-QoS server build:
  `362833a8c841` (full QoS reply, reaches "qos-payload-failed-to-decode").
- **Config now ON:** `region_force_public` (region_public.cpp, keeper — WORKS); server QoS responder in
  `gameplay_endpoint.cpp` (full reply, §3); the new `qos_probe` observe hook (§5). `ambassador_slot`=self.
- **Config OFF/irrelevant now:** gate spoof, authored-fork force, payload fill (all the old authored-launch
  machinery — the matchmaking path replaced it).

---

## 2. THE JOURNEY (what was proven, in order — all measured)

1. **Pivot to the matchmaking-SEARCH path** (from the peer/citizen path). Measured proof: with `ambassador=self`
   the client's session search REACHES Sunrise (`server ev=bap svc=42 rsp=43`), whereas the citizen path
   (`ambassador≠self`) dies entirely client-side on `group_target=0` (nothing reaches the server). Search path
   is the tractable one.
2. **sessionSearch returns the host** (was `encode_empty_message`). Server change in matchmaking (§4).
3. **Search-result schema reversed** (Ghidra): the svc-43 response is a schema-driven protobuf. The SearchResult
   sits at `f3{ f1(repeated){ f1: descriptorMsg{ f1: 128B descriptor } } }` — the result's **field 1 is the
   descriptor message, NOT an id** (every earlier guess put an id there → policy-31 fatal decode → weasel).
   Correct encoding → client `Get search results total:[1] valid:[1]`.
4. **NAT traversal** to 127.0.0.1:30976 already answered by Sunrise (`make_introduction_reply`, type 13→12).
5. **QoS handshake reversed field-by-field** (this was the long part — §3). Each fix cleared one client reason:
   type 0x29 (routing) → key at probe[9..12] (invalid id) → accept flag reply[13]=1 (qos-refused) → non-empty
   payload (qos-payload-empty) → **now** the payload must DECODE (qos-payload-failed-to-decode).
6. **Hit the session_tracker VMProtect wall** (§5) → runtime-bypass observe hook deployed.

---

## 3. THE QoS REPLY FORMAT (fully RE-derived — server sends this, `gameplay_endpoint.cpp`)

The client (bdQoS, Demonware) sends a **request/probe** to the descriptor's address every ~2s; the responder
must return a **bdQoSReplyPacket**. All offsets little-endian.

**Receive dispatcher** (client) reads the type byte: `SUB EDI,0x28; JZ request; CMP EDI,1; JZ reply` →
**type 0x28 = request, 0x29 = reply**. (raw @ 0x7ff619b09c90; handlers: handleRequest FUN_7ff619b0b380,
handleReply FUN_7ff619b0b150; both are socket callbacks called from FUN_7ff619b09b70 which parses via
FUN_7ff619b19a80.)

**REQUEST (probe) wire — 18 bytes** (parser FUN_7ff619b1a200, builder FUN_7ff619b1a1b0/serializer
FUN_7ff619b1a330):
```
[0]   type = 0x28
[1..8]  8-byte value (timestamp; [5..8] is the requested payload size, constant ~0x17F..0x182)
[9..12] KEY   (the outstanding-probe key; increments 0,1,2… per probe)
[13..16] value (uVar3; = the session id)
[17]  byte
```

**REPLY wire — 27 header bytes + payload** (deserializer FUN_7ff619b19a80 = bdQoSReplyPacket::deserialize):
```
[0]     type = 0x29
[1..4]  KEY   ← must echo request[9..12]  (handleReply looks up reply_struct+4; wrong → "invalid id")
[5..12] 8-byte value ← echo request[1..8] (client reads it back as send-timestamp for latency)
[13]    ACCEPT FLAG = 0x01 ← responder sets this (FUN_7ff619b1a180 writes pkt+0x18); 0 → "qos-refused"
[14..17] dataLen (LE) ← the payload size (= request[5..8], clamp 1..1024); 0 → "qos-payload-empty"
[18..21] u32 (0)
[22..25] u32 (0)   ← latency reader subtracts this (FUN_7ff619b19d20); 0 is fine
[26]    byte (0)
[27..]  dataLen bytes of PAYLOAD  ← STILL ZEROS → "qos-payload-failed-to-decode"  ← THE CURRENT WALL
```
handleReply also does an anti-spoof check (FUN_7ff619af3160, a 16-bit token at struct+0x84) that PASSES with our
echo (no "spoofed"). Reply max size = 0x508 (1288) per setData FUN_7ff619b1a040. The responder's own reply build
is bdQoSProbe::listen FUN_7ff619b0b9e0 (sets accept flag + securityId + response data via setData).

**What's left on the payload:** the [27..] bytes must be a valid structured session blob (the client decodes it).
All-zeros fails. This is the last field; its format lives in obscured session_tracker (§5). Do NOT keep guessing
it blind — the chosen path is the runtime bypass.

---

## 4. SERVER-SIDE MAP (Sunrise-src, branch `spawner`, paths under `Sunrise/src/`)

- **`middleware/bap/matchmaking/response/matchmaking_dynamic_response.cpp`** — `encode_search_result`:
  `f3{ f1{ f1: descriptorMsg{ f1: 128B desc } } }` (SCHEMA-CORRECT). `encode_locate_result` (field 7) is the
  known analogue. `matchmaking_response_encoder.cpp` sessionSearch → encode_search_result when a descriptor is
  present, else empty. `matchmaking_route.cpp` fills `response.descriptor` from
  `server::gameplay::search_target` (region 48) + logs `ev=bap svc=42 stage=request/searchresult`.
- **`server/gameplay/gameplay_advertisement.cpp`** — `search_target(descriptor,advId)` returns region 48's
  proven citizen descriptor + host session for the search reply.
- **`server/gameplay/endpoint/gameplay_endpoint.cpp`** — THE QoS RESPONDER. `service()` loop: NAT intro reply,
  then the QoS branch (`buffer[0]==0x28`): builds the 27+payload bdQoSReplyPacket per §3, logs
  `ev=gameplay stage=qos probe/reply`. All the QoS field constants live at the top of this file.
- `server/gameplay/peer/peer_transport.cpp` + group_host/dtls/association — the direct peer-join transport
  (connect→join→establish→membership→activity-host param). Already implemented; fires AFTER QoS passes and the
  client actually connects to 30976.
- **schema/state:** `middleware/bap/matchmaking/{definition.h,request/,response/}`, `state/matchmaking/`.

---

## 5. THE CURRENT FRONTIER — session_tracker is VMProtect-obscured (the new chat's job)

**Failure:** `client ... session_tracker: marking session #0 ... unsuitable. Reason=qos-payload-failed-to-decode`
(and earlier qos-refused / qos-payload-empty, all now cleared). The QoS reply is REACHABLE + ACCEPTED (20ms); the
client decodes the payload as a session blob and, on failure, marks the session unsuitable and re-searches.

**Why static RE stalls:** the qos-* reason strings (`0x7FF619D1EA50`..`EA98`, `unsuitable`/`suitable`
`0x7FF619D1EB22`/`EB24`) are in a pointer table with NO code xrefs; the retail log strings aren't plain ASCII
(VMProtect). The QoS result is delivered from bdQoS to session_tracker via a **dynamically-dispatched callback**
`consumer->vtable[0x10](consumer,&result,uVar7)` (raw `CALL [RBX+0x10]` @ 0x7ff619b0cdbd inside pump
FUN_7ff619b0cb40; RBX=*consumer; consumer=[RBP-0x40], a per-result object whose source is not a clean static
field). So the session_tracker QoS handler isn't statically resolvable.

**RUNTIME-BYPASS PLAN (in progress):**
- **STAGE 1 — pump-hook RAN, mis-fired; RETARGETED to aea0 (build `ce76ac82`, pending deploy+run).** The
  `ec059d241812` pump hook fired ONCE at **t=14000** (STUN/socket-router boot), ~156 s BEFORE the real QoS result
  at **t=169968** — because it dumped on *first call*, and at boot the results container (`[this+0xe8]`) was empty
  so no consumer existed. Its captured candidates (`off=0x18 → rva=0x1B81BC0`, etc.) are boot garbage. Meanwhile
  the run **reproduced the exact wall**: `status data (390 bytes) failed to decode` → `unsuitable.
  Reason=qos-payload-failed-to-decode`, session `s_id=5BED172B:19E222A9`, `qos result type=0` (our 417-byte reply
  = 27 hdr + 0x186/390 payload; SIZE echoes correctly, only the zero CONTENT fails). Full chain still green:
  svc-42 searchresult filled (140 B), NAT reply, `stage=qos probe/reply result=sent`.
  - **RE'd the delivery precisely** (pumpdisasm/qosconsumer): pump's consumer is NOT in `this`. Pump builds a
    per-result stack record via `FUN_7ff619b0aea0(container=[this+0xe8], node, &record)` and reads the consumer at
    **record+0x20**, then `*consumer`=vtable, `vtable[0x10]`=handler (`CALL [RBX+0x10]` @ `0x1A9CDBD`). aea0 runs
    only when a real result is delivered, never at boot.
  - **NEW build `ce76ac82` retargets the hook to `FUN_7ff619b0aea0` (RVA `0x1A9AEA0`)**, sig
    `void* __fastcall(container,node,record)`: calls original, then on the first real delivery dumps `record[0..0x80]`
    and resolves `record+0x20` (the consumer) → `vt[0x10]` RVA = the session_tracker QoS handler
    (`ev=qosprobe stage=consumer … rva=…` plus a `stage=cand` scan cross-check).
  - **NEXT: close the game so `ce76ac82` deploys, boot Homecoming, read the `ev=qosprobe stage=consumer` line** →
    that RVA is the STAGE-2 target.
- **STAGE 2:** RE the discovered handler (it decodes the payload + sets the unsuitable status/reason). Find where
  it marks unsuitable and how a "suitable" verdict is produced.
- **STAGE 3:** hook that handler to force the reachable session SUITABLE (skip/invert the unsuitable marking for
  our session), so matchmaking advances Gather → the peer join to 30976.

**Cheap alternative (low odds):** guess the QoS payload structure server-side (fill it with real session/
descriptor bytes) — but without the decoder it's a blind guess.

**Success signal (whole thing):** `Gather [ACTIVE]` (always INACTIVE so far), then `server ev=gameplay
stage=connect`/`stage=join`/`stage=establish` on 30976 (peer_transport) → session established → world load.

---

## 6. DLL LEDGER (this session, chronological)

| md5 | what | result |
|---|---|---|
| `efbab4c7f6d4` | clean baseline (observe-only) | revert target |
| `e870ed6100b3` | svc-42 request logging (observe) | captured request kinds; no kind=2 |
| `6fb74fdf75f1` | search result guess (wrapped) | policy-31 fatal decode (weasel) |
| `bfc84bbbf52e` | search result locate-mirrored | still weasel |
| `e54f13a2351c` | safe (search empty) | no weasel, searches forever |
| `06adcaecf091` | **SCHEMA-CORRECT search result** | `total valid:[1]`, NAT ok, no weasel |
| `2a40dd3222cc` | QoS reply type 0x28 (27B) | ignored (wrong type) |
| `fcfbc684c4d0` | QoS reply type 0x29 | reached handleReply → "invalid id" |
| `b2193f9db8ab` | QoS key = probe[9..12] | reachable → "qos-refused" |
| `54544c674ab0` | QoS accept flag reply[13]=1 | → "qos-payload-empty" |
| `362833a8c841` | QoS payload (sized zeros) | → "qos-payload-failed-to-decode" |
| `ec059d241812` | qos_probe pump hook (first-call dump) | mis-fired at boot t=14000; no consumer (results empty) |
| `ce76ac82` | **qos_probe retargeted to aea0 record-filler (CURRENT)** | pending deploy+run → capture consumer vt[0x10] RVA |

---

## 7. KEY RVAs / CONSTANTS (base `0x7FF618070000`; RVA = VA − base)

- Search: response schema table `0x7ff619caa360`; SearchResult descriptor sub-schema `0x7ff619ca8be0` (field 1
  type 0x14 = 128B). policy-31 codec table `0x7ff61a87e3e0`; codecIndex resolver PTR_PTR_7ff61a03f6e0.
- QoS bdQoS: pump `0x7ff619b0cb40` (RVA 0x1A9CB40) · result filler `0x7ff619b0aea0` (RVA 0x1A9AEA0) ·
  handleReply `0x7ff619b0b150` · handleRequest `0x7ff619b0b380` · reply deserialize `0x7ff619b19a80` ·
  request parse `0x7ff619b1a200` · request build `0x7ff619b1a1b0` · request serialize `0x7ff619b1a330` ·
  reply flag setter `0x7ff619b1a180` (pkt+0x18) · setData `0x7ff619b1a040` · sendRequest `0x7ff619b0dbc0` ·
  listen/responder `0x7ff619b0b9e0` · reason strings `0x7FF619D1EA50/EA80/EA98`, `0x7FF619D1EB22/EB24`.
- delivery `CALL [RBX+0x10]` @ `0x7ff619b0cdbd` (RBX=consumer vtable).
- Homecoming constants: region 48, slice-set PUB48.48 (hash 0x1AD5E415), game mode 0x800D0030, silo 0x5C01,
  region host session `0x9EAA300100200002`, embedded gameplay port 30976, BAP primary 1.0.0.127:30974.

---

## 8. BUILD / TEST LOOP

```bash
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1" -Config Release
```
- Full deploy needs the game closed (`-BuildOnly` compiles while it runs). Warnings are errors (/WX).
- Verify: `md5sum "/c/Destiny 2 Development/bin/x64/steam_api64.dll"` vs
  `.../Sunrise-src/build/x64/Release/steam_api64.dll`.
- NEW `.cpp`/`.h` must be added to `Sunrise/Sunrise.vcxproj` (ClCompile+ClInclude) AND registered in
  `client/runtime/client_hook_activation.cpp` (the `qos_probe` module already is).
- Log: `bin\x64\Sunrise\logs\sunrise.log` (truncated per run; `.old` = previous). Retail engine log is piped as
  `ev=retail`; server = `server ...`; client hooks = `client ev=...`.
- **In-game:** boot to ORBIT → Insert → Activity override → `mission_towerfall` → bubble → slice auto-fills 48 →
  spawn `none` → load Homecoming. (Enable the override FROM ORBIT, not from a loaded activity.)

---

## 9. RE ASSETS

- Unpacked image `C:\Destiny 2 Development\destiny2_unpacked.bin`, base `0x7FF618070000`, file offset == RVA.
  On-disk exe is packed — always use the dump. VMProtect anti-debug (no hardware breakpoints).
- Ghidra headless (Ghidra must be closed), project `C:\Users\gauta\Ghidra.gpr`, program `destiny2_unpacked.bin`:
  ```
  "C:\Users\gauta\Downloads\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" \
    "C:\Users\gauta" Ghidra -process destiny2_unpacked.bin -noanalysis [-readOnly] \
    -scriptPath "C:\Users\gauta\ghidra_scripts" -postScript <Script>.java
  ```
  Use WITHOUT `-readOnly` when a script must `disassemble`/`createFunction` on vtable targets analysis skipped.
- QoS/search scripts in `C:\Users\gauta\ghidra_scripts\`: DecompChain, DecompSchema, DumpSchema2 (search schema);
  XrefQos, DecompQosModule, QosDispatch, QosResp, QosReply, QosParse, QosSerialize, QosTypeByte, QosSend,
  QosReqBuild, QosResult, QosReplyFlag, QosResponder, QosReasonTable, QosSuitable, QosConsumer, PumpDisasm.
- Decompile outputs in `C:\Destiny 2 Development\`: `schema*_out.txt`, `qos*_out.txt`, `pumpdisasm_out.txt`, etc.

---

## 10. AFTER QoS (the remaining chain)

Once the session is suitable: `Gather [ACTIVE]` → the client JOINS the embedded host on 30976 →
`peer_transport` answers (connect/join/establish/membership/activity-host param) → session established → entity
slots flow → world loads (`changed world to: mission_towerfall`, past the infinite loading screen). Then the
CONTENT layer (cinematics/objectives/AI/encounters) is the separate, harder ceiling (isinternet's "0 acts") —
scriptable later. The user's stated goal is "get the mission to load, script content after."

---

*Everything in this file is measured this session unless marked inferred. The QoS reply format (§3) and the
search-result schema (§4) are fully reverse-engineered and working. The single open blocker is the obscured
session_tracker suitability/payload decode (§5), being attacked via the deployed runtime observe hook.*
