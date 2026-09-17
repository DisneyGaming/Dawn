#include <cstdint>
#include <iostream>
#include <type_traits>

#include "server/bap/internal.h"
#include "server/bap/encrypted/transactions/definition.h"
#include "server/gameplay/group/group_host_retirement_fence.h"

namespace {

using dawn::server::bap::Session;
using dawn::server::bap::ActivityBindingState;
using dawn::server::bap::ActivityRecordCreator;
using dawn::server::bap::ActivityRecordLease;
using dawn::server::bap::RosterPublication;
using dawn::server::bap::RegionLineage;
using dawn::server::bap::RegionLineageKind;
using dawn::server::bap::RegionPublicationDebt;
using namespace dawn::state::activity;

static_assert(std::is_same_v<decltype(Session::connectionKey), ConnectionKey>);
static_assert(std::is_same_v<decltype(Session::authenticationKey), AuthenticationKey>);
static_assert(
    std::is_same_v<decltype(Session::authenticationClock), AuthenticationGeneration>);
static_assert(std::is_same_v<decltype(Session::activityBindingClock), BindingGeneration>);
static_assert(std::is_same_v<decltype(ActivityBindingState::key), BindingKey>);
static_assert(std::is_same_v<decltype(ActivityBindingState::instance), ActivityInstanceKey>);
static_assert(std::is_same_v<decltype(ActivityBindingState::ownerLease), ActivityRecordLease>);
static_assert(std::is_same_v<decltype(ActivityBindingState::lineage), RegionLineage>);
static_assert(std::is_same_v<decltype(ActivityBindingState::regionDebt), RegionPublicationDebt>);
static_assert(std::is_same_v<decltype(RosterPublication::binding), BindingKey>);
static_assert(std::is_same_v<decltype(RosterPublication::activity), ActivityInstanceKey>);
static_assert(
    std::is_same_v<decltype(RosterPublication::publication), PublicationGeneration>);

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void connection_clock_outlives_reusable_session_storage() {
    ConnectionGeneration processClock{};
    bool exhausted = false;
    ConnectionKey first{};
    ConnectionKey second{};

    CHECK(dawn::server::bap::lifecycle::allocate_connection_key(
        3, processClock, exhausted, first));
    Session slot{};
    slot.id = 3;
    slot.connectionKey = first;

    slot = {};
    CHECK(dawn::server::bap::lifecycle::allocate_connection_key(
        3, processClock, exhausted, second));
    slot.id = 3;
    slot.connectionKey = second;

    CHECK(first.connectionId == 3);
    CHECK(first.generation.value == kFirstGeneration);
    CHECK(second.connectionId == 3);
    CHECK(second.generation.value == kFirstGeneration + 1);
    CHECK(first != second);
    CHECK(!exhausted);
}

void failed_service26_encoding_does_not_advance_authentication() {
    Session session{};
    session.id = 2;
    session.connectionKey = {2, ConnectionGeneration{7}};
    AuthenticationKey staged{};

    CHECK(!dawn::server::bap::lifecycle::allocate_authentication_key(
        session, false, staged));
    CHECK(!static_cast<bool>(staged));
    CHECK(session.authenticationClock.value == kAbsentGeneration);
    CHECK(!session.authenticationGenerationExhausted);
    CHECK(!static_cast<bool>(session.authenticationKey));
}

void successful_service26_allocates_an_exact_owner_chain() {
    Session session{};
    session.id = 4;
    session.connectionKey = {4, ConnectionGeneration{11}};
    AuthenticationKey first{};
    AuthenticationKey second{};

    CHECK(dawn::server::bap::lifecycle::allocate_authentication_key(
        session, true, first));
    session.authenticationKey = first;
    CHECK(static_cast<bool>(session.authenticationKey));
    CHECK(dawn::server::bap::lifecycle::authentication_key_is_current(session));
    CHECK(session.authenticationKey.connection == session.connectionKey);
    CHECK(session.authenticationKey.generation.value == kFirstGeneration);

    CHECK(dawn::server::bap::lifecycle::allocate_authentication_key(
        session, true, second));
    CHECK(second.connection == session.connectionKey);
    CHECK(second.generation.value == kFirstGeneration + 1);
    CHECK(second != first);
    CHECK(!dawn::server::bap::lifecycle::authentication_key_is_current(session));
    session.authenticationKey = second;
    CHECK(dawn::server::bap::lifecycle::authentication_key_is_current(session));
}

void allocators_exhaust_without_wrapping_or_publishing_a_key() {
    ConnectionGeneration connectionClock{kMaximumGeneration};
    bool connectionExhausted = false;
    ConnectionKey connection{9, ConnectionGeneration{3}};
    CHECK(!dawn::server::bap::lifecycle::allocate_connection_key(
        9, connectionClock, connectionExhausted, connection));
    CHECK(connectionClock.value == kMaximumGeneration);
    CHECK(connectionExhausted);
    CHECK(!static_cast<bool>(connection));

    Session session{};
    session.connectionKey = {9, ConnectionGeneration{5}};
    session.authenticationClock = AuthenticationGeneration{kMaximumGeneration};
    AuthenticationKey authentication{session.connectionKey, AuthenticationGeneration{8}};
    CHECK(!dawn::server::bap::lifecycle::allocate_authentication_key(
        session, true, authentication));
    CHECK(session.authenticationClock.value == kMaximumGeneration);
    CHECK(session.authenticationGenerationExhausted);
    CHECK(!static_cast<bool>(authentication));

    session.authenticationClock = AuthenticationGeneration{17};
    CHECK(!dawn::server::bap::lifecycle::allocate_authentication_key(
        session, true, authentication));
    CHECK(session.authenticationClock.value == 17);
    CHECK(session.authenticationGenerationExhausted);
    CHECK(!static_cast<bool>(authentication));
}

void absent_connection_cannot_own_an_authentication() {
    Session session{};
    AuthenticationKey authentication{};

    CHECK(!dawn::server::bap::lifecycle::allocate_authentication_key(
        session, true, authentication));
    CHECK(session.authenticationClock.value == kAbsentGeneration);
    CHECK(!session.authenticationGenerationExhausted);
    CHECK(!static_cast<bool>(authentication));
}

void reauthentication_retains_only_connection_owned_state() {
    Session session{};
    session.id = 6;
    session.connectionKey = {6, ConnectionGeneration{12}};
    session.authenticationKey = {session.connectionKey, AuthenticationGeneration{3}};
    session.authenticationClock = AuthenticationGeneration{3};
    session.authenticationGenerationExhausted = true;
    session.authenticated = true;
    session.sendNonce[0] = std::byte{0xA5};
    session.receiveNonce[0] = std::byte{0x5A};
    session.activityBindingClock = BindingGeneration{4};
    session.activity.key = {session.authenticationKey, BindingGeneration{4}};
    session.activity.instance = {77, ActivityIncarnation{1}};
    session.activity.ownerLease = {
        session.activity.instance, ActivityRecordCreator::bapService6};
    session.activity.joinedForeignSession = true;
    session.activity.advertisedRegion = 88;
    session.queuez.family4Active = true;
    session.accountResyncArmed = true;

    const auto connection =
        dawn::server::bap::lifecycle::connection_owned_state(session);
    dawn::server::bap::lifecycle::restore_connection_owned_state(session, connection);

    CHECK(session.id == 6);
    CHECK((session.connectionKey == ConnectionKey{6, ConnectionGeneration{12}}));
    CHECK(session.authenticationClock.value == 3);
    CHECK(session.authenticationGenerationExhausted);
    CHECK(!static_cast<bool>(session.authenticationKey));
    CHECK(!dawn::server::bap::lifecycle::authentication_key_is_current(session));
    CHECK(!session.authenticated);
    CHECK(session.sendNonce[0] == std::byte{});
    CHECK(session.receiveNonce[0] == std::byte{});
    CHECK(session.activityBindingClock.value == 0);
    CHECK(!static_cast<bool>(session.activity.key));
    CHECK(!static_cast<bool>(session.activity.instance));
    CHECK(!static_cast<bool>(session.activity.ownerLease));
    CHECK(!session.activity.joinedForeignSession);
    CHECK(session.activity.advertisedRegion == -1);
    CHECK(!session.queuez.family4Active);
    CHECK(!session.accountResyncArmed);
}

void allocation_and_join_have_distinct_lease_rules() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    const ActivityInstanceKey first{70, ActivityIncarnation{1}};
    const ActivityInstanceKey second{71, ActivityIncarnation{1}};
    ActivityBindingState owned{};
    owned.instance = first;
    owned.ownerLease = {first, ActivityRecordCreator::bapService6};

