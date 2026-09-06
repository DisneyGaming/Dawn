#include "client/hooks/bootflow/type31_objective_capture.h"

#include <algorithm>
#include <cstring>
#include <limits>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace sunrise::client::hooks::bootflow::type31_capture {
namespace {

constexpr std::size_t kComponentSnapshotOffset = 0x180U;
constexpr std::size_t kDecodedBodyPointerStateKeyOffset = 0x08U;
constexpr std::size_t kAuthBoolBodyOffset = 0x00U;
constexpr std::size_t kU64_0BodyOffset = 0x08U;
constexpr std::size_t kU64_1BodyOffset = 0x10U;
constexpr std::size_t kMaximumNativePrefixBytes = 17U;

[[nodiscard]] bool
safe_copy_exact(void* destination, const void* source, std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr || bytes == 0U) {
        return false;
    }

#if defined(_WIN32) && defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    // Sunrise's native observation lane is Windows-only. An unsupported build must not turn a
    // guarded game-memory read into an unguarded one.
    (void)destination;
    (void)source;
    (void)bytes;
    return false;
#endif
}

[[nodiscard]] const void* offset_address(const void* base, std::size_t offset) noexcept {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
    if (address == 0U || address > (std::numeric_limits<std::uintptr_t>::max)() - offset) {
        return nullptr;
    }
    return reinterpret_cast<const void*>(address + offset);
}

[[nodiscard]] bool capture_component_snapshot(const void* component,
                                              ComponentSnapshot& snapshot) noexcept {
    ComponentSnapshot temporary{};
    if (!safe_copy_exact(&temporary,
                         offset_address(component, kComponentSnapshotOffset),
                         sizeof temporary)) {
        return false;
    }
    snapshot = temporary;
    return true;
}

[[nodiscard]] bool
capture_decoded_body(const std::array<std::byte, kStateKeyBytes>& stateKey,
                     std::array<std::byte, kDecodedBodyBytes>& decodedBody) noexcept {
    std::uintptr_t decodedAddress{};
    std::memcpy(&decodedAddress,
                stateKey.data() + kDecodedBodyPointerStateKeyOffset,
                sizeof decodedAddress);
    if (decodedAddress == 0U) {
        return false;
    }

    std::array<std::byte, kDecodedBodyBytes> temporary{};
    if (!safe_copy_exact(
            temporary.data(), reinterpret_cast<const void*>(decodedAddress), temporary.size())) {
        return false;
    }
    decodedBody = temporary;
    return true;
}

[[nodiscard]] std::uint32_t state_key_schema(
    const std::array<std::byte, kStateKeyBytes>& stateKey) noexcept {
    std::uint32_t schema{};
    std::memcpy(&schema, stateKey.data(), sizeof schema);
    return schema;
}

[[nodiscard]] bool activation_snapshot_exact_at_entry(const CaptureContext& context) noexcept {
    return has_context_field(context.presence_mask, ContextPresence::activation)
           && static_cast<bool>(context.activation)
           && (context.activation_state == ActivationSnapshotState::current
               || context.activation_state == ActivationSnapshotState::quiescing);
}

[[nodiscard]] bool same_observation(const CaptureRecord& left,
                                    const CaptureRecord& right) noexcept {
    return left.context == right.context && left.capture_epoch == right.capture_epoch
           && left.monotonic_tick == right.monotonic_tick
           && left.producer_thread_id == right.producer_thread_id
           && left.caller_rva == right.caller_rva
           && left.component_identity == right.component_identity
           && left.definition == right.definition && left.schema == right.schema
           && left.logical == right.logical && left.state_key == right.state_key
           && left.decoded_body_pre == right.decoded_body_pre && left.before == right.before
           && left.after == right.after && left.state_key_valid == right.state_key_valid
           && left.decoded_body_pre_valid == right.decoded_body_pre_valid
           && left.component_before_valid == right.component_before_valid
           && left.component_after_valid == right.component_after_valid
           && left.context_shape_valid == right.context_shape_valid
           && left.activation_entry_exact == right.activation_entry_exact
           && left.activation_current_at_exit == right.activation_current_at_exit;
}

[[nodiscard]] constexpr bool valid_activation_state(ActivationSnapshotState state) noexcept {
    return state == ActivationSnapshotState::current
           || state == ActivationSnapshotState::quiescing
           || state == ActivationSnapshotState::stale;
}

