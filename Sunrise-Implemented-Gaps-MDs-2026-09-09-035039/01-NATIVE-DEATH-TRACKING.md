# Native death tracking

## Original gap

MISSING.md §10.7 describes death sensing as unresolved: objective-counter changes are not reliable per-entity death events. We implemented an identity-qualified native death ledger without inventing a new network death packet.

## How we did it

Each authored registry/source pair has a population count and an enabled state. Admission checks the receipt's validity, current run and generation, source registry, and cohort capacity. Actors from disabled sources are refused. An already admitted actor is not admitted again; excess actors from non-required cohorts do not become extra kill requirements.

Every accepted actor retains its full receipt, death bit, readiness information, and readiness bit. A death observation must match the current run/generation and equal the stored receipt. The first matching observation sets the death bit. Duplicate deaths, unknown actors, and observations from older attempts do not change progress.

Clearance is a separate query. The cohort must be enabled, its admitted count must equal the requested count, and all admitted actors must be dead. An incompletely spawned population therefore cannot clear merely because the actors that did spawn died. Readiness is also separate: confirmed death remains terminal evidence when it arrives before a readiness poll.

## Authenticating native evidence

The ledger relies on its adapter to authenticate receipts. The inspected boss adapter resolves the native event arena, checks the payload class, verifies current character/entity/actor relationships and the typed health owner, and requires the native dead bit. It checks the current command's owner and phase before forwarding a death.

Identity is rechecked around native health reads. Readable memory alone is insufficient: a recycled handle or old component must not become evidence for the current actor. Other populations require their own qualified adapters; this mapped boss observer is an example, not a universal event decoder.

## Result and limits

Progress can depend on the deaths of the population actually admitted. Time, proximity, and generic counters cannot replace the identity match. This does not resolve every Sense field or implement authoritative server combat.

Implementation landmarks beneath Sunrise/src/: state/activity/coo/population_service.h; client/hooks/bootflow/omega_mission_health.inl. Current bodies were inspected for this note. No new runtime test was performed.