    const ActivityRecordLease allocationLease = lifecycle::lease_after_replacement(
        ActivityBindingState{}, first, lifecycle::ActivityBindingOrigin::allocation);
    CHECK(static_cast<bool>(allocationLease));
    CHECK(allocationLease.activity == first);

    const ActivityRecordLease sameJoinLease = lifecycle::lease_after_replacement(
        owned, first, lifecycle::ActivityBindingOrigin::join);
    CHECK(sameJoinLease == owned.ownerLease);
    CHECK(!static_cast<bool>(lifecycle::lease_to_retire_after_replacement(
        owned, first, lifecycle::ActivityBindingOrigin::join, {})));

    CHECK(!static_cast<bool>(lifecycle::lease_after_replacement(
        owned, second, lifecycle::ActivityBindingOrigin::join)));
    CHECK(lifecycle::lease_to_retire_after_replacement(
              owned, second, lifecycle::ActivityBindingOrigin::join, {})
          == first);
}

void atomic_allocation_replacement_consumes_only_its_owned_predecessor() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    const ActivityInstanceKey predecessor{80, ActivityIncarnation{1}};
    const ActivityInstanceKey successor{81, ActivityIncarnation{1}};
    ActivityBindingState owned{};
    owned.instance = predecessor;
    owned.ownerLease = {predecessor, ActivityRecordCreator::bapService6};

    const ActivityRecordLease successorLease = lifecycle::lease_after_replacement(
        owned, successor, lifecycle::ActivityBindingOrigin::allocation);
    CHECK(successorLease.activity == successor);
    CHECK(!static_cast<bool>(lifecycle::lease_to_retire_after_replacement(
        owned,
        successor,
        lifecycle::ActivityBindingOrigin::allocation,
        predecessor)));

    ActivityBindingState borrowed{};
    borrowed.instance = predecessor;
    CHECK(!static_cast<bool>(lifecycle::lease_to_retire_after_replacement(
        borrowed, successor, lifecycle::ActivityBindingOrigin::allocation, {})));
}

