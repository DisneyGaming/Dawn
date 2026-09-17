# EDZ/Moon and omitted-source implementation specification

This document is package-backed research for the Lost Sector runtime. It does not claim to recover retail successor bytecode. Native identities and descriptor layouts below come from the installed package set; the stage policy is the requested reconstruction.

## Runtime contract

- Admit every authored sector population source together when the sector enters/prewarms. Do not serialize sources by room or by death.
- A combatant death receipt may remove protection or unlock the chest. Source retirement and corpse retirement are cleanup signals and are never progression gates.
- The sector chest is enabled only after the named boss combatant has emitted an actual death receipt.
- EDZ has 205 sector sources and Moon has 203 after the Nightmare sources below are included. These are sector-only totals. `population_authority.h` has a 384-source in-memory capacity and the wire index maximum is 511, so the final profiles must measure `ambient + sector` admitted sources; the sector-only headroom is 179 for EDZ and 181 for Moon.
- Exact groups, source descriptors, rule descriptors, category choices, tactical candidates, bosses, and type-4 chest objects for all 20 sectors are in `lost_sector_edz_moon_native_research.json`.

## Category-width extension

The native combat source descriptor is class `80809C36`. Its relative definition is class `8080948F`; its category array starts at `definition + 0xA8`; each class-`80808356` category row has stride 104. The wire category-count field is four bits and can represent 15, but the current native-authority `Source` mirror stores only eight category entries. The Lost Sector policy/service layer must stop truncating categories to one or two, while the authority path must reject more than its reflected eight-entry storage limit unless that mirror is extended with separate evidence. The cumulative target representation is 63.

K1 Logistics contains six real multi-category squads. Their category targets are:

- source 20 `81567946`, `squ_garage_support_low`: `[3,2,2]`
- source 21 `81567949`, `squ_garage_captain`: `[1,4,2]`
- source 26 `81567958`, `squ_arc_entry_1`: `[1,1,1]`
- source 27 `8156795B`, `squ_arc_entry`: `[1,1,1]`
- source 28 `8156795E`, `squ_arc_dregs_b`: `[2,2,1]`
- source 33 `8156796D`, `squ_boss_support_b`: `[4,4,3]`

The Empty Tank group `9A24C39A` also contains five real rusher squads currently omitted only because of the old category-width limit:

- source 52, descriptor `80FD93D9`, `sq_hive_rusher_1`: three Thrall categories, targets `[7,7,7]`
- source 60, descriptor `80FD93EB`, `sq_hive_rusher_4`: three Thrall categories, targets `[5,7,7]`
- source 64, descriptor `80FD9403`, `sq_cabal_rusher_1`: five categories using entity `80FDEBBD`, targets `[1,1,1,1,1]`
- source 70, descriptor `80FD943D`, `sq_cabal_rusher_2`: three categories using entity `80FDEBBD`, targets `[1,1,1]`
- source 76, descriptor `80FD9470`, `sq_forsaken_rusher_1`: three categories using entity `80FDF71C`, targets `[1,1,1]`

The repeated categories are authored parallel squad elements, not mutually exclusive boss forms. All five sources belong to the same Empty Tank encounter registry and must be restored. The singleton counts for the unresolved Cabal/Forsaken templates are reconstruction estimates from the package policy and remain subject to gameplay validation.

## Nightmare sources

The five Moon Nightmare generators are part of their owning Lost Sector groups. Each has one category with target 1, no rule reference in either rule field, and a following named member. Activate them with an explicit `hasRule=false`/implicit-no-rule policy:

- Logistics source/member `59/60` and `64/65`
- Revelation `80/81`
- Crew Quarters `47/48`
- Communion `62/63`

Ten sources omitted from the existing 22-sector catalog as `seasonal_nightmare_overlay` are likewise same-registry sector actors, each one category/target 1 with an exact primary rule, and should be restored:

