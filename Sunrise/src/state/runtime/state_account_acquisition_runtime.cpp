#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <limits>
#include <span>

#include "../../middleware/datagen/family4/loadout/loadout_resolver.h"
#include "../build_data/runtime.h"
#include "runtime.h"
#include "state_account_transaction_helpers.h"
#include "state_item_random_roll.h"
#include "profile_reward_staging.h"
#include "storage/internal.h"
#include "../persistence/persistence.h"

namespace sunrise::state {

using namespace runtime::detail;
namespace authored_inventory = account::inventory;
namespace item_details = build_data::items::details;
namespace inventory_buckets = build_data::inventory::buckets;
namespace family4_loadout = middleware::datagen::family4::loadout;

namespace {

[[nodiscard]] bool has_item_replacement_metadata(const PendingItemAcquisition& mutation) noexcept {
    return mutation.updatedInstanceSoid != 0;
}

[[nodiscard]] bool empty_item_replacement_metadata(
    const PendingItemAcquisition& mutation) noexcept {
    return mutation.updatedInstanceSoid == 0 && mutation.updatedInventoryIndex == 0
           && mutation.updatedInventoryRow == 0 && mutation.updatedEquipmentSlot == 0
           && mutation.updatedBeforeDefinitionIndex == 0
           && mutation.updatedAfterDefinitionIndex == 0
           && mutation.updatedBeforeDefinitionHash == 0
           && mutation.updatedAfterDefinitionHash == 0
           && mutation.updatedBeforeMutationSerial == 0
           && mutation.updatedAfterMutationSerial == 0;
}

[[nodiscard]] bool definition_identity(std::uint32_t definitionHash,
                                       std::uint16_t expectedDefinitionIndex,
                                       build_data::items::Definition& definition) noexcept {
    return definitionHash != authored_inventory::kNoDefinitionHash
           && build_data::find_item_definition_hash(definitionHash, definition)
           && definition.definitionHash == definitionHash
           && definition.definitionIndex == expectedDefinitionIndex;
}

[[nodiscard]] bool no_existing_definition(const CharacterState& character,
                                          std::uint32_t definitionHash) noexcept {
    for (const auto& item : character.equipment.slots) {
        if (item.has_value() && item->definitionHash == definitionHash) {
            return false;
        }
    }
    for (std::size_t index = 0; index < character.inventory.count; ++index) {
        if (character.inventory.values[index].definitionHash == definitionHash) {
            return false;
        }
    }
    return true;
}

/** Validates the only permitted extra change in a character acquisition after-image. */
[[nodiscard]] bool valid_item_replacement_image(const PendingItemAcquisition& mutation) noexcept {
    if (!has_item_replacement_metadata(mutation)) {
        return empty_item_replacement_metadata(mutation);
    }

    constexpr std::int32_t kMaxMutationSerial = (std::numeric_limits<std::int32_t>::max)();
    if (mutation.updatedInstanceSoid == mutation.acquiredInstanceSoid
        || mutation.updatedInstanceSoid == 0
        || mutation.updatedInventoryIndex >= mutation.expectedInventoryCount
        || mutation.expectedInventoryCount >= authored_inventory::kCharacterItemCapacity
        || mutation.beforeCharacter.inventory.count != mutation.expectedInventoryCount
        || mutation.afterCharacter.inventory.count != mutation.expectedInventoryCount + 1U
        || mutation.updatedBeforeDefinitionHash == authored_inventory::kNoDefinitionHash
        || mutation.updatedAfterDefinitionHash == authored_inventory::kNoDefinitionHash
        || mutation.updatedBeforeDefinitionHash == mutation.updatedAfterDefinitionHash
        || mutation.updatedBeforeMutationSerial < 0
        || mutation.updatedAfterMutationSerial <= mutation.updatedBeforeMutationSerial
        || mutation.updatedAfterMutationSerial >= kMaxMutationSerial
        || mutation.expectedNextInventorySerial >=
               static_cast<std::uint32_t>(kMaxMutationSerial)
        || mutation.afterCharacter.nextInventorySerial
               != mutation.expectedNextInventorySerial + 2U) {
        return false;
    }

    build_data::items::Definition beforeDefinition{};
    build_data::items::Definition afterDefinition{};
    if (!definition_identity(mutation.updatedBeforeDefinitionHash,
                             mutation.updatedBeforeDefinitionIndex,
                             beforeDefinition)
        || !definition_identity(mutation.updatedAfterDefinitionHash,
                                mutation.updatedAfterDefinitionIndex,
                                afterDefinition)) {
        return false;
    }

    const account::inventory::Item& beforeItem =
        mutation.beforeCharacter.inventory.values[mutation.updatedInventoryIndex];
    const account::inventory::Item& afterItem =
        mutation.afterCharacter.inventory.values[mutation.updatedInventoryIndex];
    if (beforeItem.instanceSoid != mutation.updatedInstanceSoid
        || beforeItem.definitionHash != mutation.updatedBeforeDefinitionHash
        || beforeItem.mutationSerial != mutation.updatedBeforeMutationSerial
        || afterItem.instanceSoid != mutation.updatedInstanceSoid
        || afterItem.definitionHash != mutation.updatedAfterDefinitionHash
        || afterItem.level != beforeItem.level || afterItem.quantity != beforeItem.quantity
        || afterItem.flags != beforeItem.flags
        || afterItem.mutationSerial != mutation.updatedAfterMutationSerial
        || !no_existing_definition(mutation.beforeCharacter,
                                    mutation.updatedAfterDefinitionHash)) {
        return false;
    }

    CharacterItemLocation location{};
    if (!find_character_item_location(
            mutation.beforeCharacter, mutation.updatedInstanceSoid, location)
        || location.equipped || location.index != mutation.updatedInventoryIndex) {
        return false;
    }

    // Everything durable on the row survives the definition change; only the definition, the
    // serial and the socket lane (native defaults for the new definition) are allowed to differ.
    account::inventory::Item expectedUpdated{};
    expectedUpdated.instanceSoid = beforeItem.instanceSoid;
    expectedUpdated.definitionHash = mutation.updatedAfterDefinitionHash;
    expectedUpdated.level = beforeItem.level;
    expectedUpdated.quantity = beforeItem.quantity;
    expectedUpdated.mutationSerial = mutation.updatedAfterMutationSerial;
    expectedUpdated.flags = beforeItem.flags;
    if (!same_stationary_item(afterItem, expectedUpdated)) {
        return false;
    }

    account::inventory::Item expectedAcquired{};
    expectedAcquired.instanceSoid = mutation.acquiredInstanceSoid;
    expectedAcquired.definitionHash = mutation.acquiredDefinitionHash;
    expectedAcquired.level = acquisition_level(mutation.beforeCharacter);
    expectedAcquired.quantity = 1;
    expectedAcquired.mutationSerial =
        static_cast<std::int32_t>(mutation.expectedNextInventorySerial);
    if (!same_stationary_item(mutation.afterCharacter.inventory.values[
                                  mutation.expectedInventoryCount],
                              expectedAcquired)) {
        return false;
    }

    for (std::size_t index = 0; index < mutation.beforeCharacter.equipment.slots.size(); ++index) {
        const auto& before = mutation.beforeCharacter.equipment.slots[index];
        const auto& after = mutation.afterCharacter.equipment.slots[index];
        if (before.has_value() != after.has_value()
            || (before.has_value() && !same_stationary_item(*before, *after))) {
            return false;
        }
    }
    for (std::size_t index = 0; index < mutation.expectedInventoryCount; ++index) {
        if (index != mutation.updatedInventoryIndex
            && !same_stationary_item(mutation.beforeCharacter.inventory.values[index],
                                     mutation.afterCharacter.inventory.values[index])) {
            return false;
        }
    }
    for (std::size_t index = mutation.afterCharacter.inventory.count;
         index < mutation.afterCharacter.inventory.values.size();
         ++index) {
        if (!same_stationary_item(mutation.beforeCharacter.inventory.values[index],
                                  mutation.afterCharacter.inventory.values[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

/** Prepares one native-row-checked selected-character inventory insertion. */
bool prepare_item_acquisition(
    std::uint16_t collectibleIndex,
    std::uint32_t definitionHash,
    PendingItemAcquisition& mutation,
    std::span<const build_data::material_requirements::Requirement> cost,
    ItemAcquisitionOptions options) noexcept {
    mutation = {};
    const AccountState account = account_snapshot();
    build_data::collectibles::Definition collectible{};
    build_data::items::Definition grantedDefinition{};
    const bool hasCollectible = collectibleIndex != build_data::collectibles::kNoCollectibleIndex;
    if (definitionHash == authored_inventory::kNoDefinitionHash || !account::valid(account)
        || !valid_profile_inventory(account)) {
        report_acquisition("prepare", "fail", "input", definitionHash, 0, 0, 0, 0, 0, 0);
        return false;
    }
    if (hasCollectible) {
        if (!build_data::find_collectible_definition(collectibleIndex, collectible)
            || collectible.itemDefinitionIndex
                   == build_data::collectibles::kUnavailableItemDefinitionIndex
            || !build_data::find_item_definition_index(collectible.itemDefinitionIndex,
                                                       grantedDefinition)
            || grantedDefinition.definitionHash != definitionHash) {
            report_acquisition("prepare", "fail", "input", definitionHash, 0, 0, 0, 0, 0, 0);
            return false;
        }
    } else if (!build_data::find_item_definition_hash(definitionHash, grantedDefinition)
               || grantedDefinition.definitionHash != definitionHash) {
        report_acquisition("prepare", "fail", "item", definitionHash, 0, 0, 0, 0, 0, 0);
        return false;
    }

    // An authored cost REPLACES the collectible's own installed set; the two never stack. The
    // collectible's set is the Collections re-pull price, not a vendor's, and running both would
    // bump the same rows' mutation serials twice over an array the first pass may have compacted.
    AccountState chargedAccount = account;
    bool profileChanged = false;
    if (hasCollectible && cost.empty()
        && !apply_collection_materials(account, collectible, chargedAccount, profileChanged)) {
        report_acquisition("prepare", "fail", "materials", definitionHash, 0, 0, 0, 0, 0, 0);
        return false;
    }
    if (!cost.empty()) {
        AccountState pricedAccount = chargedAccount;
        bool costCharged = false;
        if (!apply_authored_cost(chargedAccount, cost, pricedAccount, costCharged)) {
            // `price` rather than `materials`, so an unaffordable authored cost is never read as
            // an unaffordable collectible set.
            report_acquisition("prepare", "fail", "price", definitionHash, 0, 0, 0, 0, 0, 0);
            return false;
        }
        chargedAccount = pricedAccount;
        profileChanged = profileChanged || costCharged;
    }

    std::size_t characterIndex = account.characterCount;
    for (std::size_t index = 0; index < account.characterCount; ++index) {
        if (account.characters[index].selected) {
            characterIndex = index;
            break;
        }
    }
    if (characterIndex == account.characterCount) {
        report_acquisition("prepare", "fail", "selection", definitionHash, 0, 0, 0, 0, 0, 0);
        return false;
    }

    const CharacterState& before = account.characters[characterIndex];
    if (before.inventory.count >= before.inventory.values.size()
        || before.nextInventorySerial
               >= static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)())) {
        report_acquisition("prepare",
                           "fail",
                           "state_capacity",
                           definitionHash,
                           before.soid,
                           0,
                           before.inventory.count,
                           0,
                           0,
                           before.nextInventorySerial);
        return false;
    }

    std::uint64_t instanceSoid = 0;
    if (!next_item_instance_soid(account, instanceSoid)) {
        report_acquisition("prepare",
                           "fail",
                           "soid",
                           definitionHash,
                           before.soid,
                           0,
                           before.inventory.count,
                           0,
                           0,
                           before.nextInventorySerial);
        return false;
    }

    CharacterState after = before;
    const std::size_t inventoryIndex = after.inventory.count;
    authored_inventory::Item acquired{};
    acquired.instanceSoid = instanceSoid;
    acquired.definitionHash = definitionHash;
    acquired.level = acquisition_level(before);
    acquired.quantity = 1;
    acquired.mutationSerial = static_cast<std::int32_t>(after.nextInventorySerial++);
    acquired.sockets.policy = authored_inventory::SocketPolicy::nativeDefaults;
    // Collections reclaims retain their fixed roll. Direct rewards share installed legal pools.
    if (!hasCollectible && options.allowRandomRoll) {
        item_details::Definition detail{};
        if (!build_data::find_configured_item_detail(grantedDefinition.definitionIndex, detail)
            || detail.definitionHash != definitionHash || detail.bucketId != grantedDefinition.bucketId) {
            report_acquisition("prepare", "fail", "roll_detail", definitionHash,
                               before.soid, instanceSoid, inventoryIndex, 0, 0, 0);
            return false;
        }
        const auto seed = options.seed != 0 ? options.seed
            : static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        (void)roll_random_bytes(acquired, grantedDefinition, detail, seed);
    }
    after.inventory.values[inventoryIndex] = acquired;
    ++after.inventory.count;

    AccountState candidate = chargedAccount;
    candidate.characters[characterIndex] = after;
    family4_loadout::ResolvedLoadout resolved{};
    std::uint16_t inventoryRow = 0;
    std::uint8_t equipmentSlot = 0;
    if (!account::valid(candidate) || identity_uses_soid(candidate, instanceSoid)
        || !family4_loadout::resolve(candidate, characterIndex, resolved)
        || !find_acquired_row(resolved, instanceSoid, inventoryRow, equipmentSlot)) {
        report_acquisition("prepare",
                           "fail",
                           "resolve_or_bucket_full",
                           definitionHash,
                           before.soid,
                           instanceSoid,
                           inventoryIndex,
                           0,
                           0,
                           after.nextInventorySerial);
        return false;
    }

    mutation.beforeCharacter = before;
    mutation.afterCharacter = after;
    mutation.beforeProfileItems = account.profileItems;
    mutation.afterProfileItems = chargedAccount.profileItems;
    mutation.accountSoid = account.primarySoid;
    mutation.characterSoid = before.soid;
    mutation.acquiredInstanceSoid = instanceSoid;
    mutation.acquiredDefinitionHash = definitionHash;
    mutation.materialRequirementSetHash = collectible.materialRequirementSetHash;
    mutation.expectedNextInventorySerial = before.nextInventorySerial;
    mutation.characterIndex = characterIndex;
    mutation.expectedInventoryCount = before.inventory.count;
    mutation.expectedProfileItemCount = account.profileItemCount;
    mutation.afterProfileItemCount = chargedAccount.profileItemCount;
    mutation.inventoryIndex = inventoryIndex;
    mutation.collectibleIndex = collectibleIndex;
    mutation.inventoryRow = inventoryRow;
    mutation.equipmentSlot = equipmentSlot;
    mutation.materialRequirementCount = collectible.materialRequirementCount;
    mutation.profileChanged = profileChanged;
    mutation.prepared = true;
    report_acquisition("prepare",
                       "ok",
                       "ready",
                       definitionHash,
                       before.soid,
                       instanceSoid,
                       inventoryIndex,
                       inventoryRow,
                       equipmentSlot,
                       after.nextInventorySerial);
    return true;
}

/** Resolves a direct acquisition through the same priced, atomic insertion path. */
bool prepare_item_acquisition_for_item(
    std::uint16_t itemDefinitionIndex, PendingItemAcquisition& mutation,
    ItemAcquisitionOptions options,
    std::span<const build_data::material_requirements::Requirement> cost) noexcept {
    build_data::items::Definition definition{};
    if (!build_data::find_item_definition_index(itemDefinitionIndex, definition)) {
        mutation = {};
        return false;
    }
    return prepare_item_acquisition(build_data::collectibles::kNoCollectibleIndex,
                                     definition.definitionHash, mutation, cost, options);
}

/** Adds one checked pre-existing item replacement without touching live State. */
bool stage_item_replacement(PendingItemAcquisition& mutation,
                            std::uint64_t instanceSoid,
                            std::size_t inventoryIndex,
                            std::uint32_t beforeDefinitionHash,
                            std::uint32_t afterDefinitionHash) noexcept {
    if (!mutation.prepared || mutation.updatedInstanceSoid != 0 || instanceSoid == 0
        || instanceSoid == mutation.acquiredInstanceSoid
        || beforeDefinitionHash == authored_inventory::kNoDefinitionHash
        || afterDefinitionHash == authored_inventory::kNoDefinitionHash
        || beforeDefinitionHash == afterDefinitionHash
        || mutation.characterIndex >= kCharacterCapacity
        || inventoryIndex >= mutation.expectedInventoryCount
        || mutation.expectedInventoryCount >= authored_inventory::kCharacterItemCapacity) {
        return false;
    }

    const AccountState current = account_snapshot();
    if (mutation.characterIndex >= current.characterCount
        || current.primarySoid != mutation.accountSoid
        || !same_character(current.characters[mutation.characterIndex], mutation.beforeCharacter)
        || !same_profile_inventory(
            current, mutation.beforeProfileItems, mutation.expectedProfileItemCount)) {
        return false;
    }

    const CharacterState& before = mutation.beforeCharacter;
    if (before.inventory.count != mutation.expectedInventoryCount
        || inventoryIndex >= before.inventory.count
        || before.inventory.values[inventoryIndex].instanceSoid != instanceSoid
        || before.inventory.values[inventoryIndex].definitionHash != beforeDefinitionHash
        || !no_existing_definition(before, afterDefinitionHash)) {
        return false;
    }
    CharacterItemLocation location{};
    if (!find_character_item_location(before, instanceSoid, location) || location.equipped
        || location.index != inventoryIndex) {
        return false;
    }

    build_data::items::Definition beforeDefinition{};
    build_data::items::Definition afterDefinition{};
    if (!build_data::find_item_definition_hash(beforeDefinitionHash, beforeDefinition)
        || beforeDefinition.definitionHash != beforeDefinitionHash
        || !build_data::find_item_definition_hash(afterDefinitionHash, afterDefinition)
        || afterDefinition.definitionHash != afterDefinitionHash
        || before.nextInventorySerial >=
               static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)())
        || mutation.afterCharacter.inventory.count != mutation.expectedInventoryCount + 1U
        || mutation.expectedNextInventorySerial != before.nextInventorySerial
        || mutation.afterCharacter.nextInventorySerial != before.nextInventorySerial + 1U) {
        return false;
    }

    PendingItemAcquisition staged = mutation;
    const std::int32_t updatedMutationSerial =
        static_cast<std::int32_t>(staged.afterCharacter.nextInventorySerial);
    if (updatedMutationSerial >= (std::numeric_limits<std::int32_t>::max)()) {
        return false;
    }
    // Copy the row, then change only what the step advance owns: the definition, a fresh serial
    // and a native-default socket lane for the new definition. Level, quantity and the native
    // item-state flags ride across unchanged.
    account::inventory::Item replacement = before.inventory.values[inventoryIndex];
    replacement.definitionHash = afterDefinitionHash;
    replacement.mutationSerial = updatedMutationSerial;
    replacement.sockets = {};
    replacement.sockets.policy = authored_inventory::SocketPolicy::nativeDefaults;
    staged.afterCharacter.inventory.values[inventoryIndex] = replacement;
    ++staged.afterCharacter.nextInventorySerial;

    // Resolve against the same after-image preview and commit will use: the charged profile view
    // of the prepared acquisition, not the live one.
    AccountState candidate = current;
    candidate.profileItems = staged.afterProfileItems;
    candidate.profileItemCount = staged.afterProfileItemCount;
    candidate.characters[staged.characterIndex] = staged.afterCharacter;
    family4_loadout::ResolvedLoadout resolved{};
    std::uint16_t updatedInventoryRow = 0;
    std::uint8_t updatedEquipmentSlot = 0;
    if (!account::valid(candidate) || !valid_profile_inventory(candidate)
        || !family4_loadout::resolve(candidate, staged.characterIndex, resolved)
        || !find_unequipped_row(resolved,
                                instanceSoid,
                                updatedInventoryRow,
                                updatedEquipmentSlot)) {
        return false;
    }

    staged.updatedInstanceSoid = instanceSoid;
    staged.updatedInventoryIndex = inventoryIndex;
    staged.updatedInventoryRow = updatedInventoryRow;
    staged.updatedEquipmentSlot = updatedEquipmentSlot;
    staged.updatedBeforeDefinitionIndex = beforeDefinition.definitionIndex;
    staged.updatedAfterDefinitionIndex = afterDefinition.definitionIndex;
    staged.updatedBeforeDefinitionHash = beforeDefinitionHash;
    staged.updatedAfterDefinitionHash = afterDefinitionHash;
    staged.updatedBeforeMutationSerial = before.inventory.values[inventoryIndex].mutationSerial;
    staged.updatedAfterMutationSerial = updatedMutationSerial;
    if (!valid_item_replacement_image(staged)) {
        return false;
    }
    mutation = staged;
    report_acquisition("stage_update",
                       "ok",
                       "ready",
                       afterDefinitionHash,
                       mutation.characterSoid,
                       instanceSoid,
                       inventoryIndex,
                       updatedInventoryRow,
                       updatedEquipmentSlot,
                       mutation.afterCharacter.nextInventorySerial);
    return true;
}

bool stage_item_profile_rewards(PendingItemAcquisition& mutation,
    std::span<const ProfileExchangePayout> payouts) noexcept {
    AccountState candidate{};
    if (!preview_item_acquisition(mutation, candidate)) return false;
    std::int32_t serialFloor{};
    for (std::size_t i = 0; i < mutation.expectedProfileItemCount; ++i) {
        serialFloor = (std::max)(serialFloor, mutation.beforeProfileItems[i].mutationSerial);
    }
    const auto resolve = [](std::uint32_t hash, std::int32_t& maximum) noexcept {
        build_data::items::Definition item{};
        item_details::Definition detail{};
        inventory_buckets::Descriptor bucket{};
        if (!build_data::find_item_definition_hash(hash, item)
            || !build_data::find_configured_item_detail(item.definitionIndex, detail)
            || detail.definitionHash != hash || detail.bucketId != item.bucketId
            || detail.instancedDefinitionState != item_details::InstancedDefinitionState::stackable
            || !build_data::find_inventory_bucket_descriptor(detail.bucketId, bucket)
            || bucket.arraySelector != inventory_buckets::ArraySelector::profile
            || build_data::is_profile_action_source(item.definitionIndex, item.bucketId)) return false;
        maximum = detail.maxStackSize;
        return maximum > 0;
    };
    if (!runtime::detail::stage_profile_rewards(candidate, payouts, serialFloor, resolve)
        || !account::valid(candidate) || !valid_profile_inventory(candidate)) return false;
    mutation.afterProfileItems = candidate.profileItems;
    mutation.afterProfileItemCount = candidate.profileItemCount;
    mutation.profileChanged = !same_profile_inventory(
        candidate, mutation.beforeProfileItems, mutation.expectedProfileItemCount);
    return true;
}

/** Produces the full account after-image while a prepared character pull remains current. */
bool preview_item_acquisition(const PendingItemAcquisition& mutation,
                              AccountState& after) noexcept {
    after = {};
    if (!mutation.prepared || mutation.accountSoid == 0 || mutation.characterSoid == 0
        || mutation.acquiredInstanceSoid == 0 || mutation.characterIndex >= kCharacterCapacity
        || mutation.expectedProfileItemCount > authored_inventory::kProfileItemCapacity
        || mutation.afterProfileItemCount > authored_inventory::kProfileItemCapacity) {
        return false;
    }
    const AccountState current = account_snapshot();
    if (mutation.characterIndex >= current.characterCount
        || current.primarySoid != mutation.accountSoid
        || !same_character(current.characters[mutation.characterIndex], mutation.beforeCharacter)
        || !same_profile_inventory(
            current, mutation.beforeProfileItems, mutation.expectedProfileItemCount)) {
        return false;
    }
    after = current;
    after.profileItems = mutation.afterProfileItems;
    after.profileItemCount = mutation.afterProfileItemCount;
    after.characters[mutation.characterIndex] = mutation.afterCharacter;
    if (!valid_item_replacement_image(mutation)) {
        return false;
    }
    family4_loadout::ResolvedLoadout resolved{};
    std::uint16_t checkedRow = 0;
    std::uint8_t checkedSlot = 0;
    if (!account::valid(after) || !valid_profile_inventory(after)) {
        return false;
    }
    if (!family4_loadout::resolve(after, mutation.characterIndex, resolved)
        || !find_acquired_row(resolved, mutation.acquiredInstanceSoid, checkedRow, checkedSlot)
        || checkedRow != mutation.inventoryRow || checkedSlot != mutation.equipmentSlot) {
        return false;
    }
    if (has_item_replacement_metadata(mutation)) {
        return find_unequipped_row(resolved,
                                   mutation.updatedInstanceSoid,
                                   checkedRow,
                                   checkedSlot)
               && checkedRow == mutation.updatedInventoryRow
               && checkedSlot == mutation.updatedEquipmentSlot;
    }
    return true;
}

/** Commits one prepared insertion only while its prepare-time loadout remains current. */
bool commit_item_acquisition(PendingItemAcquisition& mutation) noexcept {
    const PendingItemAcquisition prepared = mutation;
    mutation = {};
    const auto fail = [&prepared](std::string_view reason) noexcept {
        report_acquisition("commit",
                           "fail",
                           reason,
                           prepared.acquiredDefinitionHash,
                           prepared.characterSoid,
                           prepared.acquiredInstanceSoid,
                           prepared.inventoryIndex,
                           prepared.inventoryRow,
                           prepared.equipmentSlot,
                           prepared.afterCharacter.nextInventorySerial);
        return false;
    };
    if (!prepared.prepared || prepared.characterSoid == 0 || prepared.acquiredInstanceSoid == 0
        || prepared.accountSoid == 0
        || prepared.acquiredDefinitionHash == authored_inventory::kNoDefinitionHash
        || prepared.characterIndex >= kCharacterCapacity
        || prepared.expectedInventoryCount >= authored_inventory::kCharacterItemCapacity
        || prepared.inventoryIndex != prepared.expectedInventoryCount
        || prepared.afterCharacter.inventory.count != prepared.expectedInventoryCount + 1U
        || prepared.inventoryIndex >= prepared.afterCharacter.inventory.count
        || prepared.afterCharacter.inventory.values[prepared.inventoryIndex].instanceSoid
               != prepared.acquiredInstanceSoid
        || prepared.afterCharacter.inventory.values[prepared.inventoryIndex].definitionHash
               != prepared.acquiredDefinitionHash
        || prepared.expectedProfileItemCount > authored_inventory::kProfileItemCapacity
        || prepared.afterProfileItemCount > authored_inventory::kProfileItemCapacity) {
        return fail("mutation");
    }
    if (!valid_item_replacement_image(prepared)) {
        return fail("update");
    }

    if (prepared.collectibleIndex == build_data::collectibles::kNoCollectibleIndex) {
        if (prepared.materialRequirementSetHash != 0 || prepared.materialRequirementCount != 0) {
            return fail("collectible");
        }
    } else {
        build_data::collectibles::Definition collectible{};
        if (!build_data::find_collectible_definition(prepared.collectibleIndex, collectible)
            || collectible.itemDefinitionIndex
                   == build_data::collectibles::kUnavailableItemDefinitionIndex
            || collectible.materialRequirementSetHash != prepared.materialRequirementSetHash
            || collectible.materialRequirementCount != prepared.materialRequirementCount) {
            return fail("collectible");
        }
    }

    report_acquisition("commit_begin",
                       "ok",
                       "ready",
                       prepared.acquiredDefinitionHash,
                       prepared.characterSoid,
                       prepared.acquiredInstanceSoid,
                       prepared.inventoryIndex,
                       prepared.inventoryRow,
                       prepared.equipmentSlot,
                       prepared.afterCharacter.nextInventorySerial);

    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    AccountState candidate = runtime::storage::g_state.account;
    if (prepared.characterIndex >= candidate.characterCount
        || candidate.primarySoid != prepared.accountSoid
        || !same_profile_inventory(
            candidate, prepared.beforeProfileItems, prepared.expectedProfileItemCount)) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return fail("account");
    }
    CharacterState& character = candidate.characters[prepared.characterIndex];
    if (!character.selected || character.soid != prepared.characterSoid
        || !same_character(character, prepared.beforeCharacter)) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return fail("stale");
    }

