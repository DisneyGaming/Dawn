# Mercury free-roam population and faction war

This implementation uses native Mercury population registries, source slots,
spawn rules and tactical groups recovered from the installed package data. It
does not assign invented native identities. The host policy that selects one
variant per area, renews patrols after 30 seconds, schedules the event and
escalates its four waves is reconstructed because the retail Mercury controller
was not recovered.

## Runtime policy

- A fresh Mercury instance samples the client computer's local minute once.
  The war is eligible during the first three minutes of each quarter hour:
  `:00:00 <= time < :03:00`, `:15:00 <= time < :18:00`,
  `:30:00 <= time < :33:00` and `:45:00 <= time < :48:00`. A started war
  continues after that window closes and cannot start a second time in the
  same instance.
- All 16 selected combat groups are patrol sources. Eight ordinary placements
  request three authored groups per generation. The eight larger warfront
  placements request four. The active wave temporarily owns its two sources so
  patrol renewal does not compete with wave progression.
- Four wavefronts use recovered Cabal and Vex placements in cannon-middle,
  crater, forest and pond, adding 2, 3, 4 and 5 native source requests per side.
  One request can admit a package-authored group of several actors.
- Configured wave additions remain 2, 3, 4 and 5. The largest cumulative source
  target is therefore 9: four large-area patrol requests plus the final wave's
  five additions. Installed source rows contain at most three authored
  combatant choices per request. Counting every choice as an admission bounds a
  generation at 27 actors within its 64-actor ledger. The 56 initial requests
  plus the largest active wave's 10 requests bound the whole runtime to 198
  simultaneous provisional births within the 256-entry observer. A conservative
  admission/death/retirement burst bounds to 594 mailbox events within its
  1024-entry storage. Configuration validation applies both aggregate bounds as
  well as each source's ledger bound, including when counts are edited.
- A patrol's 30-second timer starts only after the native source reports that
  its request quota was consumed and every admitted actor has both died and
  produced a retirement receipt. Queued, provisional or still-attaching native
  actors delay both the timer and renewal. The bridge atomically changes the
  source lease before the server publishes the new generation and restores its
  configured patrol target of three or four. Renewal does not manufacture a
  death, a retirement, a source-stop receipt or an admission count.
- The package string table contains incident `0xA2DD920A`, “The enemy is moving
  against each other...”. No safe native presentation dispatcher was recovered.
  Gameplay therefore starts authoritatively and reports presentation as
  unavailable instead of blocking or simulating receipt of that announcement.

## Placement coverage

The catalog contains 37 reachable placement groups. Sixteen are selected, one
for each distinct faction/area placement used by this policy. The other 21 are
alternate regular or hotspot layouts whose transforms overlap a selected
group. Selecting both layouts would stack native actors in the same area.

The active entries use their recovered fallback rule and source slot 1 where a
second source exists. The one-source Vex center-left group uses slot 0 with its
fallback rule. This avoids the primary-rule named-point dependency demonstrated
by the retained Cabal diagnostic probe. The Vex diagnostic shares its exact
center-left capability with the patrol; the distinct Cabal primary-rule probe
remains count-zero and outside the recurring policy.

