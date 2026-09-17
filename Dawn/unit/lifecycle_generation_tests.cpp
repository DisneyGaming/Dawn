#include <cstdint>
#include <iostream>
#include <type_traits>

#include "state/activity/lifecycle_generation.h"
#include "state/activity/transactions/internal.h"

namespace {

using namespace dawn::state::activity;

static_assert(std::is_trivially_copyable_v<ActivationGeneration>);
static_assert(std::is_standard_layout_v<ActivationGeneration>);
static_assert(sizeof(ActivationGeneration) == sizeof(std::uint64_t));
static_assert(!std::is_same_v<ConnectionGeneration, AuthenticationGeneration>);
static_assert(!std::is_same_v<ActivityIncarnation, ActivationGeneration>);
static_assert(!std::is_convertible_v<ConnectionGeneration, AuthenticationGeneration>);

static_assert(std::is_trivially_copyable_v<ActivityInstanceKey>);
static_assert(std::is_standard_layout_v<ActivityInstanceKey>);
static_assert(std::is_trivially_copyable_v<ConnectionKey>);
static_assert(std::is_standard_layout_v<ConnectionKey>);
static_assert(std::is_trivially_copyable_v<AuthenticationKey>);
static_assert(std::is_standard_layout_v<AuthenticationKey>);
static_assert(std::is_trivially_copyable_v<BindingKey>);
static_assert(std::is_standard_layout_v<BindingKey>);
static_assert(std::is_trivially_copyable_v<HostRegionKey>);
static_assert(std::is_standard_layout_v<HostRegionKey>);
static_assert(std::is_trivially_copyable_v<NativeActivationKey>);
static_assert(std::is_standard_layout_v<NativeActivationKey>);

static_assert(std::is_same_v<decltype(ActivityInstanceKey::sessionId), std::uint64_t>);
static_assert(std::is_same_v<decltype(ActivityInstanceKey::incarnation), ActivityIncarnation>);
static_assert(std::is_same_v<decltype(ConnectionKey::connectionId), std::uint32_t>);
static_assert(std::is_same_v<decltype(ConnectionKey::generation), ConnectionGeneration>);
static_assert(std::is_same_v<decltype(AuthenticationKey::connection), ConnectionKey>);
static_assert(
    std::is_same_v<decltype(AuthenticationKey::generation), AuthenticationGeneration>);
static_assert(std::is_same_v<decltype(BindingKey::authentication), AuthenticationKey>);
static_assert(std::is_same_v<decltype(BindingKey::generation), BindingGeneration>);
static_assert(std::is_same_v<decltype(HostRegionKey::activity), ActivityInstanceKey>);
static_assert(std::is_same_v<decltype(HostRegionKey::generation), HostRegionGeneration>);
static_assert(std::is_same_v<decltype(NativeActivationKey::module), ModuleGeneration>);
static_assert(std::is_same_v<decltype(NativeActivationKey::generation), ActivationGeneration>);

template <typename Left, typename Right>
constexpr bool kEqualityComparable = requires(Left left, Right right) { left == right; };

static_assert(!std::is_same_v<ActivityInstanceKey, ConnectionKey>);
static_assert(!std::is_same_v<AuthenticationKey, BindingKey>);
static_assert(!std::is_same_v<HostRegionKey, NativeActivationKey>);
static_assert(!std::is_convertible_v<ActivityInstanceKey, HostRegionKey>);
static_assert(!std::is_convertible_v<ConnectionKey, AuthenticationKey>);
static_assert(!kEqualityComparable<ActivityInstanceKey, HostRegionKey>);
static_assert(!kEqualityComparable<ConnectionKey, AuthenticationKey>);

constexpr bool constexpr_advance_contract() noexcept {
    SceneGeneration generation{};
    bool exhausted = false;

    if (!advance(generation, exhausted) || generation.value != kFirstGeneration || exhausted) {
        return false;
    }
    if (!advance(generation, exhausted) || generation.value != kFirstGeneration + 1 || exhausted) {
        return false;
    }

    generation.value = kMaximumGeneration;
    return !advance(generation, exhausted) && exhausted &&
           generation.value == kMaximumGeneration;
}

static_assert(constexpr_advance_contract());

constexpr bool constexpr_composite_key_contract() noexcept {
    const ActivityInstanceKey activity{41, ActivityIncarnation{7}};
    const ConnectionKey connection{0, ConnectionGeneration{3}};
    const AuthenticationKey authentication{connection, AuthenticationGeneration{5}};
    const BindingKey binding{authentication, BindingGeneration{11}};
    const HostRegionKey region{activity, HostRegionGeneration{13}};
    const NativeActivationKey activation{ModuleGeneration{17}, ActivationGeneration{19}};

    return static_cast<bool>(activity) && static_cast<bool>(connection)
           && static_cast<bool>(authentication) && static_cast<bool>(binding)
           && static_cast<bool>(region) && static_cast<bool>(activation)
           && activity == ActivityInstanceKey{41, ActivityIncarnation{7}}
           && connection == ConnectionKey{0, ConnectionGeneration{3}}
           && authentication == AuthenticationKey{connection, AuthenticationGeneration{5}}
           && binding == BindingKey{authentication, BindingGeneration{11}}
           && region == HostRegionKey{activity, HostRegionGeneration{13}}
           && activation == NativeActivationKey{ModuleGeneration{17}, ActivationGeneration{19}};
}

static_assert(constexpr_composite_key_contract());

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }

    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void zero_is_absent_and_nonzero_is_present() {
    const ActivationGeneration absent{};
    const ActivationGeneration present{kFirstGeneration};

    CHECK(absent.value == kAbsentGeneration);
    CHECK(!static_cast<bool>(absent));
    CHECK(static_cast<bool>(present));
    CHECK(absent != present);
}

