# Native plate pose migration

The Beyond Infinity, Deep Storage and Hijacked plate position commands now travel in the existing native type4 source authority. Their timer hooks only authenticate and report native pose/progress; they no longer call the local `DF6BD0` position setter. Native source creation and generation preparation remain unchanged. Capture completion still requires genuine native timer evidence.

## Exact-build evidence

Image: `destiny2_unpacked.bin`, mapped base `7FF618070000`, SHA256 `63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e`. All addresses below are RVAs. The native test verifies this hash before executing original instructions in isolated emulated memory.

The generic device dynamic record is `80805063`. Global `20768E0` resolves metadata at `3257BB0`; its reflection descriptor is `3909B08`, with nine field records beginning at `3909B88`, stride40. The payload is36 bytes: power revision/snap revision/value, lock revision/snap revision/value, then position revision/snap revision/value. Integer fields are signed32 with reflected bias `80000000`; floats are raw IEEE754. Its present-record encoding is321 bits. This differs from the type23 world-device channel layout.

The registered callback table at `1D07180` points to consumer `DF6510`; `1D07190` points to producer `DF4020`. The producer reads power from `+950/+954/+AC`, lock from `+958/+95C/+6AC`, and position from `+960/+964/+37C`. It searches the existing typed dynamic list and respects its three-record maximum (`DF410F`). The server adds this record after capture, leaving room for the separately verified interaction record when needed. Active plate sources are961 bits:252 source,388 capture,321 pose. Inactive preparation remains252 bits with zero dynamic records.

Consumer `DF6510` searches the same schema and retains the native entity authority gate at bitset `26BE0E0`. The previous local `DF6BD0` setter has the identical gate. At `DF666A..DF66A8`, the consumer compares signed snap revision against `+964`, then signed position revision against `+960`. Only a strictly newer position revision invokes the native underlying setter `DF6C70`; the new snap revision selects its snap flag. `DF6D32` stores target `+37C`, and snap stores current `+370` at `DF6D3F`. Power and lock revisions stay absent (`-1`), preserving those authored channels. Repeated same-revision packets do not invoke the position setter.

Native timer `1006F20` dispatches its start/stop action only when active `+30` differs from prior active `+78` (`1006F3D..1007000`). Its completion action is gated by latch `+79` (`10071DD..1007258`). The ordinary per-frame path updates progress and remaining time. This establishes edge-triggered timer actions, rather than an action resetting the plate every frame.

Recovered evidence copies are under `build/unit/native-pose-migration/evidence`: producer/consumer disassembly, legacy/underlying setters, timer edges, reflection/callback bytes, and image identity. These generated files are supplementary; the pinned native test is repository source.

## Server revision and feedback behavior

`mission_device_pose_service.h` owns desired position and independent signed32 position/snap revision counters. Desired-position changes advance once. Authenticated native observations retain both native high-water marks. A drifted current or target pose advances authority only after native position revision has caught up with the pending command. Repeated old observations do not keep increasing revisions. Native acknowledgement of a matching pose records high water without another command. Exhaustion at `INT32_MAX` fails closed without signed wrap. Capture timer start/stop preserves this independent pose history.

The original timer hook samples current/target position and both revision counters before and after the original native tick. Source generation, committed generation, source definition, salted entity identity, component definitions and handles are validated again after sampling. Runtime intake requires the complete bound plate receipt and current mission lifecycle. A fixed latest-sample inbox per plate deduplicates repeated frame samples and rejects older native revisions. Server snapshot consumption revalidates receipt ownership, so a retired mission/source or recycled component cannot change the next owner's publication.

No additional source generation is issued for pose drift. Deep Storage and Hijacked continue their100ms publication cadence for enabled completed frames so retained charged plates can still receive corrections. An unrelated native graph pose change may remain visible until the next authority publication; no claim of instantaneous renderer equivalence is made.

## Validation

`native_device_pose_tests.vcxproj` builds with v145, C++20, `/W4 /WX`. Its10033 checks pass, covering initial/high authored revisions, pending/replayed/acknowledged commands, current/target drift, desired changes, stale observations, nonfinite data, signed exhaustion/reset, inbox deduplication, capture/pose independence, reflected field bytes, combined active width and empty preparation.

`native_device_pose_native_tests.py` verifies the pinned image then runs original `DF4020` through payload production and original `DF6510` through return in Unicorn. It verifies distinct producer field values, position-only acceptance,1000 repeated records, stale/high revisions, native authority gate, and independent snap-revision handling. Engine position/power/lock setters and dirty callbacks are isolated observable stubs; this test does not execute rendering, effect spawning, multiplayer transport, or a live mission. No live process or installed DLL is changed by the test.

The three mission route suites additionally reject pose receipts from another run, source generation/address, entity salt, device or timer before accepting the exact owner. Root validation also covers the updated native preparation fixture, full mission routes/catalogs, and production build.

## Model/VFX adapters retained

Dendron's shield presentation still owns only model pass7's base contribution and publishes through `1150420`; native visibility-reference bits and other passes remain owned by the engine. Its blue shell separately uses parent animation input `0958590C` through `A0FE60`. Existing type23 shield aliases in the strike catalog are platform/cube devices and do not establish an authority route for these model/material contributions. No verified wire producer/consumer was found for selectively replacing them. `strike_bond_boss_shield.inl` remains a behavioral adapter, including its terminal ownership rules.

The optional active `omega_ikora_origin_probe.cpp` carrier-model suppression uses carrier `80EC0FA8` for wanted effects and skips only body/cloth/head model builders when the corresponding experiment setting is enabled. No equivalent native authority route was established, so that behavior remains.

Correction from the subsequent compiled-path audit: `activity_spawner_chain_probe.cpp` contains older scene2 persistence and+10Z purple-effect edits, but `kEnableActivitySpawnerChainProbe=false` prevents their installation. They are not active remaining behavior. The current Ikora effect-transform callback is read-only. See `NATIVE-GARDEN-PLATFORM-MIGRATION.md` for the precise active-path audit and retained model/cleanup adapters.
