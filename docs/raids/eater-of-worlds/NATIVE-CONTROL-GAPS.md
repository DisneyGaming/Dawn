# Eater of Worlds native-control gaps

Date: 13 September 2026. Target: Destiny 2 build 86657.

This note records boundaries found by offline package and executable inspection. It separates
usable native joins from fields whose gameplay meaning is not proved. None of the work below
called game code, changed process memory, or controlled a running game.

## Entity-ID allocation and refill

The client simulation manager owns an 8,192-bit available-ID mask at `+0xC118`. Generic object
creation reaches allocator `0x170F190`; scanner `0x1711D10` returns no ID when that mask is empty,
and the allocator immediately returns the invalid handle. Its low-cache branch only emits
diagnostics. It does not wait for or merge a lease grant.

Native maintenance `0x171DB20` reads the low/high pair at manager `+0xC518/+0xC51C` and calls
`0x4F88C0` when the available count is below the low watermark. `0x4F88C0` builds the native type-20
request and queues it through transport `0x4FB770`; it returns after enqueue. A delivered 8,192-bit
lease mask is merged later by `0x17129E0`. The entrance capture agrees with that ordering: a grant
arrived after the failed allocation and during cleanup.

Consequences:

- Calling maintenance from the empty-allocation path cannot rescue the allocation already in
  progress. No synchronous request/grant/merge path has been recovered.
- Directly fabricating mask bits or calling Sunrise's server prepare/commit helpers from the
  client would skip the native request, reply, revision, and lease-owner order. It is not valid.
- The implemented entrance fix only changes the existing maintenance profile while the exact
  pending `raid_envy_v310` bubble 2 / slice 16 / spawn `0x8BA80878` route is current. Native type-20
  request and `0x17129E0` merge still own every granted bit.
- The measured 400-ID startup target covers the observed 150-ID failure, but no successful trace
  yet proves the maximum demand of the complete entrance or later Eater bubbles.

## Player support and reactor occupancy: initial findings

The initial gaps below are preserved as the investigation record. The later
first-platform follow-up now supplies the exact physics/body join and production
contact reader; installed gameplay acceptance remains open.

The existing local-player physics path proves a full salted world-object handle at component
`+0x2C` and identifies native physics sync `0x4678B0`. The component's rigid-body array is at
`+0x190`, its signed body index at `+0x204`, and each array entry is `0x50` bytes with its body
pointer at `+0x20`. Body flags at `+0x4C`, motion type at `+0x160`, and velocity at `+0x230` are
used by the known sync path. Body position at `+0x1C0` remains explicitly marked as a hypothesis
in the existing teleport adapter.

Offline disassembly also finds a component list at `+0x1D0` with count `+0x1D8`. Consumer
`0x45A540` traverses it, uses `0x465FB0` to obtain body indices and `0x11CDE0` for a geometry
vector, then builds a body bitmask. No recovered field or branch identifies this list as the
player's current ground contact, identifies one entry as the supporting object, or yields a
supporting object's salted world handle and contact normal. The Vance contact observer is tied to
a mission NPC locomotion class and its hit row likewise has no proved supporting-actor handle.

The 56 reactor platforms have exact type-4 sources in registry `0x686321C8`, slots 38 through 93,
and exact type-34 `of_all_players` collection descriptors in slots 115 through 170. Type 34 uses
component `0x80809568` and authority `0x8080956A`; these descriptors do not contain a proved local
platform, player, or volume reference, and no type-34 sense producer is present. The exact generic
device output join can raise a platform through object-local `device_position`, but it does not
prove who stands on it.

Consequences:

- There is no supported ground-contact or platform-occupancy receipt to match the actual player
  handle to an acknowledged platform handle.
- A position-radius or Z-height test would require guessed geometry semantics. It is not native
  occupancy evidence.
- The type-4 placement is the authored source origin, while the generic device moves the platform;
  it is not a readback of the current top surface. The published player point is a body position,
  not a capsule footprint, grounded bit, contact normal, or supporting-object identity. A solo
  dwell rule built from those two points could accept a jump, fall, or nearby floor and therefore
  cannot be presented as standing on the platform without a newly authored approximation.
- Retaining one real player in every type-34 collection, publishing a fabricated player, or
  spoofing a player role would replace the encounter's membership predicate. These shortcuts are
  not valid solo adaptations.

### First-platform follow-up, 13 September

