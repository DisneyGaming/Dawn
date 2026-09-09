# Panoptes Boss Encounter: Unified Technical Dossier

> **File Type:** Master Consolidated Reference  
> **Source Documents Retained in this Directory:**
> - `PANOPTES-RED-EYE.md` (Red eye animation component, controller signals & native setter ABIs)
> - `PANOPTES-PLATFORMS-AND-ARC-CHARGES.md` (Platform mechanics, Arc charges, dunk sockets & elevators)
> - `PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md` (Eye vulnerability, shield events, health samples & closure defect)
> - `MISSION-SCOT-PANOPTES-HANDOFF-20260905.md` (Encounter graph execution & phase transitions)
> - `MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md` (Post-mortem of failed graph executions)
> - `MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md` (Native graph node dispatch & recovery loops)

---

## 1. Encounter Architecture & Progression Pipeline

The Panoptes, Infinite Mind boss fight in the Infinite Forest arena operates across a multi-phase encounter graph combining platforming, Arc charge relic delivery, and timed eye damage windows:

```
[Phase 1: Arena Intro & Red Eye Glow Activation]
  │  • Boss intro sequence triggers
  │  • Red eye glow activated via native scalar setter (CE0BA42D = 1.0f)
  ▼
[Phase 2: Osiris Intervention & Arc Relic Spawn]
  │  • Osiris projects illusions, disrupting Panoptes's simulation
  │  • Arc charge relic materializes on the central arena floor
  ▼
[Phase 3: Relic Carry & Dunk]
  │  • Player carries Arc charge across rising Vex platforms
  │  • Dunk interaction accepted at target socket (accepted native Arc dunk, cycle 1, epoch 13)
  ▼
[Phase 4: Teleport to Forward Eye Platform]
  │  • Elevator volume transports player to forward floating platform (epoch 14)
  │  • Eye-refill resource 80F45564 dispatches; shield disruption event E9D4F854 confirmed
  ▼
[Phase 5: Eye Vulnerability & Damage Phase]
  │  • Opening node 3 (animation clip 80F45179) opens the central eye
  │  • Initial eye_health_sample recorded: body 1.0, eye 1.0 (health handle 7CF9EDCD)
  │  • Player attacks central eye; transitions to loop node 6 (clip 80F4518E)
  ▼
[Phase 6: Recovery or Wipe Loop]
  • If damage threshold is crossed, Panoptes reels and advances to next platform phase
  • If uncrossed, Panoptes begins simulation deletion wipe
```

---

## 2. Master Reverse-Engineering Identifiers & RVAs

| Handle / Symbol | Value | Type / Offset | Technical Description |
| :--- | :--- | :--- | :--- |
| **Animation Comp** | `80F6690A` | Component Def | Panoptes master animation component |
| **Glow Float Output**| `CE0BA42D` | Provider Ord 40 | Authored eye glow float control in component `80F6690A` |
| **Boss Controller** | `80F6695E` | Controller Def | Filters output `CE0BA42D` (input 0) into `20AA7FC1` |
| **Filtered Output** | `20AA7FC1` | Controller Index 32 | Filtered/clamped `0..1` control signal consumed by eye assets |
| **Eye Material** | `80F45358` | Material Def | Panoptes central eye shader material |
| **Eye Particle** | `80F66915` | Particle Def | Eye-area particle emitter |
| **Eye Lights** | `80F451B9` / `C3`–`C6` | Light Defs | Eye illumination light bank |
| `set_anim_scalar` | `+0xA0FE60` | Function RVA | Native setter: binary searches providers by name hash |
| `write_provider` | `+0xA10180` | Function RVA | Writes new float into runtime provider memory |
| `dirty_notify` | `+0x5906A0` | Function RVA | Dirty notification dispatch updating materials and lights |
| **Shield Event** | `E9D4F854` | Event ID | Confirmed shield disruption event at platform arrival |
| **Health Handle** | `7CF9EDCD` | Handle | Panoptes eye health monitoring handle |
| **Opening Clip** | `80F45179` | Clip ID | Opening node 3 animation clip |
| **Loop Clip** | `80F4518E` | Clip ID | Loop node 6 animation clip |

---

## 3. Subsystem Deep Dives

### 3.1. Panoptes Red Eye Glow Activation Path
Panoptes's red eye appearance is controlled by authored animation scalar `CE0BA42D`:
1. `prepare_intro_vfx` confirms the current run generation matches the captured owner and resolves Panoptes's animation parent.
2. Invokes native function `+0xA0FE60`:
   ```cpp
   void set_animation_scalar(
       std::byte* animationComponent,
       const std::uint32_t* scalarName,
       float value);
   ```
3. Binary searches the component's 47 sorted definition rows, locates ordinal 40 (`CE0BA42D`), and passes the update to `+0xA10180`.
4. `+0xA10180` writes `1.0f` and calls `+0x5906A0` to notify eye material `80F45358`, particle system `80F66915`, and lights `80F451B9..C6`.

### 3.2. Eye Vulnerability & Immunity Defect
During live gameplay tests on build `5CEC908E`:
- `t=408391`: Accepted native Arc dunk, cycle 1, epoch 13.
- `t=412406`: Eye platform arrival allowed shield claim, epoch 14. Eye-refill resource `80F45564` returned dispatch success; shield event `E9D4F854` confirmed.
- `t=412422`: Initial `eye_health_sample`: Body 1.0, Eye 1.0, `crossed = 0`, health handle `7CF9EDCD`.
- `t=412422`: Opening node 3, clip `80F45179`. Cursors consume entries 0–3.
- `t=417094`: Node 6, clip `80F4518E` (classified by host as eye loop).
- **The Defect:** When shooting during the visible opening, the player sees **Immune** text and the eye closes within ~1 second. The downstream damage evaluator fails to clear the immunity mask despite shield event `E9D4F854` executing.

### 3.3. Platforms, Dunk Sockets & Relics
- Platforms raise and lower via physics volumes synchronised with Osiris's projections.
- Arc charge carry validates player weapon stowage and strips relic hold on socket deposit.
- Elevator pads apply targeted impulse velocity to carry the player to the forward damage platform.

### 3.4. Encounter Graph Execution & Recovery Loops
- Earlier graph iterations failed when node cursors advanced prematurely, skipping Phase 3.
- The native graph implementation ensures monotonic cursor traversal, gating recovery transitions on confirmed health threshold crossings (`crossed != 0`).