    candidate.profileItems = prepared.afterProfileItems;
    candidate.profileItemCount = prepared.afterProfileItemCount;
    character = prepared.afterCharacter;
    family4_loadout::ResolvedLoadout resolved{};
    std::uint16_t checkedRow = 0;
    std::uint8_t checkedSlot = 0;
    if (!account::valid(candidate) || !valid_profile_inventory(candidate)
        || identity_uses_soid(candidate, prepared.acquiredInstanceSoid)
        || !family4_loadout::resolve(candidate, prepared.characterIndex, resolved)
        || !find_acquired_row(resolved, prepared.acquiredInstanceSoid, checkedRow, checkedSlot)
        || checkedRow != prepared.inventoryRow || checkedSlot != prepared.equipmentSlot) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return fail("resolve");
    }
    // The updated row is gated here exactly as preview gated it, so commit can never publish a
    // quest step whose native row drifted between the two.
    if (has_item_replacement_metadata(prepared)
        && (!find_unequipped_row(resolved, prepared.updatedInstanceSoid, checkedRow, checkedSlot)
            || checkedRow != prepared.updatedInventoryRow
            || checkedSlot != prepared.updatedEquipmentSlot)) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return fail("resolve_update");
    }
    if (!persistence::commit_account(runtime::storage::g_state.account, candidate)) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return fail("persistence");
    }
    runtime::storage::g_state.account = candidate;
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_acquisition("commit_end",
                       "ok",
                       "published",
                       prepared.acquiredDefinitionHash,
                       prepared.characterSoid,
                       prepared.acquiredInstanceSoid,
                       prepared.inventoryIndex,
                       prepared.inventoryRow,
                       prepared.equipmentSlot,
                       prepared.afterCharacter.nextInventorySerial);
    return true;
}

