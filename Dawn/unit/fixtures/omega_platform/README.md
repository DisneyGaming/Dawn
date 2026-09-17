# Native platform direction evidence

Read-only runtime capture: build/omega-full-20260905/platform-live-38108-1788656704,
2026-09-05, installed build DA181324. The user reported no visible platform.

The source and gate had applied active authority. Device 80C22861 had current
and target position zero. Its position channel begins at +350. Its increasing
start reference at channel+68 resolves to 80BFD0FD (phase_in); its decreasing
start reference at channel+B8 resolves to 80BFD0FE (phase_out).

omega_platform_native_fixture.inl executes original DF1F70 instructions and
original float constants in a private test process with these captured links.
It records effect requests instead of executing renderer effects. Increasing
0 to 1 requests phase_in; decreasing 1 to 0 requests phase_out. Duration and
frame timing are modeled. This does not establish in-game collision or render
acceptance, but it disproves the reversed direction in imported CROWN-TRANSIT.md.

All seven authored bridge/runway/stair/railing source entities include device
80C22861 at entity definition offset E4 (package-read verification):
80F4AD97, 80F4ADB5, 80F4ADD3, 80F4ADF1, 80F4ACF1, 80F4AD42, 80F4AD29.