void stale_group_borrow_detaches_before_observation_mutation() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    ActivityBindingState borrowed{};
    borrowed.instance = {90, ActivityIncarnation{3}};
    borrowed.sensorObservation.omegaOpeningTriggered = true;

    CHECK(!lifecycle::borrowed_binding_is_stale(borrowed, true));
    CHECK(lifecycle::borrowed_binding_is_stale(borrowed, false));
    borrowed = {};
    CHECK(!borrowed.sensorObservation.omegaOpeningTriggered);
    CHECK(!lifecycle::borrowed_binding_is_stale(borrowed, false));

    ActivityBindingState owned{};
    owned.instance = {91, ActivityIncarnation{1}};
    owned.ownerLease = {owned.instance, ActivityRecordCreator::bapService6};
    CHECK(!lifecycle::borrowed_binding_is_stale(owned, false));
}

void close_reset_and_shutdown_detach_one_owner_idempotently() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    ActivityBindingState binding{};
    binding.instance = {92, ActivityIncarnation{2}};
    binding.ownerLease = {binding.instance, ActivityRecordCreator::bapService6};
    binding.rosterStaged.staged = true;
    binding.sensorObservation.omegaOpeningTriggered = true;

    const ActivityInstanceKey first = lifecycle::detach_owned_activity(binding);
    const ActivityInstanceKey second = lifecycle::detach_owned_activity(binding);
    CHECK((first == ActivityInstanceKey{92, ActivityIncarnation{2}}));
    CHECK(!static_cast<bool>(second));
    CHECK(!static_cast<bool>(binding.instance));
    CHECK(!static_cast<bool>(binding.ownerLease));
    CHECK(!binding.rosterStaged.staged);
    CHECK(!binding.sensorObservation.omegaOpeningTriggered);
}

