# Activity-roster publication ownership and rollback

## Original gap

MISSING.md §1.2–1.3 and §7 identify incomplete ownership/retry around activity state. We made outgoing roster bodies owned transactions: preparing bytes does not automatically consume the corresponding delivery state.

This is the Activity Host sensor/auth roster, not social family 2 or character family 3.

## Publication flow

1. Check that the session's activity binding is current, its activity instance exists, and no conflicting body is already staged.
2. Derive a candidate publication generation from the binding clock. Invalid ownership and exhausted generation space refuse staging.
3. Encode the roster and retain the relevant before/after delivery values with the staged publication.
4. At the caller's commit boundary, check ownership again. Only the exact binding/activity may accept the delivery-state changes.
5. On discard, restore temporary delivery changes or drop the retained after-state. A stale body must not modify a replacement binding.

## Retained state

Staging records the binding, activity incarnation, publication generation, group count, send count, roster state sequence, and associated delivery fields. Authority grants are candidates until commit rather than assumed delivered during construction.

The coordinator-owned path encodes an immutable after-image without advancing live delivery state. Its freshness check expects the next generation. The older path can temporarily advance state during construction, retaining prior values so discard can restore them. The settlement predicate distinguishes the two forms.

## Why this matters

If roster encoding succeeds but a later response component cannot fit, leaving the send counter/state byte advanced could make the retry behave as though the client already received the update. Rollback restores the values needed to send it again.

If the connection changes activity between staging and settlement, the binding/incarnation mismatch rejects the old publication. Neither delayed commit nor rollback can overwrite the new activity's counters.

## Limits

Caller acceptance is not proof that the client instantiated every object or consumed each effect. This supplies local publication ownership and rollback, not general effect acknowledgement or late-join reconstruction.

Landmarks: Sunrise/src/server/bap/encrypted/push/activity/activity_roster_push.cpp; Sunrise/src/server/bap/internal.h, especially stage_roster_publication_generation and staged_roster_is_current. No new live test is claimed.
