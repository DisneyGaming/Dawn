# Entrance BIRD: simulation entity-ID starvation

13 September 2026, build 86657. Diagnosis from a user-operated reproduction, read-only process sampling, local logs, and disassembly of the installed build. No game control, hooks, process writes, remote execution, or runtime configuration changes were used. The recorder has stopped; the game can be closed.

## Finding

The measured entrance launch consumes its 150 available simulation entity IDs during the initial object-creation burst. The available-ID mask reaches zero with one player record and 150 `sobject` records present. Native error 12 and the `sobject` creation failure occur together. Another 150 IDs become available approximately 22 milliseconds after that sampled failure, during cleanup.

This strongly identifies **entity-ID starvation before replenishment** as the immediate launch failure. It is substantially more specific than the original generic `sobject` log. It is not an instruction-by-instruction trace of the failing call, and a fix has not been implemented or validated.

The fixed simulation-record pool was only 151/1,024 occupied at the failure sample. Raising that pool's capacity is not supported by these observations. The client definitions and volumes recovered in [ENTRANCE-RECOVERY.md](ENTRANCE-RECOVERY.md) remain available independently of the launch failure.

## Reproduction and measurements

The user manually selected `raid_envy_v310`, bubble 2 / slice 16 / spawn `0x8BA80878`. The log identifies mission run 9, activity session `0x9EAA300100200012`, Destiny PID 65372. This follows two prior failures in the same slice: run 7 used `0x8BA80878`; run 8 used `0x811C9DC5`. Both earlier attempts failed in the same creation phase.

The recorder took 2,710 samples, with a nominal 20 ms delay plus read time. Saved state changes, in UTC:

- **12:52:35.594586:** one player record, 150 available IDs, no internal error.
- **12:52:39.731821:** 57 occupied records, 94 available IDs, no internal error. The bitmap changed while this sample was read; treat its individual records as transitional.
- **12:52:39.753230:** 151 occupied records: type 2 = one player, type 0 = 150 `sobject` records. Zero available IDs. Internal error 12. Bitmap stable around the record reads.
- **12:52:39.775085:** the same 151 records remain; 150 IDs are available again, but error 12 remains latched.
- **12:52:39.860622:** cleanup clears the records and error.

Corresponding native log timestamps:

- `t=4166750`: entrance slice 16 loaded successfully.
- `t=4166969`: transition enters `initial_slice_set_instantion`.
- `t=4167000`: first `failed to create 'sobject' entity`; prerequisite 35 becomes unavailable.
- `t=4167016`: launch enters cleanup; twelve additional `sobject` failures are logged.
- `t=4168297`: orbit slice loaded.

The failure precedes the later session-disconnect sequence. The user's BIRD report names the visible error; these observations establish the local failure path rather than a generic internet diagnosis.

## Native mechanism

All addresses below are relative to the main module and are specific to the checked build.

1. Generic creation wrapper `0x16EE180` calls allocator `0x170F190`. This wrapper handles several types, despite an older Sunrise diagnostic naming its return site after `player_broadcast`.
2. Allocator `0x1711D10` scans the manager's 8,192-bit available-ID mask at `+0xC118`. An empty mask leaves the requested handle at `-1`; no record construction can proceed.
3. The wrapper logs the entity's type and latches generic internal error **12** through `0x16CCD30`. The error can overwrite more specific failures, so the number alone is insufficient; the measured empty mask is the key evidence.
4. The separate fixed-record allocator `0x170B2B0` scans 1,024 bits at module `+0x30B0340`. Records are at `+0x30B0440`, stride `0x70`. Its full-pool error is 14. The observed occupancy was far below this capacity.
5. Native maintenance `0x171DB20` uses low/high watermarks at manager `+0xC518/+0xC51C`, requests replenishment below the low watermark, and returns excess IDs above the high watermark. Its target is the midpoint. A same-process post-cleanup snapshot reads **100/200**, which gives **150**. The initial reproduction did not sample these watermark fields, so do not describe their failure-time values as directly observed.
6. `0x171B1C0` assigns watermark profiles 400/600, 100/200, or 300/500 according to native role/environment checks. Do not change those role checks simply to select a larger profile.
7. The replenishment call `0x4F88C0` sends message type 20. Sunrise already parses this request and grants free IDs through its normal transactional ownership path. Its join path offers the available client share of the 8,192 IDs; current source does not contain a server-wide 150-ID hard cap. The available client cache and the server's full lease space are different quantities.

Relevant source:

- `Sunrise/src/server/bap/encrypted/activity_message/activity_message_route.cpp`: `prepare_join` and `prepare_grant`.
- `Sunrise/src/state/activity/entity_slots/transactions/activity_entity_slot_prepare.cpp`: chooses free IDs while preserving the server reserve and activity ownership.
- `Sunrise/src/middleware/bap/activity_message/activity_entity_slot_request_parser.cpp`: type-20 requested count.
- `Sunrise/src/client/hooks/bootflow/player_broadcast_create_probe.cpp`: existing layout references. This legacy diagnostic also changes allocation state and is not an appropriate read-only probe to enable.

## Next implementation step

Provide enough **valid, owned, client-visible entity IDs before the entrance instantiation burst**, and allow replenishment before creation can exhaust them. Preserve the existing server reserve, session identity, and lease transaction rules.

1. Trace the entrance's initial creation demand and the last maintenance/grant-merge boundary before instantiation. The captured run establishes demand beyond 150 `sobject` IDs; it does not count the full successful entrance population.
2. Choose the smallest startup change supported by that ordering: reserve and merge a bounded startup allowance at the appropriate lifecycle boundary, or defer/batch creation while a valid replenishment is pending. Merely sending a larger join grant may be ineffective because native maintenance returns surplus IDs before the burst.
3. Add bounded diagnostic receipts for requested/granted/merged ID counts and remaining IDs immediately before and after entrance instantiation. Existing svc8 success lines do not identify the complete refill transaction or its exact timing.
4. Validate the original entrance tuple: IDs remain available through the burst, no `sobject` failure occurs, prerequisite 35 completes, and the player reaches `activity:in_world` with control. Then check door/source availability and normal exit/reload cleanup.
5. Recheck one already-working reactor or Argos launch if the change touches shared allocation behavior. Keep arrival as the next raid milestone.

No runtime fix is included in this diagnosis. The precise safe startup allowance, the best grant/merge boundary, and whether additional entrance issues appear after this failure is removed remain open.

## Saved evidence and tool guarantees

- [Compact measurements and capture hash](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/entrance-id-starvation-evidence.json).
- [Full reproduction capture](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/simulation-20260913-085146.jsonl).
- [Post-cleanup watermark snapshot](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/simulation-20260913-085508.jsonl).
- [Read-only recorder](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/read_simulation.py).
- [Pointer decoder](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/decode_pointer.py): emulates getters in a private CPU sandbox using read-only memory copies. Emulator writes outside its own stack and execution outside the copied game image are rejected. It never calls code in the game process.
- [Offline disassembly helper](../../../build/coo/eater-live-inspection-20260913/entrance-bird-analysis/inspect_native.py), with adjacent saved function/range disassemblies.

The recorder opens the game with `QUERY_INFORMATION | VM_READ` only. It checks the packed executable SHA-256, local unpacked image SHA-256, and mapped code signatures. Reads are bounded; the process is not suspended. Bitmap stability does not make every record or cross-field observation atomic, and a sampled peak does not prove no shorter transient existed. Interpret the measurements together with the native call path.
