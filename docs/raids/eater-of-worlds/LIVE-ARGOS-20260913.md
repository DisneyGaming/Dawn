# Barrier and Argos: live area capture

13 September 2026, build 86657. **Initial area capture complete.** The user can leave the area; further analysis can use the saved evidence. The user operates all computer controls.

## Arrival and location

The log confirms successful arrival in `raid_envy_v310`, mission run **6**, session **`0x9EAA30010020000C`**. The selected destination was scenario-local bubble **6**, slice **48**, spawn **`0x68C397B7`**. The authoritative arrival log reports region 48 and that spawn hash. The bubble's map-global index is 25.

The read-only published player position was **(51.596874, -1392.526611, -1922.196899)**. This is inside all four authored volumes:

- Argos registry `0xE8D290A0`: `tv_quarantine_breakout`, slot 379, and `tv_float_away_area`, slot 380; source `0x8155C06C`.
- Barrier registry `0x91264981`: `tv_quarantine_cooking`, slot 319; source `0x8155C1D7`.
- Belly phase registry `0xE34F861D`: `tv_phase_belly_traversal`, slot 4; source `0x8155C303`.

The position is about 127.28 world units from the nearest authored point in the selected spawn set. The point was sampled after arrival and is not presented as the exact spawn transform. Polygon containment and the arrival log together confirm the intended barrier/Argos area.

## Content coverage

The live reader checked the 891 descriptor source tags in the installed-content inventory. It verified **688 readable authored descriptors** in this area, including:

- **335 of 335 Argos descriptors**, registry `0xE8D290A0`.
- **301 of 301 barrier descriptors**, registry `0x91264981`.
- **22 of 22 belly-traversal descriptors**, registry `0x93BF5E9D`.
- 20 shared/global descriptors and 10 other phase or area descriptors.

Each verified record passed source-tag, component-class, registry/type/slot, and repeated-header checks. The other 203 source tags did not pass those checks and remain unverified in this snapshot. That does not prove asset absence.

The arena adds 665 descriptor identities beyond the reactor capture and shares 23 with it. The combined coverage is **886 of the 891 known client descriptors**. No separate traversal-area load is needed to check availability of the 22 recovered belly-traversal descriptors.

The five identities not verified in either area belong to the outer Berth/entrance:

- `seq_berth_music`: registry `0xA9E6185F`, type 5 / slot 0.
- `d_entry_door`: the same registry, type 23 / slot 1.
- `m_engagement_sensor`: the same registry, type 70 / slot 2.
- `pm_entry_door`: the same registry, type 30 / slot 3.
- `pt_phase_berth_traversal`: registry `0xB270AC62`, type 31 / slot 0.

These five remain available in the offline package inventory. Their live availability can be checked when implementing the original entrance; this capture does not require another immediate load.

Later entrance test: bubble 2 / slice 16 / spawn `0x8BA80878` loaded its map slice but failed during native `sobject` creation, followed by prerequisite 35 becoming unavailable and launch cleanup. The user reported BIRD. The exact failed object is not in the current log; this tuple must not be described as a verified working entrance. [Failure evidence](../../../build/coo/eater-live-inspection-20260913/entrance-bird-failure/failure-evidence.json). The five entrance descriptors remain unverified live, and the reactor/arena captures are unaffected.

## Entity census and limits

The native allocator had **655 allocated entity slots**. The bounded reader completed header/component checks for 641 and recorded 14 unverified rows. The allocation bitmap and table pointers remained stable across the scan. A stable bitmap does not make every component value an atomic whole-world snapshot.

No component in the verified entity rows matched the inventory's authored descriptor source tags. This does not establish that Argos or encounter sensors are absent: the reader does not enumerate non-entity activity sensor components.

The capture proves authored descriptor availability and spatial identity. It does **not** recover missing host-only scheduling or prove instantiated enemies, working craniums, shield transitions, boss damage windows, detainment, wipe/reset behavior, or raid completion. Those require bindings and active-state tests during encounter-by-encounter implementation. The combined 886/891 figure applies only to known client descriptors, not to the percentage of raid gameplay reconstructed.

## Saved evidence

All files are under `build/coo/eater-live-inspection-20260913/capture-03-argos/`:

- [Capture summary](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/capture-summary.json): arrival, counts, comparison, missing identities, volume joins, and limitations.
- [Native snapshot](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/native-area-snapshot.json): verified descriptor headers and entity census.
- [Player position](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/player-position.json): process, DLL identity, symbols, timestamp, and sequence-checked coordinates.
- [Geometry](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/reactor-geometry.json): the shared recovery tool's output, including all recovered volumes and map-global-25 spawn points. Its legacy filename does not restrict this file to the reactor.
- [Launch/session logs](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/launch-and-session-log.txt): relevant lines with payload dumps excluded.
- [SHA-256 manifest](../../../build/coo/eater-live-inspection-20260913/capture-03-argos/manifest.json): hashes for the six evidence files.

No UI control, memory writes, game calls, installed-binary changes, or runtime configuration changes were used for this capture.
