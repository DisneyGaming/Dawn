# Authored hoop crossing detector

Read-only inspection of the loaded underbelly hoop objects resolves the narrow
passage detector shared by all seven hoops. The type-4 object uses entity
definition `80F42AFA`. Its `81558054 / 80FEB6ED` trigger component and
`80F42AF2 / 80808A0C` physics component point to the same native rigid body.

The body owns an `hkpCylinderShape`. Its local endpoints are `(0,0,0)` and
`(0,0,5)`, its radius is `5`, and every observed body transform has identity
rotation. The exact seven body origins and derived aperture centers are recorded
in [hoop-crossing-native-proof.json](evidence/hoop-crossing-native-proof.json).

Progress requires an authenticated player-position segment to cross a hoop's
midplane in either direction with its interpolated point inside radius `5`.
Touching the cylinder's side or moving only within its depth does not establish
passage. The caller separately rejects samples more than 250 ms apart or more
than 80 world units apart. This prevents a load, teleport, or broad arena-volume
receipt from substituting for travel through an authored hoop aperture.
