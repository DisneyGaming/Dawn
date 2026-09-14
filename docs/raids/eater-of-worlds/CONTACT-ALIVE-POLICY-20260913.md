# Platform contact supplies solo progression evidence

The user confirmed that temporarily passing the controller's alive gate caused
the next platform to rise. The user then requested that standing on the platform
be sufficient evidence of being alive, explicitly clarifying that full health
was not required.

The controller now permits progression from authenticated current-player platform
occupancy even when the optional native health reader has no result. It does not
set the player's health, mark `aliveKnown`, manufacture a health observation, or
retain the temporary memory override. Exact platform ownership, active native pose,
ordered path index, current player identity, sample freshness and the continuous
500 ms dwell remain required.

A confirmed death remains a veto only when its health identity matches the current
player. The same predicate controls platform activation, goal completion, the
transition into holdout, and the death-related respawn restriction. A retired
actor's health record cannot block its authenticated replacement. Changing the
current player still resets the unfinished path.

The goal needs every required platform contact followed by an authenticated goal
position. A goal-volume crossing alone cannot bypass any platform. The native
platform detector uses the exact authored cylinder manifold; contact is explicit
solo progression policy, not a recovered original multiplayer host rule or a
general-purpose physiological health measurement.

## Verification

The Eater regression runs the complete 56-platform/four-goal sequence with no
health observations, then runs a second sequence with death and replacement cases.
It preserves same-player death/respawn and stale-contact checks and verifies that
a replacement reaches holdout even while its predecessor's dead health record
remains present. The native detector suite still has 37 cases.

Candidate and validation evidence: `build/coo/eater-contact-alive-20260913`.
The earlier five-byte memory test was restored; its evidence remains under
`build/coo/eater-platform-recheck-20260913-1733`.

The native health reader's live failure is still unresolved. This policy removes
its role as a required positive input for reactor progression. Health-based
same-entity death/reset still requires valid health observations; ordinary player
identity replacement supplies an independent unfinished-path reset.

## Installed Release

Installed at 17:42:58 EDT on 13 September 2026 after Destiny closed. All five
targeted Release jobs passed with a stable source manifest and zero compiler
warnings: 5,618 Eater checks plus 37 native-volume checks, 12,282 roster checks,
86 position checks, the other-mission protocol regression, and the DLL build.
Installer validation passed and all 16 installed file hashes matched afterward.

- DLL SHA-256: `ab05436702c468aa5e891040c3e5a74f7c23ef13068ff973ec6375122618410d`.
- Package SHA-256: `f7cc08a50330b64762d0ce75627c3e8bc93b43ffd43eaef4ddf65d4d9c0bff7a`.
- Backup: `.sunrise/backups/lua-20260913-174258-94def721`.
- Receipt: `build/coo/eater-contact-alive-20260913/installation.json`.

The game was not launched by the agent. The new build still needs a user-operated
first-platform and subsequent-path test. The successful earlier in-memory test
proves the diagnosis but is not a complete traversal of this installed build.

## User-operated traversal and FPS report

On the next run (PID 66596), the user confirmed the mission was working. At 17:55
EDT the read-only controller snapshot showed section 2 (holdout), all 56 raised,
activated and pose-acknowledged platform bits set, and path 4 next index 19. The
health reader remained unknown. This establishes real reactor-platform progression
through all four paths with the installed contact policy; holdout combat and the
remaining raid are not accepted by this observation.

The user reported about 60 FPS at the underbelly load zone, falling to about 20
while progressing. A five-second read-only thread CPU sample found thread 53600
at 96.55% of one core (3,515.62 ms kernel and 1,312.50 ms user time). Source review
found repeated pose/binding observers still make about 4,000 tiny ReadProcessMemory
calls per retained platform callback, including after initial binding and pose
acknowledgement. All 56 platforms remain active. This is a strong source-level
performance suspect; exact hook frequency/stack attribution is not measured.
WPR could not enable system profiling (0xC5585011). The GPU samples were not paired
with a foreground-state trace and must not be treated as proof against a GPU limit.
Evidence: `build/coo/eater-fps-20260913/`.
