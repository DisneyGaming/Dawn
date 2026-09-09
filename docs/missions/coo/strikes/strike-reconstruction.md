# Research handoff: reconstructing Destiny 2 strikes on Sunrise

Compiled 2026-09-07 for handoff to a planning agent. This document exists to brief an AI agent that has never
seen this repository on everything relevant to attempting a **strike** (matchmade, 3-player, wave/boss PvE
activity) on the Sunrise offline-revival project, so it can produce an implementation plan without re-deriving
context that already exists in this workspace's history.

This is not a plan. It is the evidence base a plan should be built from. Every claim below is sourced to a file
in this repository; read the source file before treating a claim as current truth, because this project's own
documentation repeatedly warns that comments and historical claims drift out of sync with the checked-out code.

**Authorization context:** this is personal offline reverse engineering / private-server revival work on the
user's own machine and their own legally-owned game client, in the same vein as the existing project. Nothing
here targets Bungie's live servers or other players.

---

## 1. What this project is

**Sunrise** is a from-scratch offline server + client-hook (DLL detour) reimplementation of Destiny 2's
networking, activity, and mission-execution systems, built by black-box reverse engineering of the retail
client binary (no leaked source). It lets the retail Destiny 2 client run missions against a private,
locally-hosted server instead of Bungie's infrastructure.

Two missions have been the subject of deep, dated reconstruction work:

- **`mission_scot`** ("Omega", the Season of the Deep Osiris/Panoptes mission on Mercury/Lighthouse →
  Infinite Forest). This is the most complete reconstruction: a full opening → Forest → Lair → 3 Crown combat
  cycles → ending → Mercury handoff has been played end-to-end on a generic mission executor. See
  [OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md](OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md)
  and [Sunrise/docs/COO-EXECUTOR.md](Sunrise/docs/COO-EXECUTOR.md).
- **`mission_towerfall`** ("Homecoming", a Tower/Guardian-Games-era-adjacent mission). This mission is
  architecturally important for strikes specifically because, unlike Omega (a private/solo-launchable mission),
  Towerfall's investigation accidentally reconstructed the **public matchmaking connect path** — the same
  machinery strikes need. See [HOMECOMING-MATCHMAKING-HANDOFF.md](HOMECOMING-MATCHMAKING-HANDOFF.md) and
  [HOMECOMING-QOS-HANDOFF.md](HOMECOMING-QOS-HANDOFF.md).

No strike has been attempted yet in this workspace. This document is the pre-work for the first attempt.

---

## 2. Why strikes are a different (harder) problem than the missions already done

The two reconstructed missions are structurally the closest thing to strikes in this codebase, but strikes add
requirements neither mission fully exercises:

1. **Matchmaking, not private launch.** Missions launch through a forced/authored destination-selection path
   (`activity_forced_destination.cpp`) that never needed Sunrise's matchmaking service to work end-to-end.
   Strikes are matchmade PUBLIC(-ish) activities — the Homecoming investigation only reached the matchmaking
   path because its private-launch route was a dead end offline (`HOMECOMING-MATCHMAKING-HANDOFF.md §11`:
   "a private activity never establishes a peer session with the embedded host"). Strikes will need this same
   matchmaking/QoS chain, most likely from square one, because they don't have Omega's private-launch escape
   hatch.
2. **3-player fireteam, not 1.** Everything proven so far is solo. No multi-peer session establishment,
   roster/authority replication to 2+ clients, or peer-to-peer host election has been exercised.
3. **Wave-based combat across an arbitrary arena, not four scripted set-piece encounters.** Omega's Crown
   combat (§8 below) is the closest analog and is genuinely useful, but it is still a fully hand-authored,
   per-encounter wave table (`omega_enemy_crown_waves.h`) built from one specific activity's extracted data —
   not a generic "read any strike's wave data and run it" system.
4. **Checkpointing / scoring / matchmaking-visible activity state** — strikes report score, have checkpoint
   resume, and (in some playlists) drop matched players into an in-progress instance. None of this has been
   investigated here.