template <class Value>
void append_hash_value(std::uint64_t& hash, const Value& value) noexcept {
    const auto* const bytes = reinterpret_cast<const std::byte*>(&value);
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    for (std::size_t index = 0U; index < sizeof value; ++index) {
        hash ^= std::to_integer<std::uint8_t>(bytes[index]);
        hash *= kFnvPrime;
    }
}

} // namespace

class CaptureQueueLock final {
public:
    explicit CaptureQueueLock(CaptureQueue& queue) noexcept
        : queue_(queue), locked_(queue_.try_lock()) {}

    ~CaptureQueueLock() {
        if (locked_) {
            queue_.unlock();
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return locked_;
    }

private:
    CaptureQueue& queue_;
    bool locked_{};
};

NativeTarget native_target(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::apply:
        return {kApplyRva, kApplyPrefix};
    case NativeSurface::predicate:
        return {kPredicateRva, kPredicatePrefix};
    case NativeSurface::terminal:
        return {kTerminalRva, kTerminalPrefix};
    case NativeSurface::subscriber:
        return {kSubscriberRva, kSubscriberPrefix};
    case NativeSurface::listener_enumerator:
        return {kListenerEnumeratorRva, kListenerEnumeratorPrefix};
    default:
        return {};
    }
}

bool native_prefix_matches(NativeSurface surface, std::span<const std::byte> observed) noexcept {
    const NativeTarget target = native_target(surface);
    return target.rva != 0U && !target.prefix.empty() && observed.size() >= target.prefix.size()
           && std::equal(target.prefix.begin(), target.prefix.end(), observed.begin());
}

const std::byte* validated_native_target(NativeSurface surface,
                                         const std::byte* image,
                                         std::size_t imageBytes) noexcept {
    const NativeTarget target = native_target(surface);
    if (image == nullptr || target.rva == 0U || target.prefix.empty()
        || target.prefix.size() > kMaximumNativePrefixBytes || target.rva > imageBytes
        || target.prefix.size() > imageBytes - target.rva) {
        return nullptr;
    }

    const std::byte* const address = image + target.rva;
    std::array<std::byte, kMaximumNativePrefixBytes> observed{};
    if (!safe_copy_exact(observed.data(), address, target.prefix.size())
        || !native_prefix_matches(
            surface, std::span<const std::byte>{observed.data(), target.prefix.size()})) {
        return nullptr;
    }
    return address;
}

bool fully_correlated(const CaptureContext& context) noexcept {
    if (context.presence_mask != kKnownContextPresenceMask
        || !static_cast<bool>(context.activation) || context.native_identity == 0U
        || !static_cast<bool>(context.activity) || context.session.session_id == 0U
        || context.session.created_revision == 0U || context.session.record_revision == 0U
        || context.session.record_revision < context.session.created_revision
        || context.session.session_id != context.activity.sessionId || context.run_token == 0U
        || context.correlation_token == 0U || context.generation_token == 0U) {
        return false;
    }
    return valid_activation_state(context.activation_state);
}

