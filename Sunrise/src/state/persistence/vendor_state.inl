// Private SQLite codec. Included inside persistence.cpp's anonymous namespace so
// vendor state uses the account transaction, revision and failure handling.
[[nodiscard]] bool write_vendor_owner(std::uint64_t owner,
    const vendors::ProgressBank& progress, const vendors::Unlocks& unlocks) noexcept {
    Statement rank{"INSERT INTO vendor_progress VALUES(?1,?2,?3,?4,?5)"};
    Statement unlock{"INSERT INTO vendor_unlocks VALUES(?1,?2,?3,?4,?5)"};
    if (!rank.ready() || !unlock.ready()) return false;
    for (std::size_t position = 0; position < progress.size(); ++position) {
        const auto& row = progress[position];
        if (row.vendor == 0xffff) continue;
        sqlite3_reset(rank.value); sqlite3_clear_bindings(rank.value);
        if (!bind_u64(rank.value, 1, owner) || !bind_i64(rank.value, 2, position)
            || !bind_i64(rank.value, 3, row.vendor) || !bind_i64(rank.value, 4, row.points)
            || !bind_i64(rank.value, 5, row.rewards) || !step_done(rank.value)) return false;
    }
    for (int kind = 0; kind != 2; ++kind) {
        const auto& rows = kind == 0 ? unlocks.flags : unlocks.values;
        for (std::size_t position = 0; position < rows.size(); ++position) {
            const auto& row = rows[position];
            sqlite3_reset(unlock.value); sqlite3_clear_bindings(unlock.value);
            if (!bind_u64(unlock.value, 1, owner) || !bind_i64(unlock.value, 2, kind)
                || !bind_i64(unlock.value, 3, position) || !bind_i64(unlock.value, 4, row.slot)
                || !bind_i64(unlock.value, 5, row.value) || !step_done(unlock.value)) return false;
        }
    }
    return true;
}

[[nodiscard]] bool write_vendor_state(const AccountState& account) noexcept {
    if (!write_vendor_owner(account.primarySoid, account.vendorProgress, account.vendorUnlocks))
        return false;
    for (std::size_t index = 0; index < account.characterCount; ++index) {
        const auto& character = account.characters[index];
        if (!write_vendor_owner(character.soid, character.vendorProgress, character.vendorUnlocks))
            return false;
    }
    return true;
}

struct VendorOwner {
    vendors::ProgressBank* progress{};
    vendors::Unlocks* unlocks{};
};

[[nodiscard]] VendorOwner vendor_owner(AccountState& account, std::uint64_t soid) noexcept {
    if (soid != 0 && soid == account.primarySoid)
        return {&account.vendorProgress, &account.vendorUnlocks};
    for (std::size_t index = 0; index < account.characterCount; ++index) {
        auto& character = account.characters[index];
        if (soid != 0 && soid == character.soid)
            return {&character.vendorProgress, &character.vendorUnlocks};
    }
    return {};
}

[[nodiscard]] bool read_vendor_state(AccountState& account) noexcept {
    Statement progress{"SELECT owner_soid,position,vendor,points,rewards FROM vendor_progress ORDER BY owner_soid,position"};
    Statement unlocks{"SELECT owner_soid,kind,position,slot,value FROM vendor_unlocks ORDER BY owner_soid,kind,position"};
    if (!progress.ready() || !unlocks.ready()) return false;
    int result{};
    while ((result = sqlite3_step(progress.value)) == SQLITE_ROW) {
        std::uint64_t soid{};
        std::int32_t position{}, vendor{}, points{}, rewards{};
        if (!parse_u64(sqlite3_column_text(progress.value, 0), soid)
            || !column_i32(progress.value, 1, position) || !column_i32(progress.value, 2, vendor)
            || !column_i32(progress.value, 3, points) || !column_i32(progress.value, 4, rewards))
            return false;
        auto owner = vendor_owner(account, soid);
        if (!owner.progress || position < 0 || static_cast<std::size_t>(position) >= owner.progress->size()
            || vendor < 0 || vendor >= 0xffff || points < 0 || rewards < 0) return false;
        auto& row = (*owner.progress)[position];
        if (row.vendor != 0xffff) return false;
        row = {static_cast<std::uint16_t>(vendor), points, rewards};
    }
    if (result != SQLITE_DONE) return false;
    while ((result = sqlite3_step(unlocks.value)) == SQLITE_ROW) {
        std::uint64_t soid{};
        std::int32_t kind{}, position{}, slot{}, value{};
        if (!parse_u64(sqlite3_column_text(unlocks.value, 0), soid)
            || !column_i32(unlocks.value, 1, kind) || !column_i32(unlocks.value, 2, position)
            || !column_i32(unlocks.value, 3, slot) || !column_i32(unlocks.value, 4, value))
            return false;
        auto owner = vendor_owner(account, soid);
        if (!owner.unlocks || (kind != 0 && kind != 1) || position < 0 || slot < 0 || slot > 0xffff)
            return false;
        auto& rows = kind == 0 ? owner.unlocks->flags : owner.unlocks->values;
        if (static_cast<std::size_t>(position) != rows.size() || rows.size() >= vendors::kUnlockLimit)
            return false;
        rows.push_back({static_cast<std::uint16_t>(slot), value});
    }
    return result == SQLITE_DONE;
}