| Native group | Registry | Policy | Reason |
| --- | ---: | --- | --- |
| `pf_lighthouse_ca_blocks_right_a` | `B3CBA385` | active patrol | Cabal blocks-right coverage |
| `pf_lighthouse_ca_blocks_right_a_hotspot` | `11A229B9` | alternate | overlaps active blocks-right group |
| `pf_lighthouse_ca_cannon_forest_a` | `2571C34D` | active patrol | Cabal cannon/forest coverage |
| `pf_lighthouse_ca_cannon_forest_b` | `2571C34E` | alternate | second cannon/forest layout |
| `pf_lighthouse_ca_cannon_mid_a` | `CF4C43A4` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_ca_cannon_mid_a_hotspot` | `90EFDE28` | active patrol + war | Cabal cannon-middle side |
| `pf_lighthouse_ca_cannon_mid_b` | `CF4C43A7` | alternate | second cannon-middle layout |
| `pf_lighthouse_ca_cannon_tower_a` | `9D083869` | active patrol | Cabal cannon/tower coverage |
| `pf_lighthouse_ca_cannon_tower_a_hotspot` | `DC79C5E5` | alternate | overlaps active cannon/tower group |
| `pf_lighthouse_ca_crater_cannon_a` | `0775116F` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_ca_crater_cannon_a_hotspot` | `8C756CC3` | active patrol + war | Cabal crater side |
| `pf_lighthouse_ca_crater_right_a` | `9692BB5E` | active patrol | Cabal crater-right coverage |
| `pf_lighthouse_ca_crater_right_a_hotspot` | `BB43DE9A` | alternate | overlaps active crater-right group |
| `pf_lighthouse_ca_forest_a` | `3163853F` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_ca_forest_a_hotspot` | `9B219BF3` | active patrol + war | Cabal forest side |
| `pf_lighthouse_ca_pond_a` | `74337EDD` | active patrol + war | proven slot 1 fallback avoids the non-spawning slot 0 dependency |
| `pf_lighthouse_ca_pond_a_hotspot` | `C751ACF1` | alternate | overlaps active Cabal pond group |
| `pf_lighthouse_ca_tower_back_a` | `9FA0CEF3` | alternate | alternate tower approach beside the active cannon/tower and vendor area |
| `pf_lighthouse_vx_cannon_mid_a` | `1152544E` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_vx_cannon_mid_a_hotspot` | `CF2196EA` | active patrol + war | Vex cannon-middle side |
| `pf_lighthouse_vx_cannon_mid_b` | `1152544D` | alternate | second cannon-middle layout |
| `pf_lighthouse_vx_cannon_mid_b_hotspot` | `715568C1` | alternate | hotspot of second cannon-middle layout |
| `pf_lighthouse_vx_center_left_a` | `EB1E8937` | alternate | second center-left layout |
| `pf_lighthouse_vx_center_left_b` | `EB1E8934` | active patrol | Vex center-left coverage |
| `pf_lighthouse_vx_center_left_b_hotspot` | `736F7158` | alternate | overlaps active center-left group |
| `pf_lighthouse_vx_center_right_a` | `1ED6087A` | active patrol | Vex center-right coverage |
| `pf_lighthouse_vx_center_right_b` | `1ED60879` | alternate | second center-right layout |
| `pf_lighthouse_vx_center_right_b_hotspot` | `2B614515` | alternate | hotspot of second center-right layout |
| `pf_lighthouse_vx_crater_cannon_a` | `FB7F2889` | active patrol | Vex crater-cannon coverage |
| `pf_lighthouse_vx_crater_left_a` | `EA95B97D` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_vx_crater_left_a_hotspot` | `BBF1BA51` | active patrol + war | Vex crater side |
| `pf_lighthouse_vx_forest_a` | `40BC3C51` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_vx_forest_a_hotspot` | `CCF03E8D` | active patrol + war | Vex forest side |
| `pf_lighthouse_vx_pond_a` | `85938DB7` | alternate | hotspot counterpart selected for patrol and war |
| `pf_lighthouse_vx_pond_a_hotspot` | `0EFE61CB` | active patrol + war | Vex pond side |
| `pf_lighthouse_vx_steps_a` | `1780D86F` | active patrol | Vex steps coverage |
| `pf_lighthouse_vx_steps_a_hotspot` | `63D313C3` | alternate | overlaps active steps group |

## Provenance and limits

`tools/testing/mercury_ambient_inventory.py` generated the 37-group catalog
from the installed packages. `tools/testing/mercury_faction_battle_inventory.py`
produced `build/coo/mercury-faction-current.json`, including string-table and
object ownership evidence. The decompiled behavior corpus in
`build/decomp-share-20260913/DECOMP_SHARE` was also searched; it did not provide
a retail Mercury war controller or an authoritative presentation dispatch path.
The four-wave timing and counts are therefore labeled as reconstructed host
policy while every native population identity remains package-derived.

An actor-retirement receipt without a death represents streaming or despawn and
does not clear a patrol. The native hook has an observed source-less restoration
path, but that path cannot yet rebind the restored actor to an exact population
lease. The retained roster wire lifecycle is covered by tests; leaving and
re-entering Mercury while a selected patrol actor is alive still requires a live
traversal playtest. If those selected groups retire and restore actors through
the source-less path, renewal will remain blocked rather than inventing a kill.

## Roster capacity

Production Mercury contributes one generic global-participation group before
the 20-group core profile, for 21 groups in ordinary free roam. Enabling all
three optional Mercury registry owners produces 24 groups. Combining those
owners with the adventure opening and forest local overlay brings the largest
verified union to 27 groups. The prior host capacity of 20 rejected this valid
union; it was a host limit rather than an observed serialized-wire limit. Host
wire-group storage is now 32. Catalog group storage is 2304 so the added Mercury
groups retain the existing reserve of 128 ordinary entries.

`tools/testing/mercury_roster_fixture.py` constructs isolated fixtures from the
installed cache and byte-decoded package groups. `unit/mercury_roster_tests`
checks admission and the actual other-missions encoder with the generic,
profile, public-event, adventure and local groups together. The changed DLL
producer-image hash invalidates an older cache on the next launch so the added
group records are rebuilt.
