# Dendron AI: tracking, firing, and reuse notes

## Short version

Dendron's aim and firing are still handled by the game's native AI. Sunrise
does not rotate him toward a player, force his fire flag, bypass weapon checks,
or invent a target. It does three important things around that native AI:

1. Spawns and authenticates the real Dendron actor with his authored combat
   rule and tactical assignment.
2. Repairs one stale pre-combat **no-target** action by reconnecting it to the
   native primary-target slot.
3. Leaves native code to perform tracking, visibility/range checks, weapon
   eligibility, charge, cooldown, and the actual shot.

The basic flow is:

```text
authored Dendron source and tactical task
                  |
                  v
native AI chooses a live target -> primary target slot 0
                  |
                  v
opcode-24 weapon action asks for a target slot
                  |
        native decoder returns 0xFF?
             / yes           \ no
            v                 v
  scoped repair selects 0   keep native result
             \               /
                  v
native aim + visibility + range + eligibility
                  |
                  v
native charge/cooldown/fire
```

## 1. Actor and AI ownership

Dendron is requested as the authored loose combatant source:

- Registry: `0x2CB86C0F`
- Source definition: `0x80F54740`, type `1`, slot `3`
- Named actor/member: `0x80F54743`, type `2`, slot `4`
- Combat rule: `513`
- Tactical task row: `10`

That normal loose-combatant request owns spawning and AI. The tactical binding
puts Dendron into the rooftop fight using his native behavior package. Sunrise
then validates the complete live chain before touching any AI-adjacent boundary:

```text
mission run -> source generation -> actor -> entity -> character
            -> AI controller -> main state -> weapon substate
```

The salted handles, registry, source slot, generation, entity row, and backlinks
must all still match. A resource hash by itself is not accepted as identity.

## 2. Native target acquisition and tracking

The native AI first chooses the target. The recovered primary target is a weak
entity reference at `mainState + 0x1680`, called **slot 0** in the weapon action.
The code comments identify this slot as being populated by native `action54`.

Before accepting it, the adapter verifies that the target:

- is a valid live entity row;
- is not Dendron's own entity;
- still has the same handle and serial after the validation pass; and
- belongs to the same current Dendron controller and mission generation.

The firing request is an exact authored opcode-`0x24` action. Native `0xC613E0`
establishes the weapon owner, and its nested `0xC5FEF0` call decodes which target
slot that action should use. If that decoder returns `0xFF` for the exact
no-target request, the scoped adapter changes only the local decoded value to
slot `0`.

After that one local selection, the engine consumes its own target and owns:

- body/head tracking and aim;
- line-of-sight and visibility;
- distance/range checks;
- whether the current weapon can engage;
- charge and cooldown timing; and
- whether a projectile is actually fired.

There is no direct yaw/pitch write and no permanent target pointer. The scope
ends when the native dispatch returns.

## 3. Eligibility and the stale no-target repair

The important firing boundary is native `0xC31200`:

```cpp
eligibility(controller, weapon, context) -> uint8 result
```

The original function always runs first. If it returns false, Sunrise does
nothing. If it returns true, there is a short window before `0xC306A0` consumes
the target.

Dendron could enter combat with a cached pre-combat no-target action. Native
cache comparison did not consider the transition into combat enough of a change
to submit that action again. The repair therefore requires all of the following:

- the exact admitted Dendron and current mission generation;
- the real weapon substate belonging to his controller;
- the stale requested value `0xFF02`;
- cache class `29`, which is the opcode-`0x24` row;
- the exact cached no-target bytes and exact authored action; and
- the recovered native parameter environment and program.

It then rebuilds the native context and reissues that same action through
`0xC620F0`. The replay is limited to four attempts with at least one second
between attempts. The adapter does not manufacture a successful eligibility
result or call the projectile/fire routine itself.

## 4. What `kind=0` and `kind=3` mean

In `garden_fire_trace`, these are **our local trace categories**, not native AI
modes and not component kinds.

### `kind=0`

- Boundary: `0xBC8F20`
- Hook: `one_tick_hook(character)`
- Observes Dendron's firing-suppression values before and after one native tick.
- Suppression is read from `character + 0xAC0` (ticks) and `+0xAC4` (active).
- It was used to determine whether native processing changed or cleared the
  firing hold. It never clears the hold itself.

