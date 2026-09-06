# Mission definition format 2

The generic compiler builds bounded executor definitions from JSON. It has no Omega graph templates, mission names, compiled-definition pointer lookup, or global publication state. A caller supplies a trusted native `Profile` and retains the returned immutable `MissionDocument` for every run using its views.

## Root fields

- `format_version`: exactly `2`.
- `mission`: the authored mission identifier.
- `profile`: a registered native profile. Profiles supply supported operations, domains, modules, facts, dialogue metadata, presentation events and binding tables. JSON cannot register executable native code.
- `authority_schema`: the supplied profile's native schema name.
- `assets`: named registry/definition/type/slot identities belonging to registered capabilities.
- `bindings`: named commands declaring `capability`, `operation`, named `asset`, `argument`, and `wait`. All fields must agree with the capability. Capability identifiers are independent of authored step and command IDs, even where the existing Omega profile uses descriptive legacy names containing slashes.
- `graphs`: named graphs declaring `name`, native service `domain`, `steps`, and `receipts`.
- `roles`: integration role names mapped to graph IDs. Renaming a graph requires updating its references. The generic compiler imposes no particular role names.
- `entry`: the composition graph ID; it publishes registered modules and joins their registered facts.
- `modules`: registered producer names in publication order. Native integration may enforce producer dependencies.
- `observations`: entries containing a registered `fact` name and a named `receipt` in the entry graph.
- `presentation`: generic dialogue, named cue sets, named action sets and named binding tables.

## Graphs and receipts

Each step declares an `id`, an `after` array of step IDs, and a `commands` array. Each command has an `id` unique within its graph and a reference to a named `binding`.

`after` may reference a step written later in JSON. The compiler rejects missing references, duplicate IDs, self-dependencies and cycles, then performs a stable topological sort. The executor still receives bounded earlier-step dependency masks. Independent steps preserve authored order when possible. Commands within a step execute in authored order; reordering presentation actions can intentionally change behavior.

The graph's `receipts` object maps semantic names to command IDs: for example, `"camera.ready": "request_camera"`. Every waiting command requires exactly one named receipt. Commands with `wait: requested` have no receipt bindings.

Native controllers resolve receipt names against the definition pinned by their executor's `start()` call. The resulting token still includes run, incarnation, step and command. Missing names, stale tokens, wrong ownership and incompatible milestones remain rejected. A completion event cannot invent native readiness.

Wait meanings:

- `requested`: the service accepted the request.
- `nativeReady`: a qualified native readiness receipt arrived.
- `completed`: native readiness and completion were both observed.
- `observed`: a qualified observation arrived; reserved for observation operations.

The generic loader permits new graphs and additional occurrences of registered operations. Native integration can impose narrower requirements for a stateful mechanic. Structural validity alone does not prove that a script completes in game.

## Presentation

`dialogue` declares the registered bank, all its numbered rows, objective restrictions, dispatch timeout and spacing. Row selectors, native child delays and scene ownership are native contracts. Host duration estimates and scheduling intervals are editable.

`cue_sets` maps registered event-set names to arrays of `{event, cycles, actions}`. Event names resolve through the profile. Cycle numbers are 1-8, limited by the event capability; events without cycle selection use an empty array.

`action_sets` maps arbitrary names to ordered dialogue/objective action arrays. Each action has `operation`, `value`, and `delay_ms`. Dialogue values are registered non-scene-owned row numbers. Objective values are registered selectors and have zero delay.

`binding_tables` maps registered table names to traversal, objective and dialogue bindings. Assets are named references; native stage/event/row associations must match the registered table. Dialogue delays are editable.

## Omega integration

Omega selects role names for opening, Forest, reveal/retry, combat sections, ending/retry and composition. Its adapter checks required native capabilities and named receipts, native phase prerequisites, simultaneous activation of phase commands and receipts, and presentation then encounter then ending producer order. These checks live in `omega_script.cpp`, outside the generic parser.

Graph IDs, step IDs, command IDs and independent-step positions can change without redirecting native receipts. Required role and receipt names remain the interface to native mechanics. A new Omega phase or changed mechanic may require a profile update; the format does not make unsupported engine behavior available.

The old read-only `native_catalog.populations` mirror is removed from JSON. Population catalogs, enemy construction, Scene codecs, fixed navigation geometry, native transitions and special Panoptes behavior remain C++. Compiled Omega definitions remain as native phase contracts and the legacy reference.

## Limits and verification

Limits: 1 MiB input, nesting depth 16, 30000 JSON nodes, 256 bytes per string, 64 graphs, 32 steps/8 commands per graph, 256 receipts per graph, 8 composition modules, 32 composition facts, 64 dialogue rows. Graph indices are checked before narrowing. Names are nonempty printable ASCII. Unknown fields, duplicate JSON keys, unsupported types and unregistered native bindings are errors. UTF-8 BOM and ordinary JSON whitespace are accepted; comments are not JSON.

`Sunrise/unit/fixtures/mission_script_alternate.json` uses another native profile, schema, module order and graph layout. Its tests add a step through JSON alone and execute scene/population receipts through the shared executor. The executable links only the generic compiler, without the Omega profile or publication bridge. This proves compiler reuse; it is not a second mapped playable mission.

The Omega suite renames every graph, step and command, reorders independent steps, changes native receipt command slots, and exercises opening direct entry, reveal retry and ending retry/handoff. Full frozen parity suites remain required before deployment.