5. **A boss/encounter director that must generalize.** Only one boss (Panoptes) has been reconstructed, and
   its combat is a hard-coded C++ definition (`omega_combat_definition.h`) with boss-specific mechanics
   (charge routes, eye exposure, rescue Scenes) hand-written in C++. Nothing in this repo yet reads an
   arbitrary strike boss's authored mechanics generically from package data.

Treat every "works" claim about missions as a **partial precedent**, not a proof that strikes will work the
same way.

---

## 3. The generic mission executor (CoO) — what's reusable as-is

Full detail: [Sunrise/docs/COO-EXECUTOR.md](Sunrise/docs/COO-EXECUTOR.md). Key facts a planning agent needs:

- `coo/executor.h` is a bounded, engine-agnostic C++20 DAG executor: named steps, typed commands, dependency
  masks, up to 32 steps / 8 commands per step / 128 queued observations. It is not Omega-specific.
- `coo/mission_runtime.h` owns run selection, publisher leases, definition validation, ordered publication.
  A `MissionDefinition` supplies a DAG, up to 8 module bindings, up to 32 fact-to-command bindings.
- `coo/native_services.h`, `coo/receipt_queue.h`, `coo/dialogue_service.h`, `coo/population_service.h`,
  `coo/scene_service.h`, `coo/presentation_services.h` are **shared, reusable native command/receipt bridges**
  already extracted from Omega's implementation and validated against a second synthetic (non-Omega) schema
  fixture (`COO-EXECUTOR.md §"Validation and adding a mission"`: "Independent contract fixtures exercise
  another schema and module layout, dialogue bank, population catalog and Scene layout... they are not a
  playable second mission").
- **Definitions are still compiled C++ headers, not data.** There is no implemented loader that reads a
  strike's package/pkg data and derives a mission graph automatically. The doc is explicit that "external
  `.def`/JSON loading and a portable definition format are not implemented" (per-mission JSON *scripting* of
  presentation/timing exists for Omega specifically — see format-1/format-2 discussion in the same file — but
  that is edits to an already-hand-recovered graph, not automatic graph derivation from an unread package).
- **No second real mission has been mapped through this executor yet.** The doc repeats this caveat >10 times.
  Whatever a strike attempt does will be the second (and first non-Omega, first matchmade) mission ever run
  through CoO — expect to extend it, not just reuse it unchanged.
- The `to-add-a-mission` playbook (`COO-EXECUTOR.md §"Adding a mission after the gate"`) is the closest thing
  to an existing procedure: (1) verify exact activity/package/receipt identities from the installed build using
  registry/definition/type/slot identities — never guessed indices; (2) define immutable section DAGs, module
  bindings, observation mappings, presentation tables; (3) supply typed authority producers + native request
  bindings, reusing dialogue/Scene/population services where contracts fit, keeping boss-specific mechanics in
  a "mission plugin"; (4) validate receipts at intake with captured command identity; (5) replay against
  captured evidence comparing actual authority bytes, then validate native sections, full runs, and consecutive
  launches before claiming compatibility.

**On the "auto-construct from package at load, zero hardcoded Lua" claim.** The user mentioned a community
member reportedly reconstructed a mission graph with a native executor "completely with 0 hardcoded lua...
auto construct it from mission pkg at mission load." Nothing in this repository's own documentation supports
that this has been achieved here — the opposite is stated repeatedly and explicitly: mission definitions are
compiled C++, the mission **successor graph** (branch/edge logic, not just node/content identity) is
unrecovered even for the most-inventoried mission (see §7 below, Towerfall: 713 nodes / 0 explicit successor
edges), and CoO's "generic" claim is about the *executor* (DAG runner, command dispatch, receipt plumbing)
being content-agnostic, not about automatic package-to-graph derivation. A planning agent should treat that
claim as an unverified external report, not as prior art available in this codebase, and should ask the user
for that person's actual source/method if it wants to build on it rather than assume it's already true here.

