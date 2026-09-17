# Vendor Interactions: Technical Findings & Architecture

A complete, detailed reference of all findings, reverse-engineering discoveries, and implementation mechanics regarding NPC vendor interactions in the Dawn Destiny 2 server emulation project.

---

## 1. Executive Summary

In retail Destiny 2, NPC vendors serve as the primary interface for world progression: they give campaign missions, sell gear, offer daily/repeatable bounties, accept reputation token turn-ins, and dispense rank-up reward packages.

In an offline/emulated environment, vendor interactions were entirely non-functional due to multiple disconnected subsystems:
1. **World Presence**: Vendors were not spawned into the Tower world map.
2. **Catalog Publication**: Large vendor definitions (>100 KiB) exhausted catalog buffers, causing missing vendors or corrupted boot caches.
3. **Stuck Banners**: The native client's interaction picker never received "retire" acknowledgments, permanently locking vendor screens on intro dialogues.
4. **Network Transactions**: The client uses two distinct web service opcodes (**901** for purchases and **904** for quests/interactions) with non-obvious payload layouts.
5. **Missing Bounties & Exchanges**: Repeatable bounties and material recycle rows do not exist as standard vendor inventory items in the manifest.
6. **Progression & Ranks**: The client's rank walk relies strictly on specific lane structures and level gates (e.g. Character Level 20).
7. **Inventory & Pursuits**: Pursuits (quests/bounties) have no equipment slots, causing loadout resolver rejections.

Through upstream PR #87 (`chnsw/vendor-interactions`) and subsequent Tower vendor spawning and progression tooling, these subsystems were reverse-engineered and fully unified into a stable, authentic engine.

---

## 2. World Spawning: Tower Vendor Entity Protocol

Vendors in the Tower social space (`city_tower_social_d2`, scenario hash `0x80B4A0F4`) are not static map decorations—they are networked entities spawned and synchronized via **BAP Activity Message 5** (sensor authority updates).

### 2.1 The 5 Tower Bubbles & Registries

The Tower is split into 5 spatial groups (bubble registries):

| Zone / Area | Bubble | Registry Key | Group Object Tag | Slot Count | Key Vendors / Entities |
|---|---|---|---|---|---|
| **Courtyard / Plaza** | 6 | `0x50CC9C7D` | `0x80B4A6B0` | 28 | Banshee-44 (slot 0), Master Rahool (slot 1), Kadi 55-30 (slot 2), Lord Shaxx (slot 3), Tess Everis (slot 4), Commander Zavala (slot 5), Eva Levante Solstice (slot 7), Statue Vendor (slot 6) |
| **Iron Banner** | 6 | `0x27060E6C` | `0x80B4A5CD` | 4 | Lord Saladin (slot 0), Engagement Sensor (slot 1) |
| **Hangar** | 7 | `0x728E75D1` | `0x80B4AD29` | 39 | Amanda Holliday (slot 3), Arach Jalaal / Dead Orbit (slot 4), Lakshmi-2 / FWC (slot 5), Xûr (slot 6), Catwalk & Deflector mechanisms |
| **Bazaar** | 1 | `0xF8790DA5` | `0x80B4A135` | 12 | Suraya Hawthorne (slot 0), Executor Hideo / New Monarchy (slot 1), The Drifter (slot 4), Eva Levante (slot 5), Ikora Rey (slot 3) |
| **Armory / Annex** | 0 | `0x58B3D759` | `0x80B4A1BD` | 3 | Ada-1 (slot 0), Engagement Sensor (slot 1) |

### 2.2 Bitstream Encoding (Native Combatants vs. Objects)

Vendors spawn through one of two authentic packet formats:
1. **Type 1 Slots (NPC Vendors)**: Encoded as `coo::native_combatant::Source` bitstreams (**641 bits**).
   - Fields: `registry`, `generation`, `ruleSlot`, `looseRequested = 1`, `hasRule`.
   - Vendors like Zavala, Shaxx, and Banshee are modeled internally by the engine as ambient, non-hostile combatant sources tied to their authored spawn rules (`type 66`).
