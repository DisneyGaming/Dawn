# Homecoming Mission Documentation Archive

This directory contains the complete reverse-engineering and reconstruction documentation for Destiny 2's Red War opening mission **Homecoming** (`266 / mission_towerfall`).

Every original document is preserved in full with all RVA tables, IDA/Ghidra traces, packet layouts, and investigation notes intact.

---

## Technical Documents Index

### 1. Foundation & Initial Findings
- [`HOMECOMING-FINDINGS.md`](./HOMECOMING-FINDINGS.md)  
  *Comprehensive initial reverse engineering findings, RVA tables, execution chain, and collaborator comparison.*
- [`HOMECOMING-HANDOFF.md`](./HOMECOMING-HANDOFF.md)  
  *Checkpoints, manager modes, and operational status baseline.*
- [`SUNRISE-HOMECOMING-BRIEFING-FOR-CODEX.md`](./SUNRISE-HOMECOMING-BRIEFING-FOR-CODEX.md)  
  *Architecture briefing detailing client expectations and server-authoritative requirements.*
- [`HOMECOMING-NEW-CHAT-PROMPT.md`](./HOMECOMING-NEW-CHAT-PROMPT.md)  
  *Operational prompt context and baseline scope boundaries.*

### 2. Networking, Routing & Matchmaking
- [`HOMECOMING-AH-HANDSHAKE-HANDOFF.md`](./HOMECOMING-AH-HANDSHAKE-HANDOFF.md)  
  *Activity Host handshake verification, client-side arm gate (`FUN_618F94CF0`) analysis, and dual-slot commit dilemma.*
- [`HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md`](./HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md)  
  *Authored lane-2 descriptor materialization, `route=1`, manager creation (`+0x17905B7`), and mode progression (`1 -> 2 -> 4 -> 5`).*
- [`HOMECOMING-MATCHMAKING-HANDOFF.md`](./HOMECOMING-MATCHMAKING-HANDOFF.md)  
  *Public region matchmaking connect (`region_force_public`), BAP service 42/43 sessionSearch, and SearchResult protobuf schema.*
- [`HOMECOMING-QOS-HANDOFF.md`](./HOMECOMING-QOS-HANDOFF.md)  
  *Demonware bdQoS reachability probe (type 0x28) and reply (type 0x29) wire protocol, packet layouts, and suitability verdict codes.*

### 3. Manager Activation & Scripting
- [`HOMECOMING-KIND22-HANDOFF.md`](./HOMECOMING-KIND22-HANDOFF.md)  
  *Kind-22 (136-byte) host re-establishment message, `mgr_activate` at `+0x177E940`, and `+0x1AF00` identity enable field.*
- [`HOMECOMING-PHASE5-BUILDOUT.md`](./HOMECOMING-PHASE5-BUILDOUT.md)  
  *Phase 5 encounter buildout, dialogue triggers, and cue graph execution.*

### 4. Towerfall Scene Crash Investigation
- [`HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md`](./HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md)  
  *Towerfall mission handoff, 129-bit Scene authority body, and wall-breach Scene crash diagnostics.*
- [`HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md`](./HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md)  
  *Verified safe execution trace with active type-43 Scene publication suppressed.*
- [`SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt`](./SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt)  
  *Native disassembly and call trace of the faulting Scene resolver chain (`destiny2.exe+0xA9309B`, RCX=0x314).*
