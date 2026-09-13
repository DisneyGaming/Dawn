# Outer entrance: offline recovery complete

13 September 2026, build 86657. The entrance data can be read from the installed packages even though the live entrance launch fails. No further load or computer control was needed for this extraction.

## What was exported

The existing inventory already identified the five entrance client descriptors. This pass freshly decoded their installed sources, checked every descriptor against the inventory, and exported complete source-tag bytes with SHA-256 hashes:

- `seq_berth_music` — `0x80B49E7C`, offset `0x658`.
- `d_entry_door` — `0x80B49E83`, offset `0x278`.
- `m_engagement_sensor` — `0x8155C003`, offset `0x358`.
- `pm_entry_door` — `0x8155C006`, offset `0x218`.
- `pt_phase_berth_traversal` — `0x8155C00C`, offset `0x218`.

Both declared type-60 volumes were decoded with their full polygons and vertical bounds:

- `tv_entry_door`: source `0x8155C000`, registry `0xA9E6185F`, slot 4. The door monitor's serialized reference at `0x270` points to this exact identity.
- `tv_phase_berth_traversal`: source `0x8155C009`, registry `0xB270AC62`, slot 1.

The export includes the two placed groups, their typed descriptor-wrapper chains, scenario metadata, and the outer-entrance bubble entry/registry. There are **27 native tag files totaling 15,520 bytes**, plus the structured data index and hash manifest. This is a focused source-definition export, not a recursive export of all model and audio dependencies.

The groups declare seven slots: five client descriptors and two volumes. A volume lacking a client descriptor is not itself proof that its geometric data is missing. These two volumes are present in installed packages.

## What remains to fix

The previous live captures verified availability of 886/891 known client descriptors. The remaining five were **unverified live**, not absent from disk; this offline export does not change the live coverage number or claim successful gameplay.

The attempted entrance route was `raid_envy_v310`, bubble 2 / slice 16 / spawn `0x8BA80878`. The map slice loaded, then a native `sobject` creation failed and prerequisite 35 became unavailable. The user reported BIRD. The exact failed object and root cause are still unknown. Existing [failure evidence](../../../build/coo/eater-live-inspection-20260913/entrance-bird-failure/failure-evidence.json) is preserved.

Subsequent run-9 [read-only diagnosis](ENTRANCE-BIRD-DIAGNOSIS.md) strongly identifies startup entity-ID starvation: after 150 `sobject` creations, the available-ID mask is empty and error 12 is latched; the separate record pool is only 151/1,024 occupied. Another 150 IDs appear after failure. The next entrance task is to fix valid ID availability across the creation burst and verify a complete arrival. No runtime fix has been applied. The source data above does not supply missing server-side execution or prove door opening, music playback, encounter progression, or reset behavior.

## Files and reproduction

- [Entrance data index](../../../build/coo/eater-live-inspection-20260913/entrance-offline-recovery/entrance-data.json): all five definitions, seven declarations, volume geometry, wrapper references, and asset hashes.
- [Hash manifest](../../../build/coo/eater-live-inspection-20260913/entrance-offline-recovery/manifest.json): checks every exported tag and the index.
- [Extractor](tools/extract_outer_entrance.py): reads installed packages; does not open the game process or alter installed content.

From the workspace root, run `python -B docs/raids/eater-of-worlds/tools/extract_outer_entrance.py`. Add `--check` to re-extract and compare every saved byte without writing files.