/** Prepares one checked profile-stack increment or append for a Collections pull. */
bool prepare_profile_item_acquisition(
    std::uint16_t collectibleIndex,
    std::uint32_t definitionHash,
    PendingProfileItemAcquisition& mutation,
    std::span<const build_data::material_requirements::Requirement> cost, std::int32_t quantity) noexcept {
    mutation = {};
    const AccountState account = account_snapshot();
    build_data::collectibles::Definition collectible{};
    build_data::items::Definition item{};
    item_details::Definition detail{};
    inventory_buckets::Descriptor bucket{};
    if (definitionHash == authored_inventory::kNoDefinitionHash || !account::valid(account)
        || !valid_profile_inventory(account)
        || !build_data::find_item_definition_hash(definitionHash, item)
        || item.definitionHash != definitionHash
        || (collectibleIndex != build_data::collectibles::kNoCollectibleIndex
            && (!build_data::find_collectible_definition(collectibleIndex, collectible)
                || collectible.itemDefinitionIndex
                       == build_data::collectibles::kUnavailableItemDefinitionIndex
                || item.definitionIndex != collectible.itemDefinitionIndex))
        || !build_data::find_configured_item_detail(item.definitionIndex, detail)
        || detail.definitionIndex != item.definitionIndex || detail.definitionHash != definitionHash
        || detail.bucketId != item.bucketId
        || detail.instancedDefinitionState != item_details::InstancedDefinitionState::stackable
        || detail.maxStackSize <= 0
        || !build_data::find_inventory_bucket_descriptor(detail.bucketId, bucket)
        || bucket.arraySelector != inventory_buckets::ArraySelector::profile) {
        report_profile_acquisition("prepare",
                                   "fail",
                                   "definition_or_profile",
                                   definitionHash,
                                   account.primarySoid,
                                   0,
                                   0,
                                   0,
                                   account.profileItemCount,
                                   0,
                                   0,
                                   false);
        return false;
    }
    // As on the character path, an authored cost REPLACES the collectible's installed set.
    AccountState chargedAccount = account;
    bool materialsChanged = false;
    if (collectibleIndex != build_data::collectibles::kNoCollectibleIndex && cost.empty()
        && !apply_collection_materials(account, collectible, chargedAccount, materialsChanged)) {
        report_profile_acquisition("prepare",
                                   "fail",
                                   "materials",
                                   definitionHash,
                                   account.primarySoid,
                                   0,
                                   detail.bucketId,
                                   0,
                                   account.profileItemCount,
                                   0,
                                   0,
                                   false);
        return false;
    }
    if (!cost.empty()) {
        AccountState pricedAccount = chargedAccount;
        bool costCharged = false;
        if (!apply_authored_cost(chargedAccount, cost, pricedAccount, costCharged)) {
            report_profile_acquisition("prepare",
                                       "fail",
                                       "price",
                                       definitionHash,
                                       account.primarySoid,
                                       0,
                                       detail.bucketId,
                                       0,
                                       account.profileItemCount,
                                       0,
                                       0,
                                       false);
            return false;
        }
        chargedAccount = pricedAccount;
        materialsChanged = materialsChanged || costCharged;
    }
    (void)materialsChanged;
    const bool actionSource =
        build_data::is_profile_action_source(item.definitionIndex, item.bucketId);

    std::size_t profileIndex = chargedAccount.profileItemCount;
    std::int32_t previousQuantity = 0;
    std::int32_t previousMutationSerial = 0;
    std::int32_t greatestMutationSerial = 0;
    bool appended = true;
    // The serial has to clear BOTH images. `mutation.beforeItems` is the uncharged account and
    // `valid_profile_mutation_shape` refuses any before-image row whose serial is not strictly
    // below the acquired one - including a row the charge consumed to nothing and dropped. That
    // row's serial is invisible in `chargedAccount`, and `apply_material_requirements` only
    // re-serials rows whose position moved, so emptying the LAST row bumps nothing at all. Taking
    // the maximum over both images is what keeps "spend the stack to exactly zero and buy the
    // thing" from being refused as a malformed mutation.
    for (std::size_t index = 0; index < account.profileItemCount; ++index) {
        greatestMutationSerial =
            (std::max)(greatestMutationSerial, account.profileItems[index].mutationSerial);
    }
    for (std::size_t index = 0; index < chargedAccount.profileItemCount; ++index) {
        greatestMutationSerial =
            (std::max)(greatestMutationSerial, chargedAccount.profileItems[index].mutationSerial);
    }
    for (std::size_t index = 0; index < chargedAccount.profileItemCount; ++index) {
        const authored_inventory::ProfileItem& existing = chargedAccount.profileItems[index];
        if (existing.definitionHash != definitionHash) {
            continue;
        }
        if (existing.quantity > detail.maxStackSize) {
            report_profile_acquisition("prepare",
                                       "fail",
                                       "existing_quantity",
                                       definitionHash,
                                       account.primarySoid,
                                       existing.instanceSoid,
                                       detail.bucketId,
                                       index,
                                       chargedAccount.profileItemCount,
                                       existing.quantity,
                                       existing.quantity,
                                       false);
            return false;
        }
        if (appended && existing.quantity < detail.maxStackSize) {
            profileIndex = index;
            previousQuantity = existing.quantity;
            previousMutationSerial = existing.mutationSerial;
            appended = false;
        }
    }
    if (greatestMutationSerial == (std::numeric_limits<std::int32_t>::max)()
        || (appended && chargedAccount.profileItemCount >= chargedAccount.profileItems.size())) {
        report_profile_acquisition(
            "prepare",
            "fail",
            "state_capacity",
            definitionHash,
            account.primarySoid,
            appended ? 0 : chargedAccount.profileItems[profileIndex].instanceSoid,
            detail.bucketId,
            profileIndex,
            chargedAccount.profileItemCount,
            0,
            0,
            true);
        return false;
    }

    std::uint64_t acquiredInstanceSoid =
        appended ? 0 : chargedAccount.profileItems[profileIndex].instanceSoid;
    if (actionSource != (acquiredInstanceSoid != 0) && !appended) {
        report_profile_acquisition("prepare",
                                   "fail",
                                   "identity_policy",
                                   definitionHash,
                                   account.primarySoid,
                                   acquiredInstanceSoid,
                                   detail.bucketId,
                                   profileIndex,
                                   chargedAccount.profileItemCount,
                                   previousQuantity,
                                   previousQuantity,
                                   false);
        return false;
    }
    if (appended && actionSource
        && !next_profile_item_instance_soid(chargedAccount, acquiredInstanceSoid)) {
        report_profile_acquisition("prepare",
                                   "fail",
                                   "identity_capacity",
                                   definitionHash,
                                   account.primarySoid,
                                   acquiredInstanceSoid,
                                   detail.bucketId,
                                   profileIndex,
                                   chargedAccount.profileItemCount,
                                   0,
                                   0,
                                   true);
        return false;
    }

    if(quantity<=0 || quantity>detail.maxStackSize-previousQuantity)return false;
    AccountState after = chargedAccount;
    const std::int32_t acquiredMutationSerial = greatestMutationSerial + 1;
    if (appended) {
        after.profileItems[profileIndex] = {
            acquiredInstanceSoid, definitionHash, quantity, acquiredMutationSerial};
        ++after.profileItemCount;
    } else {
        after.profileItems[profileIndex].quantity+=quantity;
        after.profileItems[profileIndex].mutationSerial = acquiredMutationSerial;
    }
    const std::int32_t acquiredQuantity = after.profileItems[profileIndex].quantity;
    if (after.profileItems[profileIndex].instanceSoid != acquiredInstanceSoid
        || acquiredQuantity <= previousQuantity || acquiredQuantity > detail.maxStackSize
        || !account::valid(after) || !valid_profile_inventory(after)) {
        report_profile_acquisition("prepare",
                                   "fail",
                                   "bucket_full_or_quantity",
                                   definitionHash,
                                   account.primarySoid,
                                   acquiredInstanceSoid,
                                   detail.bucketId,
                                   profileIndex,
                                   after.profileItemCount,
                                   previousQuantity,
                                   acquiredQuantity,
                                   appended);
        return false;
    }

    mutation.beforeItems = account.profileItems;
    mutation.afterItems = after.profileItems;
    mutation.accountSoid = account.primarySoid;
    mutation.acquiredInstanceSoid = acquiredInstanceSoid;
    mutation.acquiredDefinitionHash = definitionHash;
    mutation.materialRequirementSetHash = collectible.materialRequirementSetHash;
    mutation.expectedItemCount = account.profileItemCount;
    mutation.afterItemCount = after.profileItemCount;
    mutation.profileIndex = profileIndex;
    mutation.previousQuantity = previousQuantity;
    mutation.acquiredQuantity = acquiredQuantity;
    mutation.grantQuantity = quantity;
    mutation.previousMutationSerial = previousMutationSerial;
    mutation.acquiredMutationSerial = acquiredMutationSerial;
    mutation.collectibleIndex = collectibleIndex;
    mutation.bucketId = detail.bucketId;
    mutation.materialRequirementCount = collectible.materialRequirementCount;
    mutation.actionSource = actionSource;
    mutation.appended = appended;
    mutation.prepared = true;
    if (!valid_profile_mutation_shape(mutation)) {
        mutation = {};
        report_profile_acquisition("prepare",
                                   "fail",
                                   "mutation",
                                   definitionHash,
                                   account.primarySoid,
                                   acquiredInstanceSoid,
                                   detail.bucketId,
                                   profileIndex,
                                   after.profileItemCount,
                                   previousQuantity,
                                   acquiredQuantity,
                                   appended);
        return false;
    }
    report_profile_acquisition("prepare",
                               "ok",
                               "ready",
                               definitionHash,
                               account.primarySoid,
                               acquiredInstanceSoid,
                               detail.bucketId,
                               profileIndex,
                               after.profileItemCount,
                               previousQuantity,
                               acquiredQuantity,
                               appended);
    return true;
}

