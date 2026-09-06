# Registered Arc interfaces, captured 2026-09-05

Read-only capture from PID40148, installed build30CEFB66. User confirmed that
platform materialization worked and supplied an image carrying the charge
without an available deposit interaction. Full capture lives under
build/omega-full-20260905/arc-live-40148-1788657758.

Native source associations existed. PDB-backed read-only inspection confirmed
missionArcSources held the exact item/sink bindings and missionArcHolding was
false. The host phase was route, with no accepted pickup in the log.

The runtime entity descriptors prove that these two values were incorrectly
used as lookup interfaces: 80803E70 (carry declaration),80804FB0 (sink
declaration). Their type hashes are absent from the respective lookup tables.

- Shared item80F44F83 registers interface80803F6A. Lookup resolves the group
  owner plus1230 to component80F66667/80804221/+598.
- Sink entities80F44F89,80F44F8D,80F44F91 register interface80809658. Lookup
  resolves group owner plus20 to their respective6666E/66671/66673 controllers.
- The old declaration IDs are returned at reference+1C after a valid lookup;
  they are not the query interface IDs.

Tests execute original557470 and its original lookup/iterator/reference-copy
instructions against captured descriptor and metadata bytes in a private
process. Resolver storage, group linkage, and the stack-cookie boundary are
modeled. The original lookup fails for both old IDs and succeeds with the
registered interfaces. No game process is launched or modified by these tests.

This proves lookup and ownership joins, not a live portal crossing or dunk.
