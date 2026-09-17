# Omega (Curse of Osiris Finale): Unified Technical Dossier

> **File Type:** Master Consolidated Reference  
> **Source Documents Retained in this Directory:**
> - `OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md` (Canonical complete technical record, 2,267 lines)
> - `OMEGA-PURPLE-VFX-HANDOFF-20260822.md` (Purple VFX debugging, direct bank transform & model hooks, 762 lines)
> - `OMEGA_ACTIVITY_HANDOFF_2026-08-23.md` (Activity lifecycle state machine, transitions & spawn hold)
> - `OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md` (Script field consumers & native callback tables)
> - `OMEGA_NEXT_STEPS_HANDOFF_2026-08-23.md` (Milestones, deferred gaps & roadmap)
> - `HANDOFF-OMEGA-ACTIVITY-INIT-20260823.md` (Activity initialization sequence & generational binding)

---

## 1. Executive Summary & Scene Flow

**Omega** (`mission_scot`) is the final story mission of Curse of Osiris. The opening mission sequence takes place at the Mercury Lighthouse and gateway to the Infinite Forest.

```
[Spawn & Bootflow Interception]
  │  • Player held in spawn hold until package registration, roster build & host connection commit
  │  • Spawns in Mercury Lighthouse approach bubble with authored orientation
  ▼
[Opening Portal Cutscene Sequence: Three Scene Casts]
  │  • Scene 1: Initial speech and approach gestures (pure non-VFX presentation)
  │  • Scene 2: Void energy channeling (purple outline shader 0x80B9FDBE & directed beam 0x80C220EB)
  │            • Mesh suppression hooks drop duplicate static head/body models
  │            • Terminal actor-retirement transition is skipped to hold aura continuously
  │  • Scene 3: Portal stabilization and final dialogue; Scene 2 aura remains active
  ▼
[Infinite Forest Traversal ➔ Panoptes Arena]
  • Portal backdrop transitions to open state
  • Player traverses branches toward Panoptes encounter graph
```

---

## 2. Master Reverse-Engineering Identifiers & RVAs

| Handle / Symbol | Value | Type / Offset | Technical Description |
| :--- | :--- | :--- | :--- |
| **Scene Handle** | `0x80FCCE87` | Scene Definition | Ikora void channeling scene definition |
| **Aura Effect Ref** | `0x80B9FDBE` | Effect ID | Void outline / aura particle & shader reference |
| **Beam Effect Ref** | `0x80C220EB` | Effect ID | Directed void beam particle reference |
| **Component Def** | `0x815B84E3` | Component Def | Component holding bank transform & actor handles |
| **Transform Matrix**| `+0x210` (528d) | 32-Byte Matrix | Selector-1 transform matrix in `0x815B84E3` |
| **Transform Origin**| `+0x220` (544d) | Vector3 | Spatial origin coordinates in `0x815B84E3` |
| **Actor-1 Handle** | `+0x43C` | uint32 | Factory Actor-1 handle stored in `0x815B84E3` |
| `mission_cb_init` | `+0x17A2140` | Function RVA | Initializes mission state container |
| `mission_cb_event`| `+0x17A28C0` | Function RVA | Dispatches objective and trigger updates |
| `mission_resolve` | `+0x17A4D10` | Function RVA | Resolves entity handles to active world actors |
| `spawn_hold` | `+0x54F120` | Function RVA | Bootflow spawn gate hook |

---

## 3. Subsystem Deep Dives

### 3.1. Purple Void VFX & Direct Bank Resolver
The void energy visual composite is driven by two native scene events (`0x80B9FDBE` and `0x80C220EB`) using scene `0x80FCCE87` and transform selector 1:
- Selector 1 is read through the resolver's `direct_bank` path, explicitly bypassing the callback's object context argument in favor of internal bank entry 1.
- Native writer for bank entry 1 exhibits: `source_kind = 2`, `range = 1..1`, `source_index = 1`, `flags = 0x3C`, `cast_index = -1`.
- Because `cast_index = -1`, the writer does not bind to Actor 2's live skeletal bones, defaulting to the static root transform.
- Rewriting the callback context object late had no visual effect because the direct-bank path had already locked its transform source.

### 3.2. Model Duplicate Suppression & Scene 2 Retention
- **Duplicate Suppression:** Instantiating Scene 2's actor originally generated duplicate static meshes for Ikora's body, head, and cloth. Dawn intercepts model-construction callbacks matching Scene 2's factory actor handle, suppressing mesh creation while preserving particle event subscriptions.
- **VFX Retention:** Bypasses Scene 2's terminal actor-retirement transition so particle systems do not vanish at scene end. A deferred host completion latch replaces the skipped allocator signal, allowing Scene 3 to execute while Scene 2's beam persists.

### 3.3. Closed Vex Wall Discovery
In retail, a closed triangular Vex wall blocks the gateway until Ikora channels her light:
- Definition `80F47B52` and entity `80F4AE39` represent the authored closed wall.
- However, `mission_scot` does not instantiate the destination-owned registry entries in its local authority container, causing the client to skip wall entity binding. The portal renders in its open state immediately.

### 3.4. Generational Binding & Script Field Consumers
Activity infrastructure tracks four independent generation high-water marks:
1. **Connection Generation:** Incremented on reconnect / socket rebind.
2. **Activity Incarnation Generation:** Incremented on activity restart or retry.
3. **Region Generation:** Governs bubble transitions and zone handoffs.
4. **Publication Generation:** Governs BAP message transaction commit/rollback.

Script field consumers decode incoming network updates, mapping runtime slots `0x12` and `0x23` to activity director and mission controller state.