The user confirmed normal movement while standing on the first reactor platform.
The read-only capture saved 64 stable local body samples and both body/component
blocks in `build/coo/eater-first-platform-20260913`. The standing body point is
21.11 units above the authored platform source origin. This measures a concrete
difference; it does not establish a collision surface or a reusable height rule.

The runtime now reads the exact platform generic device's current position at
`+370`, target at `+37C`, and consumed revision at `+960`. It emits an applied
pose receipt only when the finite current value equals the target, and after
revalidating the type-4 source generation, committed state, weak entity, unique
component/configuration, self handle and owning run. Original consumer `DF6510`
uses a strictly greater position revision before calling `DF6C70`. Platform
movement revisions are now independent and monotonic across raises and retries.

RTTI identifies the captured body as `hkpRigidBody`. Original native `11DE80`
uses the body's collision-entry array at `+90`, count `+98`, stride 16; each
row's second pointer names the partner world object at `+20`. The first capture
did not preserve that array's target memory. Subsequent package inspection proves
platform entity `80F42FCD`, build ordinal 2, configuration `80F42FA5`, runtime
kind `80808A0C`, offset `378`. This is the same runtime physics layout as the
captured player's `80C0C518 / 80808A0C / 660`. The subsequent failed activation
capture proves that this primary body has no contact points while the player is
on the platform. The corrected producer retains that primary identity check but
selects the same entity's exact `80F42FB5 / 8080929E / 1B0` cylinder at `+88` as
the occupancy input. See [the volume proof](evidence/platform-volume-native-proof.json).

The exact contact manager class has vtable RVA `1BAC4C0`, body A/B at `+A0/+A8`,
atom pointer at `+68`, live contact count at atom `+4`, and 32-byte contact points
from atom `+30`. Original sphere collision producer `18FAE30` constructs the
normal as normalized `A.position - B.position` and passes the same bodies and
contact to manager `127870`, which copies both vectors unchanged. The offline
verifier executes those original arithmetic and contact-write instructions in
both body orders. See [the reproducible proof](evidence/platform-contact-native-proof.json).

The production reader accepts only an exact current source/entity/component/body
join, an applied platform pose, and a current authenticated player. It rechecks
the arrays, contact manager, atom, points and identities after copying. The solo
occupancy policy requires a nonempty stable manifold with finite unit normals in
any direction, followed by 500 ms of continuous occupancy. Recorded centre and
edge normals establish that their direction is not a grounded-support predicate.
This volume policy is an adaptation, not recovery of the original host's
multiplayer membership graph. Signed contact distance must be finite and no larger
than the live world's own collision tolerance. The manager and both bodies must
name the same exact `hkpWorld` (vtable `1BAAC28`); its collision input at `+B8`
must name the dispatcher pointer stored at world `+C8`. The tolerance comes from
input `+10`. Original constructor `107D50` writes this chain, and `109220` reads
it back into the world configuration. The offline sphere fixture also verifies
contacts inside and outside that native tolerance. No hardcoded gameplay distance
is substituted.

The native reader and all-four-path controller fixture are implemented. Actual
player/platform contact and installed traversal still need an in-game test.
`tools/coo/capture_eater_platform_live.py` can retain two complete read-only
snapshots, explicit contact managers/atoms, and terminal stability checks if that
test needs further diagnosis. Its proximity gate is a capture aid only.

## Progressive roster publication

The recovered Eater roster contains 891 client descriptors across 17 non-empty groups. Its normal
publication supplies two non-empty top-level groups plus bubble-owned group keys. Bubble 6 exposes
665 bubble-owned descriptors, or 688 with the 23 top-level descriptors. Two bubble-6 registry
groups alone contain 335 and 301 descriptors.

The roster lifetime machinery can preserve a registry key's ordinal only for a native profile
that opts into retained ordinals and publishes that key first as an explicit zero-presence entry.
A later zero-to-one presence transition can construct that same key without replacing its slot
identity. Eater has no such retained-ordinal profile today. Ordinary changes to the group set
advance the roster state sequence and destroy/recreate existing authored objects.

Even with retained ordinals, phase-one publication controls a whole registry group. It cannot
stage individual descriptors inside the 335- or 301-descriptor groups. Splitting those groups
would change the exact registry/type/slot identities used by type-24 target edges and other native
references. Construction dependencies between candidate groups have not been proved. The server
publisher also has no sequence-checked view of the current client manager's unused-ID mask;
server `heldEntitySlots` describes lease ownership, not current client availability.

