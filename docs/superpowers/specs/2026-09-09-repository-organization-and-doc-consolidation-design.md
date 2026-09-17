# Repository Organization & Documentation Consolidation Design

**Date:** 2026-09-09  
**Status:** Approved by User (Brainstorming Phase Complete)  
**Topic:** Repository Root Cleanup, Folder Organization, and Documentation Merging

---

## 1. Executive Summary & Objectives

The root of this repository contains over 200 files: ~35 Markdown documents, ~150 reverse-engineering and disassembly `.txt` dump files, Python binary-analysis probes, PowerShell/CMD scripts, PDF reference architectures, CSV object tables, and development symlinks/junctions.

This clutter makes navigating active projects difficult and scatters critical mission-reconstruction knowledge across disparate, chronological handoff notes.

### Core Objectives:
1. **Clean Root Directory**: Retain only critical root-level entries: repository configuration, launcher commands, and game environment junctions/symlinks.
2. **Preserve 100% Knowledge**: Merge fragmented handoffs into comprehensive, authoritative thematic guides without discarding any reverse-engineering findings, struct offsets, packet layouts, RVA addresses, or code snippets.
3. **Structured Taxonomy**:
   - Curated documentation placed under `docs/` (`docs/handbook/`, `docs/missions/`, `docs/research/`).
   - Reverse-engineering text dumps placed under `docs/research/dumps/`.
   - Probes and binary analysis tools placed under `tools/re/`.
4. **No Broken Workflows**: Keep essential execution launchers (`launch-destiny.cmd`, `launch-scot-reveal-debug.cmd`, `dawn-dev.ps1`) in the root directory.

---

## 2. Target Repository Root Layout

After restructuring, the repository root will contain:

| Root Item | Type | Purpose |
| :--- | :--- | :--- |
| `.git/` | Directory | Git metadata |
| `.gitignore` | File | Git ignore rules |
| `.hermes.md` | File | Hermes agent prompt & context |
| `.orchestrator/` | Directory | Internal workspace orchestration |
| `.dawn/` | Directory | Dawn workspace state |
| `.superpowers/` | Directory | Superpowers plugin workspace state |
| `.worktrees/` | Directory | Git worktrees |
| `bin/` | Directory | Prebuilt binaries and tool outputs |
| `build/` | Directory | Local build artifacts |
| `destiny2_unpacked.bin` | Symlink | Symlink to game executable unpack dump |
| `docs/` | Directory | Curated repository documentation |
| `launch-destiny.cmd` | Script | Primary game client launcher |
| `launch-scot-reveal-debug.cmd` | Script | Debug launcher for Scot mission |
| `packages` | Junction | Filesystem junction to game data packages (`G:\Games\d2\packages`) |
| `Dawn/` | Directory | Core Dawn C++ source tree and project files |
| `dawn-dev.ps1` | Script | Dawn local development & build script |
| `tools/` | Directory | Diagnostic, compilation, and reverse-engineering tools |

*Note: The empty 0-byte file `activity` in root will be removed.*

---

## 3. Directory Taxonomy & Destination Map

### 3.1. `docs/handbook/`
Houses foundational architecture guides:
- **`bungie-bullshit-handbook.md`**: Moved from `internets guide to bungie bullshit.md` (human-readable comprehensive architecture, protocol, and reconstruction handbook; 5,001 lines).
- **`bungie-bullshit-agent-reference.md`**: Moved from `internets guide to bungie bullshit for ai agents.md` (ASD-STE100 strict controlled English guide for AI agents; 4,034 lines).

### 3.2. `docs/missions/`
Houses campaign mission reconstruction guides, grouped by campaign:

#### A. Homecoming (`docs/missions/homecoming/`)
- **`homecoming-networking-and-routing.md`**: Comprehensive networking and protocol architecture.
  - *Merged from:*
    - `HOMECOMING-AH-HANDSHAKE-HANDOFF.md` (Activity Host handshake, socket auth, state transitions)
    - `HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md` (Route selection tables, vtables, routing authority)
    - `HOMECOMING-MATCHMAKING-HANDOFF.md` (Matchmaking protocol, tickets, locateSession, sessions)
    - `HOMECOMING-QOS-HANDOFF.md` (QoS ping/pong probes, latency measurements, verdict codes)
- **`homecoming-mission-reconstruction.md`**: Mission execution, encounter schedules, and crash resolution.
  - *Merged from:*
    - `HOMECOMING-FINDINGS.md` (Core mission findings, crash traces, RVA offsets)
    - `HOMECOMING-HANDOFF.md` (General status and verified checkpoints)
    - `HOMECOMING-PHASE5-BUILDOUT.md` (Phase 5 encounter buildout, dialogue triggers)
    - `HOMECOMING-KIND22-HANDOFF.md` (Kind 22 transform/resolver structs and object tables)
    - `HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md` (Towerfall crash fault and type-43 publication)
    - `HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md` (Safe trace execution flow)
    - `SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt` (Native Scene-entry resolver chain disassembly)
    - `DAWN-HOMECOMING-BRIEFING-FOR-CODEX.md` (Architecture briefing & client expectations)
    - `HOMECOMING-NEW-CHAT-PROMPT.md` (Contextual notes & operational prompts)