- Methane Flush `A27443E8:5`, descriptor/rule `80BF0241` / slot 75 `80BF021D`
- DS Quarters-2 `2E3D2EB4:2`, `80BF1347` / 55 `80BF1338`
- Cargo Bay 3 `5BA616DA:20`, `80BF16DA` / 71 `80BF15CA`
- Sanctum of Bones `DB5D8740:4`, `80BD7137` / 36 `80BD712B`
- Grove of Ulan-Tan `33C30847:5`, `80BD8A6A` / 39 `80BD8A46`
- The Rift `6717656F:2`, `80C07775` / 46 `80C0775C`
- The Conflux `BB69D2E9:4`, `80C08E0F` / 65 `80C08DF1`
- The Orrery `3F8AF55C:5`, `80C0925F` / 77 `80C09246`
- The Carrion Pit `2F8DB58A:2`, `80C0A522` / 37 `80C0A500`
- Ancient's Haunt `100F6578:2`, `80C3195F` / 33 `80C31951`

Trapper's Cave registry `69A1B17C` source 8 remains excluded: it is co-resident with the distinct `o_start_bundle_interact`/`seq_dark_ether` event group and is not an ordinary sector population. Ascendant-plane overlay registries remain excluded for the same ownership reason.

## Moon traversal and protection

The user requested all Lost Sector enemies up front. Therefore traversal barriers must not wait for room clears. The exact device identities below are proven, but their authored position values are not: position semantics are asset-specific in the existing runtime (`1` removes some devices, while other devices use `0` for removal). Do not apply one universal open value. For each barrier, retain the authored/default state if it is already traversable; otherwise use a per-descriptor gameplay capture or geometry/collision test to identify the open value, then publish it at admission with snap and a positive monotonic revision. When a barrier is represented only by a type-4 shield/generator object, do not publish the blocking object, or withdraw it at admission through the existing object lifecycle. This applies to:

- Logistics group `C7C90DFD`: garage shield device/object 47 `81567997` / 48 `8156799A`; garage generator device/object 49 `8156799D` / 50 `815679A0`; loopback door device/object 51 `815679A3` / 52 `815679A6`; loopback shield device/object 55 `815679AF` / 56 `815679B2`; loopback generator device/object 57 `815679B5` / 58 `815679B8`.
- Revelation group `ADE66EB0`: loopback door device 16 `81570036`. It has no paired type-4 object in this registry.
- Crew Quarters `848F9F5C`: bridge door device 0 `8157125A`, generator device 1 `8157125D`, shield device 2 `81571260`, generator/shield objects 3 `81571263` / 4 `81571266`.
- Communion `D97A4D7A`: lobby generator/shield objects 9 `81572AB0` / 10 `81572AB3`; labs generator/shield objects 34 `81572AFB` / 35 `81572AFE`; loopback doors 88 `81572B9D` / 89 `81572BA0`.

The descriptors prove membership, native type and role names, but contain no authored runtime position scalar. For the all-at-entry adaptation, the bounded safe reconstruction is:

- At each sector run admission, retire every paired blocking type-4 object above with the run's second placement generation. This has package-proven target identity and an existing lifecycle contract; it does not depend on type-23 polarity.
- Publish Position `1.0`, snap enabled, with a fresh positive revision for the associated type-23 garage/bridge/loopback devices. Position `1.0` is a **reconstructed route-open value**, supported by the existing adventure mission and barrier services, not a package-proven Moon polarity. Keep this per-descriptor override isolated so a later gameplay capture can replace it without changing the object lifecycle.
- Communion's lobby and labs barriers have only type-4 generator/shield objects in this registry, so retiring slots 9/10 and 34/35 is the complete available native removal path. The loopback uses Position `1.0` on slots 88/89.
- Revelation needs only reconstructed Position `1.0` on slot 16 for its loopback; the forward combat route has no named barrier device in the encounter registry.
- Do not republish a blocking state when the chest unlocks or the run cancels. Keep these route devices open through cleared/cancel-pending teardown, then release ownership after bubble departure. A close-on-reset can strand a participant between the boss/chest and the patrol exit; a new run receives a newer open revision and retired-object generation.