void advance_allocates_monotonically_within_one_domain() {
    ConnectionGeneration generation{};
    bool exhausted = false;

    CHECK(advance(generation, exhausted));
    CHECK(generation.value == 1);
    CHECK(!exhausted);

    CHECK(advance(generation, exhausted));
    CHECK(generation.value == 2);
    CHECK(!exhausted);
}

void domains_have_independent_values() {
    ConnectionGeneration connection{};
    ActivationGeneration activation{};
    bool connectionExhausted = false;
    bool activationExhausted = false;

    CHECK(advance(connection, connectionExhausted));
    CHECK(advance(activation, activationExhausted));
    CHECK(connection.value == activation.value);

    CHECK(advance(connection, connectionExhausted));
    CHECK(connection.value == 2);
    CHECK(activation.value == 1);
}

void maximum_is_valid_but_cannot_wrap() {
    WipeGeneration generation{kMaximumGeneration - 1};
    bool exhausted = false;

    CHECK(advance(generation, exhausted));
    CHECK(generation.value == kMaximumGeneration);
    CHECK(!exhausted);

    CHECK(!advance(generation, exhausted));
    CHECK(generation.value == kMaximumGeneration);
    CHECK(exhausted);

    CHECK(!advance(generation, exhausted));
    CHECK(generation.value == kMaximumGeneration);
    CHECK(exhausted);
}

void exhausted_allocator_always_fails_closed() {
    PublicationGeneration generation{17};
    bool exhausted = true;

    CHECK(!advance(generation, exhausted));
    CHECK(generation.value == 17);
    CHECK(exhausted);
}

void fresh_host_lifecycle_has_exact_first_keys() {
    SessionRecord record{};
    record.sessionId = 0x1234;
    record.occupied = true;
    record.lifecycle = transactions::fresh_lifecycle();

    const ActivityInstanceKey activity = transactions::instance_key(record);
    const HostRegionKey region = transactions::host_region_key(record);
    CHECK(static_cast<bool>(activity));
    CHECK(activity.sessionId == record.sessionId);
    CHECK(activity.incarnation.value == kFirstGeneration);
    CHECK(static_cast<bool>(region));
    CHECK(region.activity == activity);
    CHECK(region.generation.value == kFirstGeneration);
    CHECK(!record.lifecycle.hostRegionExhausted);

    record.occupied = false;
    CHECK(!static_cast<bool>(transactions::instance_key(record)));
    CHECK(!static_cast<bool>(transactions::host_region_key(record)));
}

void exact_activity_lookup_rejects_a_different_incarnation() {
    ActivityState state{};
    SessionRecord& record = state.sessions[3];
    record.sessionId = 77;
    record.occupied = true;
    record.lifecycle = transactions::fresh_lifecycle();

    CHECK(transactions::find_session(state, std::uint64_t{77}) == 3);
    CHECK(transactions::find_session(
              state, ActivityInstanceKey{77, ActivityIncarnation{kFirstGeneration}})
          == 3);
    CHECK(transactions::find_session(
              state, ActivityInstanceKey{77, ActivityIncarnation{kFirstGeneration + 1}})
          == kInvalidSessionSlot);
    CHECK(transactions::find_session(state, ActivityInstanceKey{}) == kInvalidSessionSlot);
}