2. **Type 4 Slots (Static Vendor Fixtures)**: Encoded as `coo::native_device::object` (**252 bits**).
   - Used for non-actor interactive objects: `o_statue_vendor` (Courtyard slot 6) and `o_ikora_vendor` (Bazaar slot 3).

---

## 3. Vendor Catalog Publication & Memory Banks

### 3.1 The Head-of-Index Bug
Each vendor definition is massive (>100 KiB), containing hundreds of sale rows, categories, and socket references. Early server code attempted to load vendor definitions sequentially starting from row 0 of the manifest index.
- **Finding**: Tower vendors are scattered across high indices (for example, **The Drifter is at row 195**). A sequential load exhausted the memory bank long before reaching key vendors.
- **Finding**: If a single definition failed to fit or parse, the entire catalog pass aborted, wrote an empty catalog to the disk cache, and permanently broke subsequent boots.

### 3.2 Prioritized Publication & Fail-Closed Isolation
- Vendors to publish are now explicitly prioritized in `vendor_catalog.txt` by **definition hash** (which is stable across manifest revisions).
- If an individual vendor fails to read or fit the bank, only that single vendor fails (`fail-closed`); the remaining row banks remain intact so subsequent vendors still validate.
- Manifest cache format was updated (formats 46, 48, and 54) to store vendor domains and support partial catalogs.
- Reverse-engineering established that offset `+100` on a vendor sale row represents `categoryIndex` (verified across 3,304 rows against the manifest).

---

## 4. Vendor Dialogues, Banners & The Retire Hook

### 4.1 The Stuck Intro Banner Problem
When opening a vendor screen, the game client consults an internal **interaction picker**. The picker evaluates available dialogues and intro quest banners, displaying the highest-priority interaction whose native **retire test** does not return `true` (skip).
- Offline, no backend service ever answered this retire test.
- Consequently, accepting an introductory quest left the banner active. On every subsequent visit, the client re-offered the completed dialogue and refused to show the vendor's storefront or bounties.

### 4.2 Detour Implementation (`vendor_banner_retire`)
Dawn hooks the client's native retire test:
- **Signature**: `48 89 5C 24 10 48 89 6C 24 18 56 48 83 EC 20 44 8B 49 08 33 ED 0F B7 DA 48 8B F1`
- **Native Picker Memory Layout**:
  - `+0x00`: `vendorIndex` (`std::uint16_t`)
  - `+0x02`: `selectedInteraction` (`std::uint16_t`, or `0xFFFF` if none)
- **State Storage**: `state::vendors::answered_interactions` maintains a session-wide list of answered interactions per vendor.
- **Strict Transaction Ordering**: The server records an interaction as answered **only after the grant mutation commits cleanly**. If a transaction drops due to full inventory or validation failure, the banner remains unretired, preventing players from permanently losing access to unearned quests.

---

## 5. Web Service Transaction Protocols (Opcodes 901 & 904)

Player actions at a vendor trigger two distinct web service messages:

```
                      [ Client Action ]
                             |
             +---------------+---------------+
             |                               |
      [ Opcode 901 ]                  [ Opcode 904 ]
     Vendor Purchases                Quest Interactions
             |                               |
             +---------------+---------------+
                             |
                   [ settle_vendor_row ]
                             |
       +---------------------+---------------------+
       |                     |                     |
[ Bounty Roll ]       [ Exchange ]          [ Grant / Settle ]
 vendor_bounty_roll   vendor_exchange        - Substitution
 (5 held limit)       (currency changes)     - Rowless (3rd array)
                                             - Collections path
```

### 5.1 Opcode 901 (Vendor Purchases & Claiming)
- Carries `vendorIndex` and `saleIndex`.
- **Ordinary Purchases**: Resolves the sale row to its item definition and grants it through the standard acquisition pipeline (the same path used by Collections opcode 1820). Costs are currently waived as cost-bearing fields remain role-open.
- **Rowless Claims**: If `saleIndex` does not match an ordinary sale row, it is evaluated as an interaction slot indexing the vendor's **third array**. Vendor rank-up packages claim through Opcode 901 using this path.

