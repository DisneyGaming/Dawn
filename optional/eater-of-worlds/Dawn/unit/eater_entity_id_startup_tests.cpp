#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "client/hooks/bootflow/eater_entity_id_startup_policy.h"
#include "state/activity/entity_slots/runtime.h"
#include "state/activity/entity_slots/transactions/internal.h"
#include "state/activity/transactions/internal.h"
#include "state/runtime/storage/internal.h"

namespace dawn::state::runtime::storage {

State g_state{};
SRWLOCK g_stateLock{SRWLOCK_INIT};

} // namespace dawn::state::runtime::storage

namespace {

namespace activity = dawn::state::activity;
namespace slots = dawn::state::activity::entity_slots;
namespace slot_transactions = dawn::state::activity::entity_slots::transactions;
namespace startup = dawn::client::hooks::bootflow::eater_entity_id_startup;

unsigned g_checks{};

void check(bool condition, const char* message) {
    ++g_checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] std::size_t count(const slots::LeaseMask& mask) noexcept {
    std::size_t result = 0;
    for (const std::byte byte : mask) {
        unsigned value = std::to_integer<unsigned char>(byte);
        while (value != 0U) {
            value &= value - 1U;
            ++result;
        }
    }
    return result;
}

[[nodiscard]] startup::Eligibility entrance() noexcept {
    return {startup::kEntrancePackage,
            startup::kEntranceBubble,
            startup::kEntranceSlice,
            startup::kEntranceSpawn,
            true,
            true,
            true,
            true,
            true,
            true};
}

void captured_burst_requests_a_valid_allowance_before_starvation() {
    constexpr std::uint32_t kCapturedAvailable = 150U;
    constexpr std::uint32_t kObservedAttemptedObjects = 163U;
    const auto retail = startup::decide(
        entrance(), startup::CacheProfile{400U, 600U}, kCapturedAvailable);
    check(!retail.overrideProfile && retail.requestDeficit == 0U,
          "an existing nonlocal role profile is not changed");

    const auto decision =
        startup::decide(entrance(), startup::kRetailLocalProfile, kCapturedAvailable);
    check(decision.overrideProfile, "the exact entrance selects the bounded startup profile");
    check(decision.profile == startup::kEntranceStartupProfile && decision.target == 400U,
          "startup uses the native build's smallest larger midpoint");
    check(decision.requestDeficit == 250U,
          "the captured 150-ID cache requests exactly the deficit to 400");
    check(kCapturedAvailable < kObservedAttemptedObjects
              && kCapturedAvailable + decision.requestDeficit > kObservedAttemptedObjects,
          "the measured failing burst exhausts retail but leaves startup IDs available");
}

void only_the_exact_pending_current_lifetime_is_eligible() {
    auto value = entrance();
    const auto rejected = [&](const char* message) {
        check(!startup::decide(value, startup::kRetailLocalProfile, 150U).overrideProfile,
              message);
        value = entrance();
    };
    value.launchPending = false;
    rejected("an arrived or cancelled launch is rejected");
    value.committedSession = false;
    rejected("a request without a committed activity session is rejected");
    value.currentManager = false;
    rejected("a stale simulation manager is rejected");
    value.package = "raid_envy_v310_extra";
    rejected("a package prefix does not identify Eater");
    value.bubble = 7U;
    rejected("the working reactor arrival is unchanged");
    value.slice = 48U;
    rejected("a different slice is unchanged");
    value.spawn = 0x68C397B7U;
    rejected("the working Argos arrival is unchanged");
    value.hasSpawn = false;
    rejected("an unresolved spawn cannot receive the entrance policy");
}

class FakeProfileStore final {
public:
    explicit FakeProfileStore(startup::CacheProfile profile) noexcept
        : word_(startup::pack(profile)) {}

    [[nodiscard]] bool compare_exchange(std::uint64_t& expected,
                                        std::uint64_t desired) noexcept {
        if (word_ == expected) {
            word_ = desired;
            return true;
        }
        expected = word_;
        return false;
    }