Progressive publication therefore is not a safe capacity fix yet. A proved design needs an exact
client demand/readiness observation, native merge acknowledgement, dependency-safe group order,
and evidence that every indivisible group fits the available owned-ID budget. Until then, keep the
full recovered roster identity stable and do not use arbitrary descriptor counts as ID targets.

## Cranium carry receipts

Package inspection identifies exactly 120 type-4 cranium sources named `relics[*].o_relic[*]`:
60 in Barrier registry `0x91264981`, slots 116 through 175, and 60 in Argos registry
`0xE8D290A0`, slots 158 through 217. Their `(registry, slot, source)` identities are unique. Four
entity/configuration pairs occur 30 times each:

- entity `0x80F42EFE`, carry configuration `0x80F42EEB`;
- entity `0x80F42EDC`, carry configuration `0x80F42EA4`;
- entity `0x80F42F3A`, carry configuration `0x80F42F0B`;
- entity `0x80F42E8A`, carry configuration `0x80F42E86`.

Each source entity has exactly one matching class-`0x80809C36` resource. The resource is `0xCE0`
bytes, has subtype `0x80803E70` at `+0x7C`, and declares the exact runtime header
`{configuration, 0x80804221, 0x598}` at `+0x80`. This is the same generic Carry runtime ABI used
by the captured Omega charge. Native interface `0x80803F6A` and resolver `0x557470` locate it from
the live source entity. A usable component must still match the exact configuration, runtime kind,
and offset; its self handle at `+0x24` must resolve back to the component, and its entity backlink
at `+0x2C` must equal the current source entity.

Hook `0xD99620` supplies the native Carry transition. An Eater observer can call the original,
then accept only a component already bound to a current, active, committed type-4 source with the
same run, source generation, weak entity, entity serial, component self handle, and entity
backlink. Carry state is stored at `+0x470`. State 3 may use native world-attachment resolver
`0x597B10`, followed by a full live-entity check and equality with the local controlled entity
returned by `0x4B2260`.

The captured generic Carry ABI also gives a strict state-1 inventory-owner join. It requires
`+0x47C == 0x80804057`, signed `+0x480 == 0x4C8`, and `+0x494 == 0x80803E63`. The owner handle at
`+0x490` and bounded offset at `+0x498` must resolve to a component whose header is
`{configuration from +0x478, 0x80803E64, 0x468}`; that component's self handle must resolve back
to it and its entity backlink must be a live entity equal to `0x4B2260`'s local controlled entity.
If the callback holder context can be resolved by the existing bounded native ancestor walk, it
must name the same player. Do not replace a missing holder proof with the local player merely
because the carry state is 1.

A drop receipt is valid only after a previously accepted held receipt for the same current
run/source/generation/entity/component makes an original-backed transition out of states 1 and 3.
Source retirement, generation change, run change, or reset invalidates the held receipt. This join
proves native pickup and drop ownership. It does not prove elemental charge, ammunition, cooking,
oracle state, or shield progress. Station use has a separate exact controller and consumed-request
join below; it still does not supply any of those missing cranium-state meanings.

## Fire-station interactions

The fire stations do have an exact generic interaction-component join. Registry `0xE8D290A0`
contains nine type-4 `fires[n].o_interactable` sources in odd slots 141 through 157; registry
`0x91264981` contains the corresponding nine sources in odd slots 177 through 193. Each entity has
exactly one class-`0x80809C36`, `0x750`-byte interaction configuration. Declaration
`0x80804FB0` is at configuration `+0x8C`, followed at `+0x90` by runtime header
`{configuration, 0x80804FB2, 0x388}`. The nine entity/configuration pairs are:

- `0x80F42F4F` / `0x80F42F4E`, `0x80F42F52` / `0x80F42F51`, and
  `0x80F42F55` / `0x80F42F54`;
- `0x80F42F58` / `0x80F42F57`, `0x80F42F5B` / `0x80F42F5A`, and
  `0x80F42F5E` / `0x80F42F5D`;
- `0x80F42F61` / `0x80F42F60`, `0x80F42F64` / `0x80F42F63`, and
  `0x80F42F67` / `0x80F42F66`.