void full_activity_table_refuses_implicit_eviction() {
    ActivityState state{};
    CHECK(transactions::select_target(state) == 0);

    for (std::size_t index = 0; index < state.sessions.size(); ++index) {
        SessionRecord& record = state.sessions[index];
        record.sessionId = index + 1;
        record.occupied = true;
        record.lifecycle = transactions::fresh_lifecycle();
    }
    CHECK(transactions::select_target(state) == kInvalidSessionSlot);

    const ActivityInstanceKey owned = transactions::instance_key(state.sessions[7]);
    CHECK(transactions::select_target(state, owned) == 7);
    CHECK(transactions::select_target(
              state,
              ActivityInstanceKey{owned.sessionId,
                                  ActivityIncarnation{owned.incarnation.value + 1U}})
          == kInvalidSessionSlot);

    state.sessions[7] = {};
    CHECK(transactions::select_target(state) == 7);
}

void exact_retirement_is_idempotent_and_never_stranded_by_revision_exhaustion() {
    ActivityState state{};
    SessionRecord& record = state.sessions[4];
    record.sessionId = 81;
    record.occupied = true;
    record.lifecycle = transactions::fresh_lifecycle();
    const ActivityInstanceKey exact = transactions::instance_key(record);

    CHECK(transactions::retire_exact(
              state,
              ActivityInstanceKey{exact.sessionId,
                                  ActivityIncarnation{exact.incarnation.value + 1U}})
          == RetireResult::staleIncarnation);
    CHECK(transactions::retire_exact(
              state, ActivityInstanceKey{exact.sessionId, ActivityIncarnation{}})
          == RetireResult::staleIncarnation);
    CHECK(transactions::instance_key(state.sessions[4]) == exact);

    state.stateRevision = kMaximumRevision;
    CHECK(transactions::retire_exact(state, exact) == RetireResult::retired);
    CHECK(!state.sessions[4].occupied);
    CHECK(state.stateRevision == kMaximumRevision);
    CHECK(transactions::retire_exact(state, exact) == RetireResult::alreadyRetired);
}

void activity_soid_index_never_aliases_the_reserved_class_bit() {
    constexpr std::uint64_t base = 0x9EAA300100000000ULL;
    CHECK(transactions::compose_session_soid(base, kFirstSessionId)
          == (base | transactions::kSessionClass | kFirstSessionId));
    CHECK(transactions::compose_session_soid(base, kMaximumSessionId)
          == (base | transactions::kSessionClass | kMaximumSessionId));
    CHECK(transactions::compose_session_soid(base, kMaximumSessionId + 1U)
          == kAbsentSessionId);

    ActivityState state{};
    state.nextSessionId = kMaximumSessionId;
    CHECK(transactions::allocation_available(state));
    transactions::advance_allocator(state);
    CHECK(state.allocatorExhausted);
    CHECK(state.nextSessionId == kMaximumSessionId);
    CHECK(!transactions::allocation_available(state));
}

void activity_instance_presence_requires_session_and_incarnation() {
    const ActivityInstanceKey absent{};
    const ActivityInstanceKey sessionOnly{91, {}};
    const ActivityInstanceKey incarnationOnly{0, ActivityIncarnation{4}};
    const ActivityInstanceKey present{91, ActivityIncarnation{4}};

    CHECK(!static_cast<bool>(absent));
    CHECK(!static_cast<bool>(sessionOnly));
    CHECK(!static_cast<bool>(incarnationOnly));
    CHECK(static_cast<bool>(present));
}

void connection_presence_is_generation_driven() {
    const ConnectionKey absent{};
    const ConnectionKey idOnly{73, {}};
    const ConnectionKey zeroIdPresent{0, ConnectionGeneration{2}};
    const ConnectionKey nonzeroIdPresent{73, ConnectionGeneration{2}};

    CHECK(!static_cast<bool>(absent));
    CHECK(!static_cast<bool>(idOnly));
    CHECK(static_cast<bool>(zeroIdPresent));
    CHECK(static_cast<bool>(nonzeroIdPresent));
}

void authentication_and_binding_presence_require_the_complete_owner_chain() {
    const ConnectionKey connection{0, ConnectionGeneration{2}};
    const AuthenticationKey authentication{connection, AuthenticationGeneration{3}};

    CHECK(!static_cast<bool>(AuthenticationKey{{}, AuthenticationGeneration{3}}));
    CHECK(!static_cast<bool>(AuthenticationKey{connection, {}}));
    CHECK(static_cast<bool>(authentication));

    CHECK(!static_cast<bool>(BindingKey{{}, BindingGeneration{5}}));
    CHECK(!static_cast<bool>(BindingKey{authentication, {}}));
    CHECK(static_cast<bool>(BindingKey{authentication, BindingGeneration{5}}));
}

