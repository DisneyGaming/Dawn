#pragma once

#include "omega_presentation_rules.h"

namespace sunrise::state::activity::omega_lair_chase_geometry {

using omega_presentation::Point;
using omega_presentation::Volume;
using omega_presentation::Landmark;

/** Exact authored approach footprints: first/second/final island (A/B/C).
 * These are local type60 volumes referenced by native type31 proximity triggers,
 * not type30 player-monitor packets or a grounded-state test. Preserve the
 * triangle footprints: their enclosing rectangles include unrelated airspace.
 * Landmark is an inert Volume field here; callers must use contains and their
 * own encounter stage, never dispatch these records as presentation landmarks.
 * Source: omega_chase_geometry_verification.json, fresh package80F47913 and
 * original4A55A0 containment verification. */
inline constexpr std::array<Volume,3> kChaseArrivals{{
    // tv_first_island; native proximity31/27; 0x80F47913+D90
    {Landmark::lair, 0xF4D0E0B2U, 57,
     {-1662.5F, 203.0F, -71.22622680664062F}, {-1313.5F, 343.0F, 28.773771286010742F},
     {{{-1449.797119140625F, 271.6833801269531F, -71.22622680664062F}, {-1406.9075927734375F, 304.38873291015625F, -71.22622680664062F}, {-1313.5F, 315.5F, -71.22622680664062F}, {-1315.5F, 343.0F, -71.22622680664062F}, {-1418.923828125F, 321.9733581542969F, -71.22622680664062F}, {-1465.4468994140625F, 288.40679931640625F, -71.22622680664062F}, {-1485.5F, 249.0F, -71.22622680664062F}, {-1660.5F, 239.0F, -71.22622680664062F}, {-1662.5F, 203.0F, -71.22622680664062F}, {-1472.0F, 226.5F, -71.22622680664062F}}},
     {{{7, 8, 9}, {1, 2, 3}, {1, 3, 4}, {1, 4, 5}, {0, 1, 5}, {0, 5, 6}, {9, 0, 6}, {9, 6, 7}}}, 8},
    // tv_second_island; native proximity31/28; 0x80F47913+FD0
    {Landmark::lair, 0xF4D0E0B2U, 59,
     {-1661.5F, 97.00003814697266F, -86.95279693603516F}, {-1319.5F, 232.0F, 13.047203063964844F},
     {{{-1661.5F, 199.5F, -86.95279693603516F}, {-1515.0F, 98.50003051757812F, -86.95279693603516F}, {-1319.5F, 97.00003814697266F, -86.95279693603516F}, {-1485.0F, 163.0F, -86.95279693603516F}, {-1515.0355224609375F, 232.0F, -86.95279693603516F}}},
     {{{1, 2, 3}, {0, 1, 3}, {4, 0, 3}}}, 3},
    // tv_final_island; native proximity31/29; 0x80F47913+1210
    {Landmark::lair, 0xF4D0E0B2U, 61,
     {-1506.4761962890625F, 62.82191467285156F, -74.1588363647461F}, {-1465.440673828125F, 138.10488891601562F, -14.158836364746094F},
     {{{-1465.440673828125F, 137.3015899658203F, -74.1588363647461F}, {-1501.3890380859375F, 138.10488891601562F, -74.1588363647461F}, {-1506.4761962890625F, 65.26434326171875F, -74.1588363647461F}, {-1467.1727294921875F, 62.82191467285156F, -74.1588363647461F}}},
     {{{3, 0, 1}, {3, 1, 2}}}, 2}
}};

/** Authored Crown platform footprint, pt_boss_platform31/24 -> tv_boss_platform60/55.
 * Mapping arrival to encounter/no-respawn activation is reconstructed host policy;
 * no original host producer is claimed. This is distinct from the first-chase
 * dialogue volume currently labeled Landmark::arena in the presentation layer. */
inline constexpr Volume kCrownArrivalVolume =
    // tv_boss_platform; native proximity31/24; 0x80F47913+B50
    {Landmark::lair, 0xF4D0E0B2U, 55,
     {-1583.8660888671875F, -200.50302124023438F, -73.131591796875F}, {-1376.7392578125F, -19.352922439575195F, -33.131587982177734F},
     {{{-1583.8660888671875F, -130.78353881835938F, -73.131591796875F}, {-1555.182373046875F, -199.7836151123047F, -73.131591796875F}, {-1443.4632568359375F, -200.50302124023438F, -73.131591796875F}, {-1376.7392578125F, -123.24711608886719F, -73.131591796875F}, {-1458.122314453125F, -19.352922439575195F, -73.131591796875F}, {-1515.490478515625F, -19.363351821899414F, -73.131591796875F}}},
     {{{2, 3, 4}, {2, 4, 5}, {2, 5, 0}, {2, 0, 1}}}, 4};

/** Original AB0750 path milestones0..3: initial->A, A->B, B->C, C->Crown.
 * Path95FB2E01/48/55 (aps_teleport), curve80F47562. Coordinates are native;
 * applying this milestone order after each area clears is reconstructed policy.
 * sr_boss_location_2 is already in the Crown, not the second chase island. */
inline constexpr std::array<Point,4> kBossDepartures{{
    {-1425.054931640625F, 169.7905731201172F, -43.593299865722656F},
    {-1596.3707275390625F, 120.1434555053711F, -62.594085693359375F},
    {-1486.6104736328125F, 11.791311264038086F, -73.3829116821289F},
    {-1489.560791015625F, -165.89990234375F, -62.49308395385742F}
}};

} // namespace sunrise::state::activity::omega_lair_chase_geometry
