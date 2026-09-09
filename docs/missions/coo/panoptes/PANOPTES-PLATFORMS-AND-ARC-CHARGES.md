# How we restored the Panoptes platforms and Arc-charge dunks

## Short version

The platforms and Arc-charge mechanic are one chained encounter system, but each step needs a different native receipt:

```text
rescue scene completes
        |
        v
host publishes the authored route devices
        |
        v
native bridge gates move from position 0 to 1
        |
        v
player enters the real charge-platform polygon
        |
        v
native inventory state proves that the player picked up the charge
        |
        v
native sink interaction consumes that same charge
        |
        v
host marks the dunk complete and stages the authored return transport
        |
        v
the same player reaches the real eye-platform polygon
        |
        v
Panoptes's shield/eye phase may begin
```

We did not spawn substitute platforms, grant a fake inventory item, infer a dunk from proximity, or teleport the player on a timer. We restored the original device authority and only advance the mission after the corresponding native object reports a qualified result.

## The important separation

“The platform is active” is not the same event as “the player arrived.” Likewise, “the charge is near the player” is not a pickup, and “the player is near the sink” is not a dunk.

The implementation keeps four boundaries separate:

- **Device authority** materializes the bridge/runway geometry and makes the charge and sink available.
- **Polygon arrival** proves the player physically reached an authored platform volume.
- **Inventory ownership** proves the charge is attached to the player through the native item and inventory components.
- **Sink consumption** proves the native deposit request completed and consumed that exact held charge.

That separation prevents the encounter from progressing on transmitted requests, timers, approximate distances, or stale objects.

## 1. Restoring the authored platform devices

The complete route catalog is in [`omega_transit_catalog.h`](Sunrise/src/state/activity/omega/omega_transit_catalog.h). It contains 40 authored sources across all three damage cycles:

- ring cores and ring effects;
- seven bridge/runway/stair/railing sources and their gates;
- one post-dunk transport per cycle;
- charge items, deposit sinks, and endpoint effects;
- final-route and destination devices.

The Arc-specific source slots are:

- Cycle 1, registry `0040BF06`: charge slot `18`, endpoint slot `19`, sink slot `20`, and bridge slots `24` and `25`.
- Cycle 2, registry `0040BF05`: charge slot `1`, endpoint slot `2`, sink slot `3`, and bridge slots `24` and `25`.
- Cycle 3, registry `0040BF03`: charge slot `0`, endpoint slot `1`, sink slot `2`, and bridge slots `20`, `21`, and `22`.

All seven bridge entities contain the original native platform device `80C22861`. The bridge source supplies the type-4 state and its paired type-23 gate supplies the three native channels: position, power, and lock.

### Platform state policy

[`omega_transit_authority.h`](Sunrise/src/state/activity/omega/omega_transit_authority.h) derives platform state from the mission snapshot:

- Before the rescue/route handoff, the current cycle's bridges are dormant.
- When the route begins, `chargeEnabled=true` raises the current bridges.
- Pickup changes the mission from `route` to `carrying`, but the bridges remain active.
- Dropping the charge returns to `route`; the bridges still remain active.
- A successful dunk clears `chargeEnabled` but retains `chargePickedUp`, so the bridge geometry stays materialized during the return trip.
- Moving to the next cycle retires the previous cycle's geometry.
- The separate contact transport stays dormant during the charge route and carrying state. It becomes active only after an accepted dunk and retires only after the receiving eye-platform arrival.

For a bridge gate, position `0.0` means dormant and position `1.0` means materialized. Power remains `1.0`, lock remains untouched, and Destiny's native device owns the smooth interpolation.

### Correct native direction

A read-only live capture found a missing platform whose native current and target position were both zero. The original device links showed:

- increasing position uses `80BFD0FD`, the `phase_in` graph;
- decreasing position uses `80BFD0FE`, the `phase_out` graph.

The isolated platform fixture executes Destiny's original `DF1F70` channel routine and confirms that changing the target from `0` to `1` starts `phase_in` and reaches `1`, while changing `1` to `0` starts `phase_out` and returns to `0`. This corrected an earlier reversed interpretation.

### Getting authority into the live native sources

Publishing the right activity packet was not enough. Earlier live captures showed decoded active authority on the sync objects while the actual platform sources still held inactive state. The missing step was native adoption.

[`omega_cannon_delivery.h`](Sunrise/src/client/hooks/bootflow/omega_cannon_delivery.h) and [`omega_cannon_delivery.inl`](Sunrise/src/client/hooks/bootflow/omega_cannon_delivery.inl) extend the already-proven cannon delivery lane to the complete transit catalog:

1. Discovery starts from Panoptes's bound mission member and scans at most 128 native datum rows per 100 ms poll.
2. Each source is accepted only if its asset, runtime class, definition offset, metadata, salted member handle, and component offset all match.
3. The current decoded authority is resolved through native `9FEC30`.
4. The body must match the exact registry, type, slot, schema, bubble `14`, applied state, generation, and expected mission snapshot.
5. Type-4 device state is applied through the original `9F19F0` path. Type-23 gate channels are applied through the original `10699C0` path.
6. The source and authority are re-read before the native call and the adopted bytes are verified afterward.