### `kind=3`

- Boundary: `0xC31200`
- Hook: `eligibility_hook(controller, weapon, context)`
- Records native eligibility success/failure plus the relevant weapon and
  context state.
- A successful result opens the safe `before_commit` window used for the bounded
  cached-action replay described above.
- It never turns a failed native result into a pass.

For completeness, the trace also labels the duration boundary `kind=1` and the
regular character update `kind=2`. All four labels were created for diagnosis;
they are not a four-state implementation of Dendron's AI.

## 5. Fight phases and their effect on AI

Dendron's boss state machine coordinates the native animation controller with
mission progression:

1. **Opening:** the intro sequence holds him until the middle cube is destroyed
   and native opening playback finishes.
2. **Damage:** native combat AI can track and fire. Damage is clamped at the
   two-thirds and one-third health floors.
3. **Parking/sleep:** Dendron turns on his native platform, plays the authored
   sleep sequence, and becomes immune for the guardian intermission.
4. **Dormant:** both genuine guardian deaths are required before wake begins.
5. **Waking:** native gate inputs and the authored burst effect release the
   sleep sequence. Once the selector is inactive, normal damage mode resumes.
6. **Dying:** zero health starts the authored death sequence, but only the
   genuine native death callback completes the encounter.

The phase code does not patch Dendron's AI flags or suppression bytes. It adds
and removes named native animation inputs and observes the selector state. Boss
damage immunity, visual shielding, movement, and firing eligibility are separate
systems even though the player experiences them as one encounter.

## 6. The other kind 3: burst clip event

There is a second, unrelated use of the number `3`. `garden_cycle::burst` builds
a real 32-byte animation clip event:

- byte `+0x02`: clip-event kind `3`
- dword `+0x08`: absent marker `0x811C9DC5`
- dword `+0x18`: Dendron burst resource `0x80F45BA6`
- dispatcher: `0x104DA00(event, context, animator, 0)`

This requests the authored burst effect during opening/wake transitions. It is
not the `garden_fire_trace kind=3` eligibility hook, and there is no paired raw
clip-event kind 0 in Dendron's boss-cycle implementation.

## 7. Reusing the pattern for another boss

Reuse the **ownership and repair pattern**, not Dendron's constants.

For the new boss, recover and verify:

1. Its authored source, member, combat rule, tactical assignment, and real
   admission/death receipts.
2. The actor-to-entity-to-character chain and the correct AI controller, main
   state, and weapon substate.
3. Where its native AI publishes the current target and which action/slot its
   weapon reads. Do not assume `+0x1680`, opcode `0x24`, or slot `0` is shared.
4. Its target-decoder and eligibility boundaries, including the exact calling
   context and return semantics.
5. Whether it actually has a stale cached action. If the native boss already
   retargets correctly, no replay adapter is needed.
6. Its native animation sequences and phase gates. Do not copy Dendron's intro,
   sleep, wake, death, burst, or platform hashes as generic boss behavior.

A safe port keeps these rules:

- Run the original native functions.
- Qualify the exact live boss before and after every relocatable lookup.
- Prefer selecting an already-native target slot over writing an entity pointer.
- Replay only an exact authored action, only when a proven stale-cache state is
  present, and with strict retry limits.
- Let native code decide aim, line-of-sight, range, cooldown, and firing.
- Treat zero health as progress evidence, not proof of death.
- Stop all repairs on generation change, retirement, encounter completion, or
  identity mismatch.

## Source map

- `src/state/activity/strike_bond/authority.h`
- `src/state/activity/strike_bond/catalog.h`
- `src/state/activity/strike_bond/controller.cpp`
- `src/client/hooks/bootflow/strike_bond_fire_trace.h`
- `src/client/hooks/bootflow/strike_bond_fire_trace.inl`
- `src/client/hooks/bootflow/strike_bond_target_binding.h`
- `src/client/hooks/bootflow/strike_bond_target_binding.inl`
- `src/client/hooks/bootflow/strike_bond_boss_cycle.h`
- `src/client/hooks/bootflow/strike_bond_boss_cycle.inl`
