# Rebuilding the evidence

For the focused outer-entrance source export, run `python -B docs/raids/eater-of-worlds/tools/extract_outer_entrance.py`. It exports five checked client definitions, two volume geometries, and the typed wrapper/group/bubble source tags to `build/coo/eater-live-inspection-20260913/entrance-offline-recovery/`. Add `--check` to compare a fresh extraction to every saved byte and its manifest without writing. This does not require a live game session and does not claim a working entrance launch.

Run from the workspace root:

```powershell
python -B docs\raids\eater-of-worlds\tools\extract_eater_of_worlds.py
```

The script reads the installed package set, the version-54 Dawn cache, the installed metadata-bank fixture, and the supplied DECOMP_SHARE corpus. It regenerates `evidence/native-inventory.json` and validates:

- cache magic/version/domain sizing and the Eater scenario class
- scenario, slice-entry, registry, and placed-object classes
- all three `raid_envy_v310` public-activity identities and their localized display rows
- self-relative arrays and slot descriptor predicates
- descriptor registry keys and slot indices without renumbering
- squad source entity references and tactical row structure
- objective and dialogue string-container classes
- exactly six `envy`-linked behavior roots
- installed behavior class and SHA-256 against both archive raw bytes and manifest rows

The tool imports `tools/coo/package_read.py`. That reader derives package key material from the pinned owned image in memory; it does not print or persist it. The extractor writes only the JSON path selected by `--output`.

The cache layout is pinned to version 54. If the cache producer changes, rebuild `tools/coo/cache_layout.vcxproj`, update the constants after reviewing the emitted JSON, and keep the version assertion. Do not silently parse a new cache with old offsets.