bool sanitize_context(const CaptureContext& input, CaptureContext& output) noexcept {
    CaptureContext sanitized{};
    sanitized.presence_mask = input.presence_mask & kKnownContextPresenceMask;
    bool valid = input.presence_mask == sanitized.presence_mask;

    const auto clear = [&sanitized](ContextPresence field) noexcept {
        sanitized.presence_mask &= ~static_cast<std::uint32_t>(field);
    };

    if (has_context_field(sanitized.presence_mask, ContextPresence::activation)) {
        if (!static_cast<bool>(input.activation) || !valid_activation_state(input.activation_state)) {
            clear(ContextPresence::activation);
            valid = false;
        } else {
            sanitized.activation = input.activation;
            sanitized.activation_state = input.activation_state;
        }
    }
    if (!has_context_field(sanitized.presence_mask, ContextPresence::activation)) {
        sanitized.activation = {};
        sanitized.activation_state = ActivationSnapshotState::absent;
    }

    if (has_context_field(sanitized.presence_mask, ContextPresence::native_identity)) {
        if (input.native_identity == 0U) {
            clear(ContextPresence::native_identity);
            valid = false;
        } else {
            sanitized.native_identity = input.native_identity;
        }
    }

    if (has_context_field(sanitized.presence_mask, ContextPresence::activity)) {
        if (!static_cast<bool>(input.activity)) {
            clear(ContextPresence::activity);
            valid = false;
        } else {
            sanitized.activity = input.activity;
        }
    }

    if (has_context_field(sanitized.presence_mask, ContextPresence::session)) {
        if (input.session.session_id == 0U || input.session.created_revision == 0U
            || input.session.record_revision == 0U
            || input.session.record_revision < input.session.created_revision) {
            clear(ContextPresence::session);
            valid = false;
        } else {
            sanitized.session = input.session;
        }
    }

    if (has_context_field(sanitized.presence_mask, ContextPresence::activity)
        && has_context_field(sanitized.presence_mask, ContextPresence::session)
        && sanitized.activity.sessionId != sanitized.session.session_id) {
        clear(ContextPresence::activity);
        clear(ContextPresence::session);
        sanitized.activity = {};
        sanitized.session = {};
        valid = false;
    }

    const auto scalar = [&sanitized, &clear, &valid](ContextPresence field,
                                                    std::uint64_t inputValue,
                                                    std::uint64_t& outputValue) noexcept {
        if (!has_context_field(sanitized.presence_mask, field)) {
            return;
        }
        if (inputValue == 0U) {
            clear(field);
            valid = false;
            return;
        }
        outputValue = inputValue;
    };
    scalar(ContextPresence::run_token, input.run_token, sanitized.run_token);
    scalar(ContextPresence::correlation_token,
           input.correlation_token,
           sanitized.correlation_token);
    scalar(ContextPresence::generation_token,
           input.generation_token,
           sanitized.generation_token);

    output = sanitized;
    return valid;
}

LogicalPointIdentity logical_identity(std::uint32_t definition) noexcept {
    switch (definition) {
    case kObjectiveDefinition:
        return {kLogicalRegistry, kLogicalType, 18U, kLogicalRegistry, kOccupancyType, 28U};
    case kDialogueDefinition:
        return {kLogicalRegistry, kLogicalType, 19U, kLogicalRegistry, kOccupancyType, 26U};
    default:
        return {};
    }
}

bool valid_raw_record(const CaptureRecord& record) noexcept {
    return record.capture_epoch != 0U && record.component_identity != 0U
           && supported_definition(record.definition) && record.schema == kAuthoritySchema
           && record.logical == logical_identity(record.definition) && record.state_key_valid
           && record.decoded_body_pre_valid
           && state_key_schema(record.state_key) == kAuthoritySchema;
}

bool record_matches_context(const CaptureRecord& record,
                            const CaptureContext& expected) noexcept {
    return valid_raw_record(record) && record.context_shape_valid && fully_correlated(expected)
           && record.activation_entry_exact && record.context == expected;
}

bool record_matches_current_context(const CaptureRecord& record,
                                    const CaptureContext& expected) noexcept {
    return record_matches_context(record, expected) && record.activation_current_at_exit;
}

bool decode_body(const CaptureRecord& record, DecodedBodyFields& fields) noexcept {
    const std::uint8_t rawBool =
        std::to_integer<std::uint8_t>(record.decoded_body_pre[kAuthBoolBodyOffset]);
    if (!valid_raw_record(record) || rawBool > 1U) {
        return false;
    }

    DecodedBodyFields decoded{};
    decoded.auth_bool = rawBool != 0U;
    std::memcpy(
        &decoded.u64_0, record.decoded_body_pre.data() + kU64_0BodyOffset, sizeof decoded.u64_0);
    std::memcpy(
        &decoded.u64_1, record.decoded_body_pre.data() + kU64_1BodyOffset, sizeof decoded.u64_1);
    fields = decoded;
    return true;
}

std::uint64_t bounded_hash(std::span<const std::byte> bytes) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    for (const std::byte value : bytes) {
        hash ^= std::to_integer<std::uint8_t>(value);
        hash *= kFnvPrime;
    }
    return hash;
}