void source_retirement_cascades_unique_derived_rows_before_source() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    const ActivityInstanceKey source{100, ActivityIncarnation{1}};
    const ActivityInstanceKey first{101, ActivityIncarnation{1}};
    const ActivityInstanceKey second{102, ActivityIncarnation{1}};
    const std::array derived{first, second, first, source, ActivityInstanceKey{}};

    const lifecycle::ActivityRetirementCascade cascade =
        lifecycle::prepare_retirement_cascade(source, derived);
    CHECK(cascade.count == 3);
    CHECK(cascade.keys[0] == first);
    CHECK(cascade.keys[1] == second);
    CHECK(cascade.keys[2] == source);
}

void fresh_postcommit_guard_rolls_back_only_an_untransferred_fresh_allocation() {
    namespace transactions = dawn::server::bap::encrypted::transactions;
    const ActivityInstanceKey predecessor{110, ActivityIncarnation{1}};
    const ActivityInstanceKey successor{111, ActivityIncarnation{1}};
    transactions::Publication fresh{};
    fresh.activity = successor;
    fresh.activityBindingCreatedByBap = true;
    fresh.activityAllocationCommitted = true;
    CHECK(transactions::rollback_activity(fresh) == successor);

    transactions::Publication replacement = fresh;
    replacement.atomicReplacement = predecessor;
    CHECK(!static_cast<bool>(transactions::rollback_activity(replacement)));

    transactions::Publication failedPrepare = fresh;
    failedPrepare.activityAllocationCommitted = false;
    CHECK(!static_cast<bool>(transactions::rollback_activity(failedPrepare)));
}

void retiring_source_fence_closes_the_detach_to_state_retire_race() {
    namespace group = dawn::server::gameplay::group;
    group::SourceRetirementFences fences{};
    const ActivityInstanceKey source{120, ActivityIncarnation{4}};
    const bool claimantValidatedBeforeFence = true;

    CHECK(group::begin_source_retirement(fences, source));
    CHECK(group::source_retirement_fenced(fences, source));
    // A retry observes the same tombstone; it never creates a second owner or opens the gap.
    CHECK(group::begin_source_retirement(fences, source));
    CHECK(fences.count == 1);

    // The fence intentionally survives row detach and every derived retirement. Only the exact
    // source's completed State retirement reaches this boundary.
    CHECK(group::source_retirement_fenced(fences, source));
    group::finish_source_retirement(fences, source);
    CHECK(!group::source_retirement_fenced(fences, source));
    CHECK(fences.count == 0);
    // A claimant that observed live State before waiting on the fence must validate again after
    // acquiring its row. The retired exact source rejects that stale precheck.
    CHECK(claimantValidatedBeforeFence);
    CHECK(!group::source_claim_is_current(fences, source, false));
}

