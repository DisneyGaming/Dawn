# Coordinated region, membership, and roster delivery

## Original gap

MISSING.md §1.2–1.3 describes partial membership/state routes with incomplete lifecycle policy. We added a transaction boundary around related region notifications so their data and local delivery state describe one retained transition.

## One retained transition

The snapshot identifies the bound activity, explicit region source, expected/next host-region identities, destination, resulting membership, advertisement, roster wire state, authority-grant candidate, and publication generation. A mask specifies the required notifications.

The source relationship is explicit: a binding either uses its own activity or borrows an exact creator-root activity through a group relationship. Later encoding does not simply select whichever activity happens to be current.

## Encoding and rollback

Before construction, the coordinator saves the response boundary, nonce, and activity-binding state. Required components are appended in protocol order: global state, membership, then roster. All required appends must succeed.

Failure wipes newly appended bytes, restores written length and nonce, restores binding state, releases advertisement/bound-lineage leases, and clears staging. Earlier unrelated response bytes remain outside that rollback boundary.

Nonce restoration matters because discarded encrypted frames were never published. Binding restoration matters because roster encoding may have staged delivery changes. Releasing leases prevents a failed attempt from retaining activity lineage indefinitely.

## Deferred delivery

A snapshot can be pending while its advertisement/host relationship is being prepared. Retained publication-debt state records the exact binding, activity/source, committed region, membership after-state, destination, required components, and any authoritative HUD publication still owed.

That preserves the outstanding transition instead of an unqualified instruction to send whatever exists later. Periodic refreshes also use the snapshot/bundle path; pending periodic construction releases temporary leases before returning deferred.

## Checks and limits

Test-only failure injection exists after global-state, membership, and roster append, exposing partial-construction boundaries. Those tests were not run for this write-up.

This is local transactional construction. It does not make several packets atomically observable by the remote game and does not establish host migration or client effect acknowledgement.

Landmarks: Sunrise/src/server/bap/region_lineage.h; Sunrise/src/server/bap/encrypted/activity_transaction/activity_transaction_notifications.cpp.