bool default_log_fields(const CaptureRecord& record, DefaultLogFields& output) noexcept {
    if (!valid_raw_record(record)) {
        return false;
    }

    DefaultLogFields fields{};
    std::uint64_t componentHash = 14695981039346656037ULL;
    append_hash_value(componentHash, record.capture_epoch);
    append_hash_value(componentHash, record.component_identity);
    fields.component_token = componentHash;

    std::uint64_t contextHash = 14695981039346656037ULL;
    append_hash_value(contextHash, record.context.presence_mask);
    append_hash_value(contextHash, record.context.activation.module.value);
    append_hash_value(contextHash, record.context.activation.generation.value);
    append_hash_value(contextHash, record.context.native_identity);
    append_hash_value(contextHash, record.context.activity.sessionId);
    append_hash_value(contextHash, record.context.activity.incarnation.value);
    append_hash_value(contextHash, record.context.session.session_id);
    append_hash_value(contextHash, record.context.session.created_revision);
    append_hash_value(contextHash, record.context.session.record_revision);
    append_hash_value(contextHash, record.context.run_token);
    append_hash_value(contextHash, record.context.correlation_token);
    append_hash_value(contextHash, record.context.generation_token);
    append_hash_value(contextHash, record.context.activation_state);
    fields.context_hash = contextHash;

    fields.state_key_hash = bounded_hash(record.state_key);
    fields.decoded_body_pre_hash = bounded_hash(record.decoded_body_pre);
    fields.component_before_hash =
        record.component_before_valid
            ? bounded_hash(std::as_bytes(std::span{&record.before, 1U}))
            : 0U;
    fields.component_after_hash =
        record.component_after_valid
            ? bounded_hash(std::as_bytes(std::span{&record.after, 1U}))
            : 0U;
    fields.decoded_valid = decode_body(record, fields.decoded);
    output = fields;
    return true;
}

bool CaptureQueue::try_lock() noexcept {
    return !lock_.test_and_set(std::memory_order_acquire);
}

void CaptureQueue::unlock() noexcept {
    lock_.clear(std::memory_order_release);
}

void CaptureQueue::pop_front(CaptureRecord& output) noexcept {
    const CaptureRecord record = records_[head_];
    head_ = (head_ + 1U) % records_.size();
    --count_;
    output = record;
}

PushResult CaptureQueue::try_push(const CaptureRecord& record) noexcept {
    if (!valid_raw_record(record)) {
        account_rejected();
        return PushResult::rejected;
    }
    if (!record.context_shape_valid) {
        context_resolver_failures_.fetch_add(1U, std::memory_order_relaxed);
    }

    CaptureQueueLock lock{*this};
    if (!lock) {
        dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
        return PushResult::busy;
    }
    // Component addresses are diagnostic identities only. Dedupe is legal solely when both
    // records carry the same complete exact owner; absent/partial contexts always enqueue.
    if (has_last_accepted_ && last_accepted_.context_shape_valid
        && record.context_shape_valid && fully_correlated(last_accepted_.context)
        && fully_correlated(record.context) && same_observation(last_accepted_, record)) {
        duplicates_.fetch_add(1U, std::memory_order_relaxed);
        return PushResult::duplicate;
    }
    if (count_ == records_.size()) {
        dropped_full_.fetch_add(1U, std::memory_order_relaxed);
        return PushResult::full;
    }
    if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
        dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
        return PushResult::sequence_exhausted;
    }

    CaptureRecord accepted = record;
    accepted.sequence = next_sequence_++;
    const std::size_t tail = (head_ + count_) % records_.size();
    records_[tail] = accepted;
    last_accepted_ = accepted;
    has_last_accepted_ = true;
    ++count_;
    accepted_.fetch_add(1U, std::memory_order_relaxed);
    return PushResult::enqueued;
}

void CaptureQueue::account_rejected() noexcept {
    rejected_.fetch_add(1U, std::memory_order_relaxed);
}

void CaptureQueue::account_projection_failure() noexcept {
    projection_failures_.fetch_add(1U, std::memory_order_relaxed);
}

ReadResult CaptureQueue::try_pop_raw(CaptureRecord& output) noexcept {
    CaptureQueueLock lock{*this};
    if (!lock) {
        return ReadResult::busy;
    }
    if (count_ == 0U) {
        return ReadResult::empty;
    }
    pop_front(output);
    return ReadResult::success;
}

