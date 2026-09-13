# Authored strike Nightfall selection

The Campaigns panel exposes Tree of Probabilities and A Garden World through the
Strikes tab. Select **Standard strike** or **Nightfall**. Nightfalls offer **Adept**,
**Master**, and **Grandmaster (GM)**. The dedicated **Nightfalls** tab opens the
same difficulty selection directly. Launches require orbit.
Changing the picker while loading or playing keeps the row's actual activity
status and difficulty visible until that run ends.

`src/state/activity/strike_variants.h` pins eight native public activity identities.
The six Nightfalls reuse the existing `strike_pact.lua` and `strike_bond.lua`
controllers and arrival coordinates. The public activity identity and native
selection descriptor stay intact, so each native definition supplies its own
difficulty-settings reference. That selection alone did not select Grandmaster
enemy templates in the local host: the source-authority serializer previously
hard-coded source variant 0. The dialogue policy treats these six entries as
strike runs.

For Tree of Probabilities the native activities are 230 (standard), 830 (Adept),
833 (Master), and 835 (Grandmaster). For A Garden World they are 229, 808, 811,
and 813 respectively. The direct Grandmaster variants share native difficulty
settings with their adjacent Ordeal variants. Hashes, package names, difficulty
configuration tags, and modifier counts are recorded in
`nightfall-variants-native.json` alongside this document.

Native difficulty records are `813206C1` (Adept), `81327CEB` (Master), and
`81327CEF` (Grandmaster), all class `80807D82`. Public activity record field +B4
is 750, 1080, and 1100 respectively in this installed build. These are historical
build 86657 values. Bungie's contemporary [July 30, 2020 update](https://www.bungie.net/7/en/News/article/49405)
also identifies Master as 1080. Modern Destiny balance values do not apply here.

The Grandmaster settings record identifies native difficulty 5, activity tier
`0x204`, AI resistance tier 3, AI shield tier 2, 4 starting revive tokens, 0
ordinary resurrection tokens, and a 2700-second time limit. The inventory tool
resolves these names with the installed engine's FNV-1 property hashes and checks
their raw values. The public activity record points to this settings record, so
the preserved direct Grandmaster selection carries the right native source data.
The launch adapter constructs and validates the exact native activity-813 or
activity-835 descriptor, submits it unchanged to the native selection manager,
and commits it. Static analysis has not yet identified the later property-bag
materializer, so this chain does not by itself prove that every difficulty-5
property executes under the local host.

Sunrise keeps the native Grandmaster activity power at 1100 and publishes the
selected player's effective Family-4 light at least 30 below it. The default cap
is therefore 1070; the launch picker can increase the disadvantage through 50.
Family-0 banner and Family-3 roster light use the same cap. A monotonic power
revision triggers a versioned refresh of all resident records when Grandmaster is
armed or cleared, and the whole refresh retries if the selection changes while it
is being built. The selected equipment-score lanes and item-instance levels are
also bounded. Item levels use ten-power steps, so a custom cap between steps
rounds weapon level down and cannot weaken the requested disadvantage. The
effective aggregate remains independently capped because it can include upgrades
from other characters that are absent from the selected character's score array.

These changes prove the values delivered through the native account, roster,
banner, and item-instance records. Static analysis has not yet connected those
record fields to both directions of the final combat damage calculation. The
level cap closes the obvious high-power weapon publication lane, but combat
enforcement still needs an in-game damage comparison or a recovered native
damage consumer before it can be claimed as complete.

That 30-point gap is intended to feed the native enemy-overlevel HUD path, which
normally supplies sword indicators. The exact build-86657 HUD comparison has not
been recovered from static code, so the icon itself still requires a live visual
check. Sunrise does not draw a substitute sword icon.

The activity-host descriptor can encode up to 16 skull selections as a 2-bit
group and 7-bit value. Installed package inspection has not established the
group/value-to-modifier mapping, and the 36-entry Grandmaster list is a property
object rather than 36 safe skull IDs. Sunrise therefore leaves the opaque native
skull array intact. Selectable challenge controls use the validated local rules
for power disadvantage, revive count, and revive expiry instead of writing
unverified descriptor bits.

Reproduce the native identity verification from the repository root:

```powershell
python tools/testing/nightfall_variants_inventory.py --output build/nightfall-native-check.json
```

The launch argument suite verifies all eight exact identities, wrong-hash
rejection, invalid-tier rejection, campaign/strike separation and immutable
pending requests. The lifecycle suite checks publication of the native
descriptor, arrival identity, return to orbit and repeated difficulty selection.
The production UI fixture renders the new tab and launches every selected tier,
including Grandmaster from the Strikes tab. The campaign variant suite verifies
strike dialogue selection for all eight identities.

The installed enemy source descriptors preserve all six authored variant slots.
Their standard slot (variant 0) and Grandmaster slot (variant 5) provide 18 exact
category substitutions on 17 Tree of Probabilities sources and 23 exact category
substitutions on 23 A Garden World sources. The runtime catalogue records the
registry, source, category, standard entity, and Grandmaster entity for each exact
substitution. Sunrise now freezes variant 5 in the strike frame only for exact
activity 813 or 835, and only an installed-package allowlist of sources whose
variant-5 category entries are all nonempty may encode it. Every other source,
including boss, scene, special, standard-strike, and campaign sources, encodes
variant 0. The native bias-one wire value is 6 for logical variant 5 and 1 for
logical variant 0; values outside 0 through 5 are rejected before any bits are
written.

Reproduce the package-backed substitution check from the repository root:

```powershell
python tools/testing/nightfall_enemy_variant_tests.py
```

This proves that the host requests the installed Grandmaster template variant at
those placements, with independent decoding tests for the exact source-authority
wire field. It does not prove that native spawning accepts every request, that
combat damage and HUD classification consume the expected difficulty fields, or
that every substituted entity is a Champion. The four remaining authored actor
profile fields have stable widths but no recovered semantics, so Sunrise leaves
them at their existing zero values instead of guessing resistance, shield, or AI
difficulty mappings. The recovered entity records do not expose a verified
Champion subtype or display classification. A source-only death receipt is also
insufficient because one Tree source contains two independently substituted
categories. Champion revive and score credit therefore require the exact
per-actor selected category and entity from the native creation path.

These checks prove selection and host integration. A focused in-game playthrough
is still needed to accept combat balance, modifier behavior, actor streaming,
rewards, and completion under each native difficulty.

## Completion rewards

No build-86657 Nightfall loot roll table has been recovered. Dawn therefore
labels its completion payout as an authored session bonus rather than native
Nightfall loot. One authenticated successful run grants Glimmer once:

- Adept: 1,000
- Master: 2,500
- Grandmaster: 5,000

The exact installed definition `0xBC53E66E` resolves to a stackable profile
currency in bucket 21 with one slot and a maximum of 250,000. The grant tops up
that sole row by the available headroom and never creates a second Glimmer stack.
An already capped balance completes as a zero-add reward. Failed runs and
ordinary strikes do not queue a payout.

Delivery uses the same checked Family-4 account upsert and mutation-serial
notification as profile acquisitions. The account change commits only after the
complete encrypted output frame has been staged; stale State, wrong account
ownership, missing subscription, or output capacity leaves the exact reward debt
retryable. The exact session and run key makes repeated terminal publication
idempotent. Account inventory is currently process-local across all Sunrise item
transactions, so this Dawn bonus does not survive a client restart.

The public activity metadata's reward rows remain presentation hints rather than
proof of native loot. Bungie's schema distinction is documented at
<https://bungie-net.github.io/multi/schema_Destiny-Definitions-DestinyActivityRewardDefinition.html>.