    void native_replace(startup::CacheProfile profile) noexcept {
        word_ = startup::pack(profile);
    }

    [[nodiscard]] startup::CacheProfile profile() const noexcept {
        return startup::unpack(word_);
    }

private:
    std::uint64_t word_{};
};

void temporary_profile_restores_on_success_and_exception() {
    FakeProfileStore store{startup::kRetailLocalProfile};
    {
        startup::TemporaryProfileOverride override{
            store, startup::kRetailLocalProfile, startup::kEntranceStartupProfile};
        check(override.applied() && store.profile() == startup::kEntranceStartupProfile,
              "the exact adjacent profile pair changes together");
    }
    check(store.profile() == startup::kRetailLocalProfile,
          "normal completion restores the native profile");

    try {
        startup::TemporaryProfileOverride override{
            store, startup::kRetailLocalProfile, startup::kEntranceStartupProfile};
        check(override.applied(), "exception path applies the temporary profile");
        throw std::runtime_error{"simulated native boundary failure"};
    } catch (const std::runtime_error&) {
    }
    check(store.profile() == startup::kRetailLocalProfile,
          "stack unwinding restores the native profile");

    {
        startup::TemporaryProfileOverride override{
            store, startup::kRetailLocalProfile, startup::kEntranceStartupProfile};
        store.native_replace({400U, 600U});
    }
    check(store.profile() == startup::CacheProfile{400U, 600U},
          "restoration never overwrites a concurrent native lifecycle profile");
}

void grant_uses_the_real_free_lease_transaction_and_rejects_stale_commit() {
    using dawn::state::runtime::storage::g_state;
    g_state = {};
    auto& state = g_state.activity;
    auto& record = state.sessions[0];
    record.occupied = true;
    record.joined = true;
    record.sessionId = 0x9EAA300100200012ULL;
    record.memberKey = 0x1234U;
    record.lifecycle = activity::transactions::fresh_lifecycle();
    record.createdRevision = state.stateRevision;
    record.recordRevision = state.stateRevision;
    record.joinedRevision = state.stateRevision;
    record.heldEntitySlots = slot_transactions::select_free({}, 150U);
    record.serverEntitySlots = slot_transactions::reserve_high(1'024U);

    const activity::ActivityInstanceKey key{record.sessionId, record.lifecycle.incarnation};
    slots::PendingMutation grant{};
    check(slots::prepare_grant(key, 250U, grant),
          "the entrance deficit stages through the entity-slot transaction");
    check(grant.kind == slots::MutationKind::grant && count(grant.mask) == 250U,
          "the server selects the requested number of currently free lease bits");
    check(slot_transactions::empty(
              slot_transactions::intersect(grant.mask, record.heldEntitySlots))
              && slot_transactions::empty(
                  slot_transactions::intersect(grant.mask, record.serverEntitySlots)),
          "the grant overlaps neither client-held nor server-reserved ownership");
    check(slots::commit(grant) && count(record.heldEntitySlots) == 400U,
          "the normal commit grows the owned client lease to the startup target");
    check(!grant.prepared && grant.kind == slots::MutationKind::none,
          "a committed grant cannot replay");

    slots::PendingMutation stale{};
    check(slots::prepare_grant(key, 1U, stale), "a later one-slot request stages");
    const slots::LeaseMask before = record.heldEntitySlots;
    ++state.stateRevision;
    ++record.recordRevision;
    check(!slots::commit(stale) && record.heldEntitySlots == before,
          "a stale activity revision grants no IDs");
}

} // namespace

int main() {
    captured_burst_requests_a_valid_allowance_before_starvation();
    only_the_exact_pending_current_lifetime_is_eligible();
    temporary_profile_restores_on_success_and_exception();
    grant_uses_the_real_free_lease_transaction_and_rejects_stale_commit();
    std::cout << "all " << g_checks << " Eater entity-ID startup checks passed\n";
    return 0;
}
