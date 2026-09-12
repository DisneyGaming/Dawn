# Native activity handoff evidence

This pass removes no reachable ending behavior. The separate dormant portal
adapter was removed by the integration owner. It was dead hook cleanup, not a
new transport migration. The active same-activity portal transports already use
native membership messages and native teleport completion receipts.

## Active and dormant owners

`client/hooks/bootflow/world_step.cpp` includes `omega_activity_handoff.inl`,
calls its `poll`, and reads `suppress_active` from the loading-cinematic detour.
The ending request is reachable through `state/activity/coo/omega_adapter.cpp`
`NativeControllers::request_ending`; the explicit preview UI also calls
`omega_ending::request_preview`. This adapter is not dead code.

By contrast, the removed `sample_omega_portal_transport` consumed
`request_omega_forest_transition`, which had no executable caller, export,
command registration, function-pointer registration, or UI reference anywhere
in the active source. Its old world-frame poll alone could not arm it. The
request/consume/issued API, state, native wrapper constants and poll were
removed together. The still-read quiesced getter was left for separate audit.

`omega_mission_ending.inl`, included by `omega_reveal_native.cpp`, only resolves
and observes native ending resource/playback receipts. It is not the owner of
the activity selection, cleanup or cinematic suppression behavior.

## Two different transport contracts

The active Omega/Beyond same-activity region path publishes membership message12
host teleport tuples. Original `C72CE0` consumes them; actual message22 local
teleport feedback acknowledges arrival. The server holds the same target and
nonzero token through host-state1, local-state3, host-state3 and local-state0
before returning host-state0. Current `omega_ending_transit_rules.h`,
`omega/omega_ending_transit.h` and `beyond_infinity/transit.h` provide the
identity-qualified state machines. These are real server-to-native transport.

The removed adapter used `E2E7E0`/`E1D400` to request a local world-controller
operation. `E1D400` writes pending fields at manager+524/+528/+52C/+530.
`E25A30` promotes the pending request through `E2B120(...,7,...)`. That7 is a
world-controller operation, not a BAP notification type. BAP7 is `sensor_message`.

BAP1 `global_activity_state` carries an activity descriptor and an initial-slice
arm. `activity_global_state_push.cpp` currently publishes the selected activity
and ending bookend bubble15/slice121. An initial-slice value is not an independent
request to leave the current activity. BAP11 `start_new_activity`, schema80808698,
is an inbound client request containing descriptor80808716; its authenticated
Adventure handling is not a host-to-client fireteam handoff command.

## Newly recovered real network record

The original group registry0 is `world-controller-goal-data`. Registry setup
`17BAE60..17BAEA9` registers writer `17BE8B0` and reader `17BE860`, with a12-byte
decoded value. Its wire body is exactly23 bits in this order:

- Goal plus1, six bits; decoded int32 at+0.
- Reason plus1, nine bits; decoded int32 at+4.
- Sequence, eight bits; decoded byte at+8.

Thus goal28 and reason309 fit a real existing network schema. This discovery
alone does not prove that publishing those fields on a public-region group
reproduces the ending transition.

Original `17BD0F0` is the producer. On authority, a changed goal/reason updates
property+148/+14C and increments the sequence at+150, wrapping255 to1. Unchanged
values do not increment or republish. Authority check `178DAC0` accepts only
session states6..9. The registry0 constructor at `17A984C..17A9874` installs
permission words `{1,0}`; the native non-authority request check `178D4C0` does
not grant requests with permission0. Its alternate permission-enabled request
branch writes+154/+158/+15C and not the replicated value. Granting that permission
would change client authority and is not part of this migration.

Native getters `17B4A60` and `17B4A90` require the presence bit at+140 and a nonzero
sequence; absent values return-1. `17B4AC0` exposes the sequence. Bootflow reads
these native properties through `C26430(groupIndex)`, which resolves the group's
primary session and returns session+F4B8.

## Exact remaining authority boundary

The active ending adapter reads the primary fireteam selection through
`C03430()+18+primary*1C8A0`, equivalent to `C26430(0)` before its+F4B8 adjustment.
The effective transition is current-activity property7 at session+182C0, with
request+148 and native computed descriptor+378. `BFB470` independently proves
this lookup and calls `1779640` to return that effective descriptor.

Current `server/gameplay/group/group_host.cpp` admits and advertises held
public-region group sessions. Its activity-host parameter refers to the
committed public activity; current-activity is presently an empty reflection
delta. It has no authenticated fireteam-host admission/ownership transaction
for the ending's primary session. A packet addressed to another group cannot
be treated as authority for this fireteam property. The missing work is a
verified fireteam ownership and parameter-update consumer path, plus the full
native descriptor/classification and bootflow goal transition. It is not merely
a missing bit writer. No speculative parameter0 producer was added to the
public group host, and no unused production codec is claimed as a migration.

The retained ending adapter currently:

- Constructs Mercury activity29 and preserves the live fireteam nonce; calls
  `BF95D0`, `BFB1F0`, `BF97D0` to select/commit.
- Maintains the native effective no-ship classification and matching destination
  hashes during the transition. `C00650` compares those hashes when choosing
  loading presentation; the current replicated selection can refill them.
- Invokes `E2DEB0(28,309)` after selection. Its native path is `E2DBE0` validation
  then `E1B4D0`; moving this call onto a server worker would remain a client API
  mutation, not a network replacement.
- Arms the loading-cinematic suppression detour. Original `C24490` calls
  `E0DC10`; that function is exactly `xor al,al; ret` in this build. There is no
  flag or network value read on this path to replace the detour with a server
  property. A different authored/native loading path would need separate proof.

## Verification

Run `python Sunrise/unit/mission_transport_native_tests.py` from the repository.
It checks SHA256 before executing any original instructions:
`63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e`.

Result: **1,556 checks passed**. The test executes the original registered goal
writer and reader, including native bit-reader instructions, across boundary
values and sequences. It verifies native session authority, rejection with no
request permission, sequence wrap,400 duplicate publications, absent/zero
sequence getters, separation of peer requests from authority, and the original
cinematic suppression wrapper. Only publication/request notification callbacks
are stubbed; those stubs record the calls and do not mutate the property.

This is local original-code verification, not an end-to-end handoff test. No
live process or installed DLL was modified by this investigation.
