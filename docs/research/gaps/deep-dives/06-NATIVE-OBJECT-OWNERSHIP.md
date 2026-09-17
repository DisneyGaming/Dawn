# Native object initialization and destructible ownership

## Original gap

A typed spawn/device command does not prove the intended native object exists or consumed its state. Our object service adds a preparation-to-ready lifecycle with a qualified owner. Destructible handling separately requires destruction of that same owner.

## Preparation and creation

A binding identifies the source, optional device, initial position, and whether creation needs a fresh generation. Begin validates the generation lease, binding count, finite positions, representable types/slots, and unique sources.

Each object starts in prepare. A matching preparation acknowledgement moves it to create, optionally advances to the reserved next generation, and requests creation. Preparation and creation are separate steps.

## Binding the native object

Observations carry run/generation, source, entity handle, serial, and controller identity. They must match the current binding and expected generation. Once established, a different entity, serial, or known controller cannot silently replace the owner.

The first accepted entity observation advances create to bind. A device-backed object lacking its controller stays there. With a controller, it advances to apply and requests the configured position.

Ready requires an applied observation at the exact requested position and revision. Seeing the entity alone is insufficient. The acknowledgement proves revision consumption, not that the device animation finished.

Changing position increments the revision and returns a ready object to apply. Revision exhaustion is refused. Retirement disables creation and prevents later observations from making the object ready again.

## Destruction and linked devices

A destructible starts immune and binds one valid owner. Exposure makes it vulnerable. Destruction succeeds only for that exact owner while vulnerable. Wrong-owner and duplicate destruction cannot advance it.

Linked-device mappings select immune/vulnerable/destroyed positions and can retire a source on destruction. One confirmed destroyed object can therefore project consistent barrier, beam, and device changes without treating an unrelated enemy death as the object's destruction.

## Limits

These are mapped native objects with trusted observation adapters, not a generic replication writer, physics engine, or automatic controller discovery. Live integration still needs its own validation.

Landmarks: Dawn/src/state/activity/coo/object_service.h and lifecycle_service.h; supporting explanation in Dawn/docs/UNIVERSAL-MISSION-SERVICES.md. Bodies were inspected, but no new live test was performed.
