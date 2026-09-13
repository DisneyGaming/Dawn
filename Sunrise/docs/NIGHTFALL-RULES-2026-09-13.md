# Nightfall challenge rules

The two strike families retain their installed native Adept, Master and
Grandmaster activity identities. The selection page now opens a separate
**Challenge modifiers** window. Each difficulty keeps its own draft; launch
copies and sanitizes that draft before submitting the native activity. Changing
the picker cannot change a run already loading or in progress.

## Implemented policy

Grandmaster requires locked equipment and socket selections, ordinary ammunition,
Extinguish and the revive limit. Teleport and noclip are forcibly inactive during
GM, including queued teleport requests and hotkeys. The saved client preferences
are preserved. Restrictions begin immediately before native launch submission,
remain through loading and the run, and clear in orbit or on an unrelated actual
destination. Equipment commits hold a shared rules lease so a transaction cannot
race the launch boundary or bypass a newly armed lock.

The authored GM preset requests a minimum **30 Power disadvantage**. The modifier
window can raise this to 50, reduce the initial four revives, or shorten their
45-minute lifetime. GM base restrictions cannot be unchecked. These are this
project's challenge settings, not a claim that build 86657's opaque modifier
object natively contains these exact values.

The current membership publisher supplies one participant per activity. In this
solo host, that participant's death is a full fireteam wipe. Extinguish therefore
ends a GM on the first authenticated death. On lower tiers, optional limited
revives permit the selected number of ordinary solo respawns before failure.
This is not an implementation of multiplayer teammate resurrection arbitration.

Player observation calls the version-checked native local-entity and interface
lookup helpers (`4B2260`, `557470`). It validates the health runtime, complete
owner identity and salted health lease before reading the native death bit.
The lease survives the switch to the spectator Ghost within the same session;
missing components and streaming retirement are never counted as deaths. A region
session change discards the old body lease and requires a fresh living-player
sample before accepting death, while preserving the original run timer and
revive budget. Old run/session samples and duplicate dead samples are
rejected. The native lifetime encoder publishes terminal phase 8 on failure,
with no success result. Completion and further score credit are then rejected.

## Power and scoring

The player-owned native power publication and resident-record refresh are
described in [the native variant notes](NIGHTFALL-VARIANTS-2026-09-13.md). Both GM
records author activity Power 1100, so the 30-point preset caps the published
player aggregate at 1070; it never raises a character already below that value.
The enemy difficulty indicator is left to the native client. No sword icon is
drawn or forced by the Dawn interface. A fresh gameplay check is needed to verify
the combat power consumers and the indicator shown by this installed client.

The source-authority serializer previously forced template variant 0 even in a
Grandmaster activity. It now receives a frozen GM choice from the exact activity
and requests variant 5 at 23 Garden sources and 17 Tree sources. Installed-package
checks prove every category at those sources has a valid variant-5 choice.
The production wire test encodes every ordinary source in both strikes and checks
that all forty supported GM replacements reach the variant field, while other
sources retain their authored selection.

The Dawn run score is authored here: 100 per accepted ordinary enemy death,
5,000 for the authenticated strike boss, and a 500-point death penalty bounded
at zero. It is driven by accepted controller receipts, not requested populations
or a guessed kill counter. The ledger supports a 1,000-point Champion credit and
one additional revive, but source-level GM substitutions alone do not qualify a
Champion. The production callers currently pass no Champion credit until that
per-actor classification is proved. This score is not the retail scoring formula.

## Completion bonus

One authenticated successful Nightfall queues a Dawn-authored Glimmer bonus:
1,000 for Adept, 2,500 for Master, or 5,000 for Grandmaster. The exact installed
currency definition and account mutation path are documented in
[the completion reward notes](NIGHTFALL-VARIANTS-2026-09-13.md#completion-rewards).
The grant respects the 250,000 cap, retries unavailable delivery, and rejects
duplicate completion, failed runs, wrong account ownership and stale claims.
The selector labels this as a session bonus: like current account inventory
changes, it lasts until the client closes. Native Nightfall loot tables have
not been recovered.

## Validation and remaining native acceptance

`nightfall_rules_tests` checks immutable launch options, the GM minimum,
restrictions and restoration, cross-session region continuity, stale and
duplicate player observations, limited respawns and expiry, terminal result
ownership, and equipment/launch concurrency. `nightfall_wire_tests` checks both
strikes and verifies that failure wins over an existing completion publication
without encoding success. The existing native launch argument and lifecycle
fixtures remain in use. The production ImGui/DX11 fixture opens and closes the
modifier window and verifies that its selected values reach the exact GM launch.

The final focused runs passed 80 rules/reward checks in both Release and Debug,
32,923 native launch argument checks and 438 lifecycle checks. The production
power encoder suite passed 73 checks and the source/terminal wire suite passed
1,525. The installed-package Grandmaster substitution suite passed all four tests.
The rendered production modifier window completed without ImGui errors.

The native power projection is applied at the common selected-character encoder,
so inventory acquisition, dismantling, and item-lock updates cannot overwrite it
with an uncapped character summary. A versioned full refresh restores original
power on departure. All power changes affect published records, not saved item
levels or account inventory.

A clean client restart and native playthrough must still check: character power
refresh on entry/exit, actual damage scaling and difficulty indicators, ordinary
and queued teleport/noclip rejection, native player death observation, the
phase-8 return to orbit, and clean ordinary-strike replay afterward. Unit tests
and an encoded terminal publication do not establish those visual outcomes.
A completed Nightfall must also confirm the visible Glimmer bonus and one-time
delivery in the live client.
