# Mercury native callback audit

The Mercury host publishes native population, placement, engagement,
participant, interaction, and named-point authority through the existing BAP
encoder. The client has no registered plugin callback surface for the related
engine events. The remaining observers therefore bracket unchanged native
callbacks and send value-only receipts to the embedded server. They do not
replace native movement, combat, interaction predicates, or object creation.

This audit distinguishes an authority command sent by the executor from proof
that the client consumed it. A command cannot acknowledge itself. Counts,
elapsed time, or the server's requested state cannot substitute for a native
actor birth, death, retirement, placed object, successful use, or participant
application.

## Result by hook group

1. **Enemy admission, death candidate, and retirement: retained.** The host can
   request an exact source generation, but its source sense only reports the
   generation and consumed request count. It does not report each salted actor
   identity, health death event, or allocator generation advance. The native
   callbacks at `A0D510`, `C72390`, and `A85540` remain the only proven route to
   exact admission, real death, and retirement receipts. Removing any of them
   would force inferred births/deaths or make the 30-second patrol renewal and
   faction-war clearance unsafe.

2. **Named-point create, interface, remove, and release: retained.** The server
   publishes the installed named-point list, while the shared `575690` creation
   owner and observers at `4E25D0`, `569D10`, and `34F790` prove the authored
   point was created, its interface resolved, and its salted allocation really
   retired. No native upstream message carries those four facts. The executor
   can request the list but cannot prove local construction or release.

3. **Vance contact job and consume diagnostics: removed from normal boot.** The
   `A2B010` and `D69E90` observers only produced bounded `mutation=none` contact
   logs. No server or Mercury controller consumes them. Normal boot now leaves
   both targets untouched. The code remains behind the compile-time
   `kEnableVanceContactDiagnostics` switch for a dedicated diagnostic build.

4. **Public-event placement and engagement application: retained.** The narrow
   receipt owner at `9F19F0` proves the local type-4 object, source, selector,
   and entity after native application. The local rally mode has no usable
   source sense receipt. `9F1820` proves type-70 authority application; the
   server separately consumes the native engagement sense message. Readiness
   deliberately requires both application and sense, so the sense message does
   not replace the application receipt. The broad legacy schema probe remains
   compile-time disabled.

5. **Carry, dunk, create, and interaction: retained.** The executor already
   publishes the native interaction enable command. The unchanged engine still
   owns range, angle, line-of-sight, input, carry attachment, consumption, and
   object creation. The observers at `D99620`, `F36640`, `9EFFC0`, and `F32CD0`
   prove those local outcomes and are shared by other missions. No authenticated
   upstream message preserves their exact item, holder, sink, player, and
   consumed counter identities.

6. **Participant application: retained.** `BF5AA0` proves the exact type-71
   body was applied to the current native component and that its local-player
   bit became active. The native identity helpers also supply the identifier
   encoding expected by that body. Server membership identifies the session,
   but it does not prove this local component application or supply an
   independently verified matching native encoding.

The safe reduction is therefore two interceptors: the Vance job and consume
diagnostics. The other groups already use the narrowest proven native callback
or upstream sense boundary available in this installed build. Moving their
state machines into the executor would not remove the need for a native receipt
and would weaken lifecycle evidence.

## Executor polling assessment

The existing game-frame `poll_native_population_admissions()` boundary could
eventually poll retained actor leases for retirement. The server update thread
is not the appropriate place to read native actor memory. The current admission
ledger retains actor identity and ownership receipts, but no actor
`Weak{serial, handle}`. The generic weak helper at `351C90` is verified for
components; its use with actor handles still needs direct evidence.

`Read::weak()` currently returns only a boolean. A replacement must distinguish
a successfully observed serial change from an unreadable allocation table:
serial change proves the old lifetime ended, while an unreadable table requires
a retry. It also needs bounded scanning outside the pending-queue lock, mailbox
backpressure, owner-release and handle-reuse checks, and parity for every activity
that shares retirement receipts. Consequently `A85540` remains attached in this
candidate; treating failed reads as retirement would create false clearances.

Reproduce the source-ownership audit from the repository root:

```powershell
python tools/testing/mercury_native_hook_audit_tests.py
```

A clean client restart is required before an installed DLL can demonstrate that
normal boot no longer attaches `A2B010` or `D69E90`.
