# Lighthouse Vex-wall portal effect — working fix and handoff

**5 September 2026 · Mission Scot · User-confirmed working**

The missing portal effect was blocked by player state. The wall's native controller was receiving activation, but its authored filter required hash `52B968BA` in the contacting player's participation record. Dawn always encoded that list as empty. Seeding the required hash into the initial keyed player record allowed the native filter, activation queue, and effect controller to run.

The user confirmed: **“OK IT WORKED.”** The successful log independently records the complete native activation chain in **two mission runs**. This is now the working effects baseline. Panoptes cinematic work was paused while this issue was addressed.

## 1. The original symptom and why the first attempt failed

Walking into the Vex wall caused an abrupt forced teleport with no observed warp effect. The visual reference supplied by the user was from retail footage; it was not evidence that our build had already played the effect.

We first activated the `lighthouse_teleport` carrier and added a 1,200 ms lead-in to the existing forced hop. The user still saw no effect. Delaying a position change did not satisfy the native controller's activation requirements.

We then separated the investigation into four questions:

1. Does the carrier receive active authority?
2. Does it create the expected effect controller?
3. Does wall contact reach the native activation callback?
4. Does that callback accept the actor and queue the effect?

The first three succeeded. The fourth was the blocker.

## 2. The authored object and effect chain

The relevant carrier is `BA5F26EF / type 4 / index 0`, named `lighthouse_teleport`. Its definition is `80F47B52`; the runtime definition reference is `80F47B52 / 80809928 / 4C8`.

Its authored placement is approximately **(366.69455, 249.99928, 100.59293)**. The diagnostic wall window was an approximation around the extracted collision shape. It should not be confused with a directly observed collision manifold.

The direct asset references are:

```text
BA5F26EF / 4 / 0
  -> carrier definition 80F47B52
  -> entity 80F4AE39
  -> attached controller resource 8156EEC5
  -> controller definition 8156EEC5 / 80803D2D / 168
  -> child effect entity 80C6600C
  -> child resource 80FEC666
       -> render resource 80FEB6F1
       -> sound resource 80C2B106
            -> Wwise bank 80C2B105
            -> event AC668354 / media 81001499
```

These references were recovered from installed assets. The reference chain alone did not prove playback; the later native receipts and the user's successful test established the working path.

[Direct asset-reference evidence](</C:/Destiny 2 Development/build/scot-portal-presentation-20260905/authored-contact-evidence.json>)

## 3. How we found the rejecting condition

Read-only native hooks first established that active carrier authority created controller `8156EEC5`, and that its activation callback ran at wall entry. Its activation count and level stayed at zero.

Disassembly showed that accepting a new queue entry increments the count synchronously. Therefore, the zero count could not be explained merely by a delayed animation.

The controller has two non-inverted conditions joined by AND:

- `80804D72`, payload `00001000`: an entity-type mask check. This passed in the failing runs.
- `80804D83`, payload `52B968BA`: a hash-membership check against the actor-associated player record. This failed.

Further probes showed that the actor record resolved and its list was present and readable, but **the list contained zero entries**. There was no activation-queue call after that rejection.

[Failing run: record resolves, list empty, filter rejects](</C:/Destiny 2 Development/build/scot-portal-condition-20260905/dawn-test-20260905-045734.log:11014>)

All native addresses below are RVAs in the pinned `destiny2_unpacked.bin`, whose image base is `0x7FF618070000`:

```text
9F19F0  carrier authority application
DE7280  effect-controller creation
DE16D0  activation callback
  -> DE17D0  direct activation branch for this asset
     -> DE2010  actor filter
        -> A5AA00 / A5AB20 / A5AC00  authored predicate evaluation
           -> 100BE00 -> 1087CD0  handler for 80804D83
              -> 4AF530  resolve actor-associated record
              -> A560C0 -> A56500  resolve the record's hash list
              -> A567D0  search for the required hash
     -> DE1600  activation queue, reached only after filter success
        -> DE1BC0  increment count at +C4 and set level at +C8 to 1.0
```

