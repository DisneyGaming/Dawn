# Incremental character and orbit-roster refreshes

## Original gap

MISSING.md §6 discusses queuez records, stale appearance, and incomplete publication. Our build has explicit incremental refreshes for equipment and equipped-socket changes, separate from activity sensor rosters.

## Initial records

Family 3 builds the character list plus one character record for each character in use. For each record it resolves equipped instances and equipment light, encodes appearance, compresses the object, and retains its object identity in the snapshot.

## Equipment-change transaction

The operation supplies a pending state mutation. Refresh preparation projects the changed character into a temporary account snapshot, validates its identity/index and selected-character relationship, and resolves equipment from that candidate state.

If family 3 is active, staging allocates a refresh at one advanced revision. The updated character record is appended first, followed by the changed roster list. Both belong to that same revision and the update is incremental, not a full snapshot.

The surrounding outcome staging also handles required family-4 inventory and family-0 appearance changes. Failure to build a required part returns failure before publishing the pending state and peer-version changes. Construction does not commit equipment merely to discover what the output should contain.

## Equipped-socket transaction

An equipped socket can change shaders/perk banks without changing the roster list's base-definition references. That refresh therefore emits only the family-3 character record, with includeRoster false. It also stages family-0 appearance when that family is active.

Inventory-only socket changes do not require a rendered-character refresh. This avoids treating every inventory edit as a full roster replacement.

## Validation and limits

Validation checks the active family root, nonzero character identity, matching character/index, selected character, and usable advanced revision. These checks bind the projected appearance to the intended account/character.

This does not prove every blank orbit card, unknown field, or subscription discrepancy is fixed. Family-2 social roster is separate. Family-4 companion/duplicate snapshots remain separate from the incremental refresh mechanism.

Landmarks beneath Dawn/src/server/bap/: encrypted/push/snapshot/roster_snapshot.cpp; encrypted/queuez/queuez_outcome_staging.cpp; encrypted/queuez/staging/queuez_character_staging.cpp; encrypted/push/queuez/queuez_banner_push.cpp. No game run was performed for these notes.