---

## 4. Combat/wave/AI knowledge already reconstructed (the "assign combat directory to squads" idea)

The user's proposed shortcut — "for combatant AI just gotta assign the combat directory to the squads" —
has real grounding in what's already reverse-engineered, but the granularity matters:

- **Spawner/source schema** (`Sunrise/src/state/activity/omega_combatant_authority.h`): the native type-1
  spawner's authority body (schema `80807EC9`, 641 bits, or 673 bits with a second category) is fully decoded
  and has a validated bit-exact writer. A `Source` names: a registry, a generation, a "rule slot" (which
  authored template/category the spawner should pull from), a loose-request count (how many members to
  request from that category), and an optional `TacticalGroup` (a native type-3 tactical group + authored row
  — this is very likely the "squad" assignment mechanism the user is thinking of). **Important:** the header's
  own comment states "The native spawner still owns template selection, placement, request queuing, actor
  creation and AI initialization" — Sunrise supplies *what to request and how many*, not squad AI logic
  itself. The actual combatant AI (pathing, target selection, ability use) is 100% native/retail; Sunrise never
  reimplements it, only triggers it correctly.
- **Wave table** (`Sunrise/src/state/activity/omega_enemy_crown_waves.h`): a fully reconstructed, statically
  validated 55-entry wave table for Omega's Crown encounter, covering 3 cycles (Fallen, Hive, final-platform
  Vex) x up to 4 sub-waves each, with per-wave requested counts, "member vs. squad" flags, and a boss-departure
  wave explicitly marked `required=false` because the boss can leave before those enemies die. This is real,
  validated, working data for **one specific encounter of one specific mission** — it is a template for the
  *shape* of a strike's wave table, not reusable data for any other activity. A strike's wave table would need
  to be independently extracted the same way (package walk + captured native counts), per strike, per boss.
- **Combat DAG** (`coo/omega_combat_definition.h`, referenced in COO-EXECUTOR.md §"Reveal, Lair and Crown
  ownership batch"): 7 sequential combat sections (Lair intro, 3 chase islands, 3 Crown cycles) each running in
  the generic executor, handling summon requests, population barriers, wave transitions, deletion, rescue
  Scenes, charge routes, eye exposure, recovery, relocation, and ending handoff. The **boss-specific mechanics**
  (Panoptes health/flight, Arc-charge ownership, rescue decisions) are explicitly kept as native Omega
  mechanics/"mission plugins," not part of the generic layer.

**Implication for planning:** "assign the combat directory to squads" is directionally correct as a mental
model — Sunrise's job in combat is spawner-request orchestration (who to summon, when, from where, into which
tactical slot), while retail native code does all AI — but each strike boss/encounter still needs its own
wave-table extraction and, likely, its own boss-mechanics plugin, following the same manual RE process that
took Panoptes weeks. There is no existing generic "read any encounter director and run it" implementation.

---

## 5. The matchmaking / QoS connect chain — the load-bearing prerequisite for any strike

This is arguably **the single most important prior-art section for a strike attempt**, more than either
mission's content reconstruction, because strikes cannot use Omega's private-launch shortcut.

Full detail across two dated handoffs, read in order:
[HOMECOMING-MATCHMAKING-HANDOFF.md](HOMECOMING-MATCHMAKING-HANDOFF.md) →
[HOMECOMING-QOS-HANDOFF.md](HOMECOMING-QOS-HANDOFF.md).

### 5.1 What's proven working (as of 2026-08-18, Towerfall/solo)

1. Forcing a mission's slice-set to "public" (`region_public.cpp`, hooking reader RVA `0xC210F0` at the exact
   caller RVA `0xE2B3BD` only) routes the client onto the **public citizen-join / matchmaking path** instead of
   the dead-end private/fireteam path. This is described as a "keeper — it works."