The predicate-handler registration was traced through `100B920` and `A59BE0`. Descriptor slot `2078140` points to class `80804D83`; handler vtable `1C37420` has evaluator `100BE00` at +8. This established the handler from registration data rather than from a guessed name.

## 4. The actual missing data

The native reflected schemas connect the player-participation packet to the list read by the effect:

```text
Type 13 participation authority: 80804F30
  +48 -> 808094DD
           +0C -> 808094E1
                    six-bit count
                    counted array of unsigned 32-bit hashes
                    capacity: 32
```

The corresponding participation sense schema `80804F2F` contains the same member. The observed runtime list binding used schema `808094DD` and offset `0x238` (decimal 568). Runtime handles varied between runs and must not be hardcoded.

In `write_participation`, Dawn wrote the list count as literal zero. The host therefore never supplied `52B968BA` to this field. The working change encodes count **1**, followed by the exact required 32-bit hash.

The count field already existed. Adding one hash increases the participation body by **32 bits**:

- No region: 192 bits before, 224 bits with the hash.
- With the optional region: 224 bits before, 256 bits with the hash.
- Object-block framing adds its own bits; these sizes refer only to the participation body.

The following array, arrival hold, respawn-related fields, signed scalar, and subsequent object framing must retain their alignment.

## 5. The code change that worked

The repair changes five files:

- [omega_portal_entry.h](</C:/Destiny 2 Development/Dawn/src/state/activity/omega/omega_portal_entry.h:12>) — defines the required hash from the authored predicate.
- [sensor_auth_update.h](</C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/sensor_auth_update.h:162>) — adds the independent initial-player-state flag.
- [activity_roster_snapshot.cpp](</C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp:550>) — sets that flag for synthetic Mission Scot before native ownership.
- [activity_sensor_auth_bodies.cpp](</C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp:171>) — writes count plus hash and accounts for the additional body bits.
- [omega_progression_tests.cpp](</C:/Destiny 2 Development/Dawn/unit/omega_progression_tests.cpp:617>) — covers the new list and full-packet ownership behavior.

The key logic is:

```cpp
// Exact value required by the authored contact predicate.
inline constexpr std::uint32_t kRequiredPlayerHash = 0x52B968BAU;

// Seed it only for the existing synthetic Mission Scot path.
snapshot.omegaPortalPlayerHash = syntheticOmega && !omegaQuiesced;

// Inside the keyed participation body, at the existing hash-list count.
encoded = encoded && writer.write(snapshot.omegaPortalPlayerHash ? 1U : 0U, 6);
if (encoded && snapshot.omegaPortalPlayerHash)
    encoded = writer.write(state::activity::omega::portal_entry::kRequiredPlayerHash, 32);
```

`auth_body_bits` also adds 32 bits when the flag is enabled. Unkeyed participation slots still receive no participation body.

**Seed this during initial player participation. Do not add participation resends after native mission ownership.** The existing encoder suppresses those later updates because applying a neutral player record can overwrite live mission inputs and reset progression. The new player-hash flag is deliberately independent of `omegaPortalEntry`, which becomes active later after the opening event.

The fix supplies player data. It does not force the predicate to return true, directly edit the runtime list, call effect activation manually, or replace the asset's effects with a custom visual/audio implementation. The earlier carrier-activation implementation remains part of the baseline.

[Exact successful change](</C:/Destiny 2 Development/build/scot-portal-list-20260905/changes.patch>)

## 6. Proof from the successful test

The successful game session loaded DLL SHA-256:

```text
7C058C3C5290D72785D3A47DF6E6CD242E5999D7391B90DE2008AB2BD61FA6A0
```

At **106046 ms in run 1**, the log records:

- A readable hash list with count **1**.
- An entry equal to `52B968BA`.
- Both authored predicates returning **1**.
- The aggregate actor filter returning **1**.
- The native activation queue growing from **0 to 1**.
- The controller changing from count **0**, level **0.000** to count **1**, level **1.000**.

At **192875 ms in run 2**, the same sequence succeeds again with different runtime handles. This demonstrates that the fix survives a new mission run rather than depending on a stale handle.

