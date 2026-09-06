# How we turned on Panoptes's red eye

## Short version

We turned on Panoptes's existing eye-glow control instead of creating a new red effect.

The original Panoptes animation component (`80F6690A`) owns a float output named `CE0BA42D`. That output feeds the boss controller (`80F6695E`), which filters it into `20AA7FC1`. The resulting value is already consumed by Panoptes's eye material, eye-area particles, and lights. We safely changed `CE0BA42D` from `0.0` to `1.0` through Destiny 2's original native setter (`A0FE60`) immediately before the intro graph was queued.

The effective signal path is:

```text
Panoptes animation component 80F6690A
    CE0BA42D = 1.0
            |
            v
boss controller 80F6695E, input 0
            |
            v
filtered/clamped output 20AA7FC1
            |
            +--> eye material 80F45358
            +--> eye-area particle 80F66915
            +--> eye lights 80F451B9 / C3 / C4 / C5 / C6
```

The assets supply the red appearance. Our code only enables the authored glow signal.

## What we first ruled out

Panoptes has a separate illumination input, `F4ECD569`, which is toggled by the summon sequence. That value is transient and is not the persistent red-eye control. His body health, eye health, and alive values also stayed valid during the intro, so the missing eye was not fixed by changing health or encounter state.

Asset tracing established that:

- `CE0BA42D` is an output of animation component `80F6690A`.
- The controller maps that output to input index `0`.
- The controller computes `20AA7FC1` at index `32` from it.
- `20AA7FC1` is referenced by the eye material, the eye-area particle system, and the eye lights.
- The recovered expression is effectively a filtered/clamped version of `CE0BA42D` in the range `0..1`.

That gave us one upstream switch capable of activating all three authored visual consumers together.

## The native write path

The animation component contains 47 scalar providers. `CE0BA42D` is provider ordinal 40. Destiny's original function at RVA `A0FE60` has this effective ABI:

```cpp
void set_animation_scalar(
    std::byte* animationComponent,
    const std::uint32_t* scalarName,
    float value);
```

The native function binary-searches the component's 47 sorted definition rows, selects the matching runtime provider, and passes the update to `A10180`. That second native function writes the new value and calls the original dirty-notification path at `5906A0` so all bound consumers see the change.

This notification is why we used the native function instead of writing directly to provider memory. A raw memory write could make the number look correct while leaving the material, particles, or lights unaware of the update.

## The implementation

The implementation lives in:

- [`omega_boss_vfx_start.h`](Sunrise/src/client/hooks/bootflow/omega_boss_vfx_start.h), which identifies the scalar and validates the exact source layout.
- [`omega_boss_vfx_start.inl`](Sunrise/src/client/hooks/bootflow/omega_boss_vfx_start.inl), which performs the guarded initialization and readback.
- [`omega_boss_animation_glow.inl`](Sunrise/src/client/hooks/bootflow/omega_boss_animation_glow.inl), which reads the real native provider without changing it.
- [`omega_boss_graph_runtime.inl`](Sunrise/src/client/hooks/bootflow/omega_boss_graph_runtime.inl), which calls the initializer immediately before queuing Panoptes's intro graph.

At the start of a new Panoptes intro, `prepare_intro_vfx(owner, call)` does the following:

1. Confirms the hook still accepts side effects.
2. Confirms the current mission-run generation matches the captured owner.
3. Resolves the same Panoptes actor, character, entity, biped, and animation parent again.
4. Requires the authored activity member to be enabled and its native command queue to be empty.
5. Validates the full original `80F6690A` source layout: exact asset identity and size, 47 sorted provider definitions, provider offsets, and `CE0BA42D` at ordinal 40.
6. Reads the current value through the verified provider path.
7. If the value is `0.0`, reserves the one allowed attempt and calls `A0FE60` with `CE0BA42D` and `1.0`.
8. Resolves and validates the complete owner and source chain again, then reads the provider back and requires exactly `1.0` before reporting success.
9. Queues the normal Panoptes intro graph independently. Failure to initialize the visual does not suppress the working intro.

An already-native value of `1.0` is accepted without another write. Intermediate values are left alone because they may represent native animation in progress.

## Ownership and retry safety

The animation parent is resolved from the AI actor's native `+0x50` link. The component's `+0x1470` field must point back to that AI actor. We explicitly do not substitute the character's separate animation-state handle.

The write claim is recorded before native code runs. There is at most one attempt per Panoptes owner and mission run. If the actor or run changes during the native dirty callback, the result is treated as uncertain and is not retried. If native behavior later resets the value, the bridge does not fight it by forcing the value back to `1.0` every frame.

This keeps the change bounded to Panoptes's validated intro owner and avoids converting it into a permanent renderer or memory override.

## Runtime confirmation

The live log shows the complete data path working:

```text
stage=vfx_start source=80F6690A scalar=CE0BA42D
requested=1 observed=1 confirmed=1

stage=vfx_values animation_CE0BA42D=1 input_CE0BA42D=1
computed_20AA7FC1=0.00515597

stage=vfx_values animation_CE0BA42D=1 input_CE0BA42D=1
computed_20AA7FC1=0.754859

stage=vfx_values animation_CE0BA42D=1 input_CE0BA42D=1
computed_20AA7FC1=1
```

These entries are in [`Sunrise/logs/sunrise.log`](Sunrise/logs/sunrise.log) around lines 13458-13518. They show that the native provider accepted `1.0`, the controller received it, and the downstream filtered value ramped to `1.0`.

## Verification

The dedicated native fixture in [`omega_boss_vfx_native_tests.cpp`](Sunrise/unit/omega_boss_vfx_native_tests.cpp) executes Destiny's unchanged `A0FE60` and `A10180` implementations against the original `80F6690A` asset. It verifies every one of the 47 provider names, exact provider selection, native dirty notification, equal-value behavior, unknown-name behavior, ownership changes, run changes, disabled members, occupied queues, invalid layouts, intermediate values, idempotence, and no uncertain retry.

Results:

- `omega_boss_vfx_native_tests`: 2,410 checks passed in Debug and Release.
- `omega_boss_eye_diagnostics_tests`: 418 checks passed in Debug and Release.
- The installed candidate also produced live `vfx_start confirmed=1` and downstream `20AA7FC1=1` receipts.

The detailed reverse-engineering record is in [`vfx-audit.md`](build/omega-panoptes-vfx-fps-enemies-20260905/vfx-audit.md). The recovered expression is recorded in [`glow-expression-evidence.json`](build/omega-panoptes-vfx-fps-enemies-20260905/glow-expression-evidence.json).

## Important boundary

This is a safe steady-state initialization of the verified authored glow input. It is not a byte-for-byte reconstruction of Bungie's original command timing. The exact deferred command that originally set the scalar, and any additional authored timing parameter, were not fully recovered. The implementation therefore logs `mode=steady_state_candidate authored_timing=0`.

What is established is the useful part: we enable the original animation scalar through its original setter, its original notification fanout drives the original material/particle/light chain, and the live controller output reaches `1.0`.