Protection is retained as a boss mechanic and is reconstructed from unique authored names:

- Logistics: effect 46 `fallen_immune_shield_ultra_servitor_lrg_hopon` with filter 75 `fallen_immune_shield_ultra_servitor_lrg_filter` targets boss source/member 42/43. Enable it at admission. Remove it after actual deaths of both Nightmare generator named members 60 and 65.
- Crew Quarters: effect 46 `ho_nm_lg_1` with filter 55 `of_nm_lg_1` targets boss 43/44. Remove it after Nightmare member 48 dies.
- Communion: effect 98 `ho_nm_lg_1` with filter 113 `of_nm_lg_1` targets boss 60/61. Remove it after Nightmare member 63 dies.

These joins are stronger than a proximity guess because each owning registry has one large-Nightmare effect/filter pair and one boss plus the listed generator members. The exact retail successor edge has not been recovered, so retain this as reconstructed behavior in diagnostics/tests.

K1 Revelation `ADE66EB0` has four authored crystal objects and four matching class-`80804F3B` type-24 controllers:

- objects 69-72: `o_crystal_1`, `o_crystal_2`, `o_crystal_3`, `o_crystal_boss`, descriptors `815700D5`, `815700D8`, `815700DB`, `815700DE`
- controllers 88-91: `ch_crystal_1`, `ch_crystal_2`, `ch_crystal_3`, `ch_crystal_boss`, descriptors `81570108`, `8157010B`, `8157010E`, `81570111`; component/sense/authority classes `80804F3B/80804F3D/80804F40`
- guardian Wizard sources are 34, 41, and 48; boss Ogre is source/member 64/65; Nightmare generator is 80/81.

All combat sources still activate together. Keep crystals 1-3 protected until their corresponding guardian Wizard dies, then accept destruction of that crystal. After all three crystal destruction receipts, expose the boss crystal. Its destruction removes protection from boss source 64. Do not substitute a proximity trigger for crystal destruction. A proven type-24 authority writer already exists at `Dawn/src/state/activity/coo/native_atom_authority.h` (`write_channels`, schema `80804F40`, up to four revision/value/blend rows) and is exercised by `strike_pact/authority.h`.

The exact joins are now recovered in `tools/coo/moon_revelation_crystal_channel_research.md`:
controller slots 88/89/90/91 target type-4 slots 69/70/71/72 one-to-one. Their declared channel-row
count, semantic names, and value polarity are not recovered. No evidence ties those presentation
channels to protection or damage. Keep the type-24 controllers package-authored and optional; do
not make progression depend on a guessed row or scalar.

The executable path uses the existing type-4 object lifecycle and the package-authored linked
protection pair. Do not instantiate crystal objects 69-72 initially. Actual deaths of Wizard
sources 34/41/48 activate objects 69/70/71 respectively. Require each object's observed live
instance followed by its bounded health/destruction receipt. After all three receipts, activate
boss crystal 72. Keep linked effect 76 `ho_nm_lg` enabled against its authored filter 95
`of_nm_lg` from admission; boss-crystal destruction disables effect 76 and exposes boss
source/member 64/65. The Wizard-to-crystal and boss-crystal-to-protection edges are reconstructed
from the public mechanic and the unique authored identities. A disappearance, cleanup, or
retirement without the health receipt does not advance the stage.

## Validation

- For every source, assert all package categories survive catalog generation and population publication in order.
- Assert EDZ/Moon combined ambient plus active-sector admission stays within 384 and every published source index stays at or below 511.
- Assert all authored sector sources are requested at admission, irrespective of room or death state.
- Assert traversal positions/objects are open before a player can reach each boundary, and test collision for every descriptor rather than assuming `0` or `1` globally.
- Assert protection removal consumes actual named-member or crystal-destruction receipts; cleanup/retirement does not satisfy it.
- Assert chest enablement consumes actual boss death and remains disabled for admission, damage, disappearance, and source retirement alone.
