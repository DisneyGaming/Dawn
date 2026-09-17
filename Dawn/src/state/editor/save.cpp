// SPDX-License-Identifier: GPL-3.0-only
#include <Windows.h>
#include "edit.h"
#include "../runtime/runtime.h"
#include "../runtime/storage/internal.h"
#include "../build_data/abilities/ability_bucket_catalog.h"
#include "../persistence/persistence.h"
#include "../../middleware/datagen/family4/loadout/loadout_resolver.h"
#include "../../client/content/items/packages/internal.h"
#include <memory>

namespace dawn::state::editor {
namespace {
namespace packages = client::content::items::packages;
namespace reader = middleware::content::packages::reader;
namespace tables = middleware::content::packages::tables;
bool ability_rows(const AccountState& account, const Catalog& catalog,
    std::span<build_data::abilities::Definition> rows, std::size_t& count) {
    // Keep the prebuilt combinations available after saving any character's loadout.
    if (!build_data::abilities::snapshot(rows, count)) return false;
    reader::BlockKeys keys{};
    auto scratch = std::make_unique<reader::Scratch>();
    struct Cleanup { reader::BlockKeys& keys; reader::Scratch& scratch;
        ~Cleanup() { reader::close_files(scratch); SecureZeroMemory(&keys, sizeof keys); } } cleanup{keys, *scratch};
    core::path::Buffer directory{};
    if (!packages::collect_keys(keys) || !packages::package_directory(directory)) return false;
    reader::Source source{directory.chars.data(), &keys};
    std::array<std::uint32_t, packages::kContainerCandidates> tags{};
    std::size_t tagCount{}; tables::Array array{};
    std::vector<std::byte> globals, root, table, definition, blob;
    bool found = false;
    if (!packages::investment_globals_tags(tags, tagCount)) return false;
    for (std::size_t i = 0; i < tagCount && !found; ++i) {
        std::uint32_t rootTag{}, tableTag{};
        found = reader::read_tag(source, *scratch, tags[i], globals) && tables::child_tag(globals, 0, rootTag)
            && reader::read_tag(source, *scratch, rootTag, root) && tables::slot_tag(root, 97, tableTag)
            && reader::read_tag(source, *scratch, tableTag, table) && tables::find_array_at(table, 8, array);
    }
    if (!found) return false;
    for (std::size_t i = 0; i < account.characterCount; ++i) {
        const auto& character = account.characters[i];
        const auto& subclass = character.equipment.slots[11];
        if (!subclass) continue;
        const auto* item = catalog.find(subclass->definitionHash);
        if (!item) return false;
        build_data::abilities::Selection selection{character.movementAbilityEntry, character.grenadeAbilityEntry,
            character.superAbilityEntry, character.meleeAbilityEntry, character.classAbilityEntry};
        build_data::abilities::Definition row{};
        if (!build_data::find_ability_buckets(item->detail.socketEntryListIndex, selection, row)) {
            tables::IndexRow index{};
            if (!tables::index_row(table, array, item->detail.socketEntryListIndex, index)
                || !reader::read_tag(source, *scratch, index.targetTag, definition)
                || !packages::build_ability_buckets(source, *scratch, definition, blob, selection, row)) return false;
            row.socketEntryListIndex = item->detail.socketEntryListIndex; row.selection = selection;
        }
        bool duplicate = false;
        for (std::size_t j = 0; j < count; ++j) if (rows[j].socketEntryListIndex == row.socketEntryListIndex && rows[j].selection == row.selection) duplicate = true;
        if (!duplicate) {
            if (count == rows.size()) return false;
            rows[count++] = row;
        }
    }
    return true;
}
}
bool save(Draft& draft, const Catalog& catalog, std::string& error) {
    if (!draft.dirty) { error = "No changes to save."; return false; }
    auto prepared = std::make_unique<AccountState>(draft.after);
    if (!prepare_commit(draft, catalog, *prepared, error)) return false;
    auto validation = std::make_unique<AccountState>(*prepared);
    auto resolved = std::make_unique<middleware::datagen::family4::loadout::ResolvedLoadout>();
    for (std::size_t c = 0; c < draft.after.characterCount; ++c) {
        const auto& character = draft.after.characters[c];
        unsigned exoticWeapons = 0, exoticArmor = 0;
        for (std::size_t slot = 0; slot < character.equipment.slots.size(); ++slot) {
            const auto& item = character.equipment.slots[slot];
            if (!item) continue;
            const auto* definition = catalog.find(item->definitionHash);
            if (!definition || definition->slot != slot || !fits_class(*definition, character.characterClass)) {
                error = "Equipped gear must match the character class and equipment slot."; return false;
            }
            exoticWeapons += definition->kind == GearKind::weapon && definition->definition.tier == 5;
            exoticArmor += definition->kind == GearKind::armor && definition->definition.tier == 5;
        }
        if (exoticWeapons > 1 || exoticArmor > 1) { error = "Only one exotic weapon and one exotic armor piece can be equipped."; return false; }
        for (std::size_t i = 0; i < validation->characterCount; ++i) validation->characters[i].selected = i == c;
        if (!middleware::datagen::family4::loadout::resolve(*validation, c, *resolved)) {
            error = "The loadout does not fit the game's inventory or socket layout. Check your changes."; return false;
        }
    }
    std::vector<build_data::abilities::Definition> abilities(build_data::abilities::kDefinitionCapacity);
    std::size_t abilityCount{};
    if (!ability_rows(draft.after, catalog, abilities, abilityCount)) { error = "The selected subclass abilities could not be resolved."; return false; }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& account = runtime::storage::g_state.account;
    bool success = false;
    if (account != draft.before) {
        error = "Your account changed while editing. Reload the account before saving; your draft has been kept.";
    } else if (!persistence::backup_for_editor()) {
        error = "Could not create the save backup. Your account was not changed.";
    } else if (!persistence::commit_account(account, *prepared)) {
        error = "Could not commit this save. Your account was not changed.";
    } else {
        account = *prepared;
        draft.after = draft.before = *prepared;
        draft.dirty = false;
        success = true;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (success) {
        // Publish the new subclass combinations immediately and persist them for the next launch.
        (void)build_data::publish_ability_buckets(std::span(abilities).first(abilityCount));
        error = "Saved. Restart the game to load your changes. Backup: Dawn/editor-backups.";
    }
    return success;
}
}
