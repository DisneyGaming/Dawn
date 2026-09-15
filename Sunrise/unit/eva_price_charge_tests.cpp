/**
 * Authored vendor prices: what a priced acquisition charges, and what it refuses.
 *
 * Exercises the new charging entry point `apply_authored_cost` AND the two prepare functions that
 * decide whether an authored price or a collectible's own installed set is the one charged. Three
 * things here are the point of the whole change:
 *
 *   * a row that also owns a collectible cost is charged the vendor price INSTEAD of the
 *     Collections re-pull price, while `materialRequirementSetHash`/`materialRequirementCount`
 *     still carry the collectible's own values, because that is what commit validates;
 *   * a price the account cannot afford refuses whole, leaving the account untouched, which is
 *     what makes `prepare_*` return false and the purchase answer the refused status;
 *   * a price that spends a stack to exactly zero still produces a well-formed mutation, even when
 *     the emptied stack was the last profile row and carried the greatest mutation serial.
 *
 * Links `state_account_profile_runtime.obj` and `state_account_acquisition_runtime.obj` from the
 * Release build and stubs every build-data lookup, so no game data and no live State are needed.
 */
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

#include "core/logging/log.h"
#include "state/runtime/runtime.h"
#include "state/runtime/state.h"
#include "state/runtime/state_account_transaction_helpers.h"

