# Scot (A Garden World): Unified Technical Dossier

> **File Type:** Master Consolidated Reference  
> **Source Documents Retained in this Directory:**
> - `MISSION-SCOT-HANDOFF-20260905.md` (Mission progression, spawner delivery & encounter checkpoints)
> - `MISSION-SCOT-LIGHTHOUSE-PORTAL-EFFECT-FIX-20260905.md` (Vex portal wall warp effect defect & participation hash fix)
> - `MISSION-SCOT-OSIRIS-FIRST-DAMAGE-20260905.md` (Osiris rescue sequence, Fallen wave deletion & Arc delivery)
> - `GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md` (Ghost dialogue audio lines, mission trigger queues & graph comparison, 1,185 lines)

---

## 1. Executive Summary & Mission Scope

**Mission Scot** (A Garden World / Past Mercury simulation) guides the player from the Mercury Lighthouse approach into the Infinite Forest to rescue Osiris's projection and battle the Root Mind.

```
[Mercury Lighthouse Approach]
  │  • Player contacts Vex gateway portal at (366.69, 249.99, 100.59)
  │  • Participation record seeded with hash 52B968BA triggers native warp VFX & Wwise audio
  ▼
[Infinite Forest Traversal]
  │  • Branched tree-graph evaluation loads Past Mercury simulation nodes
  │  • Ghost dialogue rows play dynamically without blocking wave spawns
  ▼
[Osiris Rescue & First Damage Phase]
  │  • Three progressive Fallen assault waves cleared (deletion event 1D3CF86F at t=366562)
  │  • Osiris NPC 99BD2FEB/1/0 delivered through native routine 4E8FB0
  │  • Intro Scene 99BD2FEB/43/9 delivered through native routine B41330
  │  • Dialogue rows 15–16 trigger upon verified rescue_ready receipt
  │  • Ring cores BF06 (slots 28–30) and Arc charges activate damage loop
```

---

## 2. Master Reverse-Engineering Identifiers & RVAs

| Handle / Symbol | Value | Type / Offset | Technical Description |
| :--- | :--- | :--- | :--- |
| **Portal Carrier** | `BA5F26EF / 4 / 0` | Carrier Handle | Name: `lighthouse_teleport` |
| **Carrier Def** | `80F47B52` | Definition Ref | `80F47B52 / 80809928 / 4C8` |
| **Portal Coordinates**| `(366.69, 249.99, 100.59)` | Vector3 | Placement coordinates of the Vex portal wall |
| **Portal Entity** | `80F4AE39` | Entity ID | Native portal gateway entity |
| **Portal Controller**| `8156EEC5` | Controller Ref | `8156EEC5 / 80803D2D / 168` |
| **Child Effect Ent** | `80C6600C` | Entity ID | Portal vortex child effect entity |
| **Render Resource** | `80FEB6F1` | Resource ID | Portal vortex visual mesh / shader |
| **Sound Resource** | `80C2B106` | Resource ID | Portal audio sound emitter |
| **Wwise Bank** | `80C2B105` | Audio Bank | Event: `AC668354`, Media: `81001499` |
| **Player Record Hash**| `52B968BA` | Participation Hash | Required hash for condition `80804D83` |
| **Fallen Deletion** | `1D3CF86F` | Event ID | Event confirming wave 3 Fallen clearance (`t=366562`) |
| **Osiris NPC** | `99BD2FEB / 1 / 0` | Actor Reference | Def: `80F4799D`, Class: `8080948F`, Def Offset: `878` |
| **Osiris Scene** | `99BD2FEB / 43 / 9`| Scene Reference | Def: `80F479BF`, Class: `80806266`, Meta Offset: `+164` |
| `npc_delivery` | `+0x4E8FB0` | Function RVA | Native routine delivering cataloged NPC bodies |
| `scene_delivery` | `+0xB41330` | Function RVA | Native routine reading decoded Scene body and starting selector |

---

## 3. Subsystem Deep Dives

### 3.1. Lighthouse Vex-Wall Portal Effect Fix
Walking into the Vex portal wall originally triggered an abrupt position hop without playing the warp effect.
- **Investigation:** Carrier controller `8156EEC5` executed its callback at wall contact, but its activation count remained zero.
- **Condition Analysis:** The controller evaluates two non-inverted conditions joined by `AND`:
  - `80804D72` (payload `00001000`): Entity type mask check (passed).
  - `80804D83` (payload `52B968BA`): Hash-membership check against the contacting player's participation record (failed because Dawn encoded the list as empty).
- **Resolution:** Seeding hash `52B968BA` into the initial player participation record satisfied condition `80804D83`, allowing the controller to accept wall contact and execute the full vortex VFX and Wwise sound event `AC668354`. Confirmed working in live playthroughs.

### 3.2. Osiris Rescue & First Damage Phase Mechanics
- **Wave Clearance:** Three progressive Fallen waves spawn in the arena. Encounter clearance is verified when native deletion event `1D3CF86F` is logged.
- **Osiris NPC Delivery:** Uses cataloged NPC source delivery through original function `+0x4E8FB0` using stable decoded body `80807EC9`. Definition offset is `878` (distinguishing it from enemy spawners at `728`).
- **Scene Delivery:** Delivered via native routine `+0xB41330`, which reads the decoded body and starts the authored selector. Runtime class filter is `80806266` (avoiding the source-reference metadata at `+164` which is `80806382`).
- **Arc Relics & Sinks:** Ring cores `BF06` (slots 28–30) and Arc sink devices receive generation-2 preparation in decoded storage. Charge/sink source ownership uses the typed member reference and native owner resolver.

### 3.3. Ghost Dialogue Graphs: Scot vs. Towerfall
- In Scot, dialogue row 14 dispatches upon Fallen wave clearance, while rows 15–16 gate on the accepted `rescue_ready` receipt from Osiris's scene delivery.
- **Architectural Difference:** Unlike Towerfall's linear Underwatch corridors (where dialogue completion directly drives Scene state machines and door unlocks), Scot's Infinite Forest graph uses branch-based node evaluation:
  - Traversal branches evaluate whether the active node matches destination seed parameters.
  - Dialogue rows are decoupled from combatant spawn health, allowing banter to play dynamically without blocking wave spawns.
