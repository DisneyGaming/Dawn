# Checkpoint platform performance fix

The foreground replay measured 105.83 FPS before encounter activation, 46.47 FPS
after the first 13 platforms, and 35.24 FPS after the next 11. CPU frame work grew
from 9.28 to 21.40 to 28.29 ms while GPU work stayed at 5.15 to 6.67 to 7.60 ms.
See FPS-LOAD-ZONE-20260913.md and the saved boundary-180425 capture.

## Implementation

Platform source callbacks now keep a bounded cache of 56 entries. Each entry is
keyed by the complete native object receipt, source address and entity bundle. It
holds the primary physics component, exact FB5 volume, generic pose device, and
volume body/shape. The existing exhaustive component discovery remains the miss
path, including rejection of distinct same-kind components. Repeated observations
validate exact definitions, owner, self-handle resolution and current body/shape
directly. Relocation or replacement invalidates the matching entry and triggers
rediscovery. Source generation, committed generation, weak identity, entity row,
bundle, current runtime request and cache reset epoch remain checked.

Completed pose acknowledgements now skip device polling until the controller
issues a new pose revision. Pending acknowledgements still read native actual,
target and revision, validate their owner, and submit to the existing controller.
The optimization never writes poses, health, occupancy or progression evidence.

Contact polling is coalesced to one sample every 50 ms for the current player,
attempt and expected platform. A new player, attempt or platform can sample
immediately. Two internally stable manifold scans bracket current object/body
validation; the previous code made four scans. The existing 500 ms dwell and
250 ms maximum sample gap remain unchanged. Missing, stale or wrong-owner
observations cannot advance a platform.

Small object/contact projections replace repeated copies of the complete
1,392-asset Frame in these hot callbacks. Runtime locks are never entered while
holding the platform cache lock. Reset clears all bindings, occupancy states and
poll state and advances an epoch so old work cannot publish after reset.

An acknowledged retained-platform callback is estimated from the adapter code to
require about 121 ReadProcessMemory calls instead of roughly 4,000, approximately
97% fewer. This is a static operation count, not an FPS promise or measured
function timing. The new helper tests enforce bounded validation without metadata
traversal. Linear asset lookups and native game work still exist.

## Validation and deployment

Release candidate: build/coo/eater-platform-performance-20260913.
Validation, packaging and installation receipts are stored in that directory.
The final stable-source validation status is recorded there, not inferred from
an earlier build. Targeted jobs cover the raid controller (all four paths and
56 platforms with and without positive health observations), native cache/contact
regressions, roster, local-player identity and other mission wire protocols.

New cache regressions cover direct identity changes, relocation, changed volume
body/shape, alias uniqueness, composite discovery/rebinding, failure clearing,
bounded read cost and polling timing/identity transitions. Narrow projections are
checked for inactive/finished runs, invalid indices, pending poses, completed
paths and changed player/attempt/command state.

Live FPS and gameplay acceptance for the new DLL remain to be measured after the
user launches it. The agent does not launch or control the game. The installation
must use the regular Release package and retain the previous installed backup.

## Installed result

The PC crash interrupted the first DLL compile. The resumed stable-source Release
run passed all five jobs: 5,634 controller checks, 184 native cache/contact checks,
12,282 roster checks, 86 player-position checks, the other-mission protocol suite,
and the Dawn DLL build. No additional validation round was run after that.

Installed on 2026-09-13 at 18:40:56 local time. DLL SHA-256:
`7fe0062e4e55e9b5cbb301ca20d635131292079cbaf0329a4909e58bda924539`.
The installer verified all 16 destination hashes and retained the previous build
at `.dawn/backups/lua-20260913-184056-fd02396f`.
The candidate directory contains `installation.json` and the package receipt.
Live FPS and gameplay acceptance remain pending the user's next launch.
