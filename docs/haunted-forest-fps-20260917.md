# Haunted Forest gateway ownership performance

The user reported sustained frame drops while moving through Haunted Forest.
Live measurements of Destiny PID 11024 found thread 72840 using about 87% of
one CPU core, with GPU 3D utilization ranging from approximately 61% to 81%.

A bounded 250-sample capture of that thread found 79 instruction pointers in
`ZwQueryVirtualMemory`. The copied stack prefixes contained 73 direct return
addresses after `VirtualQuery` calls in `prepare_registered_forest`: 59 at
DLL RVA `18C8BF` (gateway loop) and 14 at `18C7E0` (worker owner). The matching
installed PDB resolves those addresses to lines 1860 and 1853 respectively.
The stack-prefix scan is not a complete unwind; the exact return sites plus
the sampled system calls identify this repeated permissions check as a hot
path. The sampling pauses ended immediately after register/stack capture;
the longest recorded pause was about 1.1 ms.

Every worker tick checked the readability of the same image-backed authority
bitmap separately for the worker and every qualified gateway child. The
bitmap contains just 8,192 bits (1,024 bytes). With many child objects, these
redundant kernel queries consumed a substantial share of the game thread.

The fix validates the complete bitmap once per worker tick and retains a
read-only view for that invocation. Each owner still reads its current bit.
The generator registration, full entity identity, weak serial, and native
retirement checks are unchanged; no authority values or child identities are
cached between ticks. The original setter still performs missing-authority
repairs.

Validation:

- Release x64 build passes.
- Haunted Forest scope/bitmap suite: 16,827 checks, zero failures. It covers
  all 8,192 slots, salted handles, the invalid handle, permission denial,
  null tables, live bit changes, and a single range check across all lookups.
- Compiled Tower loading stack regression still passes at 2,506,672 bytes
  across the recorded DLL chain.
- The obsolete `omega_forest_recipe_tests` harness was initially tried, but
  it references recipe/seed APIs removed by the incoming branch. The new
  tests live in the current, passing `haunted_forest_scope_tests` suite.

Evidence: `.codex-tools/forest-cpu-samples.json`, `forest-fps-build.log`,
`forest-fps-stack-budget.json`, and `fotl-test-haunted_forest_scope.log`.
The corrected DLL requires a game restart; in-game FPS improvement has not
yet been measured.