/** Prepares a bounded free credit for one installed profile currency. */
ProfileCurrencyGrantResult
prepare_profile_currency_grant(std::uint32_t definitionHash,
                               std::int32_t quantity,
                               PendingProfileItemAcquisition& mutation) noexcept {
    mutation = {};
    const AccountState account = account_snapshot();
    build_data::items::Definition item{};
    item_details::Definition detail{};
    inventory_buckets::Descriptor bucket{};
    if (definitionHash == authored_inventory::kNoDefinitionHash || quantity <= 0
        || !account::valid(account) || !valid_profile_inventory(account)
        || !build_data::find_item_definition_hash(definitionHash, item)
        || item.definitionHash != definitionHash
        || !build_data::find_configured_item_detail(item.definitionIndex, detail)
        || detail.definitionIndex != item.definitionIndex || detail.definitionHash != definitionHash
        || detail.bucketId != item.bucketId
        || detail.instancedDefinitionState != item_details::InstancedDefinitionState::stackable
        || detail.maxStackSize <= 0
        || !build_data::find_inventory_bucket_descriptor(detail.bucketId, bucket)
        || bucket.arraySelector != inventory_buckets::ArraySelector::profile
        || bucket.slotCount != 1
        || build_data::is_profile_action_source(item.definitionIndex, item.bucketId)) {
        return ProfileCurrencyGrantResult::rejected;
    }

    std::size_t profileIndex = account.profileItemCount;
    std::size_t matches = 0;
    std::int32_t greatestMutationSerial = 0;
    for (std::size_t index = 0; index < account.profileItemCount; ++index) {
        const auto& existing = account.profileItems[index];
        greatestMutationSerial =
            (std::max)(greatestMutationSerial, existing.mutationSerial);
        if (existing.definitionHash == definitionHash) {
            ++matches;
            profileIndex = index;
        }
    }
    if (matches > 1 || greatestMutationSerial == (std::numeric_limits<std::int32_t>::max)()) {
        return ProfileCurrencyGrantResult::rejected;
    }
    if (matches == 1 && account.profileItems[profileIndex].quantity >= detail.maxStackSize) {
        return account.profileItems[profileIndex].quantity == detail.maxStackSize
                   ? ProfileCurrencyGrantResult::capped
                   : ProfileCurrencyGrantResult::rejected;
    }
    const bool appended = matches == 0;
    if (appended && account.profileItemCount >= account.profileItems.size()) {
        return ProfileCurrencyGrantResult::rejected;
    }
    const std::int32_t previousQuantity =
        appended ? 0 : account.profileItems[profileIndex].quantity;
    const std::int32_t credited =
        profile_currency_credit(previousQuantity, detail.maxStackSize, quantity);
    if (credited <= 0) {
        return ProfileCurrencyGrantResult::rejected;
    }
    AccountState after = account;
    const std::int32_t nextSerial = greatestMutationSerial + 1;
    if (appended) {
        after.profileItems[profileIndex] = {0, definitionHash, credited, nextSerial};
        ++after.profileItemCount;
    } else {
        after.profileItems[profileIndex].quantity += credited;
        after.profileItems[profileIndex].mutationSerial = nextSerial;
    }
    if (!account::valid(after) || !valid_profile_inventory(after)) {
        return ProfileCurrencyGrantResult::rejected;
    }
    mutation.beforeItems = account.profileItems;
    mutation.afterItems = after.profileItems;
    mutation.accountSoid = account.primarySoid;
    mutation.acquiredDefinitionHash = definitionHash;
    mutation.expectedItemCount = account.profileItemCount;
    mutation.afterItemCount = after.profileItemCount;
    mutation.profileIndex = profileIndex;
    mutation.previousQuantity = previousQuantity;
    mutation.acquiredQuantity = previousQuantity + credited;
    mutation.previousMutationSerial =
        appended ? 0 : account.profileItems[profileIndex].mutationSerial;
    mutation.acquiredMutationSerial = nextSerial;
    mutation.bucketId = detail.bucketId;
    mutation.appended = appended;
    mutation.rewardGrant = true;
    mutation.prepared = true;
    if (!valid_profile_mutation_shape(mutation)) {
        mutation = {};
        return ProfileCurrencyGrantResult::rejected;
    }
    return ProfileCurrencyGrantResult::prepared;
}

