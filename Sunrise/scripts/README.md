# Mission scripts

`omega.json` uses format 2 and the registered `omega.archive.v1` native profile. It is loaded through the generic mission compiler in `coo/mission_script.cpp`. The Omega integration checks its native phase requirements before publication.

## Edit and validate

1. Edit `Sunrise/scripts/omega.json` beside the installed DLL.
2. From the game root, run `build/coo/validation-generic/coo_script_tests-local/Release/coo_script_tests.exe --validate Sunrise/scripts/omega.json`.
3. Restart Destiny. Omega reads the file once per process, resolving its path from the DLL directory. Death and mission restarts retain the same immutable definition.
4. Look for `ev=coo_script mission=omega result=loaded format=2` and the file fingerprint in `Sunrise/logs/sunrise.log`.

Supported edits do not need a DLL rebuild. A failed load blocks executor publication and logs its cause; it does not automatically select the compiled legacy mission. The validator checks references and native contracts, not gameplay completion.

Format 1 belongs to the preceding accepted DLL. When rolling back this migration, restore both the accepted DLL and its `previous-omega.json` backup (to `Sunrise/scripts/omega.json`). Keep the existing settings backup with them. The installer records both script hashes.

See `FORMAT.md` for the common format and authoring boundaries. New native capabilities or a new game's wire schema still require a verified C++ profile and native integration.