ReadResult CaptureQueue::try_pop_exact(const CaptureContext& expected,
                                       CaptureRecord& output) noexcept {
    if (!fully_correlated(expected)) {
        return ReadResult::invalid_context;
    }

    CaptureQueueLock lock{*this};
    if (!lock) {
        return ReadResult::busy;
    }
    if (count_ == 0U) {
        return ReadResult::empty;
    }
    if (!record_matches_context(records_[head_], expected)) {
        return ReadResult::context_mismatch;
    }
    pop_front(output);
    return ReadResult::success;
}

ReadResult CaptureQueue::try_pop_exact_current(const CaptureContext& expected,
                                               CaptureRecord& output) noexcept {
    if (!fully_correlated(expected)) {
        return ReadResult::invalid_context;
    }

    CaptureQueueLock lock{*this};
    if (!lock) {
        return ReadResult::busy;
    }
    if (count_ == 0U) {
        return ReadResult::empty;
    }
    if (!record_matches_context(records_[head_], expected)) {
        return ReadResult::context_mismatch;
    }
    if (!records_[head_].activation_current_at_exit) {
        return ReadResult::not_current;
    }
    pop_front(output);
    return ReadResult::success;
}

ReadResult CaptureQueue::try_front_context(CaptureContext& output) noexcept {
    CaptureQueueLock lock{*this};
    if (!lock) {
        return ReadResult::busy;
    }
    if (count_ == 0U) {
        return ReadResult::empty;
    }
    output = records_[head_].context;
    return ReadResult::success;
}

ReadResult CaptureQueue::try_discard_exact(const CaptureContext& expected) noexcept {
    if (!fully_correlated(expected)) {
        return ReadResult::invalid_context;
    }

    CaptureQueueLock lock{*this};
    if (!lock) {
        return ReadResult::busy;
    }
    if (count_ == 0U) {
        return ReadResult::empty;
    }
    if (!record_matches_context(records_[head_], expected)) {
        return ReadResult::context_mismatch;
    }
    CaptureRecord discarded{};
    pop_front(discarded);
    return ReadResult::success;
}

QueueCounters CaptureQueue::counters() const noexcept {
    return QueueCounters{
        accepted_.load(std::memory_order_relaxed),
        duplicates_.load(std::memory_order_relaxed),
        rejected_.load(std::memory_order_relaxed),
        dropped_full_.load(std::memory_order_relaxed),
        dropped_busy_.load(std::memory_order_relaxed),
        dropped_sequence_exhausted_.load(std::memory_order_relaxed),
        context_resolver_failures_.load(std::memory_order_relaxed),
        projection_failures_.load(std::memory_order_relaxed),
    };
}

ResetResult CaptureQueue::try_reset() noexcept {
    CaptureQueueLock lock{*this};
    if (!lock) {
        return ResetResult::busy;
    }
    records_ = {};
    last_accepted_ = {};
    head_ = 0U;
    count_ = 0U;
    next_sequence_ = 1U;
    has_last_accepted_ = false;
    accepted_.store(0U, std::memory_order_relaxed);
    duplicates_.store(0U, std::memory_order_relaxed);
    rejected_.store(0U, std::memory_order_relaxed);
    dropped_full_.store(0U, std::memory_order_relaxed);
    dropped_busy_.store(0U, std::memory_order_relaxed);
    dropped_sequence_exhausted_.store(0U, std::memory_order_relaxed);
    context_resolver_failures_.store(0U, std::memory_order_relaxed);
    projection_failures_.store(0U, std::memory_order_relaxed);
    return ResetResult::reset;
}

CaptureBuildResult inspect_capture_target(const void* component,
                                          const void* stateKey16,
                                          CaptureTarget& output) noexcept {
    if (component == nullptr || stateKey16 == nullptr) {
        return CaptureBuildResult::null_pointer;
    }

    std::uint32_t definition{};
    if (!safe_copy_exact(&definition, component, sizeof definition)) {
        return CaptureBuildResult::unreadable;
    }
    if (!supported_definition(definition)) {
        return CaptureBuildResult::unsupported_definition;
    }

    std::uint32_t schema{};
    if (!safe_copy_exact(&schema, stateKey16, sizeof schema)) {
        return CaptureBuildResult::unreadable;
    }
    if (schema != kAuthoritySchema) {
        return CaptureBuildResult::wrong_schema;
    }

    output = CaptureTarget{definition, schema, logical_identity(definition)};
    return CaptureBuildResult::ready;
}