void matchmaking_release_failure_cannot_retain_the_activity_lease() {
    namespace lifecycle = dawn::server::bap::lifecycle;
    Session session{};
    session.id = 3;
    session.connectionKey = {3, ConnectionGeneration{9}};
    session.authenticationClock = AuthenticationGeneration{4};
    session.authenticationKey = {session.connectionKey, session.authenticationClock};
    session.authenticated = true;
    session.matchmakingContext = {2, 7};
    session.activity.instance = {121, ActivityIncarnation{1}};
    session.activity.ownerLease = {
        session.activity.instance, ActivityRecordCreator::bapService6};
    session.activity.rosterStaged.staged = true;
    const bool matchmakingReleased = false;

    // Close/open/shutdown all execute this detach before consulting the independent context
    // release result. A failed context retains its own evidence, never this owner or staged work.
    const ActivityInstanceKey retirement =
        lifecycle::detach_owned_activity(session.activity);
    CHECK(!matchmakingReleased);
    CHECK(static_cast<bool>(retirement));
    const lifecycle::DeferredMatchmakingRetirement pending =
        lifecycle::deferred_matchmaking_retirement(session);
    lifecycle::restore_deferred_matchmaking_retirement(session, pending);
    CHECK(session.matchmakingRetirementPending);
    CHECK(session.matchmakingContext.generation == 7);
    CHECK(session.id == 3);
    CHECK((session.connectionKey == ConnectionKey{3, ConnectionGeneration{9}}));
    CHECK(!session.authenticated);
    CHECK(!static_cast<bool>(session.authenticationKey));
    CHECK(!static_cast<bool>(session.activity.ownerLease));
    CHECK(!static_cast<bool>(session.activity.instance));
    CHECK(!session.activity.rosterStaged.staged);
    CHECK(!static_cast<bool>(lifecycle::detach_owned_activity(session.activity)));
}

void session_value_initialization_restores_semantic_sentinels() {
    Session session{};
    session.activity.advertisedRegion = 88;
    session.id = 6;
    session = {};

    CHECK(session.id == 0);
    CHECK(session.activity.advertisedRegion == -1);
    CHECK(!static_cast<bool>(session.connectionKey));
    CHECK(!static_cast<bool>(session.authenticationKey));
}

void binding_candidate_is_owned_by_authentication_and_publishes_nothing() {
    Session session{};
    session.connectionKey = {4, ConnectionGeneration{9}};
    session.authenticationClock = AuthenticationGeneration{3};
    session.authenticationKey = {session.connectionKey, session.authenticationClock};
    BindingKey candidate{};

    CHECK(dawn::server::bap::lifecycle::stage_activity_binding_key(session, candidate));
    CHECK(candidate.authentication == session.authenticationKey);
    CHECK(candidate.generation.value == kFirstGeneration);
    CHECK(session.activityBindingClock.value == kAbsentGeneration);
    CHECK(dawn::server::bap::lifecycle::is_staged_activity_binding_key(session, candidate));

    session.activityBindingClock = candidate.generation;
    CHECK(!dawn::server::bap::lifecycle::is_staged_activity_binding_key(session, candidate));
}

void binding_candidates_advance_under_one_authentication() {
    Session session{};
    session.connectionKey = {7, ConnectionGeneration{2}};
    session.authenticationClock = AuthenticationGeneration{5};
    session.authenticationKey = {session.connectionKey, session.authenticationClock};
    BindingKey first{};
    BindingKey second{};

    CHECK(dawn::server::bap::lifecycle::stage_activity_binding_key(session, first));
    session.activityBindingClock = first.generation;
    CHECK(dawn::server::bap::lifecycle::stage_activity_binding_key(session, second));
    CHECK(first.generation.value == kFirstGeneration);
    CHECK(second.generation.value == kFirstGeneration + 1U);
    CHECK(first != second);
}