2. Sunrise's matchmaking service (BAP svc 42→43, `matchmaking_route.cpp` +
   `middleware/bap/matchmaking/response/`) can be made to answer a client's `sessionSearch` request with the
   locally-hosted region session instead of an empty result. The wire schema for the search-result response was
   fully reverse-engineered: `f3{ f1(repeated){ f1: descriptorMsg{ f1: 128B descriptor } } }` — critically, the
   search result's field 1 is a **descriptor submessage**, not a bare session id, which several earlier blind
   guesses got wrong and crashed the decoder ("policy-31 fatal decode / weasel").
3. NAT traversal to the embedded gameplay host (127.0.0.1:30976) works once the search result is correct.
4. The **QoS (Quality of Service) reachability handshake** with the embedded host was fully reverse-engineered
   field-by-field: request/reply packet layout, the accept flag, the payload-length field, and the anti-spoof
   token check. This is documented byte-for-byte in `HOMECOMING-QOS-HANDOFF.md §3` and is directly reusable
   wire-format knowledge for any future connect attempt (strikes included) — the packet formats are activity-
   agnostic transport, not mission content.

### 5.2 The blocker where this work stopped

The QoS reply's **payload** (bytes after the 27-byte header) must be a structured "session blob" that the
client's `session_tracker` decodes to judge the session suitable/unsuitable. Sunrise currently sends zeros
there, and the client reports `Reason=qos-payload-failed-to-decode`, marking the session unsuitable and
re-searching forever. The decoder lives in VMProtect-obscured code reached only through a dynamically-dispatched
vtable callback (`CALL [RBX+0x10]`), which defeated static RE. The last documented state is a **runtime-bypass
plan in progress**: a pump-hook was retargeted to intercept the per-result record at
`FUN_7ff619b0aea0` (RVA `0x1A9AEA0`) to capture the consumer object's vtable slot 0x10 at runtime and identify
the actual handler function, so it can be hooked directly (force "suitable") rather than reverse-engineering
the payload format blind. **This was the literal next step when the handoff was written; there is no evidence
in this workspace that it was completed.** Search `Sunrise/src/client/hooks/homecoming/` (or wherever
`qos_probe` currently lives) and `bin/x64/Sunrise/logs/sunrise.log` for `ev=qosprobe stage=consumer` to check
whether this was ever resolved before starting new work.

### 5.3 What this means for a strike plan

- Any strike attempt should **first check whether the QoS/matchmaking connect has since been completed** for
  Towerfall or any other activity (grep recent commits / logs / newer handoff files dated after 2026-08-18) —
  redoing this from scratch would be a large waste if it's already solved.
- If unsolved, this connect chain is a **prerequisite**, not a strike-specific problem — it blocks reaching a
  strike's world/roster at all, before any combat or graph work matters. It should likely be its own workstream
  ahead of strike-specific content reconstruction.