The existing source, weak-entity, bundle, component-self, and entity-backlink checks can therefore
bind these controllers without a broad interaction match. Hook `0xF32CD0` observes the original
prompt output only. A completed station-use receipt needs the existing `0xF36640` post-original
proof on the same current controller: requester `0x352310` must resolve to the authenticated local
cranium holder, requested count `+0x2DC` must be greater than nonnegative consumed count `+0x2D8`,
the original must advance consumed count exactly to requested count, leave the requested count and
eight-byte requester association unchanged, and leave active byte `+0x2D0` set. The pre-call
controller must be unlocked at `+0x2C0`; its active byte may already be zero or one because the
package evidence does not establish whether these stations are one-shot or repeatable.

The station entity also proves its native dynamic-authority receiver. In all nine distinct entity
definitions, the class-`0x80809C22` interface table has exactly one row
`{0x80804FB0, exact configuration, 0x80809C50, 0x648, 0x80C23296}`. Link `0x80C23296` is a
class-`0x80809C54` implementation of interface `0x80809AE3` with the two `0x80804FB0` method rows
at slots 4 and 5 used by the pinned native `0xF37800`/`0xF33930` consumers. The signed relative
value `-0x2C0` at each configuration's `+0x648` resolves exactly to its `+0x388` interaction body.
The separate inherited row at `+0x5F8` links `0x80C707B5` and subtype `0x80804FBB` slots 16/17;
it is explicitly excluded. This complete join allows the existing `0x80804FB8` enabled command to
be published only for a requested, active, acknowledged station source while preserving native
prompt predicates and use counters.

For each `n`, the interaction source is the exact named sibling of the preceding even-slot
`fires[n].o_fire`. The corresponding `fires[n].ch_fire` type-24 descriptor targets that fire
object: Argos channel slots are 249 and 251 through 265; Barrier channel slots are 278 and 280
through 294. No direct pointer from the interaction configuration to the fire object was found,
so an adapter must pin this lane join to the recovered registry and `fires[n]` names. It may treat
an original-consumed request while the same player holds an exact cranium as a station-use
receipt. It must not infer elemental type, charge amount, cooking duration, or a type-24 channel
value, and it must not write `ch_fire`.

The completed-use adapter captures exactly one current cranium whose native Carry component still
authenticates the real local player before `0xF36640`. The final receipt names that same cranium,
and the controller requires it to equal the retained carry owner. A native drop nested inside the
use is held in a per-thread, nested-scope queue only until the station receipt is offered. The drop
is then revalidated and its current Carry mode is reread; a nested re-pickup cancels it. A scope
from another thread, an old reset epoch, or a duplicate scope cannot drain the queue. Station use
does not clear carry state and cannot invent a drop.

## Remaining native control boundary

The existing `coo/native_atom_authority.h` writer supports generic type-24 component
`0x80804F3B` / authority `0x80804F40`. It emits up to four ordinal rows containing a
biased signed revision, value and blend. Native adopter `0x1069900` copies the
`0x34`-byte decoded block. Consumer `0x106A780` reads count at component `+0x180` and
rows at `+0x184` with stride 12; it compares revisions through `0x5EC580`, then calls
`0x5F2DF0(target, rowIndex, revision, value, blend)`. Initialization `0x1069160` gets
target-specific channel count, identity and defaults from `0x5EC5A0`, `0x5EC5B0` and
`0x5EC630`. The wire rows contain no channel-name hash.

Strike Pact's existing one-row, off=0/full=100 laser contract proves only its own
targets. The Eater exports prove 142 channel edges and 18 fire targets but do not
prove the row count, ordinal meaning, values, blend behavior or revision transition
for each of the three fire entity classes. There is also no current test that executes
the original type-24 decoder and consumer together. A type-23 pose test does not
establish those semantics. The next usable proof is a captured Eater authority
transition or equivalent package/behavior join, followed by an original-code fixture
using `0x1069900` and `0x106A780` against the exact fire targets. Station-use completion
does not supply that proof or the elemental/cooking mapping.

Package inspection proves platform output wiring and exact type-24 target references, but does not
prove writable values for cranium charge, oracle completion, shield convergence, or Argos
weak-point state. Health observations must additionally bind a live acknowledged object instance,
its exact build-verified health component, and the same generation/entity/serial for the full
live-to-dead edge. A zero value, object disappearance, or detached callback is not by itself a
native death receipt.
