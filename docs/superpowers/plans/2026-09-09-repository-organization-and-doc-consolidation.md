# Repository Organization & Documentation Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the repository root by moving reverse-engineering dumps and tools into dedicated directories, and consolidating fragmented mission handoffs into authoritative guides with 100% knowledge retention.

**Architecture:** Create structured directories under `docs/` (`docs/handbook/`, `docs/missions/homecoming/`, `docs/missions/coo/`, `docs/research/`) and `tools/re/`. Relocate ~150+ raw text dump files and RE tools. Synthesize markdown handoffs by mission and subsystem, ensuring every table, offset, code snippet, and status finding is preserved in the merged targets.

**Tech Stack:** Git, PowerShell, Markdown, Python (reverse-engineering tools).

**Spec:** [`docs/superpowers/specs/2026-09-09-repository-organization-and-doc-consolidation-design.md`](file:///d:/Documents/VSCode%20stuff/evil-ass-repo-of-doom-and-despair/docs/superpowers/specs/2026-09-09-repository-organization-and-doc-consolidation-design.md)

## Global Constraints

- **No Information Loss**: Every technical fact, struct layout, packet field, RVA offset, IDA/Ghidra address, memory diagram, table, code excerpt, and unresolved bug description from source documents must be included in the unified documents.
- **Root Whitelist**: Only `.git/`, `.gitignore`, `.hermes.md`, `.orchestrator/`, `.sunrise/`, `.superpowers/`, `.worktrees/`, `bin/`, `build/`, `docs/`, `Sunrise/`, `tools/`, `destiny2_unpacked.bin`, `packages`, `launch-destiny.cmd`, `launch-scot-reveal-debug.cmd`, and `sunrise-dev.ps1` are permitted to remain in the repository root.
- **Root Safety**: Delete only the 0-byte `activity` file. Do not delete or alter junctions (`packages`) or symlinks (`destiny2_unpacked.bin`).
- **Traceability**: Every consolidated document must include a header listing the exact original filenames merged into it.

---

### Task 1: Create Directories, Relocate RE Tools & Text Dumps, and Remove 0-Byte Activity

**Files:**
- Create directories:
  - `docs/handbook/`
  - `docs/missions/homecoming/`
  - `docs/missions/coo/omega/`
  - `docs/missions/coo/panoptes/`
  - `docs/missions/coo/scot/`
  - `docs/missions/coo/strikes/`
  - `docs/research/gaps/`
  - `docs/research/dumps/`
  - `tools/re/`
- Move to `tools/re/`:
  - `scan_manager_active_writers.py`
  - `sunrise_ai_probe.py`
  - `deploy-omega.ps1`
- Move to `docs/research/dumps/`:
  - All root `*_out.txt`, `*_decomp.txt`, `omega_*_schema.txt`, `omega_scene_schema_data_out.txt`, `managed_session_promotion_trace.txt`, etc. (~150 files)
- Remove from root:
  - `activity` (0-byte empty file)

**Interfaces:**
- Consumes: Existing root files.
- Produces: Cleaned tool and dump directories; root freed of raw dumps and scripts.

- [ ] **Step 1: Create all target directories**

```powershell
New-Item -ItemType Directory -Force -Path `
  "docs\handbook", `
  "docs\missions\homecoming", `
  "docs\missions\coo\omega", `
  "docs\missions\coo\panoptes", `
  "docs\missions\coo\scot", `
  "docs\missions\coo\strikes", `
  "docs\research\gaps", `
  "docs\research\dumps", `
  "tools\re"
```

- [ ] **Step 2: Move tools and probe scripts to `tools/re/`**

```powershell
git mv "scan_manager_active_writers.py" "tools/re/scan_manager_active_writers.py"
git mv "sunrise_ai_probe.py" "tools/re/sunrise_ai_probe.py"
git mv "deploy-omega.ps1" "tools/re/deploy-omega.ps1"
```

- [ ] **Step 3: Move text dump and disassembly files to `docs/research/dumps/`**

```powershell
Get-ChildItem -Path "." -Filter "*.txt" -File | Where-Object { $_.Name -ne "SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt" } | ForEach-Object {
    git mv $_.Name "docs/research/dumps/$($_.Name)"
}
```

- [ ] **Step 4: Remove 0-byte `activity` file**

```powershell
Remove-Item -Path "activity" -Force
```

- [ ] **Step 5: Verify Task 1 moves and commit**

```powershell
git status -s
git commit -m "refactor(repo): move RE tools and dump files into dedicated folders"
```

---

### Task 2: Relocate Handbooks & General Research Documents

**Files:**
- Move: `internets guide to bungie bullshit.md` -> `docs/handbook/bungie-bullshit-handbook.md`
- Move: `internets guide to bungie bullshit for ai agents.md` -> `docs/handbook/bungie-bullshit-agent-reference.md`
- Move: `VENDOR-INTERACTIONS-FINDINGS.md` -> `docs/research/vendor-interactions.md`
- Move: `SESSION-NOTES.md` -> `docs/research/session-notes.md`
- Move: `D2-Server-Infrastructure.pdf` -> `docs/research/D2-Server-Infrastructure.pdf`
- Move: `edz_freeroam_objects.csv` -> `docs/research/edz_freeroam_objects.csv`
- Move: `STRIKE-RECONSTRUCTION-RESEARCH.md` -> `docs/missions/coo/strikes/strike-reconstruction.md`
- Move directory: `Sunrise-Implemented-Gaps-MDs-2026-09-09-035039` -> `docs/research/gaps/deep-dives/`
- Create consolidated: `docs/research/gaps/implemented-gaps-summary.md` (combines `IMPLEMENTED-GAPS.md` and `IMPLEMENTED-VS-MISSING.md`)
- Remove from root: `IMPLEMENTED-GAPS.md`, `IMPLEMENTED-VS-MISSING.md`

- [ ] **Step 1: Move handbooks to `docs/handbook/`**

```powershell
git mv "internets guide to bungie bullshit.md" "docs/handbook/bungie-bullshit-handbook.md"
git mv "internets guide to bungie bullshit for ai agents.md" "docs/handbook/bungie-bullshit-agent-reference.md"
```

- [ ] **Step 2: Move research files, PDF, CSV, and strikes doc**

```powershell
git mv "VENDOR-INTERACTIONS-FINDINGS.md" "docs/research/vendor-interactions.md"
git mv "SESSION-NOTES.md" "docs/research/session-notes.md"
git mv "D2-Server-Infrastructure.pdf" "docs/research/D2-Server-Infrastructure.pdf"
git mv "edz_freeroam_objects.csv" "docs/research/edz_freeroam_objects.csv"
git mv "STRIKE-RECONSTRUCTION-RESEARCH.md" "docs/missions/coo/strikes/strike-reconstruction.md"
git mv "Sunrise-Implemented-Gaps-MDs-2026-09-09-035039" "docs/research/gaps/deep-dives"
```

- [ ] **Step 3: Synthesize `implemented-gaps-summary.md`**

Combine `IMPLEMENTED-GAPS.md` and `IMPLEMENTED-VS-MISSING.md` into `docs/research/gaps/implemented-gaps-summary.md`, linking to the individual deep dive documents in `docs/research/gaps/deep-dives/`. Remove the original root files:
```powershell
git rm "IMPLEMENTED-GAPS.md" "IMPLEMENTED-VS-MISSING.md"
```

- [ ] **Step 4: Verify Task 2 and commit**

```powershell
git status -s
git add docs/
git commit -m "docs: relocate handbooks, general research, and gaps summary"
```

---

### Task 3: Consolidate Homecoming Documentation

**Files:**
- Create: `docs/missions/homecoming/homecoming-networking-and-routing.md`
  - Merging:
    - `HOMECOMING-AH-HANDSHAKE-HANDOFF.md`
    - `HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md`
    - `HOMECOMING-MATCHMAKING-HANDOFF.md`
    - `HOMECOMING-QOS-HANDOFF.md`
- Create: `docs/missions/homecoming/homecoming-mission-reconstruction.md`
  - Merging:
    - `HOMECOMING-FINDINGS.md`
    - `HOMECOMING-HANDOFF.md`
    - `HOMECOMING-PHASE5-BUILDOUT.md`
    - `HOMECOMING-KIND22-HANDOFF.md`
    - `HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md`
    - `HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md`
    - `SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt`
    - `SUNRISE-HOMECOMING-BRIEFING-FOR-CODEX.md`
    - `HOMECOMING-NEW-CHAT-PROMPT.md`
- Remove original root files via `git rm`.

- [ ] **Step 1: Write `docs/missions/homecoming/homecoming-networking-and-routing.md`**
  - Section 1: Overview & Traceability (listing source files)
  - Section 2: Activity Host (AH) Handshake & Socket Protocol
  - Section 3: Authored Route Selection & Routing Table Architecture
  - Section 4: Matchmaking Protocol, Session Discovery & Tickets
  - Section 5: QoS Measurement, Probing Protocol & Verdict Codes
  - Include all hex structures, packet diagrams, and call tables from the 4 source files.

- [ ] **Step 2: Write `docs/missions/homecoming/homecoming-mission-reconstruction.md`**
  - Section 1: Overview & Traceability (listing source files)
  - Section 2: Homecoming Campaign Architecture & Client Flow (Codex briefing)
  - Section 3: Towerfall Crash Analysis, Scene Resolver Chain & Mitigation
  - Section 4: Kind 22 Transforms & Object Resolution
  - Section 5: Phase 5 Encounter Buildout & Scripted Triggers
  - Section 6: Verified Checkpoints, Safe Trace Verification & Open Gaps
  - Include all RVA offsets, stack traces, disassembly listings, and state machine transitions.

- [ ] **Step 3: Remove consolidated Homecoming root files**

```powershell
git rm `
  "HOMECOMING-AH-HANDSHAKE-HANDOFF.md" `
  "HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md" `
  "HOMECOMING-MATCHMAKING-HANDOFF.md" `
  "HOMECOMING-QOS-HANDOFF.md" `
  "HOMECOMING-FINDINGS.md" `
  "HOMECOMING-HANDOFF.md" `
  "HOMECOMING-PHASE5-BUILDOUT.md" `
  "HOMECOMING-KIND22-HANDOFF.md" `
  "HANDOFF-HOMECOMING-TOWERFALL-2026-08-27.md" `
  "HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md" `
  "SCENE-ENTRY-RESOLVER-CHAIN-2026-08-27.txt" `
  "SUNRISE-HOMECOMING-BRIEFING-FOR-CODEX.md" `
  "HOMECOMING-NEW-CHAT-PROMPT.md"
```

- [ ] **Step 4: Verify Task 3 and commit**

```powershell
git add docs/missions/homecoming/
git commit -m "docs(homecoming): consolidate networking, routing, and mission reconstruction guides"
```

---

### Task 4: Consolidate Curse of Osiris (Omega, Panoptes, Scot) Documentation

**Files:**
- Create: `docs/missions/coo/omega/omega-mission-reconstruction.md`
  - Merging:
    - `OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md`
    - `OMEGA_ACTIVITY_HANDOFF_2026-08-23.md`
    - `OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md`
    - `OMEGA_NEXT_STEPS_HANDOFF_2026-08-23.md`
    - `HANDOFF-OMEGA-ACTIVITY-INIT-20260823.md`
- Create: `docs/missions/coo/omega/omega-vfx-and-rendering.md`
  - Merging:
    - `OMEGA-PURPLE-VFX-HANDOFF-20260822.md`
- Create: `docs/missions/coo/panoptes/panoptes-boss-encounter.md`
  - Merging:
    - `PANOPTES-RED-EYE.md`
    - `PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md`
    - `PANOPTES-PLATFORMS-AND-ARC-CHARGES.md`
    - `MISSION-SCOT-PANOPTES-HANDOFF-20260905.md`
    - `MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md`
    - `MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md`
- Create: `docs/missions/coo/scot/scot-mission-reconstruction.md`
  - Merging:
    - `MISSION-SCOT-HANDOFF-20260905.md`
    - `MISSION-SCOT-LIGHTHOUSE-PORTAL-EFFECT-FIX-20260905.md`
    - `MISSION-SCOT-OSIRIS-FIRST-DAMAGE-20260905.md`
    - `GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md`
- Remove original root files via `git rm`.

- [ ] **Step 1: Write `docs/missions/coo/omega/omega-mission-reconstruction.md`**
  - Section 1: Overview & Traceability
  - Section 2: Complete Mission Walkthrough, Flow & Encounter Progression
  - Section 3: Activity Lifecycle, Spawn Hold, and Transitions
  - Section 4: Script Field Consumers & Native Callbacks
  - Section 5: Activity Initialization Sequence
  - Section 6: Next Steps, Milestones & Remaining Gaps

- [ ] **Step 2: Write `docs/missions/coo/omega/omega-vfx-and-rendering.md`**
  - Section 1: Overview & Traceability
  - Section 2: Purple VFX Investigation & Root Cause
  - Section 3: Model Render Transformations & Component Bindings
  - Section 4: Visual Boundary & Attachments Schema

- [ ] **Step 3: Write `docs/missions/coo/panoptes/panoptes-boss-encounter.md`**
  - Section 1: Overview & Traceability
  - Section 2: Encounter Architecture & Phase Graph
  - Section 3: Eye State Machine (Red Eye, Vulnerability, and Immunity Shields)
  - Section 4: Platform Mechanics, Elevators, and Arc Charge Deployment
  - Section 5: Native Graph vs. Failed Graph Post-Mortem & Verified Transitions

- [ ] **Step 4: Write `docs/missions/coo/scot/scot-mission-reconstruction.md`**
  - Section 1: Overview & Traceability
  - Section 2: A Garden World / Scot Mission Progression & Objectives
  - Section 3: Osiris First Damage Event & Combat Mechanics
  - Section 4: Lighthouse Portal Effect Bug & Fix Implementation
  - Section 5: Ghost Dialogue Audio Cues & Mission Event Graph (Scot vs. Towerfall)

- [ ] **Step 5: Remove consolidated CoO root files**

```powershell
git rm `
  "OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md" `
  "OMEGA_ACTIVITY_HANDOFF_2026-08-23.md" `
  "OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md" `
  "OMEGA_NEXT_STEPS_HANDOFF_2026-08-23.md" `
  "HANDOFF-OMEGA-ACTIVITY-INIT-20260823.md" `
  "OMEGA-PURPLE-VFX-HANDOFF-20260822.md" `
  "PANOPTES-RED-EYE.md" `
  "PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md" `
  "PANOPTES-PLATFORMS-AND-ARC-CHARGES.md" `
  "MISSION-SCOT-PANOPTES-HANDOFF-20260905.md" `
  "MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md" `
  "MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md" `
  "MISSION-SCOT-HANDOFF-20260905.md" `
  "MISSION-SCOT-LIGHTHOUSE-PORTAL-EFFECT-FIX-20260905.md" `
  "MISSION-SCOT-OSIRIS-FIRST-DAMAGE-20260905.md" `
  "GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md"
```

- [ ] **Step 6: Verify Task 4 and commit**

```powershell
git add docs/missions/coo/
git commit -m "docs(coo): consolidate Omega, Panoptes, and Scot mission documentation"
```

---

### Task 5: Final Verification, Link Integrity & Clean State Check

**Files:**
- Repository root inspection
- Verification of test/launcher validity

- [ ] **Step 1: Check root file listing against whitelist**

```powershell
Get-ChildItem -Path "." -File | Select-Object Name
```
Ensure ONLY whitelisted root files remain:
- `.gitignore`
- `.hermes.md`
- `destiny2_unpacked.bin`
- `launch-destiny.cmd`
- `launch-scot-reveal-debug.cmd`
- `sunrise-dev.ps1`

- [ ] **Step 2: Check git status to ensure all modified and untracked files are accounted for**

```powershell
git status
```

- [ ] **Step 3: Validate launcher integrity**

Verify that `launch-destiny.cmd` and `sunrise-dev.ps1` work as expected without missing root dependencies.

- [ ] **Step 4: Final commit and summary documentation**

Create walkthrough document detailing all new locations and consolidated structure.