#### B. Curse of Osiris (`docs/missions/coo/`)
- **`omega/omega-mission-reconstruction.md`**: Complete Omega mission flow and activity lifecycle.
  - *Merged from:*
    - `OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md`
    - `OMEGA_ACTIVITY_HANDOFF_2026-08-23.md`
    - `OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md`
    - `OMEGA_NEXT_STEPS_HANDOFF_2026-08-23.md`
    - `HANDOFF-OMEGA-ACTIVITY-INIT-20260823.md`
- **`omega/omega-vfx-and-rendering.md`**: VFX rendering pipeline.
  - *Merged from:*
    - `OMEGA-PURPLE-VFX-HANDOFF-20260822.md`
- **`panoptes-boss-encounter.md`**: Panoptes encounter mechanics and boss graph.
  - *Merged from:*
    - `PANOPTES-RED-EYE.md`
    - `PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md`
    - `PANOPTES-PLATFORMS-AND-ARC-CHARGES.md`
    - `MISSION-SCOT-PANOPTES-HANDOFF-20260905.md`
    - `MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md`
    - `MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md`
- **`scot-mission-reconstruction.md`**: A Garden World / Scot mission reconstruction.
  - *Merged from:*
    - `MISSION-SCOT-HANDOFF-20260905.md`
    - `MISSION-SCOT-LIGHTHOUSE-PORTAL-EFFECT-FIX-20260905.md`
    - `MISSION-SCOT-OSIRIS-FIRST-DAMAGE-20260905.md`
    - `GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md`
- **`strike-reconstruction.md`**: Strike reconstruction analysis.
  - *Moved from:*
    - `STRIKE-RECONSTRUCTION-RESEARCH.md`

### 3.3. `docs/research/`
Houses general research documents, gap comparisons, and raw evidence:
- **`vendor-interactions.md`**: Moved from `VENDOR-INTERACTIONS-FINDINGS.md`.
- **`session-notes.md`**: Moved from `SESSION-NOTES.md`.
- **`D2-Server-Infrastructure.pdf`**: Moved from root.
- **`edz_freeroam_objects.csv`**: Moved from root.
- **`gaps/`**:
  - `implemented-gaps-summary.md`: Merged synthesis of `IMPLEMENTED-GAPS.md` and `IMPLEMENTED-VS-MISSING.md`.
  - `deep-dives/`: Directory relocated from `Dawn-Implemented-Gaps-MDs-2026-09-09-035039/`.
- **`dumps/`**:
  - Relocates all ~150+ `*_out.txt` and `*_decomp.txt` trace files from root (including `c8_state_out.txt`, `omega_*_out.txt`, `qos_*_out.txt`, `service7_*_out.txt`, `authority_*_decomp.txt`, etc.).

### 3.4. `tools/re/`
Houses reverse-engineering scripts and probes:
- `scan_manager_active_writers.py`: Capstone memory scanner.
- `dawn_ai_probe.py`: Ghidra decompilation and scalar reference locator.
- `deploy-omega.ps1`: Deployment automation script.

---

## 4. Knowledge Preservation & Merging Rules

When combining files into the consolidated guides:
1. **Zero Information Loss**: Every technical fact, struct layout, packet field, RVA offset, IDA/Ghidra address, memory diagram, table, code excerpt, and unresolved bug description must be included in the unified document.
2. **Logical Subsystem Organization**: Group information by subsystem with clear headings rather than raw chronological diffs.
3. **Traceability**: Add a header note to each merged document listing the original source filenames that were consolidated into it.
4. **Link Integrity**: Internal markdown links referencing relocated files should be updated to point to their new canonical locations.

---

## 5. Verification Plan

1. **File Inventory Count**:
   - Verify that all ~35 original Markdown files have either been moved directly or fully absorbed into a consolidated document.
   - Verify that all ~150+ `.txt` dump files are present in `docs/research/dumps/`.
   - Verify that all Python/PS1 tools are present in `tools/re/`.
2. **Root Cleanliness Check**:
   - Run `dir` / `ls` on root to confirm only approved files (`launch-destiny.cmd`, `launch-scot-reveal-debug.cmd`, `dawn-dev.ps1`, `packages`, `destiny2_unpacked.bin`, `.gitignore`, `.hermes.md`, and core directories) remain.
3. **Script Execution Check**:
   - Verify that `launch-destiny.cmd` and `dawn-dev.ps1` continue to resolve paths properly from root.
4. **Git Status Cleanliness**:
   - Verify git status tracks all new paths and renames accurately.
