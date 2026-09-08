# Lua mission format

Mission files are `.lua` source. They use the builders documented in [Lua mission authoring](../docs/LUA-MISSION-AUTHORING.md). The interpreter produces an owned definition tree (`coo/script_value.h`); `coo/mission_script.cpp` validates it and publishes immutable executor views. There is no mission JSON parser, intermediate JSON text, or extension fallback.

## Native capabilities

`command("capability")` selects an operation, native asset, argument, and receipt policy registered in the mission's C++ binding profile. A script cannot invent native authority or engine functionality. Deadly Trial and Gateway put those mappings in their `bindings.h` files. Omega currently registers its capabilities in `omega_script.cpp` from its existing native definitions.

`command("cue", {id="later_cue", argument=500})` changes an adjustable argument when the profile permits it. The alias must be distinct. `eventAfter` delays run from the authenticated native event selected by that capability. Zero and delays above the registered maximum are rejected.

`complete` requests native mission completion through the adapter. It retains run ownership and clears owned objectives and markers. Authors must include the intended gameplay gates in the graph.

## Graphs and composition

Graphs contain named steps, dependencies, commands, and named receipts. `graph` automatically creates receipts for waiting commands unless an explicit receipt map is supplied. Forward dependency references are supported; cycles fail validation.

The outer mission selects graphs, role mappings, a composition entry, native modules, and completion facts. Deadly Trial and Gateway also consume authored `phases` and observation conditions. Omega's native consumers select its established role names instead.

## Presentation

`presentation{...}` selects validated dialogue rows, timings, objective cues, named cue/action sets, native binding tables, and markers. Dialogue bank and selector identity remain native. Cue and action order matters; named sets are looked up by their ID.

```lua
presentation{
    markers={{objective="0x722FE621", target="vance"}},
}
```

Only registered objectives and marker targets are accepted. Each objective may have one marker mapping. Unmapped objectives clear the previous marker.

## Omega integration

`omega.lua` supplies all 14 executable graphs, including opening, Forest, reveal/retry, seven combat sections, ending/retry, and mission composition. It also supplies dialogue and presentation data. Lua preserves the previously validated Omega behavior.

Omega still has native state-machine constraints in `omega_script.cpp`: required roles/capabilities and receipts, native prerequisites, simultaneous phase activation and receipts, and presentation/encounter/ending producer order. Graph, step, and command IDs can change when references remain valid. Independent steps may be reordered. Native combat section order, progression observations, and special Panoptes interactions still live in C++.

Deadly Trial and Gateway have already removed their compiled story sequence contracts. Omega's Lua conversion does not yet remove its native sequence constraints. Keep those limits explicit when reconstructing or editing missions.

## Validation

The shared Lua sandbox and schema checks enforce resource limits, registered references, supported arguments, bounded graphs, valid dependencies and conditions, and qualified native receipts. `coo_script_tests` additionally exercises Omega's native adapter, renamed/reordered graphs, presentation edits, retries, and handoff. Full native replay suites load the shipped Lua.

See [scripts README](README.md) for build, packaging, installation, and reload instructions.