No sync dirty flags are edited, adopted authority is not replayed, and native pointers are never retained across polls. Only salted member references and offsets are cached.

## 2. Detecting the real platform arrivals

[`omega_reveal_native.cpp`](Sunrise/src/client/hooks/bootflow/omega_reveal_native.cpp) contains the authored arrival volumes. The route-specific volumes are:

- Cycle 1 charge platform: asset `80F47639`, registry `0040BF06`, slot `69`.
- Cycle 2 charge platform: asset `80F47712`, registry `0040BF05`, slot `80`.
- Cycle 1 eye return: asset `80F47639`, registry `0040BF06`, slot `81`.
- Cycle 2 eye return: asset `80F47712`, registry `0040BF05`, slot `76`.
- Cycle 3 eye return: asset `80F47845`, registry `0040BF03`, slot `73`.

The code verifies each asset's registry, type, slot, vertex count, triangle count, relative arrays, and triangle indices. It then passes the player's real position and the original authored shape to Destiny's native point-in-polygon function at `4A55A0`.

A charge-platform receipt is accepted only while the matching cycle is in `route` or `carrying`. An eye-platform receipt is accepted only after a dunk, while the state is `shield`, and only for the same player who held and deposited the charge.

The arrival polygons do not activate a platform and platform activation does not forge an arrival.

## 3. Recognizing the native Arc-charge pickup

The implementation is in [`omega_mission_arc.inl`](Sunrise/src/client/hooks/bootflow/omega_mission_arc.inl).

### Joining the charge and sink

Once their type-4 authority has been adopted, `observe_mission_arc_source` records the current cycle's exact charge and sink bindings. Every later use revalidates the full source identity, generation, active byte, weak entity reference, and owning member.

An early failure came from confusing component declaration IDs with registered lookup interfaces. Runtime descriptor captures established the correct interfaces:

- The shared charge item uses registered interface `80803F6A`, which resolves component `80F66667 / 80804221 / +598`.
- The three sink entities use registered interface `80809658`, which resolves controllers `80F6666E`, `80F66671`, and `80F66673` of class `80804FB2` at `+388`.
- `80803E70` and `80804FB0` are declaration IDs returned by a successful lookup, not valid query-interface IDs.

The production join uses Destiny's original component lookup at `557470` with the corrected registered interfaces.

### Proving that the player holds the item

The existing native carry transition at `D99620` is observed after its original function runs. A bounded 100 ms reconciliation poll also re-reads committed state in case the callback happened before source discovery; the poll never calls the native transition itself.

The item must be the exact current charge entity and its component must be `80F66667 / 80804221 / +598`. The live capture showed that a genuinely held charge can use carry state `1`, not only state `3`.

For state `1`, the code requires all of the following:

- the item reports its real world attachment through native `597B10`;
- its typed inventory-owner reference is present;
- that owner resolves to an `80803E64 / +468` inventory component;
- the inventory component belongs to the same player entity;
- the item's world attachment and typed inventory owner agree;
- the complete player and component handles still match their live pools.

Only then is a `ChargeReceipt` committed. It contains the mission token and epoch, mission generation, charge source member, item entity, player entity, sink component, registry, charge slot, and sink slot. The mission moves from `route` to `carrying`.

If the native item later leaves the carried states without a successful use in progress, the exact receipt is dropped and the mission returns to `route`.

## 4. Unlocking the native deposit interaction

The sink controller defaults to locked when its interaction setup is absent. Merely activating the sink source therefore produced a visible charge with no deposit prompt.

We added the sink's original dynamic interaction override to the type-4 authority record. The override uses schema `80804FB8`:

- dormant or retired sink: native mode `1`, locked;
- active charge route or carrying state: native mode `2`, unlocked.

Destiny's original decode and application path (`9FA4B0`, `9F9B30`, and `F33930`) applies the override and invalidates the prompt-eligibility cache. It does not submit a use, alter the requested/consumed counters, or invent a player association.

Native predicate evidence also established that the carried charge exposes the authored property `9C99BE55` through provider `80F7A6E0`, and that the original sink readiness check at `F30540` accepts an unused sink but rejects pending or completed one-shot requests.

## 5. Accepting a real dunk

The existing native sink-use callback at `F36640` is wrapped, but the original function always runs first.

Before that call, the wrapper requires:

- the exact held `ChargeReceipt` for the current mission token;
- phase `carrying`;
- the exact current cycle's sink component and active source;
- the same interacting player as the held charge;
- a valid outstanding native request, with requested count ahead of consumed count.

After the original call, it accepts the dunk only if:

- the sink and source identities are unchanged;
- the native used byte is set;
- the consumed counter now equals the prior requested counter;
- the interacting player association is unchanged and still resolves to the charge holder;
- the mission token, generation, registry, charge slot, sink slot, item, and player still match the retained receipt.

