# Hijacked native movement feasibility

Status: the existing movement adapter remains. This check made no production changes.

The active path is compiled `omega_lair_cinematic.cpp` (project ClCompile entry), which includes `hijacked_boss_native.inl`, calls `observe()` from its full-body callback and calls `dispatch_pending()` from the damage callback. The adapter still sends opcode45 through the original animation request entry. Its full actor/source/component checks and actual selector/position arrival receipts remain necessary. This is not an unused helper and is not counted as migrated.

## A native member exists

Installed definition80B42323 contains the authored boss member153E22CD/type2/22 at component offset2904, native component8080834E, sense80807DA2 and authority80807DA1. Its parent reference is153E22CD/type1/21. Therefore absence of a server member codec is not the blocker: the existing native .6 program receiver can consume a kind9 named movement program.

That receiver does not accept this mission's authored destinations. The native kind9 producer uses A0F0E0 -> AB4030 -> A97800. A97800 calls4FFEC0 for its scoped target, then evaluates the returned path through AB0C30 and AAF990. Original4FFEC0 checks target type48 at4FFEE6..4FFEED and type58 at4FFEF3..4FFEF6; every other type returns an invalid handle. The exact type48 comparison instruction is at4FFEEA and its branch at4FFEED. Type48 reaches registry lookup4EA140 with selector1; type58 reaches it with selector0 and follows the path wrapper. A97800 does not validate this failed result before dereferencing its path handle atA978AE..A978FA.

Installed locator80B423BD+270 references153E22CD/type47/54, and80B423C3+270 references153E22CD/type47/57. Their point positions are in80B421AA rows270 and2A0. These are actual point references, not path milestones. The final-stage target is an authored spawn position. The current 299-asset mission catalog has no type48 path. Its only type58 entries are the unrelated exterior Fallen dropship entry/exit in registry3E9B74F3 slots101/102. Replacing a reference's type byte does not create a registered path and would make the unchecked path reader unsafe.

The existing server source authority already selects the boss's tactical task by stage in `hijacked/authority.h`. That assignment does not establish an equivalent producer for this explicit sequence6 teleport and its destination. Adding a type2-owned spawn request would also create another actor rather than safely attaching this command to the current loose-source actor. Any later migration needs both a proven target-producing authority contract and native member binding that retains the existing actor, readiness, retirement, and .6 acknowledgement. An unrelated dropship path or a synthetic arrival is not an equivalent replacement.

## Reproducible evidence

Run `python Sunrise/unit/fixtures/verify_hijacked_movement_target.py` from the repository root. It pins the original image SHA256 to63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e and executes original4FFEC0 and4E2990 in Unicorn.

The test passed277 native resolver cases: all256 type bytes, absent references, a successful type48 lookup, and type47 points under different registry/slot identities. Only types48 and58 reach the fixture's registry boundary. Type47 always returnsFFFFFFFF without a registry lookup. The test also checks the installed member/locator/point definitions, their source identities, all299 catalog assets, and package hashes. Output is `build/coo/hijacked-research/native-target-feasibility.json`.

The registry lookup itself is a fixture boundary. This proves that the recovered kind9 path command cannot consume Hijacked's authored point targets. It does not claim that every other possible native movement protocol has been ruled out. No game process was accessed, no movement was issued, and no arrival was synthesized.