namespace {

bool check(bool value, const char* expression, int line) noexcept {
    if (!value) {
        std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
    }
    return value;
}

} // namespace

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!check(static_cast<bool>(value), #value, __LINE__)) {                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)

namespace sunrise::core::log {
void write(Channel, Level, std::string_view) noexcept {}
void writef(Channel, Level, const char*, ...) noexcept {}
} // namespace sunrise::core::log

namespace sunrise::state {

/** Glimmer, Candy, Legendary Shards and one instanced action source: every charge rule is covered. */
namespace {

struct CatalogRow final {
    std::uint32_t hash;
    std::uint16_t index;
    std::uint8_t bucketId;
    bool stackable;
    bool profileBucket;
    bool actionSource;
};

/** Hashes are the real authored ones so a reader can match this test to the price file. */
constexpr std::array<CatalogRow, 5> kCatalog{{
    {0xBC53E66EU, 10, 1, true, true, false},  // Glimmer
    {0xF372F896U, 11, 2, true, true, false},  // Candy
    {0x3CF2E8E2U, 12, 1, true, true, false},  // Legendary Shards
    {0xDEADBEEFU, 13, 3, true, true, true},   // an action source: never chargeable
    {0x954F4813U, 14, 2, true, true, false},  // Restless Shell, the granted item
}};

[[nodiscard]] const CatalogRow* row_by_index(std::uint16_t index) noexcept {
    for (const CatalogRow& row : kCatalog) {
        if (row.index == index) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace

namespace build_data {

bool find_item_definition_index(std::uint16_t index, items::Definition& definition) noexcept {
    const CatalogRow* row = row_by_index(index);
    if (row == nullptr) {
        return false;
    }
    definition = {};
    definition.definitionHash = row->hash;
    definition.definitionIndex = row->index;
    definition.bucketId = row->bucketId;
    return true;
}

bool find_item_definition_hash(std::uint32_t hash, items::Definition& definition) noexcept {
    for (const CatalogRow& row : kCatalog) {
        if (row.hash == hash) {
            return find_item_definition_index(row.index, definition);
        }
    }
    return false;
}

bool find_configured_item_detail(std::uint16_t index, items::details::Definition& detail) noexcept {
    const CatalogRow* row = row_by_index(index);
    if (row == nullptr) {
        return false;
    }
    detail = {};
    detail.definitionHash = row->hash;
    detail.definitionIndex = row->index;
    detail.bucketId = row->bucketId;
    detail.maxStackSize = 999999;
    detail.instancedDefinitionState = row->stackable
                                          ? items::details::InstancedDefinitionState::stackable
                                          : items::details::InstancedDefinitionState::instanced;
    return true;
}

/** Each synthetic bucket owns its own disjoint slot window, as the installed descriptors do. */
bool find_inventory_bucket_descriptor(std::uint8_t bucketId,
                                      inventory::buckets::Descriptor& bucket) noexcept {
    constexpr std::uint16_t kSlotsPerBucket = 16;
    if (bucketId == 0 || bucketId > 3) {
        return false;
    }
    bucket = {};
    bucket.bucketId = bucketId;
    bucket.arraySelector = inventory::buckets::ArraySelector::profile;
    bucket.firstSlot = static_cast<std::uint16_t>((bucketId - 1) * kSlotsPerBucket);
    bucket.slotCount = kSlotsPerBucket;
    return true;
}

bool is_profile_action_source(std::uint16_t index, std::uint8_t) noexcept {
    const CatalogRow* row = row_by_index(index);
    return row != nullptr && row->actionSource;
}

} // namespace build_data
} // namespace sunrise::state

namespace {

namespace state = sunrise::state;
namespace collectibles = sunrise::state::build_data::collectibles;

/**
 * Restless Shell's collectible: the Collections re-pull price the authored vendor price replaces.
 *
 * 4000 Glimmer and 5 Legendary Shards is the shape the research pass read off sale row 83, and the
 * whole point of the change is that a 200-Candy vendor rule is charged instead of this.
 */
constexpr std::uint16_t kRestlessShellCollectible = 7;
constexpr std::uint32_t kRestlessShellSetHash = 0x0BADC0DEU;
constexpr std::uint16_t kRestlessShellSetIndex = 3;
constexpr std::uint32_t kCollectibleGlimmer = 4000;
constexpr std::uint32_t kCollectibleShards = 5;

/** Set by a test when the prepare under test must see a collectible; cleared otherwise. */
bool g_collectibleInstalled = false;
/** The account every `prepare_*` under test reads, in place of live State. */
state::AccountState g_snapshot{};

} // namespace

namespace sunrise::state {

namespace build_data {

bool find_collectible_definition(std::uint16_t collectibleIndex,
                                 collectibles::Definition& definition) noexcept {
    if (!g_collectibleInstalled || collectibleIndex != kRestlessShellCollectible) {
        return false;
    }
    definition = {};
    definition.collectibleHash = 0xFEEDFACEU;
    definition.collectibleIndex = kRestlessShellCollectible;
    definition.itemDefinitionIndex = 14; // Restless Shell
    definition.materialRequirementSetHash = kRestlessShellSetHash;
    definition.materialRequirementSetIndex = kRestlessShellSetIndex;
    definition.materialRequirementCount = 2;
    definition.materialRequirements[0].quantity = kCollectibleGlimmer;
    definition.materialRequirements[0].itemDefinitionIndex = 10; // Glimmer
    definition.materialRequirements[0].deleteOnAction = true;
    definition.materialRequirements[1].quantity = kCollectibleShards;
    definition.materialRequirements[1].itemDefinitionIndex = 12; // Legendary Shards
    definition.materialRequirements[1].deleteOnAction = true;
    return true;
}

} // namespace build_data

/** The prepare functions read State through this; the tests hand them a fixture instead. */
AccountState account_snapshot() noexcept {
    return g_snapshot;
}

namespace account {
namespace settings {
bool valid(const AccountSettings&) noexcept {
    return true;
}
} // namespace settings
namespace inventory {
bool valid(const Equipment&) noexcept {
    return true;
}
bool valid(const CharacterItems&) noexcept {
    return true;
}
} // namespace inventory
} // namespace account

/**
 * Storage the acquisition runtime commits through. No case here commits, so the lock and the state
 * exist only to satisfy the linker; nothing reads them.
 */
namespace runtime::storage {
SRWLOCK g_stateLock = SRWLOCK_INIT;
State g_state{};
} // namespace runtime::storage

/**
 * Character-path helpers the acquisition runtime references. Every case here is on the PROFILE
 * path, which reaches none of them; they answer "not found" so a stray call cannot pass silently.
 */
namespace runtime::detail {

void report_acquisition(std::string_view,
                        std::string_view,
                        std::string_view,
                        std::uint32_t,
                        std::uint64_t,
                        std::uint64_t,
                        std::size_t,
                        std::uint16_t,
                        std::uint8_t,
                        std::uint32_t) noexcept {}

bool same_stationary_item(const account::inventory::Item&,
                          const account::inventory::Item&) noexcept {
    return false;
}

bool same_character(const CharacterState&, const CharacterState&) noexcept {
    return false;
}

bool next_item_instance_soid(const AccountState&, std::uint64_t&) noexcept {
    return false;
}

bool next_profile_item_instance_soid(const AccountState&, std::uint64_t&) noexcept {
    return false;
}

bool identity_uses_soid(const AccountState&, std::uint64_t) noexcept {
    return false;
}

std::int32_t acquisition_level(const CharacterState&) noexcept {
    return 0;
}

bool find_acquired_row(const middleware::datagen::family4::loadout::ResolvedLoadout&,
                       std::uint64_t,
                       std::uint16_t&,
                       std::uint8_t&) noexcept {
    return false;
}

bool find_unequipped_row(const middleware::datagen::family4::loadout::ResolvedLoadout&,
                         std::uint64_t,
                         std::uint16_t&,
                         std::uint8_t&) noexcept {
    return false;
}

bool find_character_item_location(const CharacterState&,
                                  std::uint64_t,
                                  CharacterItemLocation&) noexcept {
    return false;
}

} // namespace runtime::detail
} // namespace sunrise::state

namespace sunrise::middleware::datagen::family4::loadout {
bool resolve(const state::AccountState&, std::uint64_t, ResolvedLoadout&) noexcept {
    return false;
}
} // namespace sunrise::middleware::datagen::family4::loadout

namespace {

namespace detail = sunrise::state::runtime::detail;
namespace requirements = sunrise::state::build_data::material_requirements;
namespace inventory = sunrise::state::account::inventory;

/** One charge row in the exact shape `price_for_vendor_row` builds. */
[[nodiscard]] requirements::Requirement cost_row(std::uint16_t itemIndex,
                                                 std::uint32_t quantity) noexcept {
    requirements::Requirement row{};
    row.quantity = quantity;
    row.itemDefinitionIndex = itemIndex;
    row.condition = requirements::kUnconditionalRequirement;
    row.deleteOnAction = true;
    row.omitFromRequirements = false;
    return row;
}

/** An account holding the authored Glimmer stack and an earned Candy stack. */
[[nodiscard]] state::AccountState authored_account(std::int32_t glimmer,
                                                   std::int32_t candy) noexcept {
    state::AccountState account{};
    account.primarySoid = 0x1234ULL;
    std::size_t count = 0;
    if (glimmer > 0) {
        account.profileItems[count++] = {0, 0xBC53E66EU, glimmer, 1};
    }
    if (candy > 0) {
        account.profileItems[count++] = {0, 0xF372F896U, candy, 2};
    }
    account.profileItemCount = count;
    return account;
}

[[nodiscard]] std::int32_t balance(const state::AccountState& account,
                                   std::uint32_t definitionHash) noexcept {
    std::int32_t total = 0;
    for (std::size_t index = 0; index < account.profileItemCount; ++index) {
        const inventory::ProfileItem& item = account.profileItems[index];
        if (item.definitionHash == definitionHash) {
            total += item.quantity;
        }
    }
    return total;
}

/** Reads one definition's quantity out of a prepared mutation's after-image. */
[[nodiscard]] std::int32_t after_balance(const state::PendingProfileItemAcquisition& mutation,
                                         std::uint32_t definitionHash) noexcept {
    std::int32_t total = 0;
    for (std::size_t index = 0; index < mutation.afterItemCount; ++index) {
        if (mutation.afterItems[index].definitionHash == definitionHash) {
            total += mutation.afterItems[index].quantity;
        }
    }
    return total;
}

} // namespace

int main() {
    // An empty price is the uncharged grant this build shipped with: nothing moves, and the call
    // still succeeds, so every existing call site keeps its behaviour.
    {
        const state::AccountState before = authored_account(250000, 1000);
        state::AccountState after{};
        bool changed = true;
        CHECK(detail::apply_authored_cost(before, {}, after, changed));
        CHECK(!changed);
        CHECK(after.profileItemCount == before.profileItemCount);
        CHECK(balance(after, 0xBC53E66EU) == 250000);
        CHECK(balance(after, 0xF372F896U) == 1000);
    }

    // An authored price charges exactly its own rows and nothing else. Restless Shell, sale row 83,
    // is authored 200 Candy; the Glimmer its collectible would have taken is untouched here, and
    // the case below proves the collectible really is the one being replaced.
    {
        const state::AccountState before = authored_account(250000, 1000);
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::AccountState after{};
        bool changed = false;
        CHECK(detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(changed);
        CHECK(balance(after, 0xF372F896U) == 800);
        CHECK(balance(after, 0xBC53E66EU) == 250000);
    }

    // BrayTech Werewolf (Random Roll), sale row 10: more than one cost row on one purchase.
    {
        const state::AccountState before = authored_account(250000, 1000);
        const std::array<requirements::Requirement, 2> price{cost_row(11, 250), cost_row(10, 5000)};
        state::AccountState after{};
        bool changed = false;
        CHECK(detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(changed);
        CHECK(balance(after, 0xF372F896U) == 750);
        CHECK(balance(after, 0xBC53E66EU) == 245000);
    }

    // Short on Candy: refused whole. Nothing is charged and nothing is granted, because `prepare_*`
    // returns false and the purchase answers the refused status.
    {
        const state::AccountState before = authored_account(250000, 199);
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // Short on the SECOND row of a multi-row price: still refused whole, and the first row is not
    // spent on the way, because every row is balance-gated before any row is deleted.
    {
        const state::AccountState before = authored_account(4000, 1000);
        const std::array<requirements::Requirement, 2> price{cost_row(11, 250), cost_row(10, 5000)};
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // A currency the account holds no stack of at all is refused, never granted free. This is the
    // Chocolate Strange Coin / Reveler's Essence case: authored prices in a currency
    // `settings.json` does not author and nothing has credited yet.
    {
        const state::AccountState before = authored_account(250000, 0);
        const std::array<requirements::Requirement, 1> price{cost_row(11, 1)};
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // A conditional row is refused: a native variant gate has no meaning for a row that was never
    // part of an installed set, and charging it would spend a balance nobody authored.
    {
        const state::AccountState before = authored_account(250000, 1000);
        std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        price[0].condition = 0;
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // A row that is NOT consumed is refused rather than forwarded. `apply_material_requirements`
    // gates on the balance and then deletes nothing for such a row, which would be an affordability
    // check followed by a free grant - the exact failure this whole path exists to remove.
    {
        const state::AccountState before = authored_account(250000, 1000);
        std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        price[0].deleteOnAction = false;
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // An action source cannot be charged: removing an instance-backed profile row would also owe a
    // resident release, so the whole purchase is refused rather than half-applied.
    {
        const state::AccountState before = authored_account(250000, 1000);
        const std::array<requirements::Requirement, 1> price{cost_row(13, 1)};
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // A cost this build does not carry is refused rather than skipped, so an authored price can
    // never make a row free by naming an item that is not installed.
    {
        const state::AccountState before = authored_account(250000, 1000);
        const std::array<requirements::Requirement, 1> price{cost_row(9999, 1)};
        state::AccountState after{};
        bool changed = true;
        CHECK(!detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(!changed);
    }

    // Spending a stack to exactly zero releases its row, which is the shape commit adopts wholesale.
    {
        const state::AccountState before = authored_account(250000, 200);
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::AccountState after{};
        bool changed = false;
        CHECK(detail::apply_authored_cost(before, std::span{price}, after, changed));
        CHECK(changed);
        CHECK(after.profileItemCount == before.profileItemCount - 1U);
        CHECK(balance(after, 0xF372F896U) == 0);
        CHECK(balance(after, 0xBC53E66EU) == 250000);
    }

    // The replacement itself, through the real `prepare_profile_item_acquisition`: the row owns a
    // collectible whose installed set is 4000 Glimmer + 5 Legendary Shards, and the authored
    // 200-Candy price is charged INSTEAD. The collectible's set hash and count still ride the
    // mutation unchanged, because that pair is what commit compares against the collectible.
    g_collectibleInstalled = true;
    {
        g_snapshot = authored_account(250000, 1000);
        g_snapshot.profileItems[2] = {0, 0x3CF2E8E2U, 50, 3}; // Legendary Shards
        g_snapshot.profileItemCount = 3;
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::PendingProfileItemAcquisition mutation{};
        CHECK(state::prepare_profile_item_acquisition(
            kRestlessShellCollectible, 0x954F4813U, mutation, std::span{price}));
        CHECK(mutation.prepared);
        CHECK(after_balance(mutation, 0xF372F896U) == 800);
        CHECK(after_balance(mutation, 0xBC53E66EU) == 250000);
        CHECK(after_balance(mutation, 0x3CF2E8E2U) == 50);
        CHECK(after_balance(mutation, 0x954F4813U) == 1);
        CHECK(mutation.materialRequirementSetHash == kRestlessShellSetHash);
        CHECK(mutation.materialRequirementCount == 2);
        CHECK(detail::valid_profile_mutation_shape(mutation));
    }

    // The same row with NO authored price keeps the Collections re-pull charge, so deleting a rule
    // restores exactly this and does not make the row free.
    {
        g_snapshot = authored_account(250000, 1000);
        g_snapshot.profileItems[2] = {0, 0x3CF2E8E2U, 50, 3};
        g_snapshot.profileItemCount = 3;
        state::PendingProfileItemAcquisition mutation{};
        CHECK(state::prepare_profile_item_acquisition(
            kRestlessShellCollectible, 0x954F4813U, mutation));
        CHECK(after_balance(mutation, 0xBC53E66EU)
              == 250000 - static_cast<std::int32_t>(kCollectibleGlimmer));
        CHECK(after_balance(mutation, 0x3CF2E8E2U)
              == 50 - static_cast<std::int32_t>(kCollectibleShards));
        CHECK(after_balance(mutation, 0xF372F896U) == 1000);
        CHECK(mutation.materialRequirementSetHash == kRestlessShellSetHash);
        CHECK(mutation.materialRequirementCount == 2);
        CHECK(detail::valid_profile_mutation_shape(mutation));
    }
    g_collectibleInstalled = false;

    // The serial regression. A price that spends the LAST profile row to exactly zero deletes it,
    // and `apply_material_requirements` re-serials nothing because no surviving row moved - so the
    // acquired serial has to be taken over the UNCHARGED before-image too, or the shape validator
    // refuses a purchase the player could exactly afford. Candy here is both the last row and the
    // greatest serial, which is the ordinary state right after farming the Forest.
    {
        g_snapshot = {};
        g_snapshot.primarySoid = 0x1234ULL;
        g_snapshot.profileItems[0] = {0, 0xBC53E66EU, 250000, 1};
        g_snapshot.profileItems[1] = {0, 0xF372F896U, 200, 40};
        g_snapshot.profileItemCount = 2;
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::PendingProfileItemAcquisition mutation{};
        CHECK(state::prepare_profile_item_acquisition(
            state::build_data::collectibles::kNoCollectibleIndex,
            0x954F4813U,
            mutation,
            std::span{price}));
        CHECK(mutation.acquiredMutationSerial > 40);
        CHECK(after_balance(mutation, 0xF372F896U) == 0);
        CHECK(after_balance(mutation, 0x954F4813U) == 1);
        CHECK(detail::valid_profile_mutation_shape(mutation));
    }

    // Short by one on the same row: refused, and no mutation is prepared, which is what produces
    // the refused status on the wire.
    {
        g_snapshot = {};
        g_snapshot.primarySoid = 0x1234ULL;
        g_snapshot.profileItems[0] = {0, 0xBC53E66EU, 250000, 1};
        g_snapshot.profileItems[1] = {0, 0xF372F896U, 199, 40};
        g_snapshot.profileItemCount = 2;
        const std::array<requirements::Requirement, 1> price{cost_row(11, 200)};
        state::PendingProfileItemAcquisition mutation{};
        CHECK(!state::prepare_profile_item_acquisition(
            state::build_data::collectibles::kNoCollectibleIndex,
            0x954F4813U,
            mutation,
            std::span{price}));
        CHECK(!mutation.prepared);
    }

    std::printf("eva_price_charge_tests: ok\n");
    return 0;
}
