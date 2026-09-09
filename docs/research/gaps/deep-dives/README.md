# How we implemented the gaps

Ten separate feature notes, prepared September 9, 2026. This archive contains Markdown explanations only. No source files, code excerpts, scripts, binaries, or Lua/executor documentation are included.

## Features

1. [Native death tracking](01-NATIVE-DEATH-TRACKING.md)
2. [Reset ownership](02-RESET-OWNERSHIP.md)
3. [Activity-roster publication](03-ACTIVITY-ROSTER-PUBLICATION.md)
4. [Region/membership coordination](04-REGION-MEMBERSHIP-COORDINATION.md)
5. [Character-roster refreshes](05-CHARACTER-ROSTER-REFRESHES.md)
6. [Native object ownership](06-NATIVE-OBJECT-OWNERSHIP.md)
7. [Native movement and cinematics](07-NATIVE-MOVEMENT-AND-CINEMATICS.md)
8. [Matchmaking descriptor lookup](08-MATCHMAKING-DESCRIPTOR-LOOKUP.md)
9. [Package-derived activity rosters](09-PACKAGE-DERIVED-ACTIVITY-ROSTERS.md)
10. [Character appearance encoding](10-CHARACTER-APPEARANCE-ENCODING.md)

Each note describes the gap, implementation flow, ownership/validation rules, and limits. Notes 9 and 10 explain relevant existing machinery rather than wholly new capabilities absent from the friend's description.

## Evidence and scope

The baseline is the supplied MISSING.md. Your friend's repository was not available; this compares our implementation with its written descriptions. Core tracking, lifecycle, object, publication, matchmaking, and appearance bodies were read during preparation. Remaining context comes from the earlier source inspection and recorded implementation documentation. No build, tests, package regeneration, installed-DLL verification, or live playthrough was performed for this documentation task.

Source paths and function names are navigation references, not included files. Some contain mission names or the coo directory because that is where the inspected implementation resides. The notes explain native mechanisms and infrastructure, not mission scripting or walkthroughs.

This work does not establish a general physics/AI server, durable persistence, social family-2 records, full matchmaking, reconnect/late join, or host migration. Family-4 duplicate subscription behavior and unknown record fields remain separate gaps.