If a carry/drop callback fires inside the native use operation, the drop is deferred. A successful consumption wins and produces one dunk receipt; a failed consumption produces the deferred drop instead.

### The player-record correction

The first live native dunk proved that the prompt, user input, and charge consumption worked, but it exposed one final identity mismatch. The weak value stored by the sink at `F33A90` is a **player-record handle**, while the held receipt contains the player's controlled **world-entity handle**.

The final code resolves the player record through the native player pool, validates the full record handle at `+0x44`, reads the controlled entity at `+0x54`, and then validates that entity in the world pool. Comparing those two different handle domains directly was the reason the host could miss an otherwise genuine native dunk.

## 6. What an accepted dunk changes

[`omega_mission_state.h`](Sunrise/src/state/activity/omega/omega_mission_state.h) accepts `State::dunk` only for the exact currently held receipt. It then:

- clears the held state;
- disables the charge and sink route;
- marks `chargeDunked=true`;
- emits the authored rescue-scene dunk event;
- stops the obsolete carrying scene where applicable;
- advances the mission epoch and schedules the `shield` phase.

That state change retires the charge/ring devices, keeps the bridge geometry present, and activates the separate authored post-dunk transport. When the same player reaches the eye-platform polygon, `eyePlatform=true` retires that transport and unlocks the shield command. Only then can Panoptes expose the eye and enter the damage phase.

At the end of a damage/recovery cycle, `reset_cycle()` clears `chargeEnabled`, `chargeDunked`, `chargePlatform`, `eyePlatform`, and `chargePickedUp` before the next cycle begins.

## Runtime evidence

The live log in [`Sunrise/logs/sunrise.log`](Sunrise/logs/sunrise.log) contains the staged route working through pickup:

```text
stage=transit_delivery ... registry=0040BF06 ... slot=24 ... adopted=1
stage=transit_delivery ... registry=0040BF06 ... slot=26 ... adopted=1
stage=arc_source ... cycle=1 ... role=0 generation=3
stage=arc_source ... cycle=1 ... role=1 generation=3
stage=arrival ... milestone=5 ... receipt=native_polygon
stage=arc_pickup ... cycle=1 ... state=1 receipt=native_carried
```

Those records appear around lines 21578-22796. They prove native adoption of the bridge source/gate, charge and sink discovery, authored charge-platform arrival, and the held-inventory join.

The user separately confirmed platform materialization and later completed a native dunk under build `E10D118E`. The final player-record correction is installed in build `2D0DB518`. Its manifest is [`candidate-20260905-230930/candidate-manifest.json`](build/omega-full-20260905/candidate-20260905-230930/candidate-manifest.json).

## Verification

The final candidate records these focused regression results in both Debug and Release:

- `omega_ending_tests`: 8,156 ending/transit checks.
- `omega_mission_rescue_tests`: 6,439 production rescue and Arc checks.
- `omega_mission_state_tests`: 18,632 state-machine checks.
- `omega_boss_graph_runtime_tests`: 6,143 full runtime checks.

The tests include original native instruction execution and captured runtime layouts:

- [`omega_platform_native_fixture.inl`](Sunrise/unit/omega_platform_native_fixture.inl) runs the original platform channel routine and verifies all seven bridge gates.
- [`omega_arc_unlock_native_fixture.inl`](Sunrise/unit/omega_arc_unlock_native_fixture.inl) runs the original interaction-override decoder and sink lock handler.
- [`omega_arc_eligibility_native_fixture.inl`](Sunrise/unit/omega_arc_eligibility_native_fixture.inl) runs the original charge-property getter and sink readiness check.
- [`omega_mission_rescue_tests.cpp`](Sunrise/unit/omega_mission_rescue_tests.cpp) exercises source joins, captured held inventory, drops during use, exact native consumption, player-record translation, all three cycles, platform lifecycles, wire encoding, and native authority adoption.
- [`omega_mission_state_tests.cpp`](Sunrise/unit/omega_mission_state_tests.cpp) verifies that proximity, another player, a stale epoch, the wrong charge, and an unconsumed request cannot advance the shield phase.

Supporting captures and explanations are under [`Sunrise/unit/fixtures/omega_arc`](Sunrise/unit/fixtures/omega_arc) and [`Sunrise/unit/fixtures/omega_platform`](Sunrise/unit/fixtures/omega_platform).

## Current acceptance boundary

The platform materialization, charge pickup, deposit prompt, user-driven native dunk, and charge consumption have live evidence. The final player-record translation and post-dunk state/transport sequence are covered by the frozen native and state-machine tests.

The final installed build still needs one complete live confirmation that the authored post-dunk transport automatically carries the player to the receiving eye platform and that the resulting `eyePlatform` receipt opens Panoptes's damage phase. Activation of a transport source is not itself proof of physical contact, arrival, or eye damage.
