# Player release installer

This installer consumes an extracted prebuilt release. It does not build code or
use the development installer in the parent directory. It supports Windows
PowerShell 5.1 and newer.

Publisher workflow, from the repository root:

```powershell
.\tools\install\New-DawnRelease.ps1 -Release '0.1.0-test1'
```

The default input is `build/x64/Release/steam_api64.dll`. `-DllPath` and
`-OutputDirectory` can override those paths. Use a DLL built and validated with
the current content. The tool creates a new directory and ZIP, refuses to
overwrite an existing release, and prints the ZIP hash. Send players the ZIP.
They extract it and double-click `Install-Dawn.cmd`.

## Replacement and migration contract

- Check the executable's exact file version, `86657.20.08.23.1800.d2_rc`.
- Validate every payload file against its required SHA-256 manifest; reject
  unknown paths, extra files, traversal, and links/junctions.
- Resolve one source runtime. Prefer the runtime named by the installed root
  DLL's product metadata; otherwise accept a unique candidate. Ambiguous
  installs need `-SourceRuntime`. Do not merge different player databases.
- Support settings layout 6 (the current runtime layout). Refuse older, newer,
  or malformed settings before changing installed files. Database schema
  upgrades remain the game runtime's responsibility.
- Carry the selected SQLite database and its WAL/SHM/journal files together,
  opaque device identity, personal configuration, and event choices. Preserve
  the old JSON account seed when no database exists.
- Merge missing configuration keys. Update shipped arrival overrides by
  package name; retain unrelated custom overrides and other preferences.
- Stage and verify the complete release before replacing anything. Install
  identical runtime content/data at root `Dawn` and `bin/x64/Dawn`, and the
  Release DLL at both corresponding locations.
- Replace whole Dawn runtime folders. This removes stale scripts and presets.
  Unknown prior Dawn files, caches, and logs remain in the full backup; caches
  regenerate on first boot. Legacy Sunrise/Restoration directories stay intact.
- Record every replacement in a durable journal under `.dawn/release-backups`.
  Failed replacements roll back. An interrupted run must be restored before
  the next installation. `-Restore` works without a payload or a network.
- A rollback also preserves displaced current files under `after-restore`,
  including player progress created after the installation.
- Refuse running Destiny processes and runtime folders containing source
  checkouts. Never stop the game, launch it, or change execution policy globally.

`-WhatIf` validates the package, source settings, and installation and prints
the intended change without modifying game files. `-Restore -BackupPath ...`
selects a particular backup beneath the chosen game root.

Manifest hashing detects missing or altered payload files. It is not publisher
authentication; distribute the installer and bundle through a trusted channel.
This script does not make an untested DLL or mission release ready for public use.