### 5.2 Opcode 904 (Quest Steps & Banners)
- Carries a 16-bit `slotIndex` and a 32-bit `saleIndex`.
- **The Slot vs. Row Discovery**: The 16-bit `slotIndex` simply records where the UI click landed on screen. Indexing sale rows with `slotIndex` was a severe historical bug that caused quest acquisitions to grant unrelated armor mods!
- **32-Bit Row Field**: The 32-bit field is the true row identifier. Requests lacking this field are strictly rejected.
- **Rowless (-1) Interactions**: When `saleIndex == -1` (`0xFFFFFFFF`), the tile is an interaction rather than an inventory sale row.
  - The slot first indexes the vendor's **third array** at offset `+0` (`rewardItemIndex`), which contains authored rank rewards (verified against captured Zavala claims).
  - If the third array has no reward, it falls back to the **installed array** at offset `+0`, which holds definition hashes (used by Amanda Holliday's campaign starters).

---

## 6. The Vendor Rules Engine

To allow rapid balancing and behavior authoring without recompiling or restarting the server, vendor behaviors are driven by plain-text rule files placed in `bin\x64\Dawn\` beside `settings.json`. They are re-read on every request.

### 6.1 `vendor_catalog.txt` (Published Vendors)
Names vendor definition hashes to publish in priority order. Ensures major social vendors (Drifter, Banshee, Zavala, Shaxx, Rahool, Holliday, Saint-14, planetary contacts) load before bank capacity is reached.

### 6.2 `vendor_item_substitute.txt` (Dummy Placeholders -> Real Quests)
Items with `DestinyItemType = 20` (`Dummy`) are UI placeholders. Granting a dummy item puts an invisible ghost item in the player's inventory.
- **Campaign Starters**: Amanda Holliday's Legacy Campaign tiles are dummy items:
  - **Red War** (`0xBEB63647`): Substituted with `0x37DD26F0` (*Homecoming* quest step).
  - **Curse of Osiris** (`0x6CBEA754`): Substituted with `0x6706D3EC` (*The Gateway* quest step).
  - **Warmind** (`0x65683247`): Substituted with `0xF5B78E7F` (*Ice and Shadow* quest step).
- *Note on UI persistence*: These campaign tiles do not immediately vanish upon purchase because the client gates them on kind-0 value slots (`VAL(12578)`, `VAL(12609)`, `VAL(12624)`), which are backed by family-5 overrides processed once at boot.

### 6.3 `vendor_bounty_roll.txt` (Repeatable Bounties)
- **Why Authored**: Repeatable bounties ("Additional Bounties", costing 3,000 Glimmer) **do not exist in any vendor's sale row list in the manifest**. They exist solely as standalone item definitions.
- **Structure**: Keyed by `vendorDefinitionHash` and `triggerCategory`.
- **Rule**: Characters can hold a maximum of **5 repeatable bounties** from a given vendor at any time. Excess rolls are rejected, matching retail rules.

### 6.4 `vendor_exchange.txt` (Material & Shader Recycling)
- **Structure**: `vendorHash rowIndex costItem costQty [payoutItem payoutQty...]`
- Handles recycling (Drifter synths for Glimmer, Master Rahool's 277 shader recycling rows for Glimmer and Legendary Shards).
- Operates via `prepare_vendor_exchange` by mutating profile stacks through the account change ring, ensuring the client renders the authentic floating `+N` currency notification toast.

### 6.5 `vendor_progression.txt` (Token Turn-ins)
- Maps `vendorDefinitionHash`, `saleRow`, `progressionIndex`, `scope`, `pointsAwarded`, and `pointsPerRank`.
- Represents the 1, 5, and 10 token turn-in bands across vendors (Devrim, Sloane, Asher, Failsafe, Vance, Ana Bray, Zavala, Shaxx, Banshee, Benedict 99-40).

---

## 7. Reputation, Progression Lanes & Rank-Up Packages

### 7.1 Progression Data Architecture (The 3 Lanes)
Vendor progression in Destiny 2 is stored in 3 distinct lanes:
- **Lane 0**: Total accumulated progression points.
- **Lane 1**: Step / Rank index.
- **Lane 2**: Claimed packages / reset counters.

### 7.2 Critical Discovery: The Client Rank Walk
Reverse-engineering the client's internal rank-walking function (`sub_7FF7B6DF3380`) revealed:
- **The client only reads Lane 0 during turn-ins**: It compares Lane 0 against authored step point totals, advancing one rank per step. It ignores Lanes 1 and 2 during this walk.
- **Package Availability Rule**: The vendor screen offers an unclaimed rank-up reward engram when:
  $$\text{Lane 1} > \text{Lane 2}$$
  (Earned Rank exceeds Claimed Rank packages).
- **Mid-Rank Turn-ins**: Retail Destiny 2 awarded reputation per token, but only granted packages when crossing a rank threshold. A turn-in that lands mid-rank updates Lane 0/1 but completes with `reputationOnly` (granting no physical item).

### 7.3 Character Level 20 Gate
Faction rank-up engrams enforce a strict client-side gate: the character must be **Level 20 or higher** (Progression Slot 7). If character level is below 20, vendor engrams display as locked ("Requires Level 20").

---

## 8. Pursuits & Loadout Integrity

### 8.1 Slotless Inventory Items
Pursuits (quest steps, bounties, tokens) do not equip to character slots (Kinetic, Energy, Helmet, etc.).
- **The Bug**: The original Dawn loadout resolver required an equipment slot for every inventory item. As a result, newly granted pursuits were rejected and lost during loadout calculation.
- **The Fix**: The loadout resolver was updated to allow slotless items in general inventory, enforcing slots only for items actually equipped.

### 8.2 Classification (`holds_pursuit`)
To prevent duplicate quest pickups and honor the 5-bounty cap:
- Gear is identified by having an authored equipment slot.
- Consumables have `maxStackSize > 1`.
- Pursuits have **no equipment slot** and `maxStackSize == 1`.
- Dismantling slotless pursuits skips restamping surviving items' serial numbers, which previously caused items in the UI bucket to reshuffle erratically.

---

## 9. Diagnostic & Progression UI Tools

The `vendor_rank_panel` component in Core UI provides live inspection and modification of vendor state:
- **Direct Progression Editing**: Presets for Zavala, Shaxx, Banshee, Ikora, Devrim, Sloane, Failsafe, Asher, Vance, Ana Bray, Saladin, Benedict, Hawthorne, and Obelisks.
- **Automated Rank-Up Triggering**: Automatically configures Lane 1 and Lane 2 to present available reward engrams in-game.
- **One-Click Banner Retirement**: Allows instant retirement of intro banners (interactions 0..39) or clearing answered states for testing.
- **Real-Time BAP Resync**: All changes immediately call `bap::arm_all_account_resync()`, reflecting reputation changes in-game without reloading the zone.

---

## 10. Technical Reference Summary

| Concept | Identifier / Offset | Purpose / Details |
|---|---|---|
| **Tower Scenario** | `0x80B4A0F4` | `city_tower_social_d2` |
| **Vendor Spawning** | BAP Activity Msg 5 | Type 1: 641-bit combatant source; Type 4: 252-bit object |
| **Banner Retire Hook** | Signature `48 89 5C 24 10...` | Detours native picker retire check; skips answered dialogues |
| **Picker Layout** | `+0x00`: `vendorIndex`, `+0x02`: `interaction` | Native memory offsets in picker state structure |
| **Opcode 901** | Web Service Message | Vendor purchases; fallback to 3rd array for rank claims |
| **Opcode 904** | Web Service Message | Quest interaction; 32-bit row field (`-1` = rowless interaction) |
| **3rd Array `+0x00`** | `rewardItemIndex` (`std::uint16_t`) | Authored reward package item definition index |
| **Installed Array `+0x00`** | `definitionHash` (`std::uint32_t`) | Target definition hash for campaign starter tiles |
| **Sale Row `+100`** | `categoryIndex` (`std::int32_t`) | Category index correlating row to manifest |
| **Rank Walk Function** | `sub_7FF7B6DF3380` | Native function walking Lane 0 points against rank steps |
| **Engram Unlock Gate** | Slot 7, Lane 1 $\ge 20$ | Requires Character Level $\ge 20$ to open vendor engrams |