void binding_generation_exhaustion_is_sticky_and_never_publishes() {
    Session session{};
    session.connectionKey = {9, ConnectionGeneration{4}};
    session.authenticationClock = AuthenticationGeneration{6};
    session.authenticationKey = {session.connectionKey, session.authenticationClock};
    session.activityBindingClock = BindingGeneration{kMaximumGeneration};
    BindingKey candidate{session.authenticationKey, BindingGeneration{12}};

    CHECK(!dawn::server::bap::lifecycle::stage_activity_binding_key(session, candidate));
    CHECK(!static_cast<bool>(candidate));
    CHECK(session.activityBindingClock.value == kMaximumGeneration);
    CHECK(session.activityBindingGenerationExhausted);

    session.activityBindingClock = BindingGeneration{17};
    CHECK(!dawn::server::bap::lifecycle::stage_activity_binding_key(session, candidate));
    CHECK(session.activityBindingClock.value == 17);
    CHECK(!static_cast<bool>(candidate));
}

void binding_replacement_value_does_not_copy_delivery_state() {
    Session session{};
    const AuthenticationKey authentication{
        {3, ConnectionGeneration{1}},
        AuthenticationGeneration{1},
    };
    session.activity.key = {authentication, BindingGeneration{1}};
    session.activity.instance = {55, ActivityIncarnation{1}};
    session.activity.rosterSends = 7;
    session.activity.advertisedRegion = 81;
    session.activity.rosterStaged.staged = true;

    ActivityBindingState next{};
    next.key = {authentication, BindingGeneration{2}};
    next.instance = session.activity.instance;
    session.activity = next;

    CHECK(session.activity.key == next.key);
    CHECK(session.activity.instance == next.instance);
    CHECK(session.activity.rosterSends == 0);
    CHECK(session.activity.advertisedRegion == -1);
    CHECK(!session.activity.rosterStaged.staged);
}

void every_successful_same_activity_join_advances_binding_and_preserves_only_role() {
    Session session{};
    session.connectionKey = {3, ConnectionGeneration{4}};
    session.authenticationClock = AuthenticationGeneration{6};
    session.authenticationKey = {session.connectionKey, session.authenticationClock};
    const ActivityInstanceKey activity{55, ActivityIncarnation{2}};
    session.activity.instance = activity;
    session.activity.joinedForeignSession = false;
    session.activity.rosterSends = 9;

    BindingKey first{};
    CHECK(dawn::server::bap::lifecycle::stage_activity_binding_key(session, first));
    ActivityBindingState firstJoin{};
    firstJoin.key = first;
    firstJoin.instance = activity;
    firstJoin.joinedForeignSession =
        dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
            session.activity, activity, true);
    session.activityBindingClock = first.generation;
    session.activity = firstJoin;

    BindingKey second{};
    CHECK(dawn::server::bap::lifecycle::stage_activity_binding_key(session, second));
    ActivityBindingState secondJoin{};
    secondJoin.key = second;
    secondJoin.instance = activity;
    secondJoin.joinedForeignSession =
        dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
            session.activity, activity, true);

    CHECK(first.generation.value == kFirstGeneration);
    CHECK(second.generation.value == kFirstGeneration + 1U);
    CHECK(first != second);
    CHECK(!firstJoin.joinedForeignSession);
    CHECK(!secondJoin.joinedForeignSession);
    CHECK(firstJoin.rosterSends == 0);
    CHECK(secondJoin.rosterSends == 0);
}

void foreign_role_is_set_on_different_join_preserved_on_same_join_and_cleared_on_allocate() {
    ActivityBindingState unbound{};
    const ActivityInstanceKey first{70, ActivityIncarnation{1}};
    CHECK(dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
        unbound, first, true));

    ActivityBindingState foreign{};
    foreign.instance = first;
    foreign.joinedForeignSession = true;
    CHECK(dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
        foreign, first, true));
    CHECK(dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
        foreign, ActivityInstanceKey{71, ActivityIncarnation{1}}, true));
    CHECK(!dawn::server::bap::lifecycle::joined_foreign_session_after_replacement(
        foreign, first, false));
}

