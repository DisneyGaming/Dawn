# Eater load-zone FPS investigation, 2026-09-13

The user initially described the drop as occurring at the Underbelly load boundary,
then clarified during the replay: it happens after the dropdown, when the encounter
and darkness zone activate. Before that FPS is in the hundreds, afterward in the
40s. The replay below captures this clarified trigger. No game input, game memory
mutation, DLL change, or installation was performed here.

## Foreground replay: encounter activation correlates with the first large drop

Read-only recording boundary-180425 captured run 2, PID 66596, for 120 seconds.
PresentMon recorded frames; a separate reader sampled foreground ownership,
controller state and thread CPU times about every 0.5 seconds. The unique trace
session exited successfully; AMD's existing RSXTraceSession remained untouched.

Foreground samples only:

- Before a reactor path began: 121 frames, 105.83 FPS, CPU busy 9.28 ms,
  GPU busy 5.15 ms. This baseline is short, approximately 1.1 seconds.
- First path begun, before first activation: 974 frames, 46.47 FPS,
  CPU busy 21.40 ms, GPU busy 6.67 ms.
- Progressing through the first path: 2709 frames, 48.24 FPS,
  CPU busy 20.54 ms, GPU busy 6.48 ms.
- Second path begun: 150 frames, 35.24 FPS, CPU busy 28.29 ms,
  GPU busy 7.60 ms. This is a short end-of-capture sample.

The script couples the darkness-zone restriction with path 1 beginning, then
creates its 13 platforms. The capture sees this transition at about 1.13 seconds.
It sees path 2 begin near 115.66 seconds, adding 11 platforms for 24 total. The
observed slowdown is mainly on the CPU side, and grows when another whole path
is created rather than monotonically with every single platform activation.

This supports the existing repeated native-platform validation suspect for BOTH
the clarified initial hit and subsequent progression losses. It is not yet a
function-level profile or an optimization A/B test. CPU busy here is PresentMon's
metric. Focus and controller boundaries are sampled, so frame classification can
be off by about one sample. Background groups are retained separately in evidence
and excluded from the comparison above.

Evidence: build/coo/eater-fps-20260913/boundary-180425/{metadata.json,frames.csv,
samples.jsonl,raid.log,summary.json,presentmon.log}. Capture and analysis scripts
are record_load_boundary.py and summarize_boundary.py in the parent directory.

## Earlier region transition is not the clarified FPS trigger

The current log records the entrance-to-reactor publication transition from
packed region 16 to 56 at t=111235 ms. The first native platform bindings appear
at t=158469 ms, 47.234 seconds later. The reactor path is not begun until
t=158328 ms; before then the contact observer exits before component discovery.
Thus the expensive successful retained-platform validation path was not yet
active at this earlier publication transition. The earlier diagnosis treated the
user's FPS trigger as this transition; the user's clarification and foreground
replay supersede that interpretation. No separate persistent load-zone fault has
been established.

General source observation still runs for the selected raid. It searches the
asset catalog and copies the entire controller frame before rejecting unmanaged
matching sources. This is a possible load-dependent cost, not a measured cause.
The shared source hook also invokes other mission observers. Their cost has not
been timed. Native scene rendering/streaming costs have not been isolated.

The entrance entity-ID workaround's special work requires a pending entrance
launch and does not qualify once the game is in mission. Its small general hook
entry/launch-snapshot cost remains; it is not evidence of a growing ID scan.

## Later progression provides a specific growth pattern

First-binding events appear in path batches:

- Path 1: 13 platforms at t=158469..158641 ms; cumulative 13.
- Path 2: 11 platforms at t=236469..236531 ms; cumulative 24.
- Path 3: 13 platforms at t=313828..313906 ms; cumulative 37.
- Path 4: 19 platforms at t=380953..381219 ms; cumulative 56.

The script creates each whole path in batches, retains previous paths, and the
source observers lack already-bound/pose-acknowledged fast paths. Consequently
the increase is associated with path creation, not necessarily each individual
platform rise. Approximately 4,000 small ReadProcessMemory calls per successful
retained platform callback remains a static estimate, not a measured event rate.

## Frame sample and limits

Found AMD's existing PresentMon under Program Files/AMD/CNext/CNext. Its existing
RSXTraceSession was left running and untouched. A separate, uniquely named
CodexEaterFps20260913 session recorded the current game PID 66596 for 15 seconds
and exited normally. It required no elevation and did not change the game's
foreground state. No CPU stack trace was available.

289 recorded frames averaged:

- Frame time: 51.535 ms (19.404 FPS).
- CPU busy metric: 51.424 ms; CPU wait: 0.110 ms.
- GPU busy: 5.782 ms; GPU wait: 45.753 ms.
- Present sync interval: 0 for every recorded frame.

These are PresentMon metrics, not function-level attribution. A foreground check
at the end returned PID 63560, not Destiny. Therefore this sample describes the
observed background state and cannot establish active-play GPU load or the
earlier 200-to-60 transition. It is consistent with the previous kernel-heavy
thread sample and the current ~20 FPS report.

The saved Destiny cvars file has framerate_cap_enabled=0. That is not proof of
all live/driver presentation settings. Log output was about 81 KB/s over a
20-second window containing the load transition, then about 12 KB/s before
platform creation and 8-17 KB/s during traversal. The transient burst alone does
not establish a persistent FPS cause. Gate diagnostic logging uses one global
last-state value for multiple components, allowing stable different gates to
appear changed as callbacks alternate; its current measured rate is modest.

## Next verification

Optimize repeated platform source discovery and pose acknowledgements while
preserving exact source, owner, generation, self and relocation validation.
Coalesce contact sampling and avoid redundant full-frame snapshots. Validate the
change in Release, then compare the same encounter milestones with a fresh
foreground capture. Add narrowly scoped timing counters if further attribution
is needed. No further replay is required to preserve the current diagnosis.

Evidence: build/coo/eater-fps-20260913/load-timeline.json,
load_timeline.py, current-frames.csv, current-frame-summary.json,
thread-cpu.json and the existing dawn.log. No performance fix applied yet.
