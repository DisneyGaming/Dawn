# Equipment-derived character appearance encoding

## Comparison status

MISSING.md §6.2 specifically says appearanceValue is retained but not encoded. The inspected encoder still does not read that scalar. It nevertheless encodes substantial equipment-derived appearance. These are different claims.

## Shared record construction

Family 0 and family 3 share appearance construction while retaining different outer record layouts. The builder fills character SOID and race/gender/class, copies the expected header block, and initializes explicit empty sentinels.

It writes level and equipment light, builds render entries, populates available stat rows and ability buckets, then applies overflow hashes and perk banks. The inputs are resolved equipped instances, not a single appearance flag.

## Render entries

Each equipped instance targets its equipment slot; out-of-range slots are refused. The encoder writes the instance SOID and base-definition index, then resolves available item details.

Resolved entries receive gear-art and class-appropriate art-arrangement selections. Equipped plugs can alter art and material pairs. An item without an art block keeps its initialized empty sentinel rather than accidentally selecting valid art row zero.

Sentinel initialization also covers ability buckets, unused hashes, definition-index banks, keyed rows, and stat tables. This prevents an unfilled entry from being interpreted as meaningful zero-valued data where the layout has a distinct empty value.

## Publishing updates

The shared builder serves initial records and incremental refreshes. Equipment changes can rebuild appearance from candidate equipment state. Equipped socket changes can rebuild shader/perk-related content without replacing the roster list. The refresh transaction is described in 05-CHARACTER-ROSTER-REFRESHES.md.

## Limits

This does not prove every art arrangement, perk, header field, or blank-banner symptom is correct. Unknown fields remain unknown. The specific appearanceValue omission should stay on the comparison list.

These are family-0 banner and family-3 character records, not social family-2 objects.

Landmarks: Dawn/src/middleware/datagen/character_record/character_record_encoder.cpp and its appearance helpers, especially character_appearance_render.cpp, character_appearance_sentinels.cpp, and the stats/banks/abilities encoders. The shared builder and render path were inspected for these docs. No visual comparison or live game test was performed.
