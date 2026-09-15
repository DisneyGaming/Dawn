# A Garden World strike

This implements the A Garden World strike from the Lighthouse approach through
Dendron and the closing exchange. The launcher exposes it under Curse of Osiris.
The campaign scanner and Panoptes ending belong to a different mission.

## Ownership and code map

[The mission Lua](../scripts/strike_bond.lua) owns route dependencies, encounter
requests, dialogue, objectives and completion through the shared CoO executor.
The native adapters supply recovered bindings and authenticate engine receipts.
Population, object lifecycle, scene commands, dialogue, generations and protocol
publication use the [shared mission services](UNIVERSAL-MISSION-SERVICES.md).

- [Catalog](../src/state/activity/strike_bond/catalog.h) and
  [bindings](../src/state/activity/strike_bond/bindings.h): authored resources,
  registry slots, devices, scenes and registered executor capabilities.
- [Controller](../src/state/activity/strike_bond/controller.cpp),
  [frame](../src/state/activity/strike_bond/frame.h) and
  [runtime](../src/state/activity/strike_bond/runtime.cpp): mission state and receipts.
- [Authority](../src/state/activity/strike_bond/authority.h) and
  [roster](../src/server/bap/encrypted/push/activity/strike_bond_roster.h): native
  source, member, device, shield and region publication.
- [Boss cycle](../src/client/hooks/bootflow/strike_bond_boss_cycle.inl),
  [damage](../src/client/hooks/bootflow/strike_bond_boss_damage.inl),
  [effects](../src/client/hooks/bootflow/strike_bond_boss_shield.inl) and
  [retirement](../src/client/hooks/bootflow/strike_bond_boss_retirement.inl):
  authenticated native encounter adapters.

The complete implementation includes native character and target observers as
well as server authority. It also integrates launch preparation, Forest generation,
player-position observations, lens damage and object receipts. Shared task-cost
handling remains available to Tree of Probabilities without replacing its mission logic.

## Route and traversal

The route proceeds through the Lighthouse, Infinite Forest C, Simulant Past and
Spire. The Forest retains native island generation, gateway placement and Daemon
rules. Regional checkpoint publication follows arrival in the Past and Spire.

Forest C's two-endpoint selection uses the shared typed route contract; its grid,
side coordinates, retained unused-side heights, budget and authored topology live
in the Garden binding data, and the existing type-37 authority path applies it.

1. Defend the Lighthouse approach, open the Forest entrance and cross the Forest.
2. Clear the exit defense and enter the Past through its native gateway.
3. Cross the terraces, destroy the security modules and defeat the shielded
   Minotaurs protecting the route. Destroying a cube removes its linked barrier
   or guardian shield; genuine guardian death releases the dependent encounter.
4. Defeat the cannon defenders and climb the Spire. The lower cannon is available
   before the middle-floor encounter; the remaining encounter dependencies still
   control access to the roof fight.
5. Destroy the roof's middle cube, complete Dendron's opening and enter the boss cycle.

All eight placed cannon effects are requested when their respective areas become
active: four in the Past and four in the Spire. Effect presentation is independent
of the launch-source and encounter gates, so approaching a cannon does not reveal
an otherwise missing effect or bypass its progression requirements.

Guardian creation, AI readiness, scene startup, cube exposure and death are distinct
receipts. Optional beam creation never blocks cube exposure or progression. Route
beams retire with their cubes, including when a late creation arrives after destruction.
Native type-26 actor shields remain tied to each intact cube independently of AI release.

## Dendron encounter

Dendron is prepared during the Spire ascent through the authored loose combatant
source and rooftop tactical assignment. His intro binds the admitted actor and
intact middle cube. Cube destruction releases the intro; completed opening playback
is required before normal combat begins.

Health thresholds at two-thirds and one-third start the two guardian intermissions.
Damage is clamped at those boundaries so one hit cannot skip the required phases.
Dendron turns toward the appropriate side using native platform interpolation,
folds into the authored dormant state and stops at the associated guardian pair.
Movement selects an equivalent nearby platform phase where necessary; it does not
teleport the actor to the destination or increase the native turn speed.

Each intermission creates only its own two cubes and Minotaurs. Once native scene
startup and actor readiness are confirmed, both Minotaurs receive their native AI
release immediately while their cubes remain intact. Each cube independently owns
its guardian's shield. Both genuine guardian deaths are required to release the
boss's wake sequence and resume vulnerability. The later pair is not spawned early.

Dendron uses authored intro, sleep, wake, burst and death actions. A zero-health
sample starts the death presentation but does not count as a kill. Ending progression
requires the authenticated native health-death event for the admitted boss.

## Face effects and death cleanup

The purple face effect becomes present with the fight. Its model pass base is
recovered from the authenticated authored mesh ranges, rather than the first mutable
runtime flag sample. The adapter owns only the base bit and preserves native
visibility references and every other pass flag. Effective draw state is published
when the binding or contribution changes, without a periodic draw-count override.

The blue outer shell has a separate material input. Native parent phase output
`0958590C` feeds its bound controller and model input; the existing native setter
selects the damage-phase value that hides the shell while preserving the purple
face geometry. Other encounter phases retain the authored shell presentation.
Material resources are not edited globally, and damage immunity remains a separate
mechanic. Terminal state removes the adapter's own effect contribution.

Native death detaches Dendron from his source before the next source-retirement
publication. Source retirement alone therefore cannot find the remaining corpse.
The adapter retains the live actor/entity lease and dispatches cleanup at the
existing `4EC1A0` source-retirement boundary after genuine death authorization.
It checks the run, source, salted actor, parent, character graph, entity, bundle,
world identity and allocator service before calling native `56A8F0` once.
No lock crosses that call; the engine schedules its own teardown. This cleanup
adds no further detour and does not mute audio or use a death-delay timer.
The current runtime path has been observed accepting the retirement mark and
subsequently replacing the retired entity.

## Ending and reconstruction limits

Genuine boss death interrupts unfinished optional encounter work, requests the
native chest, retracts changing cover, removes respawn restrictions and navigation,
and retains the closing dialogue before mission completion. Branching dialogue
comes from the native bank rather than concatenated alternatives.

The roof uses all 32 authored cover placements and native device animation. Its
four groups of eight and twelve-second selection cadence are reconstruction choices;
the exact retail cover schedule has not been recovered. Music currently advances
through selected native candidates by region and boss phase, but the retail
phase-to-score mapping remains a reconstruction rather than a proven match.

The playable route and current boss presentation do not establish exact retail
parity for every opening orientation, population variation, dialogue branch or
wipe/re-entry state. Preserve native ownership and genuine receipts when extending
these areas; a request, detached actor or elapsed presentation window must not be
substituted for a required creation, scene-completion or death observation.
