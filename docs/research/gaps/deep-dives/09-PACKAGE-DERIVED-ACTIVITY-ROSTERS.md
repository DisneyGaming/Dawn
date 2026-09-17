# Package-derived activity-roster construction

## Comparison status

This is relevant supporting implementation, not an entirely new difference. MISSING.md already acknowledges cached scenario/roster domains and some global-key safety work. It should not be summarized as having no activity-roster builder.

## Extracting structural identities

The scenario reader walks package-defined destinations and slice sets. Registry objects and slot descriptors provide the roster groups. Stored data retains registry keys and slot types, flags, and indices.

Indices are preserved because a sparse native identity is not the same as its compact list position. Densely renumbering it could direct authority at the wrong object.

The builder tracks slice-set group coverage through roster-key intersection machinery and records participation/lifetime slot presence. Unresolved slice sets are reported to that machinery instead of silently accepted as proof a key is universal.

## Runtime publication

The destination snapshot loads retained group data from the build-data store and exposes its arrays to the wire encoder. It builds top-level and bubble-sub-block views.

Ordinary coverage distinguishes global/active groups, selected-slice bubble groups, inactive-bubble groups, and absent groups. Bubble masks control where ordinary keys belong. Authored local keys can be added for the selected slice, bounded by scratch capacity.

Player identity is resolved separately. A shortened join character identity is matched against account characters, and publication uses the full SOID. With no match, the selected character is the fallback. Participation-slot groups provide the player-binding location.

## Result and limits

The roster structure can come from installed content rather than one hardcoded mission layout. This provides the identities needed for authority publication, but naming a group does not establish every slot's semantics or readiness.

Do not infer that every sub-block/global-key discrepancy or every destination is fixed. The machinery is implemented; exhaustive all-destination correctness was not established. It is also unrelated to queuez family-2 social identities.

Landmarks: Dawn/src/client/content/scenarios/scenario_roster_build.cpp and scenario_roster_groups.cpp; Dawn/src/middleware/content/packages/tables/roster_intersection.cpp; Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp. This explanation uses the earlier inspected builder/runtime and their retained interfaces. No package regeneration or all-destination test was performed for this archive.