/** Produces the exact account after-image while the captured profile view is still current. */
bool preview_profile_item_acquisition(const PendingProfileItemAcquisition& mutation,
                                      AccountState& after) noexcept {
    after = {};
    const AccountState current = account_snapshot();
    const bool ready = materialize_profile_acquisition(current, mutation, after);
    report_profile_acquisition("preview",
                               ready ? "ok" : "fail",
                               ready ? "ready" : "stale_or_invalid",
                               mutation.acquiredDefinitionHash,
                               mutation.accountSoid,
                               mutation.acquiredInstanceSoid,
                               mutation.bucketId,
                               mutation.profileIndex,
                               mutation.afterItemCount,
                               mutation.previousQuantity,
                               mutation.acquiredQuantity,
                               mutation.appended);
    return ready;
}

/** Commits one profile-stack after-image only while its exact prepare-time view remains current. */
bool commit_profile_item_acquisition(PendingProfileItemAcquisition& mutation) noexcept {
    const PendingProfileItemAcquisition prepared = mutation;
    mutation = {};
    if (!valid_profile_mutation_shape(prepared)) {
        report_profile_acquisition("commit",
                                   "fail",
                                   "mutation",
                                   prepared.acquiredDefinitionHash,
                                   prepared.accountSoid,
                                   prepared.acquiredInstanceSoid,
                                   prepared.bucketId,
                                   prepared.profileIndex,
                                   prepared.afterItemCount,
                                   prepared.previousQuantity,
                                   prepared.acquiredQuantity,
                                   prepared.appended);
        return false;
    }

    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    AccountState candidate{};
    const bool ready =
        materialize_profile_acquisition(runtime::storage::g_state.account, prepared, candidate);
    const bool committed = ready
        && persistence::commit_account(runtime::storage::g_state.account, candidate);
    if (committed) {
        runtime::storage::g_state.account = candidate;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_profile_acquisition("commit",
                               committed ? "ok" : "fail",
                               committed ? "published" : "stale_invalid_or_not_durable",
                               prepared.acquiredDefinitionHash,
                               prepared.accountSoid,
                               prepared.acquiredInstanceSoid,
                               prepared.bucketId,
                               prepared.profileIndex,
                               prepared.afterItemCount,
                               prepared.previousQuantity,
                               prepared.acquiredQuantity,
                               prepared.appended);
    return committed;
}

/** Commits one completion credit and its durable debt as a single SQLite transaction. */
bool commit_profile_item_reward(PendingProfileItemAcquisition& mutation,
                                std::uint64_t debtId,
                                std::int32_t credited) noexcept {
    const PendingProfileItemAcquisition prepared = mutation;
    mutation = {};
    if (!valid_profile_mutation_shape(prepared) || !prepared.rewardGrant || debtId == 0
        || credited <= 0 || credited != prepared.acquiredQuantity - prepared.previousQuantity) {
        report_profile_acquisition("reward_commit", "fail", "mutation_or_debt",
                                   prepared.acquiredDefinitionHash, prepared.accountSoid,
                                   prepared.acquiredInstanceSoid, prepared.bucketId,
                                   prepared.profileIndex, prepared.afterItemCount,
                                   prepared.previousQuantity, prepared.acquiredQuantity,
                                   prepared.appended);
        return false;
    }

    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    AccountState candidate{};
    const bool ready =
        materialize_profile_acquisition(runtime::storage::g_state.account, prepared, candidate);
    const bool committed = ready
        && persistence::commit_account_and_reward(runtime::storage::g_state.account, candidate,
                                                  debtId, credited);
    if (committed) runtime::storage::g_state.account = candidate;
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_profile_acquisition("reward_commit", committed ? "ok" : "fail",
                               committed ? "published" : "stale_invalid_or_not_durable",
                               prepared.acquiredDefinitionHash, prepared.accountSoid,
                               prepared.acquiredInstanceSoid, prepared.bucketId,
                               prepared.profileIndex, prepared.afterItemCount,
                               prepared.previousQuantity, prepared.acquiredQuantity,
                               prepared.appended);
    return committed;
}

} // namespace sunrise::state
