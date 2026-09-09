# Panoptes Boss Encounter Documentation Archive

This directory contains the complete reverse-engineering and reconstruction documentation for the Panoptes, Infinite Mind boss encounter in Curse of Osiris.

Every original document is preserved in full with all scalar setter ABIs, health sampling traces, platform mechanics, and graph execution records intact.

---

## Technical Documents Index

- [`PANOPTES-RED-EYE.md`](./PANOPTES-RED-EYE.md)  
  *Panoptes red eye glow activation via native animation scalar setter `A0FE60` (`CE0BA42D = 1.0` ➔ controller `20AA7FC1`).*
- [`PANOPTES-PLATFORMS-AND-ARC-CHARGES.md`](./PANOPTES-PLATFORMS-AND-ARC-CHARGES.md)  
  *Platform raising/lowering mechanics, elevator teleporters, Arc charge carry, and dunk socket interactions.*
- [`PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md`](./PANOPTES-EYE-IMMUNITY-HANDOFF-20260906.md)  
  *Eye vulnerability phases, shield event `E9D4F854`, health sample telemetry, and abrupt closure / immunity defect diagnostics.*
- [`MISSION-SCOT-PANOPTES-HANDOFF-20260905.md`](./MISSION-SCOT-PANOPTES-HANDOFF-20260905.md)  
  *Encounter progression handoff, boss intro graph, and transition to DPS phases.*
- [`MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md`](./MISSION-SCOT-PANOPTES-FAILED-GRAPH-HANDOFF-20260905.md)  
  *Post-mortem analysis of failed graph executions and invalid node cursor advancements.*
- [`MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md`](./MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md)  
  *Native graph node dispatch, monotonic progression enforcement, and recovery loops.*
