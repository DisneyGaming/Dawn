#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <thread>

#include "core/logging/log.h"
#include "core/settings/settings.h"
#include "middleware/bap/frame.h"
#include "server/bap/activity_retirement_test_support.h"
#include "server/bap/encrypted/bap_connection_publication.h"
#include "server/bap/encrypted/transactions/service_outcome_commit.h"
#include "server/bap/runtime.h"
#include "server/gameplay/group/group_host.h"
#include "server/gameplay/group/group_host_sessions.h"
#include "server/gameplay/group/group_host_sessions_test_support.h"
#include "state/activity/bubble_authority/runtime.h"
#include "state/activity/destination/activity_destination_snapshot.h"
#include "state/activity/entity_slots/runtime.h"
#include "state/activity/membership/activity_membership_query.h"
#include "state/activity/runtime.h"
#include "state/matchmaking/matchmaking_state.h"
#include "state/matchmaking/matchmaking_test_support.h"
#include "state/runtime/runtime.h"
#include "state/runtime/storage/internal.h"

namespace sunrise::state::runtime::storage {

State g_state{};
SRWLOCK g_stateLock{SRWLOCK_INIT};

} // namespace sunrise::state::runtime::storage

namespace sunrise::core::log {

void write(Channel, Level, std::string_view) noexcept {}

} // namespace sunrise::core::log

namespace sunrise::core::settings {

const Settings& get() noexcept {
    static const Settings value{};
    return value;
}

} // namespace sunrise::core::settings

namespace sunrise::server::gameplay {

void report(core::log::Level, const char*, ...) noexcept {}

} // namespace sunrise::server::gameplay

namespace sunrise::middleware::bap {

bool parse_frame(std::span<const std::byte>, OuterFrame&) noexcept {
    return false;
}

} // namespace sunrise::middleware::bap

namespace sunrise::server::bap::plaintext {

bool consume(Session&,
             Scratch&,
             const middleware::bap::OuterFrame&,
             std::span<std::byte>,
             std::size_t&) noexcept {
    return false;
}

} // namespace sunrise::server::bap::plaintext

namespace sunrise::server::bap::encrypted {

bool consume(Session&,
             Scratch&,
             const middleware::bap::OuterFrame&,
             std::span<std::byte>,
             std::size_t&) noexcept {
    return false;
}

bool consume_deferred(Session&,
                      Scratch&,
                      std::span<std::byte>,
                      std::size_t&,
                      bool&) noexcept {
    return false;
}

} // namespace sunrise::server::bap::encrypted

namespace sunrise::state {

AccountState account_snapshot() noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const AccountState result = runtime::storage::g_state.account;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return result;
}

bool commit_equipment_swap(PendingEquipmentSwap&) noexcept {
    return false;
}

bool commit_item_acquisition(PendingItemAcquisition&) noexcept {
    return false;
}

bool commit_socket_plug(PendingSocketPlug&) noexcept {
    return false;
}

bool commit_item_state(PendingItemState&) noexcept {
    return false;
}

bool commit_profile_item_acquisition(PendingProfileItemAcquisition&) noexcept {
    return false;
}

bool commit_item_dismantle(PendingItemDismantle&) noexcept {
    return false;
}

} // namespace sunrise::state

namespace sunrise::state::activity::membership {

bool snapshot_region_view(ActivityInstanceKey, HostRegionKey, RegionView&) noexcept {
    return false;
}

bool commit(PendingMutation&) noexcept {
    return false;
}

} // namespace sunrise::state::activity::membership

namespace sunrise::state::activity::entity_slots {

bool commit(PendingMutation&) noexcept {
    return false;
}

} // namespace sunrise::state::activity::entity_slots

namespace sunrise::state::activity::bubble_authority {

void clear_grants(ActivityInstanceKey) noexcept {}

} // namespace sunrise::state::activity::bubble_authority

namespace sunrise::state::matchmaking {

bool commit(PendingMutation&) noexcept {
    return false;
}

} // namespace sunrise::state::matchmaking

namespace sunrise::server::bap::encrypted::push::activity {

bool seed_identity(state::activity::ActivityInstanceKey,
                   std::uint64_t,
                   std::uint64_t) noexcept {
    return false;
}

bool seed_transition_token(state::activity::ActivityInstanceKey) noexcept {
    return false;
}

} // namespace sunrise::server::bap::encrypted::push::activity

