# Tower loading stack overflow after Festival integration

The 2026-09-17 16:03:46 crash is `C00000FD` in `__chkstk` while entering
`native_activity::update` from `build_roster_snapshot`, before Tower arrival.
The later `network_update` hitch assertions are a consequence of that failure.
The installed DLL was `D2ADE3C696F2A9C6E56904457D1A69BDCF5D1B70D18CD8BEBB995D1D251E3B7F`.

PDB symbol resolution and the Release DLL's x64 unwind records show 2,581,520
bytes for `native_activity::update` and 4,390,336 bytes across the recorded DLL
call chain. The game executable reserves only 4,194,304 bytes of stack.
MSVC generated separate 63,304-byte `NativeActivityFrame` temporaries for the
many `return {}` paths, even though only one return can execute.

Rejected updates now copy one immutable, default-constructed frame. This keeps
the exact native defaults and rejection behavior while eliminating those
per-branch temporary buffers. The rebuilt update frame uses 697,856 bytes and
the recorded DLL call chain uses 2,506,672 bytes. No stack limit, exception
handler, Festival availability, or save state is changed.

Validation:

- Release x64 build passes.
- `tools/testing/native_activity_stack_budget_tests.py` fails on the crashing
  DLL and passes on the corrected DLL. It reads compiled unwind metadata and
  matching PDB symbols, limits the recorded chain to 3 MiB, and limits the
  update function to 1 MiB. It is a regression bound, not a whole-program proof.
- The Forest mode suite passes 445 checks, including new Tower, Mercury, and
  Forest startup/publication smoke tests on separate 1 MiB threads. The main
  harness's larger fixture stack therefore cannot mask those startup failures.
- All 7,328 shared Festival/New Light/vendor flag checks pass.
- All four pinned-native Tower recovery replay tests pass.

Evidence is retained in `.codex-tools/fotl-tower-stack-before.json`,
`fotl-tower-stack-after.json`, `fotl-tower-stack-build.log`,
`fotl-test-haunted_forest_mode.log`, `fotl-tower-flags-regression.log`, and
`fotl-tower-recovery-regression.log`.

Deployment replaces both installed DLL/PDB pairs, with the previous pairs
backed up under the game's `.dawn/backup` directory. Save files and Festival
settings are left intact. A live Tower load remains to be verified.