void roster_publication_candidates_do_not_advance_until_published() {
    ActivityBindingState binding{};
    const AuthenticationKey authentication{
        {8, ConnectionGeneration{2}}, AuthenticationGeneration{3}};
    binding.key = {authentication, BindingGeneration{4}};
    binding.instance = {90, ActivityIncarnation{5}};
    PublicationGeneration candidate{};

    CHECK(dawn::server::bap::lifecycle::stage_roster_publication_generation(
        binding, candidate));
    CHECK(candidate.value == kFirstGeneration);
    CHECK(binding.rosterPublicationClock.value == kAbsentGeneration);
    CHECK(!dawn::server::bap::lifecycle::staged_roster_is_current(binding));

    binding.rosterPublicationClock = candidate;
    binding.rosterStaged.binding = binding.key;
    binding.rosterStaged.activity = binding.instance;
    binding.rosterStaged.publication = candidate;
    binding.rosterStaged.staged = true;
    CHECK(dawn::server::bap::lifecycle::staged_roster_is_current(binding));
}

void roster_publication_exhaustion_does_not_mutate_delivery() {
    ActivityBindingState binding{};
    const AuthenticationKey authentication{
        {8, ConnectionGeneration{2}}, AuthenticationGeneration{3}};
    binding.key = {authentication, BindingGeneration{4}};
    binding.instance = {90, ActivityIncarnation{5}};
    binding.rosterPublicationClock = PublicationGeneration{kMaximumGeneration};
    PublicationGeneration candidate{12};

    CHECK(!dawn::server::bap::lifecycle::stage_roster_publication_generation(
        binding, candidate));
    CHECK(!static_cast<bool>(candidate));
    CHECK(!binding.rosterPublicationGenerationExhausted);
    binding.rosterPublicationClock = PublicationGeneration{9};
    CHECK(dawn::server::bap::lifecycle::stage_roster_publication_generation(
        binding, candidate));
    CHECK(binding.rosterPublicationClock.value == 9);
    CHECK(candidate.value == 10);
}

void region_lineage_and_debt_are_exact_binding_owned_values() {
    const AuthenticationKey authentication{
        {12, ConnectionGeneration{4}}, AuthenticationGeneration{8}};
    const BindingKey binding{authentication, BindingGeneration{3}};
    const ActivityInstanceKey bound{220, ActivityIncarnation{9}};
    const ActivityInstanceKey source{219, ActivityIncarnation{6}};
    const HostRegionKey committed{source, HostRegionGeneration{5}};

    ActivityBindingState state{};
    state.key = binding;
    state.instance = bound;
    state.lineage = {bound, source, RegionLineageKind::groupDerivedBorrow};
    state.regionDebt.binding = binding;
    state.regionDebt.activity = bound;
    state.regionDebt.regionSource = source;
    state.regionDebt.committedHostRegion = committed;
    state.regionDebt.regionIndex = 88;
    state.regionDebt.present = true;
    state.regionDebt.publishesHud = true;

    CHECK(static_cast<bool>(state.lineage));
    CHECK(state.lineage.bound == state.instance);
    CHECK(state.regionDebt.binding == state.key);
    CHECK(state.regionDebt.activity == state.lineage.bound);
    CHECK(state.regionDebt.regionSource == state.lineage.source);
    CHECK(state.regionDebt.committedHostRegion.activity == state.lineage.source);
    CHECK(state.regionDebt.publishesHud);

    ActivityBindingState successor{};
    successor.key = {authentication, BindingGeneration{4}};
    successor.instance = bound;
    CHECK(successor.key != state.regionDebt.binding);
    CHECK(!successor.regionDebt.present);
}