- Once solved for one activity, the QoS wire format and matchmaking search-result schema are almost certainly
  reusable as-is for a strike (they're transport-layer, not content-layer) — the main incremental work would be
  3-peer session establishment instead of 1, which is unexercised territory (see §2 point 2).
- `HOMECOMING-QOS-HANDOFF.md §5` explicitly flags the "honest ceiling" even after connect succeeds: a working
  session gives entity slots and script-runners, **not guaranteed enemies/acts** — a third party's parallel
  effort ("isinternet") reportedly reached a full public-region connect and still saw "0 acts." Content
  population is a separate, harder layer on top of connectivity.

---

## 6. Mission-content reconstruction methodology (generalize this to strikes)

`OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md §18.16` contains a battle-tested,
phase-by-phase playbook distilled from the Omega work, explicitly framed as reusable for "another mission."
This is the best available process template for tackling a strike's content layer once connectivity exists.
Summarized (see the source file §18 for full detail and the "why" behind each rule):

- **Phase A — launch truth:** recover exact activity package/index/destination/bubble/slice/spawn-hash; confirm
  the client reaches package registration and private host startup before touching scene code.
- **Phase B — roster truth:** walk package-authored registry objects; recover group keys and slot
  type/index/flags; preserve top-level vs. bubble-local ownership; prove phase-1 group registration before
  sending any object bodies.
- **Phase C — schema truth:** identify auth/sense schemas per descriptor; recover constructor defaults, biases,
  optional-field presence, inherited trailers (the doc has many hard-won examples of zero-fill silently
  breaking validity — see §18.7); encode one known-safe persistent body first.
- **Phase D — mission runtime truth:** initialize activity script + mission director with valid shared state;
  keep object *generation* stable (changing it tears down/rebuilds objects — a repeated footgun, §18.6); find
  real client sense edges for triggers; advance one scalar/latch at a time.
- **Phase E — scene truth:** seed inactive selectors before active edges; record timeline/cast/entity/factory
  identities *separately* (conflating them caused repeated confusion in the Omega work); A/B individual casts
  in isolation to localize which scene owns which visible/audible/VFX behavior (§18.13 — this specific
  technique, binary isolation of scene casts, was the single most productive debugging method used).
- **Phase F — presentation truth:** identify the exact component/resource producing unwanted output; suppress
  at the narrowest construction/consumer boundary, not the whole scene/actor; require actual visual
  confirmation, not just a successful memory write (the "+10 Z" experiment in §11.7 is a documented cautionary
  tale: a mutation can apply and read back correctly while having zero visible effect, because the renderer
  consumes a different, later transform).
- **Phase G — completion truth:** find the real native lifecycle edge after the authoritative handoff; defer
  host-state mutation out of allocator-sensitive teardown callbacks; publish destination/presentation deltas
  before marking script state complete.
- **Phase H — bounded continuation:** archive source/DLL-hash/log/screenshot per run; state the predicted
  outcome before testing; **abandon a route when its predicted effect fails despite confirmed application**
  (don't loop on "one more offset"); never let a diagnostic mutation become an undocumented permanent
  dependency.

Universal lessons worth flagging to a planning agent specifically (full list at
`OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md §18.1`–`§18.15`):

- Content discovery (a package has an asset) is not runtime existence (the client instantiated it) is not
  authority application (a body was applied) is not visible/gameplay effect. Each rung needs independent proof.
- A packet can encode correctly, have the right bit length, decode to the right registry/type/index, and still
  do nothing because no runtime object exists to receive it (`§17` documents this exact failure for a closed
  Vex wall object that was never mounted into the mission's authority container — the same class of problem
  will very likely recur for strike-specific destination-owned objects, e.g. arena hazards, encounter doors).
- Slot **array ordinal is not slot index** — always carry explicit registry+type+index+flags, never infer index
  from position.
- Object **`stateSequence`/generation is a lifetime/rebuild trigger, not a revision counter** — bumping it to
  push a state update destroys and recreates the object instead.
- Use evidence labels (confirmed / strong inference / hypothesis / disproven / untested) throughout any new
  research doc for a strike, the same way this one does — the Omega doc's own postmortem is that promoting
  plausible guesses to fact caused repeated wasted cycles.

---

## 7. Known gap: mission graph successor/branch logic is unrecovered

[GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md](GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md) is the
authoritative statement of this gap. Key numbers:

- The Towerfall package-content export inventoried **713 named nodes / structural containment links**, 89
  trigger-capable nodes, 201 action-capable nodes, 3,738 co-resident trigger/action candidate pairs, and
  **0 explicit successor edges**. §12.3 states plainly: "Sharing a group, matching an index, appearing near
  each other in a dump, or having a plausible name establishes a candidate—not causality." No automated or
  manual method in this codebase currently recovers "what happens after event X" from package data alone.
- The actual game-logic bridge between a local trigger volume and a published mission-state change (the
  "mission VM") is explicitly unresolved for Ghost dialogue dispatch (§4) — the producer that turns a local
  event into a published Type-53 dialogue record was never found; only the consumer side (record → audible
  line) was recovered.
- What **is** reconstructed instead is per-activity **hand-authored beat sequencing**: Towerfall currently uses
  a manually authored 3-beat manifest (`tower_watch_cue_manifest.h` — opening / breach / path-unlocked) with
  hash-transition detection standing in for real encounter-clear signals, explicitly flagged as unverified
  semantic equivalence to actual combat completion (§11.4).

**Implication:** a strike's encounter sequencing (wave 1 → wave 2 → boss phase → boss death → extraction) will
almost certainly need the same manual, per-strike beat-authoring approach used for Omega's Crown cycles and
Towerfall's 3-beat manifest — not automatic derivation from package data. Budget for this as the largest single
line item in any strike plan.

---

## 8. Relevant source tree map

Paths are relative to the repo root (`D:\Documents\VSCode stuff\evil-ass-repo-of-doom-and-despair`), Sunrise
source lives under `Sunrise/src/`.

**Generic mission executor (reusable):**
- `Sunrise/src/state/activity/coo/` — mission_runtime, executor, native_services, receipt_queue,
  dialogue_service, population_service, scene_service, presentation_services/cues (exact filenames per
  `Sunrise/docs/COO-EXECUTOR.md`).
- `Sunrise/docs/COO-EXECUTOR.md`, `Sunrise/docs/TOWERFALL-BOOTSTRAP-AND-MISSION-TABLES.md`,
  `Sunrise/docs/IKORA-ANIMATION-AND-ENDING.md`.

**Omega/Crown combat (reference implementation for wave/spawner patterns):**
- `Sunrise/src/state/activity/omega_combatant_authority.h` — spawner/tactical-group authority body writer.
- `Sunrise/src/state/activity/omega_enemy_crown_waves.h`, `omega_enemy_crown_catalog.h` — Crown wave table.
- `Sunrise/src/state/activity/omega_enemy_lair_wave.h` — Lair wave data.
- `Sunrise/src/state/activity/omega_boss_combat_action.h`,
  `Sunrise/src/client/hooks/bootflow/omega_boss_combat_runtime.inl`,
  `Sunrise/src/client/hooks/bootflow/omega_boss_combat_start.h` — boss-specific mechanics/hooks.
- `Sunrise/src/client/hooks/bootflow/opening_authority/` — Ghost/VM bridge, Type-53/68 capture evidence
  models (dialogue, objectives — directly reusable pattern for a strike's HUD/objective wiring).

**Matchmaking / QoS / networking (prerequisite for any strike):**
- `Sunrise/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp`
- `Sunrise/src/middleware/bap/matchmaking/` (definition.h, request/, response/)
- `Sunrise/src/server/gameplay/gameplay_advertisement.cpp`
- `Sunrise/src/server/gameplay/group/group_host_sessions.cpp`
- `Sunrise/src/server/gameplay/endpoint/gameplay_endpoint.cpp` — QoS responder.
- `Sunrise/src/server/gameplay/peer/peer_transport.cpp`, `group/group_host.cpp`, `dtls/dtls_host.cpp`,
  `association/association_host.cpp` — the peer/host transport that answers a join once QoS passes.
- `Sunrise/src/state/matchmaking/` — matchmaking_state.cpp/h, transactions/matchmaking_commit.cpp,
  matchmaking_prepare.cpp.
- `Sunrise/src/client/hooks/homecoming/` — `region_public.cpp` (working public-slice-set force), `activate.cpp`,
  `authored_probe.cpp` (largely superseded/dead-ends, kept for the documented lessons in
  `HOMECOMING-MATCHMAKING-HANDOFF.md §12`).

**Bootstrap / activity launch:**
- `Sunrise/src/state/activity/forced/definition.h`, `activity_forced_destination.cpp` — forced-destination
  override mechanism used for both Omega and Towerfall private launches (not matchmaking).
- `Sunrise/src/client/hooks/bootflow/towerfall_executor_bootstrap.cpp` — the launch-record correction
  technique (rewrite selected source/destination indices before native publication derives dependent fields).
- `Sunrise/src/middleware/content/packages/tables/scenario_reader.h`, `Sunrise/src/state/build_data/scenarios/`
  — scenario/bubble/registry extraction.

**Exports / extracted data (inspect before assuming any strike data exists):**
- `Sunrise/exports/omega_inventory.md`, `Sunrise/exports/towerfall_cue_edges.md` — note: no equivalent
  strike export exists yet; producing one (via the same package-walk tooling) is almost certainly step 1 of
  any strike plan.

**Build/verification tooling:**
- `tools/coo/verify.py`, `tools/coo/install_candidate.ps1`, `tools/coo/freeze_baseline.py` — the CoO
  regression/installation harness; a strike definition should go through the same verify → install → in-game
  acceptance checklist documented in `COO-EXECUTOR.md §"Omega acceptance checklist"`.

---

## 9. Open questions a planning agent should resolve before writing a plan

1. **Is the QoS session_tracker payload-decode blocker (§5.2) still open?** Check for newer handoff files,
   commits, or log evidence past 2026-08-18 before assuming this needs to be redone.
2. **Which specific strike is the target?** Everything here is mission-shaped evidence; the target strike's
   own package needs its own Phase-A/B identity recovery (§6) before any of the Omega/Towerfall specifics
   (registry keys, schema hashes, RVAs) can be assumed to transfer — they are almost certainly build- and
   content-specific, not universal constants.
3. **Solo-first or fireteam-first?** Given that no multi-peer session work exists at all, a planning agent
   should seriously consider scoping an initial milestone to a solo/private-launch strike attempt (reusing
   Omega's forced-destination technique, if the target strike's launch path allows it) to de-risk the content
   layer before taking on 3-peer session establishment and real matchmaking simultaneously.
4. **What does "combat directory" mean concretely for the target strike's boss?** Determine whether the boss
   has Panoptes-style unique mechanics (requiring a bespoke mechanics plugin, per `COO-EXECUTOR.md`'s
   "mission plugin" pattern) or is closer to a pure wave-clear encounter (closer to Crown cycles 1–2, which are
   largely spawner/wave-table driven).
5. **Does the community-member claim in the user's original message (auto-graph-construction, zero hardcoded
   Lua) refer to work outside this repository?** If so, get their actual approach/source before assuming it
   can be integrated — nothing in this codebase currently does that, and §3/§7 above document why it's a hard
   unsolved problem here (no data-driven definition loader; no recovered successor-edge extraction).

---

## 10. Source index (files referenced above)

- [Sunrise/docs/COO-EXECUTOR.md](Sunrise/docs/COO-EXECUTOR.md)
- [Sunrise/docs/TOWERFALL-BOOTSTRAP-AND-MISSION-TABLES.md](Sunrise/docs/TOWERFALL-BOOTSTRAP-AND-MISSION-TABLES.md)
- [OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md](OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md)
- [GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md](GHOST-DIALOGUE-MISSION-GRAPHS-SCOT-TOWERFALL.md)
- [HOMECOMING-MATCHMAKING-HANDOFF.md](HOMECOMING-MATCHMAKING-HANDOFF.md)
- [HOMECOMING-QOS-HANDOFF.md](HOMECOMING-QOS-HANDOFF.md)
- [MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md](MISSION-SCOT-PANOPTES-NATIVE-GRAPH-20260905.md)
- `Sunrise/src/state/activity/omega_combatant_authority.h`
- `Sunrise/src/state/activity/omega_enemy_crown_waves.h`

Other files in the repo root (`HANDOFF-*.md`, `HOMECOMING-*.md`, `OMEGA_*.md`, `PANOPTES-*.md`,
`MISSION-SCOT-*.md`) contain further dated, narrower investigations (VFX binding, Ikora animation, red-eye/arc
charge mechanics, portal effect fixes) not summarized here because they are Omega/Towerfall-specific content
detail rather than strike-transferable architecture. Consult them if the planning agent needs deeper precedent
on a specific subsystem (e.g. boss VFX binding, Scene lifecycle) mentioned only briefly above.