[Run 1 successful activation](</C:/Destiny 2 Development/build/scot-portal-list-20260905/dawn-success-20260905-050938.log:10343>)

[Run 2 successful activation](</C:/Destiny 2 Development/build/scot-portal-list-20260905/dawn-success-20260905-050938.log:12375>)

The archived successful session contains **no** `ev=teleport stage=hop` or `omega_gate stage=hop` receipts. It does record native `normal_z_leg` transitions toward `PRV88.88` immediately after activation in both runs. That is a useful distinction from the earlier failed tests, where the forced-hop receipts were present. It does not independently prove every intermediate spawn position or the complete intended bubble-15-to-Forest route.

The user's confirmation supplies the observed effect result; the telemetry independently establishes the native data and activation path.

[Machine-readable live validation and archived-log hash](</C:/Destiny 2 Development/build/scot-portal-list-20260905/LIVE-VALIDATION.json>)

## 7. Build, tests, and exact working baseline

Working frozen candidate:

[Frozen candidate manifest](</C:/Destiny 2 Development/build/scot-portal-list-20260905/candidate-20260905-050219/candidate-manifest.json>)

- Build ID: `ADEA1A2577021C4276216B71575577E6D90352AFA9C6D0F12D751AF5D19DAF8D`.
- Source SHA-256: `1F93CF19342EE857C5186083E79A9C8B75711C0BD5EF0A067AE4C7A1DE88A37B`.
- Release build: zero warnings and zero errors.
- Ten regression runs passed: Debug and Release for progression, forest recipe, forest roster, experiment settings, and native hook bundle.
- Packet tests verify list count/value, optional region, player identity, later field alignment, a following-record sentinel, short-buffer rejection, and omission for unkeyed slots.
- Full-packet tests verify initial participation publication and continued suppression after native ownership, with the carrier both inactive and active.
- 27 native signatures and the reflected schema/width assertions passed.
- The isolated hook harness tested 31 hooks; no game code was executed by that harness.
- 1,262 other baseline source files were unchanged.
- Installation backed up the previous DLL/settings/cache, checked that Destiny was closed, and verified the installed DLL hash. Settings were preserved. The user launched the game manually.

[Build verification](</C:/Destiny 2 Development/build/scot-portal-list-20260905/verification.json>) · [Installed DLL receipt](</C:/Destiny 2 Development/build/scot-portal-list-20260905/installed.json>) · [Release build log](</C:/Destiny 2 Development/build/scot-portal-list-20260905/candidate-20260905-050219/evidence/release-build.log>) · [Detailed research notes](</C:/Destiny 2 Development/build/scot-portal-list-20260905/RESEARCH.md>)

The original `verification.json` and candidate manifest describe the pre-gameplay validation stage and may still say playback was pending. `LIVE-VALIDATION.json` and this handoff record the later successful test; the earlier build evidence was retained rather than silently rewritten.

## 8. What to preserve and what remains separate

Preserve the initial player hash, the exact counted-array encoding, the keyed-record scope, the current carrier activation, and suppression of later participation resends. Keep the diagnostic receipts until any cleanup has its own successful regression run.

Two earlier observation mistakes should not be repeated:

- `80F47B52`'s template offset +70 is not its runtime definition offset. The live reference uses +4C8.
- The type-4 pool resolver returned an authority payload, not a component header. Its leading generation/index/active words were once mislabeled as source identity. Direct callback hooks provided the reliable component references; the pool output is now labeled `authority_payload`.

The text name behind `52B968BA` and its exact retail assignment/removal lifecycle remain unknown. This working implementation seeds it in initial Mission Scot player state. The user's requested intermediate spawn and migration route, any removal of the old forced-hop fallback, and the Panoptes spawn cinematic remain separate work. This document records the successful portal-effect fix, not completion of those tasks.

Manual reproduction: launch the existing CMD, enter Mission Scot, let the opening occur, and walk into the wall normally. Confirm the effect in game and check for count 1, hash match, filter success, queue insertion, and controller activation in the log.

[Manual launcher](</C:/Destiny 2 Development/launch-scot-reveal-debug.cmd>)
