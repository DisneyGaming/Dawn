# Homecoming: Unified Reconstruction & Technical Dossier

> **File Type:** Master Consolidated Reference  
> **Source Documents Retained in this Directory:**
> - `HOMECOMING-FINDINGS.md` (Initial RE findings, RVA tables, execution chain)
> - `HOMECOMING-HANDOFF.md` (Checkpoints, manager modes & operational status)
> - `HOMECOMING-AH-HANDSHAKE-HANDOFF.md` (AH handshake, arm gate & dual-slot commit)
> - `HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md` (Lane-2 route selection, manager creation & mode progression)
> - `HOMECOMING-MATCHMAKING-HANDOFF.md` (Public matchmaking, BAP svc 42/43, SearchResult protobuf)
> - `HOMECOMING-QOS-HANDOFF.md` (bdQoS request/reply wire protocol & verdict codes)
> - `HOMECOMING-KIND22-HANDOFF.md` (Kind-22 host re-establishment & manager activation)
> - `HOMECOMING-PHASE5-BUILDOUT.md` (Encounter buildout, dialogue triggers & cue graphs)
> - `HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md` (Towerfall mission handoff & Scene crash)
> - `HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md` (Safe trace execution flow)
> - `SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt` (Native Scene-entry resolver chain disassembly)
> - `DAWN-HOMECOMING-BRIEFING-FOR-CODEX.md` (Architecture briefing & client expectations)
> - `HOMECOMING-NEW-CHAT-PROMPT.md` (Contextual boundaries & baseline prompts)

---

## 1. Executive Summary & Operational Pipeline

Reviving Destiny 2's opening Red War mission **Homecoming** (`266 / mission_towerfall`) offline under **Dawn** requires coordinating prelaunch donor contracts, public matchmaking emulation, demonware QoS reachability, activity-host handshakes, and native scene authority parsing.

```
[Game Boot / UI Selection]
  │  • Chosen (activity 282) provides authored prelaunch contract
  │  • Dawn rewrites launch contract to 266 (mission_towerfall) before publication
  ▼
[Public-Region Matchmaking Connect (BAP svc 42 ➔ 43)]
  │  • region_force_public treats mission as PUBLIC (bubble PUB48.48, slice set 72)
  │  • ambassador_slot = self initiates sessionSearch via BAP
  │  • Server returns SearchResult protobuf containing 128-byte host descriptor
  ▼
[NAT Traversal & Demonware bdQoS Reachability Probe]
  │  • Introduction packet exchange (type 13 ➔ 12) with embedded host 127.0.0.1:30976
  │  • bdQoS probe (type 0x28, 18B) answered by bdQoSReplyPacket (type 0x29, 27B header + payload)
  │  • Client session_tracker validates payload suitability
  ▼
[Activity Host (AH) Handshake & World Load]
  │  • Client receives startup response from AH [id 9EAA3001:00200003]
  │  • Join request returns result 0 ➔ "ready for instantiation! AH->9eaa300100200003"
  │  • Client successfully executes world change to mission_towerfall
  ▼
[Authored Manager Lifecycle & Mode Progression]
  │  • Native manager starter (+0x17905B7) creates manager (identity 2)
  │  • Mode progression: Mode 1 (0x17F8) ➔ Mode 2 (0x1880) ➔ Mode 4 (0x90) ➔ Mode 5 (0xA0)
  │  • Kind-22 136-byte message triggers mgr_activate at +0x177E940
  ▼
[Underwatch Opening & Cue Progression]
  │  • Opening cue 0x4FCECAB6: Underwatch spawn, objective display, Ghost dialogue record 0
  │  • Breach cue 0x432D2C95: Wall breach event & Cabal boarding
  │  • Path unlock cue 0x432D2C96: Door unlocks upon encounter clearance
  ▼
[Scene Resolution Barrier & Mitigation]
  • Active type-43 Scene publication crashes at destiny2.exe+0xA9309B (RCX=0x314)
  • Mitigated by running safe pre-active-Scene DLL (suppressing type-43 publication)
```

---

## 2. Master Reverse-Engineering RVA & Address Reference

Base address: `0x7FF618070000` (unpacked Season of Arrivals executable `destiny2_unpacked.bin`).