namespace {

namespace activity = sunrise::state::activity;
namespace bap = sunrise::server::bap;
namespace group = sunrise::server::gameplay::group;
namespace matchmaking = sunrise::state::matchmaking;

std::atomic_int g_failureCount{};

void check(bool condition, const char* expression, int line) noexcept {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    g_failureCount.fetch_add(1, std::memory_order_relaxed);
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

struct Barrier final {
    HANDLE reached{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    HANDLE resume{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    std::atomic_bool timedOut{};

    Barrier() noexcept {
        CHECK(reached != nullptr);
        CHECK(resume != nullptr);
    }

    Barrier(const Barrier&) = delete;
    Barrier& operator=(const Barrier&) = delete;

    ~Barrier() {
        if (resume != nullptr) {
            SetEvent(resume);
            CloseHandle(resume);
        }
        if (reached != nullptr) {
            CloseHandle(reached);
        }
    }

    void pause() noexcept {
        SetEvent(reached);
        if (WaitForSingleObject(resume, 10'000) != WAIT_OBJECT_0) {
            timedOut.store(true, std::memory_order_relaxed);
        }
    }

    void wait_until_reached() noexcept {
        CHECK(WaitForSingleObject(reached, 10'000) == WAIT_OBJECT_0);
    }

    void continue_now() noexcept {
        SetEvent(resume);
    }
};

struct ThreadDone final {
    HANDLE event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};

    ThreadDone() noexcept {
        CHECK(event != nullptr);
    }

    ThreadDone(const ThreadDone&) = delete;
    ThreadDone& operator=(const ThreadDone&) = delete;

    ~ThreadDone() {
        if (event != nullptr) {
            CloseHandle(event);
        }
    }

    void signal() noexcept {
        SetEvent(event);
    }

    void join(std::thread& thread) noexcept {
        if (WaitForSingleObject(event, 10'000) != WAIT_OBJECT_0) {
            std::cerr << "activity retirement integration watchdog expired\n";
            std::abort();
        }
        thread.join();
    }
};

std::atomic<Barrier*> g_groupBarrier{};
group::test_support::Point g_groupBarrierPoint{};
activity::ActivityInstanceKey g_groupBarrierSource{};
std::atomic<std::uint64_t> g_allocatedSessionId{};
std::atomic<std::uint64_t> g_allocatedIncarnation{};

std::atomic<Barrier*> g_bapBarrier{};
activity::ActivityInstanceKey g_bapBarrierSource{};
std::atomic<group::HostActivityLineageLease*> g_eventLease{};

struct RetirementObservation final {
    activity::ActivityInstanceKey activity{};
    activity::RetireResult result{activity::RetireResult::alreadyRetired};
};

std::array<RetirementObservation, 128> g_retirements{};
std::atomic_size_t g_retirementCount{};

void group_hook(group::test_support::Point point,
                activity::ActivityInstanceKey source,
                activity::ActivityInstanceKey allocated) noexcept {
    Barrier* barrier = g_groupBarrier.load(std::memory_order_acquire);
    if (barrier == nullptr || point != g_groupBarrierPoint
        || source != g_groupBarrierSource) {
        return;
    }
    if (static_cast<bool>(allocated)) {
        g_allocatedSessionId.store(allocated.sessionId, std::memory_order_relaxed);
        g_allocatedIncarnation.store(allocated.incarnation.value, std::memory_order_relaxed);
    }
    barrier->pause();
}

void bap_hook(bap::test_support::Point point,
              activity::ActivityInstanceKey key,
              activity::RetireResult result,
              bool completed) noexcept {
    if (point == bap::test_support::Point::sourceStateRetired) {
        const std::size_t index = g_retirementCount.fetch_add(1U, std::memory_order_relaxed);
        if (index < g_retirements.size()) {
            g_retirements[index] = {key, result};
        }
    }
    if (point == bap::test_support::Point::eventLockAcquired) {
        group::HostActivityLineageLease* lease =
            g_eventLease.exchange(nullptr, std::memory_order_acq_rel);
        if (lease != nullptr) {
            group::release_host_activity_lineage(*lease);
        }
    }
    Barrier* barrier = g_bapBarrier.load(std::memory_order_acquire);
    if (barrier != nullptr && point == bap::test_support::Point::sourceBeginComplete
        && completed && key == g_bapBarrierSource) {
        barrier->pause();
    }
}

void clear_observations() noexcept {
    g_retirements = {};
    g_retirementCount.store(0, std::memory_order_relaxed);
    g_allocatedSessionId.store(0, std::memory_order_relaxed);
    g_allocatedIncarnation.store(0, std::memory_order_relaxed);
}

std::size_t retirement_count(activity::ActivityInstanceKey key,
                             activity::RetireResult result) noexcept {
    std::size_t count = 0;
    const std::size_t end =
        (std::min)(g_retirementCount.load(std::memory_order_relaxed), g_retirements.size());
    for (std::size_t index = 0; index < end; ++index) {
        if (g_retirements[index].activity == key && g_retirements[index].result == result) {
            ++count;
        }
    }
    return count;
}

void reset_state_storage() noexcept {
    AcquireSRWLockExclusive(&sunrise::state::runtime::storage::g_stateLock);
    sunrise::state::runtime::storage::g_state = sunrise::state::State{};
    ReleaseSRWLockExclusive(&sunrise::state::runtime::storage::g_stateLock);
}

void reset_fixture() noexcept {
    g_groupBarrier.store(nullptr, std::memory_order_release);
    g_bapBarrier.store(nullptr, std::memory_order_release);
    g_eventLease.store(nullptr, std::memory_order_release);
    group::test_support::set_hook(nullptr);
    bap::test_support::set_hook(nullptr);
    bap::test_support::reset_storage();
    group::test_support::reset_storage();
    matchmaking::test_support::reset();
    reset_state_storage();
    clear_observations();
    bap::test_support::set_hook(bap_hook);
}

std::size_t occupied_state_records() noexcept {
    std::size_t count = 0;
    AcquireSRWLockShared(&sunrise::state::runtime::storage::g_stateLock);
    for (const activity::SessionRecord& record :
         sunrise::state::runtime::storage::g_state.activity.sessions) {
        count += record.occupied ? 1U : 0U;
    }
    ReleaseSRWLockShared(&sunrise::state::runtime::storage::g_stateLock);
    return count;
}

void check_conservation(std::size_t extraGuards = 0) noexcept {
    const bap::test_support::Snapshot bapSnapshot = bap::test_support::snapshot();
    const group::test_support::Snapshot groupSnapshot = group::test_support::snapshot();
    CHECK(occupied_state_records()
          == bapSnapshot.ownerLeaseCount + bapSnapshot.pendingCount
                 + groupSnapshot.uniqueOwnerCount + extraGuards);
}

activity::ActivityInstanceKey allocate_record(
    activity::ActivityInstanceKey replaces = {}) noexcept {
    std::uint64_t sessionId = activity::kAbsentSessionId;
    activity::PendingAllocation allocation{};
    const bool prepared = static_cast<bool>(replaces)
                              ? activity::prepare_session(replaces, sessionId, allocation)
                              : activity::prepare_session(sessionId, allocation);
    CHECK(prepared);
    const activity::ActivityInstanceKey key = allocation.instanceKey;
    CHECK(static_cast<bool>(key));
    CHECK(activity::commit(allocation));
    CHECK(activity::contains(key));
    return key;
}

bool bap_event(sunrise::client::network::BapEvent event, std::uint32_t id) noexcept {
    sunrise::client::network::BapRequest request{};
    request.event = event;
    request.connectionId = id;
    sunrise::client::network::BapResponse response{};
    return bap::consume(request, response);
}

void open_owned(std::uint32_t id,
                activity::ActivityInstanceKey key,
                matchmaking::ContextHandle context = {}) noexcept {
    CHECK(bap_event(sunrise::client::network::BapEvent::open, id));
    CHECK(bap::test_support::configure_session(id, key, true, context));
}

activity::ActivityInstanceKey create_child(std::uint64_t groupSessionId,
                                           activity::ActivityInstanceKey source) noexcept {
    bool claimed = false;
    CHECK(group::activity_host_session(groupSessionId, 88, source, claimed)
          == activity::kAbsentSessionId);
    CHECK(claimed);
    group::allocate_claimed_host_sessions();
    const activity::ActivityInstanceKey child = group::held_host_activity(groupSessionId);
    CHECK(static_cast<bool>(child));
    CHECK(activity::contains(child));
    return child;
}

void fence_removal_stale_precheck_uses_real_claim() {
    reset_fixture();
    const activity::ActivityInstanceKey source = allocate_record();
    open_owned(1, source);
    check_conservation();

    Barrier barrier{};
    g_groupBarrierPoint = group::test_support::Point::sourcePrecheckComplete;
    g_groupBarrierSource = source;
    g_groupBarrier.store(&barrier, std::memory_order_release);
    group::test_support::set_hook(group_hook);

    bool claimed = true;
    std::uint64_t result = 1;
    ThreadDone done{};
    std::thread claimant([&]() noexcept {
        result = group::activity_host_session(0x1001, 88, source, claimed);
        done.signal();
    });
    barrier.wait_until_reached();
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(!activity::contains(source));
    barrier.continue_now();
    done.join(claimant);
    CHECK(!barrier.timedOut.load(std::memory_order_relaxed));
    CHECK(result == activity::kAbsentSessionId);
    CHECK(!claimed);
    group::allocate_claimed_host_sessions();
    const group::test_support::Snapshot groupSnapshot = group::test_support::snapshot();
    CHECK(groupSnapshot.occupiedRows == 0);
    CHECK(groupSnapshot.pendingOwners == 0);
    CHECK(groupSnapshot.fenceCount == 0);
    CHECK(occupied_state_records() == 0);
    check_conservation();
}

void claim_pin_eviction_allocation_reset_and_pending_drain_honor_real_fence() {
    reset_fixture();
    const activity::ActivityInstanceKey source = allocate_record();
    open_owned(1, source);
    std::array<activity::ActivityInstanceKey, 8> children{};
    for (std::size_t index = 0; index < children.size(); ++index) {
        children[index] = create_child(0x2000 + index, source);
    }
    bool ninthClaimed = false;
    CHECK(group::activity_host_session(0x2100, 88, source, ninthClaimed)
          == activity::kAbsentSessionId);
    CHECK(ninthClaimed);
    group::test_support::Snapshot before = group::test_support::snapshot();
    CHECK(before.occupiedRows == 8);
    CHECK(before.currentOwners == 7);
    CHECK(before.unfilledRows == 1);
    CHECK(before.pendingOwners == 1);
    CHECK(before.uniqueOwnerCount == 8);
    check_conservation();

    Barrier barrier{};
    g_bapBarrierSource = source;
    g_bapBarrier.store(&barrier, std::memory_order_release);
    ThreadDone done{};
    bool closed = false;
    std::thread retirement([&]() noexcept {
        closed = bap_event(sunrise::client::network::BapEvent::close, 1);
        done.signal();
    });
    barrier.wait_until_reached();
    const group::test_support::Snapshot fenced = group::test_support::snapshot();
    CHECK(fenced.fenceCount == 1);
    CHECK(fenced.currentOwners == before.currentOwners);
    CHECK(fenced.pendingOwners == before.pendingOwners);
    CHECK(fenced.unfilledRows == before.unfilledRows);
    check_conservation(1);

    bool exactClaimed = true;
    CHECK(group::activity_host_session(0x2200, 88, source, exactClaimed)
          == activity::kAbsentSessionId);
    CHECK(!exactClaimed);
    group::HostActivityLineageLease rejectedLease{};
    CHECK(!group::acquire_host_activity_lineage(children[1], rejectedLease));
    bool evictionClaimed = true;
    CHECK(group::activity_host_session(
              0x2300, 88, activity::kAbsentSessionId, evictionClaimed)
          == activity::kAbsentSessionId);
    CHECK(!evictionClaimed);
    group::allocate_claimed_host_sessions();
    group::reset_host_sessions();
    const group::test_support::Snapshot afterAttempts = group::test_support::snapshot();
    CHECK(afterAttempts.currentOwners == fenced.currentOwners);
    CHECK(afterAttempts.pendingOwners == fenced.pendingOwners);
    CHECK(afterAttempts.unfilledRows == fenced.unfilledRows);
    CHECK(afterAttempts.fenceCount == fenced.fenceCount);

    barrier.continue_now();
    done.join(retirement);
    CHECK(closed);
    CHECK(!barrier.timedOut.load(std::memory_order_relaxed));
    CHECK(occupied_state_records() == 0);
    const group::test_support::Snapshot drained = group::test_support::snapshot();
    CHECK(drained.occupiedRows == 0);
    CHECK(drained.pendingOwners == 0);
    CHECK(drained.fenceCount == 0);
    check_conservation();
}

void allocation_commit_store_race_rolls_back_real_guard() {
    reset_fixture();
    const activity::ActivityInstanceKey source = allocate_record();
    open_owned(1, source);
    bool claimed = false;
    CHECK(group::activity_host_session(0x3001, 88, source, claimed)
          == activity::kAbsentSessionId);
    CHECK(claimed);

    Barrier barrier{};
    g_groupBarrierPoint = group::test_support::Point::allocationCommittedBeforeStore;
    g_groupBarrierSource = source;
    g_groupBarrier.store(&barrier, std::memory_order_release);
    group::test_support::set_hook(group_hook);
    ThreadDone done{};
    std::thread allocation([&]() noexcept {
        group::allocate_claimed_host_sessions();
        done.signal();
    });
    barrier.wait_until_reached();
    const activity::ActivityInstanceKey fresh{
        g_allocatedSessionId.load(std::memory_order_relaxed),
        activity::ActivityIncarnation{
            g_allocatedIncarnation.load(std::memory_order_relaxed)},
    };
    CHECK(static_cast<bool>(fresh));
    CHECK(activity::contains(source));
    CHECK(activity::contains(fresh));
    CHECK(group::test_support::snapshot().unfilledRows == 1);
    check_conservation(1);

    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(!activity::contains(source));
    CHECK(activity::contains(fresh));
    barrier.continue_now();
    done.join(allocation);
    CHECK(!barrier.timedOut.load(std::memory_order_relaxed));
    CHECK(!activity::contains(fresh));
    CHECK(group::test_support::snapshot().occupiedRows == 0);
    CHECK(occupied_state_records() == 0);
    check_conservation();
}

struct PartialCascadeFixture final {
    activity::ActivityInstanceKey source{};
    activity::ActivityInstanceKey first{};
    activity::ActivityInstanceKey second{};
    activity::ActivityInstanceKey pinnedChild{};
    group::HostActivityLineageLease lease{};
};

PartialCascadeFixture make_partial_cascade_fixture() noexcept {
    PartialCascadeFixture fixture{};
    fixture.source = allocate_record();
    open_owned(1, fixture.source);
    fixture.first = create_child(0x4001, fixture.source);
    fixture.second = create_child(0x4002, fixture.source);
    fixture.pinnedChild = create_child(0x4003, fixture.second);
    CHECK(group::acquire_host_activity_lineage(fixture.pinnedChild, fixture.lease));
    return fixture;
}

void check_partial_cascade_blocked(const PartialCascadeFixture& fixture) noexcept {
    CHECK(!activity::contains(fixture.first));
    CHECK(activity::contains(fixture.source));
    CHECK(activity::contains(fixture.second));
    CHECK(activity::contains(fixture.pinnedChild));
    const group::test_support::Snapshot groupSnapshot = group::test_support::snapshot();
    const bap::test_support::Snapshot bapSnapshot = bap::test_support::snapshot();
    CHECK(groupSnapshot.currentOwners == 2);
    CHECK(groupSnapshot.pinnedRows == 1);
    CHECK(groupSnapshot.fenceCount == 2);
    CHECK(bapSnapshot.pendingCount == 1);
    CHECK(bapSnapshot.pending[0] == fixture.source);
    CHECK(retirement_count(fixture.first, activity::RetireResult::retired) == 1);
    check_conservation();
}

void pinned_partial_cascade_retries_outside_bap_lock() {
    reset_fixture();
    PartialCascadeFixture fixture = make_partial_cascade_fixture();
    CHECK(!bap_event(sunrise::client::network::BapEvent::close, 1)
          || !activity::contains(fixture.first));
    check_partial_cascade_blocked(fixture);
    group::release_host_activity_lineage(fixture.lease);
    CHECK(!fixture.lease.pinned);
    CHECK(occupied_state_records() == 0);
    CHECK(group::test_support::snapshot().fenceCount == 0);
    CHECK(bap::test_support::snapshot().pendingCount == 0);
    CHECK(retirement_count(fixture.source, activity::RetireResult::retired) == 1);
    CHECK(retirement_count(fixture.second, activity::RetireResult::retired) == 1);
    CHECK(retirement_count(fixture.pinnedChild, activity::RetireResult::retired) == 1);
    check_conservation();
}

void pinned_partial_cascade_drains_at_real_event_tail() {
    reset_fixture();
    PartialCascadeFixture fixture = make_partial_cascade_fixture();
    (void)bap_event(sunrise::client::network::BapEvent::close, 1);
    check_partial_cascade_blocked(fixture);
    g_eventLease.store(&fixture.lease, std::memory_order_release);
    CHECK(bap_event(sunrise::client::network::BapEvent::open, 4));
    CHECK(!fixture.lease.pinned);
    CHECK(occupied_state_records() == 0);
    CHECK(group::test_support::snapshot().fenceCount == 0);
    CHECK(bap::test_support::snapshot().pendingCount == 0);
    CHECK(retirement_count(fixture.source, activity::RetireResult::retired) == 1);
    CHECK(retirement_count(fixture.first, activity::RetireResult::retired) == 1);
    CHECK(retirement_count(fixture.second, activity::RetireResult::retired) == 1);
    CHECK(retirement_count(fixture.pinnedChild, activity::RetireResult::retired) == 1);
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 4));
    check_conservation();
}

void cycle_and_full_current_pending_boundary_use_real_coordinator() {
    reset_fixture();
    const activity::ActivityInstanceKey source = allocate_record();
    const activity::ActivityInstanceKey child = allocate_record();
    open_owned(1, source);
    CHECK(group::test_support::seed_current(0x5001, child, source));
    CHECK(group::test_support::seed_current(0x5002, source, child));
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(occupied_state_records() == 0);
    CHECK(group::test_support::snapshot().occupiedRows == 0);
    CHECK(group::test_support::snapshot().fenceCount == 0);
    CHECK(bap::test_support::snapshot().pendingCount == 0);
    CHECK(g_retirementCount.load(std::memory_order_relaxed) == 2);

    reset_fixture();
    std::array<activity::ActivityInstanceKey, activity::kSessionCapacity> keys{};
    for (activity::ActivityInstanceKey& key : keys) {
        key = allocate_record();
    }
    open_owned(1, keys[0]);
    for (std::size_t index = 0; index < 8; ++index) {
        CHECK(group::test_support::seed_current(0x5100 + index, keys[index + 1], keys[0]));
    }
    for (std::size_t index = 0; index < 8; ++index) {
        CHECK(group::test_support::seed_pending(keys[index + 8], keys[0]));
    }
    const group::test_support::Snapshot full = group::test_support::snapshot();
    CHECK(full.currentOwners == 8);
    CHECK(full.pendingOwners == 8);
    CHECK(full.uniqueOwnerCount == 15);
    check_conservation();
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(occupied_state_records() == 0);
    CHECK(group::test_support::snapshot().uniqueOwnerCount == 0);
    CHECK(group::test_support::snapshot().fenceCount == 0);
    CHECK(bap::test_support::snapshot().pendingCount == 0);
    CHECK(g_retirementCount.load(std::memory_order_relaxed) == 16);
    for (const activity::ActivityInstanceKey key : keys) {
        CHECK(retirement_count(key, activity::RetireResult::retired) == 1);
    }
    check_conservation();
}

void close_and_open_quarantine_only_real_failed_matchmaking_context() {
    reset_fixture();
    matchmaking::ContextHandle context{};
    CHECK(matchmaking::acquire_context(context));
    const activity::ActivityInstanceKey source = allocate_record();
    open_owned(1, source, context);
    matchmaking::test_support::fail_next_releases(1);
    CHECK(!bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(!activity::contains(source));
    CHECK(retirement_count(source, activity::RetireResult::retired) == 1);
    bap::Session quarantined{};
    CHECK(bap::test_support::snapshot_session(1, quarantined));
    CHECK(quarantined.matchmakingRetirementPending);
    CHECK(quarantined.matchmakingContext.slot == context.slot);
    CHECK(quarantined.matchmakingContext.generation == context.generation);
    CHECK(static_cast<bool>(quarantined.connectionKey));
    CHECK(!quarantined.authenticated);
    CHECK(!static_cast<bool>(quarantined.authenticationKey));
    CHECK(!static_cast<bool>(quarantined.activity.instance));
    CHECK(!static_cast<bool>(quarantined.activity.ownerLease));
    CHECK(std::all_of(quarantined.sendNonce.begin(),
                      quarantined.sendNonce.end(),
                      [](std::byte value) noexcept { return value == std::byte{}; }));
    CHECK(!bap_event(sunrise::client::network::BapEvent::poll, 1));
    CHECK(!bap::test_support::with_session_locked(
        1, +[](bap::Session&) noexcept { return true; }));
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    bap::Session cleared{};
    CHECK(!bap::test_support::snapshot_session(1, cleared));
    const matchmaking::test_support::ReleaseSnapshot release =
        matchmaking::test_support::snapshot();
    CHECK(release.attempts == 2);
    CHECK(release.injectedFailures == 1);
    CHECK(release.releases == 1);
    CHECK(release.last.slot == context.slot);
    CHECK(release.last.generation == context.generation);
    bap::shutdown();
    CHECK(retirement_count(source, activity::RetireResult::retired) == 1);
    CHECK(occupied_state_records() == 0);

    reset_fixture();
    matchmaking::ContextHandle openContext{};
    CHECK(matchmaking::acquire_context(openContext));
    const activity::ActivityInstanceKey openSource = allocate_record();
    open_owned(1, openSource, openContext);
    bap::Session before{};
    CHECK(bap::test_support::snapshot_session(1, before));
    matchmaking::test_support::fail_next_releases(1);
    CHECK(!bap_event(sunrise::client::network::BapEvent::open, 1));
    CHECK(!activity::contains(openSource));
    bap::Session pending{};
    CHECK(bap::test_support::snapshot_session(1, pending));
    CHECK(pending.matchmakingRetirementPending);
    CHECK(!static_cast<bool>(pending.activity.instance));
    CHECK(bap_event(sunrise::client::network::BapEvent::open, 1));
    bap::Session reopened{};
    CHECK(bap::test_support::snapshot_session(1, reopened));
    CHECK(!reopened.matchmakingRetirementPending);
    CHECK(!static_cast<bool>(reopened.activity.instance));
    CHECK(reopened.connectionKey != before.connectionKey);
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    CHECK(retirement_count(openSource, activity::RetireResult::retired) == 1);
    check_conservation();
}

void authentication_and_shutdown_detach_activity_under_release_failure() {
    reset_fixture();
    matchmaking::ContextHandle firstContext{};
    CHECK(matchmaking::acquire_context(firstContext));
    const activity::ActivityInstanceKey first = allocate_record();
    open_owned(1, first, firstContext);
    matchmaking::test_support::fail_next_releases(1);
    CHECK(!matchmaking::release_context(firstContext));
    CHECK(bap::test_support::reset_authentication(1));
    CHECK(!activity::contains(first));
    bap::Session reset{};
    CHECK(bap::test_support::snapshot_session(1, reset));
    CHECK(static_cast<bool>(reset.connectionKey));
    CHECK(!reset.authenticated);
    CHECK(!static_cast<bool>(reset.activity.instance));
    CHECK(reset.matchmakingContext.generation == matchmaking::kInvalidGeneration);
    CHECK(matchmaking::release_context(firstContext));
    CHECK(retirement_count(first, activity::RetireResult::retired) == 1);

    matchmaking::ContextHandle shutdownContext{};
    CHECK(matchmaking::acquire_context(shutdownContext));
    const activity::ActivityInstanceKey second = allocate_record();
    CHECK(bap::test_support::configure_session(1, second, true, shutdownContext));
    matchmaking::test_support::fail_next_releases(1);
    bap::shutdown();
    CHECK(!activity::contains(second));
    CHECK(retirement_count(second, activity::RetireResult::retired) == 1);
    CHECK(matchmaking::release_context(shutdownContext));
    CHECK(occupied_state_records() == 0);
    check_conservation();
}

bool publish_one_owned_replacement(bap::Session& session) noexcept {
    const activity::ActivityInstanceKey replaces =
        bap::lifecycle::owns_published_activity(session.activity)
            ? session.activity.ownerLease.activity
            : activity::ActivityInstanceKey{};
    std::uint64_t sessionId = activity::kAbsentSessionId;
    activity::PendingAllocation allocation{};
    if (!(static_cast<bool>(replaces)
              ? activity::prepare_session(replaces, sessionId, allocation)
              : activity::prepare_session(sessionId, allocation))) {
        return false;
    }
    bap::encrypted::ServiceOutcome outcome{};
    outcome.transaction = allocation;
    bap::encrypted::transactions::Publication publication{};
    if (!bap::encrypted::transactions::prepare_publication(outcome, publication)) {
        return false;
    }
    publication.regionLineage = {
        publication.activity, publication.activity, bap::RegionLineageKind::ownedActivity};
    publication.hasRegionLineage = true;
    activity::BindingKey staged{};
    if (!bap::lifecycle::stage_activity_binding_key(session, staged)
        || !bap::encrypted::can_publish_connection_fields(session, staged, publication)
        || !bap::encrypted::transactions::commit(outcome, publication)) {
        return false;
    }
    // Represents the already-preflighted, non-failing caller copy boundary.
    std::array<std::byte, 1> callerCopy{std::byte{0xA5}};
    if (callerCopy[0] != std::byte{0xA5}) {
        return false;
    }
    return bap::encrypted::publish_connection_fields(
        session, staged, publication, bap::encrypted::ConnectionFields{});
}

bool fail_precommit_without_replacing(bap::Session& session) noexcept {
    const bap::ActivityBindingState before = session.activity;
    std::uint64_t sessionId = activity::kAbsentSessionId;
    activity::PendingAllocation allocation{};
    if (!activity::prepare_session(
            before.ownerLease.activity, sessionId, allocation)) {
        return false;
    }
    ++allocation.expectedStateRevision;
    bap::encrypted::ServiceOutcome outcome{};
    outcome.transaction = allocation;
    bap::encrypted::transactions::Publication publication{};
    if (!bap::encrypted::transactions::prepare_publication(outcome, publication)) {
        return false;
    }
    publication.regionLineage = {
        publication.activity, publication.activity, bap::RegionLineageKind::ownedActivity};
    publication.hasRegionLineage = true;
    activity::BindingKey staged{};
    return bap::lifecycle::stage_activity_binding_key(session, staged)
           && !bap::encrypted::transactions::commit(outcome, publication)
           && session.activity.key == before.key && session.activity.instance == before.instance
           && session.activity.ownerLease == before.ownerLease
           && activity::contains(before.instance);
}

void full_table_real_commit_and_publication_replaces_seventeen_times() {
    reset_fixture();
    std::array<activity::ActivityInstanceKey, activity::kSessionCapacity> keys{};
    for (activity::ActivityInstanceKey& key : keys) {
        key = allocate_record();
    }
    CHECK(bap_event(sunrise::client::network::BapEvent::open, 1));
    CHECK(bap::test_support::configure_session(1, keys[0], true));
    for (std::size_t index = 0; index < 8; ++index) {
        CHECK(group::test_support::seed_current(0x6000 + index, keys[index + 1], {}));
    }
    for (std::size_t index = 0; index < 7; ++index) {
        CHECK(group::test_support::seed_pending(keys[index + 9], {}));
    }
    CHECK(occupied_state_records() == activity::kSessionCapacity);
    check_conservation();
    for (std::size_t replacement = 0; replacement < 17; ++replacement) {
        bap::Session before{};
        CHECK(bap::test_support::snapshot_session(1, before));
        CHECK(bap::test_support::with_session_locked(1, publish_one_owned_replacement));
        bap::Session after{};
        CHECK(bap::test_support::snapshot_session(1, after));
        CHECK(after.activity.instance != before.activity.instance);
        CHECK(after.activity.ownerLease.activity == after.activity.instance);
        CHECK(!activity::contains(before.activity.instance));
        CHECK(activity::contains(after.activity.instance));
        CHECK(occupied_state_records() == activity::kSessionCapacity);
        check_conservation();
    }
    bap::Session predecessor{};
    CHECK(bap::test_support::snapshot_session(1, predecessor));
    CHECK(bap::test_support::with_session_locked(1, fail_precommit_without_replacing));
    bap::Session preserved{};
    CHECK(bap::test_support::snapshot_session(1, preserved));
    CHECK(preserved.activity.instance == predecessor.activity.instance);
    CHECK(preserved.activity.ownerLease == predecessor.activity.ownerLease);
    CHECK(occupied_state_records() == activity::kSessionCapacity);
    check_conservation();
    CHECK(bap_event(sunrise::client::network::BapEvent::close, 1));
    group::reset_host_sessions();
    CHECK(occupied_state_records() == 0);
    check_conservation();
}

void borrower_invalidation_and_lock_order_stress_reach_final_drain() {
    reset_fixture();
    const activity::ActivityInstanceKey source = allocate_record();
    open_owned(1, source);
    std::array<activity::ActivityInstanceKey, 4> children{};
    for (std::size_t index = 0; index < children.size(); ++index) {
        children[index] = create_child(0x7000 + index, source);
    }
    CHECK(bap_event(sunrise::client::network::BapEvent::open, 2));
    CHECK(bap::test_support::configure_session(2, children[0], false));

    std::atomic_bool start{};
    std::array<ThreadDone, 4> done{};
    std::array<std::thread, 4> workers{
        std::thread([&]() noexcept {
            while (!start.load(std::memory_order_acquire)) {
                YieldProcessor();
            }
            for (std::uint64_t index = 0; index < 32; ++index) {
                bool claimed = false;
                (void)group::activity_host_session(0x7100 + index, 88, source, claimed);
                group::allocate_claimed_host_sessions();
            }
            done[0].signal();
        }),
        std::thread([&]() noexcept {
            while (!start.load(std::memory_order_acquire)) {
                YieldProcessor();
            }
            for (std::size_t pass = 0; pass < 64; ++pass) {
                group::HostActivityLineageLease lease{};
                if (group::acquire_host_activity_lineage(children[pass % children.size()], lease)) {
                    group::release_host_activity_lineage(lease);
                }
            }
            done[1].signal();
        }),
        std::thread([&]() noexcept {
            while (!start.load(std::memory_order_acquire)) {
                YieldProcessor();
            }
            for (std::size_t pass = 0; pass < 16; ++pass) {
                group::reset_host_sessions();
            }
            done[2].signal();
        }),
        std::thread([&]() noexcept {
            while (!start.load(std::memory_order_acquire)) {
                YieldProcessor();
            }
            (void)bap_event(sunrise::client::network::BapEvent::close, 1);
            done[3].signal();
        }),
    };
    start.store(true, std::memory_order_release);
    for (std::size_t index = 0; index < workers.size(); ++index) {
        done[index].join(workers[index]);
    }
    group::reset_host_sessions();
    bap::shutdown();
    const group::test_support::Snapshot groupSnapshot = group::test_support::snapshot();
    const bap::test_support::Snapshot bapSnapshot = bap::test_support::snapshot();
    CHECK(groupSnapshot.uniqueOwnerCount == 0);
    CHECK(groupSnapshot.pendingOwners == 0);
    CHECK(groupSnapshot.fenceCount == 0);
    CHECK(bapSnapshot.ownerLeaseCount == 0);
    CHECK(bapSnapshot.borrowedBindingCount == 0);
    CHECK(bapSnapshot.pendingCount == 0);
    CHECK(occupied_state_records() == 0);
    check_conservation();
}

} // namespace

int main() {
    fence_removal_stale_precheck_uses_real_claim();
    claim_pin_eviction_allocation_reset_and_pending_drain_honor_real_fence();
    allocation_commit_store_race_rolls_back_real_guard();
    pinned_partial_cascade_retries_outside_bap_lock();
    pinned_partial_cascade_drains_at_real_event_tail();
    cycle_and_full_current_pending_boundary_use_real_coordinator();
    close_and_open_quarantine_only_real_failed_matchmaking_context();
    authentication_and_shutdown_detach_activity_under_release_failure();
    full_table_real_commit_and_publication_replaces_seventeen_times();
    borrower_invalidation_and_lock_order_stress_reach_final_drain();
    bap::test_support::set_hook(nullptr);
    group::test_support::set_hook(nullptr);
    if (g_failureCount.load(std::memory_order_relaxed) != 0) {
        std::cerr << g_failureCount.load(std::memory_order_relaxed)
                  << " activity retirement integration check(s) failed\n";
        return 1;
    }
    std::cout << "all production-linked activity retirement integration checks passed\n";
    return 0;
}
