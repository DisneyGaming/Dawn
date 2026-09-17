# Mission development tools

Use `verify_lua.py` for current mission tests and DLL builds, `package_lua.py` to stage a verified Lua payload, and `install_candidate.ps1` to install it with rollback. See [mission scripts](../../Dawn/scripts/README.md) for commands. `verify.py` retains shared MSBuild/hash helpers and forwards direct invocations to the current runner.

Native package extraction and catalog generators remain separate. Their recovered data and reports can still use JSON; these files are not mission programs. Generators must not overwrite authored Lua or restore a compiled story sequence.

The old JSON comparison runners for Gateway ending/integration/response, mainland, region, return cue, traversal, and universal services have been retired, along with the dated Lua-ownership packager. They targeted fixed historical snapshots. Their source is preserved under `build/coo/mission-json-retirement-20260908/retired-tools.zip`; previous validation and installation archives remain intact. Remaining dated native-investigation runners still document their own original scope and accepted hashes.