| Function / Symbol | RVA | Full Virtual Address (VA) | Subsystem & Technical Role |
| :--- | :--- | :--- | :--- |
| `route_commit` | `+0x1751F50` | `0x7FF6197C1F50` | Evaluates route byte `+0x12`; branches to authored vs. local |
| `route_update` | `+0x17AC390` | `0x7FF61981C390` | Applies route descriptor update to active manager |
| `selection_pump` | `+0x175E520` | `0x7FF6197CE520` | Reads slot state byte `state[slot*0x1B + 2]` |
| `authored_mode1` | `+0x1773200` | `0x7FF6197E3200` | Transitions manager into authored Mode 1 (payload `0x17F8`) |
| `local_mode6` | `+0x1772440` | `0x7FF6197E2440` | Transitions manager into local Mode 6 |
| `mgr_starter` | `+0x17905B7` | `0x7FF6198005B7` | Native caller that instantiates manager identity 2 |
| `mode_setter` | `+0x17B3971` | `0x7FF619823971` | Caller initiating Mode 1 entry with 6,136-byte payload |
| `mgr_activate` | `+0x177E940` | `0x7FF6197EE940` | Kind-22 decode handler activating manager |
| `arm_action` | `+0x17207B0` | `0x7FF61907B0` | Stages authored launch (`F907B0`) |
| `arm_gate` | `+0x1724CF0` | `0x7FF6194CF0` | Client gate requiring slots not both committed (`F94CF0`) |
| `consumer_tick` | `+0x16E750` | `0x7FF6185E750` | Consumer tick polling pending flag `tick_obj+0x4C` (`F5E750`) |
| `chosen_gate` | `+0x1763B20` | `0x7FF6197D3B20` | Dispatches authored launch trigger |
| `chosen_disp` | `+0x134FDF0` | `0x7FF6193BFDF0` | Dispatcher called when gate passes |
| `authored_ctor` | `+0x1757C00` | `0x7FF6197C7C00` | Authored launch constructor (`FUN_7FF6197C7C00`) |
| `scene_update` | `+0xB3F620` | `0x7FF618BAF620` | Scene component update tick |
| `scene_resolver`| `+0x501AD0` | `0x7FF618571AD0` | Resolves 8-byte Scene entry reference to 16-byte datum |
| `scene_fault` | `+0xA9309B` | `0x7FF618B0309B` | Crash site: `cmp eax, dword ptr [rcx]` (`RCX=0x314`) |
| `qos_recv` | `+0x1AA9C90` | `0x7FF619B09C90` | bdQoS packet receive dispatcher |
| `qos_req_hndlr`| `+0x1AAB380` | `0x7FF619B0B380` | bdQoS request packet handler |
| `qos_rep_hndlr`| `+0x1AAB150` | `0x7FF619B0B150` | bdQoS reply packet handler |
| `qos_deser` | `+0x1AB9A80` | `0x7FF619B19A80` | `bdQoSReplyPacket::deserialize` |

---

## 3. Detailed Subsystem Analysis

### 3.1. Activity Host (AH) Handshake & Arm Gate
- **Handshake Verification:** Live logging confirmed that Homecoming reaches `ready for instantiation! AH->9eaa300100200003` and executes world change to `mission_towerfall` identical to the working Farm (`cine_farm_376`).
- **Arm Gate Wall (`FUN_618F94CF0`):** Checks activity client slots. When both slots are committed as `LOCAL`, the arm action `F907B0` is skipped. Directly invoking `F907B0` stages `obj+0x3F0`, but `F94CF0`'s reset branch clears activation within milliseconds.

### 3.2. Public-Region Matchmaking & Protobuf Schema
To bypass offline private session stalls, `region_force_public` routes Homecoming through bubble `PUB48.48`:
- Setting `ambassador_slot = self` forces the client to search for local sessions via BAP service 42/43.
- The service 43 response requires a strict protobuf layout:
  ```protobuf
  message SearchResultResponse {
    message SearchResults {
      repeated message ResultEntry {
        message DescriptorMsg {
          bytes host_descriptor = 1; // 128 bytes
        }
        DescriptorMsg descriptor_msg = 1; // Field 1 must be message, NOT integer ID
      }
      repeated ResultEntry results = 1;
    }
    SearchResults search_results = 3;
  }
  ```

### 3.3. Demonware bdQoS Wire Protocol
Probing occurs every ~2s against `127.0.0.1:30976`:
- **Request (type `0x28`, 18 bytes):**
  - `[0x00]`: `0x28`
  - `[0x01..0x08]`: Timestamp and requested payload size (`0x17F..0x182`)
  - `[0x09..0x0C]`: Incrementing transaction KEY
  - `[0x0D..0x10]`: Target session ID
- **Reply (type `0x29`, 27 header bytes + payload):**
  - `[0x00]`: `0x29`
  - `[0x01..0x04]`: KEY (must match request `[0x09..0x0C]`; mismatch triggers `invalid id`)
  - `[0x05..0x0C]`: Timestamp echo
  - `[0x0D]`: Accept flag (must be `0x01`; `0x00` triggers `qos-refused`)
  - `[0x0E..0x11]`: Payload length (if 0, triggers `qos-payload-empty`)
  - `[0x1B..]`: Binary session blob parsed by `session_tracker` (unparsed payload triggers `qos-payload-failed-to-decode`)

### 3.4. Towerfall Scene Crash Fault & Stack Flow
Publishing the 129-bit Scene authority body (`selector=0x80B82771`, entry `0x57318E3B/type2/index1`):
1. `+0xB3F620`: Scene component update calls resolver `+0x501AD0`.
2. Resolver fails to bind runtime context and returns failure.
3. `+0xB3F6CB`: Sets `RCX = NULL`.
4. Call chain propagates null source: `+0xB3F6D2` ➔ `+0x4E83CD` ➔ `+0x4E343F` ➔ `+0xA92AE0`.
5. `+0xA92AE0` adds `0x2F0` to null (`0x2F0`) and invokes `+0xA93070`.
6. `+0xA93070` writes `0x2F0 + 0x24 = 0x314` as the iterator pointer.
7. `+0xA9309B`: Faults executing `cmp eax, dword ptr [rcx]` reading address `0x0000000000000314`.
- **Mitigation:** Run safe build with active type-43 Scene publication disabled. Directive, dialogue, and script-state remain active.

### 3.5. Kind-22 & Towerfall Entity Objects
- Kind-22 136-byte message triggers `mgr_activate` at `+0x177E940`, returning `result = 1`.
- 98 captured Towerfall entity objects populate bubble 9, decoding Type-1 Sense bodies at widths `92`, `124`, and `156`.
- Cue keys advance: Opening `0x4FCECAB6` ➔ Breach `0x432D2C95` ➔ Path Unlock `0x432D2C96`.
