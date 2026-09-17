#pragma once
#include "../../../internal.h"
#include "../../../../runtime/activity/native_activity_clock.h"

namespace dawn::server::bap::encrypted::push::activity {
// Keep the exact group-owned edge pinned through caller-copy. The source clock
// is already configured by the creator runtime; this is a recipient mapping,
// never a second executor or independently started clock.
struct BorrowedClockPublication final {
    RegionLineage edge{};
    gameplay::group::HostActivityLineageLease lease{};
    runtime::activity::activity_clock::Domain domain{};
    bool present{};
};
[[nodiscard]] bool append_borrowed_clock_notifications(const Session& session,Scratch& scratch,
    std::span<const std::byte,state::kAesKeySize> key,
    std::array<std::byte,state::kBapNonceSize>& nonce,std::span<std::byte> response,
    std::size_t& written,BorrowedClockPublication& publication) noexcept;
[[nodiscard]] bool borrowed_clock_publication_is_current(const Session& session,
    const BorrowedClockPublication& publication) noexcept;
void release_borrowed_clock_publication(BorrowedClockPublication& publication) noexcept;
}