void host_region_presence_requires_the_complete_activity_owner() {
    const ActivityInstanceKey activity{91, ActivityIncarnation{4}};

    CHECK(!static_cast<bool>(HostRegionKey{{}, HostRegionGeneration{6}}));
    CHECK(!static_cast<bool>(
        HostRegionKey{ActivityInstanceKey{91, {}}, HostRegionGeneration{6}}));
    CHECK(!static_cast<bool>(HostRegionKey{activity, {}}));
    CHECK(static_cast<bool>(HostRegionKey{activity, HostRegionGeneration{6}}));
}

void native_activation_presence_requires_module_and_activation() {
    CHECK(!static_cast<bool>(NativeActivationKey{}));
    CHECK(!static_cast<bool>(NativeActivationKey{ModuleGeneration{8}, {}}));
    CHECK(!static_cast<bool>(NativeActivationKey{{}, ActivationGeneration{9}}));
    CHECK(static_cast<bool>(
        NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{9}}));
}

void composite_key_equality_covers_every_component() {
    const ActivityInstanceKey activity{91, ActivityIncarnation{4}};
    const ConnectionKey connection{7, ConnectionGeneration{2}};
    const AuthenticationKey authentication{connection, AuthenticationGeneration{3}};
    const BindingKey binding{authentication, BindingGeneration{5}};

    CHECK((activity == ActivityInstanceKey{91, ActivityIncarnation{4}}));
    CHECK((activity != ActivityInstanceKey{92, ActivityIncarnation{4}}));
    CHECK((activity != ActivityInstanceKey{91, ActivityIncarnation{5}}));

    CHECK((connection == ConnectionKey{7, ConnectionGeneration{2}}));
    CHECK((connection != ConnectionKey{8, ConnectionGeneration{2}}));
    CHECK((connection != ConnectionKey{7, ConnectionGeneration{3}}));

    CHECK((authentication
           == AuthenticationKey{ConnectionKey{7, ConnectionGeneration{2}},
                                AuthenticationGeneration{3}}));
    CHECK((authentication
           != AuthenticationKey{ConnectionKey{8, ConnectionGeneration{2}},
                                AuthenticationGeneration{3}}));
    CHECK((authentication
           != AuthenticationKey{ConnectionKey{7, ConnectionGeneration{4}},
                                AuthenticationGeneration{3}}));
    CHECK((authentication
           != AuthenticationKey{ConnectionKey{7, ConnectionGeneration{2}},
                                AuthenticationGeneration{4}}));

    CHECK((binding == BindingKey{authentication, BindingGeneration{5}}));
    CHECK((binding
           != BindingKey{AuthenticationKey{connection, AuthenticationGeneration{4}},
                         BindingGeneration{5}}));
    CHECK((binding != BindingKey{authentication, BindingGeneration{6}}));

    CHECK((HostRegionKey{activity, HostRegionGeneration{6}}
           == HostRegionKey{activity, HostRegionGeneration{6}}));
    CHECK((HostRegionKey{activity, HostRegionGeneration{6}}
           != HostRegionKey{ActivityInstanceKey{92, ActivityIncarnation{4}},
                            HostRegionGeneration{6}}));
    CHECK((HostRegionKey{activity, HostRegionGeneration{6}}
           != HostRegionKey{activity, HostRegionGeneration{7}}));

    CHECK((NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{9}}
           == NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{9}}));
    CHECK((NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{9}}
           != NativeActivationKey{ModuleGeneration{10}, ActivationGeneration{9}}));
    CHECK((NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{9}}
           != NativeActivationKey{ModuleGeneration{8}, ActivationGeneration{10}}));
}

} // namespace

int main() {
    zero_is_absent_and_nonzero_is_present();
    advance_allocates_monotonically_within_one_domain();
    domains_have_independent_values();
    maximum_is_valid_but_cannot_wrap();
    exhausted_allocator_always_fails_closed();
    fresh_host_lifecycle_has_exact_first_keys();
    exact_activity_lookup_rejects_a_different_incarnation();
    full_activity_table_refuses_implicit_eviction();
    exact_retirement_is_idempotent_and_never_stranded_by_revision_exhaustion();
    activity_soid_index_never_aliases_the_reserved_class_bit();
    activity_instance_presence_requires_session_and_incarnation();
    connection_presence_is_generation_driven();
    authentication_and_binding_presence_require_the_complete_owner_chain();
    host_region_presence_requires_the_complete_activity_owner();
    native_activation_presence_requires_module_and_activation();
    composite_key_equality_covers_every_component();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " lifecycle generation check(s) failed\n";
        return 1;
    }

    std::cout << "all lifecycle generation checks passed\n";
    return 0;
}
