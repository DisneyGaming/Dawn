# Eater of Worlds archive

Eater of Worlds is excluded from this Dawn build. Its raid launcher, Contest mode,
mission runtime, native hooks, roster publication, and Lua script are not part of
the shipping project.

The `Dawn`, `tools`, and `docs` folders preserve the dedicated files under their
original repository paths. `moved-files.txt` lists those paths. Shared files remain
in the main project with only their Eater integrations removed.

`restore-integrations.patch` preserves those shared integrations, including their
tests. It is a reference for a future reintegration, not a build input. Restoring
the raid requires moving the archived files back, reviewing and applying that
patch against the current project, and rebuilding and testing the raid.

The archived test projects and tools retain their original relative paths and are
not standalone projects in this folder. The installer deploys only the active
`Dawn/scripts` directory and the built DLL; it does not deploy this archive.

The original raid notes start at [the raid documentation](docs/raids/eater-of-worlds/README.md).