void immutable_staged_roster_matches_the_next_clock_without_advancing_live_state() {
    const AuthenticationKey authentication{
        {13, ConnectionGeneration{2}}, AuthenticationGeneration{7}};
    ActivityBindingState binding{};
    binding.key = {authentication, BindingGeneration{2}};
    binding.instance = {310, ActivityIncarnation{4}};
    binding.rosterPublicationClock = PublicationGeneration{6};
    binding.rosterStaged.binding = binding.key;
    binding.rosterStaged.activity = binding.instance;
    binding.rosterStaged.publication = PublicationGeneration{7};
    binding.rosterStaged.sourceHostRegion = {
        binding.instance, HostRegionGeneration{3}};
    binding.rosterStaged.hasAfter = true;
    binding.rosterStaged.staged = true;

    CHECK(dawn::server::bap::lifecycle::staged_roster_is_current(binding));
    CHECK(binding.rosterPublicationClock.value == 6);
    binding.rosterStaged.publication = PublicationGeneration{8};
    CHECK(!dawn::server::bap::lifecycle::staged_roster_is_current(binding));
}

void stale_roster_publication_cannot_match_or_rollback_a_successor() {
    const AuthenticationKey authentication{
        {5, ConnectionGeneration{7}}, AuthenticationGeneration{2}};
    const ActivityInstanceKey activity{33, ActivityIncarnation{4}};
    ActivityBindingState predecessor{};
    predecessor.key = {authentication, BindingGeneration{1}};
    predecessor.instance = activity;
    predecessor.rosterPublicationClock = PublicationGeneration{1};
    predecessor.rosterStaged.binding = predecessor.key;
    predecessor.rosterStaged.activity = activity;
    predecessor.rosterStaged.publication = predecessor.rosterPublicationClock;
    predecessor.rosterStaged.priorSends = 3;
    predecessor.rosterStaged.staged = true;
    CHECK(dawn::server::bap::lifecycle::staged_roster_is_current(predecessor));

    ActivityBindingState successor{};
    successor.key = {authentication, BindingGeneration{2}};
    successor.instance = activity;
    successor.rosterSends = 1;
    successor.rosterStaged = predecessor.rosterStaged;
    CHECK(!dawn::server::bap::lifecycle::staged_roster_is_current(successor));
    if (!dawn::server::bap::lifecycle::staged_roster_is_current(successor)) {
        successor.rosterStaged = {};
    }
    CHECK(successor.rosterSends == 1);
    CHECK(!successor.rosterStaged.staged);
}

} // namespace

int main() {
    connection_clock_outlives_reusable_session_storage();
    failed_service26_encoding_does_not_advance_authentication();
    successful_service26_allocates_an_exact_owner_chain();
    allocators_exhaust_without_wrapping_or_publishing_a_key();
    absent_connection_cannot_own_an_authentication();
    reauthentication_retains_only_connection_owned_state();
    session_value_initialization_restores_semantic_sentinels();
    allocation_and_join_have_distinct_lease_rules();
    atomic_allocation_replacement_consumes_only_its_owned_predecessor();
    stale_group_borrow_detaches_before_observation_mutation();
    close_reset_and_shutdown_detach_one_owner_idempotently();
    source_retirement_cascades_unique_derived_rows_before_source();
    fresh_postcommit_guard_rolls_back_only_an_untransferred_fresh_allocation();
    retiring_source_fence_closes_the_detach_to_state_retire_race();
    matchmaking_release_failure_cannot_retain_the_activity_lease();
    binding_candidate_is_owned_by_authentication_and_publishes_nothing();
    binding_candidates_advance_under_one_authentication();
    binding_generation_exhaustion_is_sticky_and_never_publishes();
    binding_replacement_value_does_not_copy_delivery_state();
    every_successful_same_activity_join_advances_binding_and_preserves_only_role();
    foreign_role_is_set_on_different_join_preserved_on_same_join_and_cleared_on_allocate();
    roster_publication_candidates_do_not_advance_until_published();
    roster_publication_exhaustion_does_not_mutate_delivery();
    stale_roster_publication_cannot_match_or_rollback_a_successor();
    region_lineage_and_debt_are_exact_binding_owned_values();
    immutable_staged_roster_matches_the_next_clock_without_advancing_live_state();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " BAP lifecycle generation check(s) failed\n";
        return 1;
    }
    std::cout << "all BAP lifecycle generation checks passed\n";
    return 0;
}