CaptureBuildResult prepare_capture(PendingCapture& pending,
                                   const CaptureContext& context,
                                   CaptureMetadata metadata,
                                   CaptureTarget target,
                                   const void* component,
                                   const void* stateKey16) noexcept {
    pending.context_ = {};
    pending.metadata_ = {};
    pending.target_ = {};
    pending.component_identity_ = 0U;
    pending.state_key_.fill(std::byte{});
    pending.decoded_body_pre_.fill(std::byte{});
    pending.before_ = {};
    pending.component_before_valid_ = false;
    pending.context_shape_valid_ = true;
    pending.activation_entry_exact_ = false;
    pending.ready_ = false;

    if (component == nullptr || stateKey16 == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    if (!supported_definition(target.definition)
        || target.logical != logical_identity(target.definition)) {
        return CaptureBuildResult::unsupported_definition;
    }
    if (target.schema != kAuthoritySchema) {
        return CaptureBuildResult::wrong_schema;
    }

    std::uint32_t currentDefinition{};
    std::array<std::byte, kStateKeyBytes> stateKey{};
    if (!safe_copy_exact(&currentDefinition, component, sizeof currentDefinition)
        || !safe_copy_exact(stateKey.data(), stateKey16, stateKey.size())) {
        return CaptureBuildResult::unreadable;
    }
    if (currentDefinition != target.definition || state_key_schema(stateKey) != target.schema) {
        return CaptureBuildResult::target_changed;
    }

    std::array<std::byte, kDecodedBodyBytes> decodedBody{};
    if (!capture_decoded_body(stateKey, decodedBody)) {
        return CaptureBuildResult::unreadable;
    }

    ComponentSnapshot before{};
    const bool beforeValid = capture_component_snapshot(component, before);
    pending.context_shape_valid_ = sanitize_context(context, pending.context_);
    pending.metadata_ = metadata;
    pending.target_ = target;
    pending.component_identity_ = reinterpret_cast<std::uintptr_t>(component);
    pending.state_key_ = stateKey;
    pending.decoded_body_pre_ = decodedBody;
    pending.before_ = before;
    pending.component_before_valid_ = beforeValid;
    pending.activation_entry_exact_ = activation_snapshot_exact_at_entry(pending.context_);
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_capture(PendingCapture& pending,
                                  const void* component,
                                  bool activationCurrentAtExit,
                                  CaptureRecord& output) noexcept {
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }

    pending.ready_ = false;
    if (reinterpret_cast<std::uintptr_t>(component) != pending.component_identity_) {
        return CaptureBuildResult::component_mismatch;
    }

    ComponentSnapshot after{};
    const bool afterValid = capture_component_snapshot(component, after);
    CaptureRecord record{};
    record.context = pending.context_;
    record.capture_epoch = pending.metadata_.capture_epoch;
    record.monotonic_tick = pending.metadata_.monotonic_tick;
    record.producer_thread_id = pending.metadata_.producer_thread_id;
    record.caller_rva = pending.metadata_.caller_rva;
    record.component_identity = pending.component_identity_;
    record.definition = pending.target_.definition;
    record.schema = pending.target_.schema;
    record.logical = pending.target_.logical;
    record.state_key = pending.state_key_;
    record.decoded_body_pre = pending.decoded_body_pre_;
    record.before = pending.before_;
    record.after = after;
    record.state_key_valid = true;
    record.decoded_body_pre_valid = true;
    record.component_before_valid = pending.component_before_valid_;
    record.component_after_valid = afterValid;
    record.context_shape_valid = pending.context_shape_valid_;
    record.activation_entry_exact = pending.activation_entry_exact_;
    record.activation_current_at_exit =
        pending.activation_entry_exact_ && activationCurrentAtExit
        && pending.context_.activation_state == ActivationSnapshotState::current;
    if (!valid_raw_record(record)) {
        return CaptureBuildResult::consumed;
    }

    output = record;
    return record.component_before_valid && record.component_after_valid
               ? CaptureBuildResult::complete
               : CaptureBuildResult::partial;
}

} // namespace sunrise::client::hooks::bootflow::type31_capture
