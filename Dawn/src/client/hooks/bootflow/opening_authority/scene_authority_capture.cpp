#include "scene_authority_capture.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace dawn::client::hooks::bootflow::opening_authority::scene_capture {
namespace {

template <typename Value>
[[nodiscard]] Value read_value(const std::byte* source) noexcept {
    Value value{};
    std::memcpy(&value, source, sizeof value);
    return value;
}

[[nodiscard]] bool nonzero(const NativeActivationKey& value) noexcept {
    return value.module_generation != 0U && value.activation_generation != 0U;
}

[[nodiscard]] std::uint32_t read_msb_u32(std::span<const std::byte> bytes,
                                        std::size_t startBit) noexcept;

[[nodiscard]] bool source_is_partial(CopyOutcome first, CopyOutcome second) noexcept {
    return first == CopyOutcome::partial || second == CopyOutcome::partial;
}

[[nodiscard]] bool seh_copy_exact_bytes(void* destination,
                                        const void* source,
                                        std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr) {
        return false;
    }
#if defined(_MSC_VER) && defined(_WIN32)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(destination, source, bytes);
    return true;
#endif
}

[[nodiscard]] bool authority_owner_pair_valid(const OwnerCallProvenance& outer,
                                              const OwnerCallProvenance& nested) noexcept {
    return valid_owner_provenance(outer, kReceiveOuterRva, 0U, 0U)
           && valid_owner_provenance(nested,
                                     kAuthorityDecodeRva,
                                     kReceiveOuterRva,
                                     kReceiveOuterRecoveredBytes)
           && nested.parent_call_id == outer.call_id
           && nested.producer_thread_id == outer.producer_thread_id
           && nested.tls_depth == outer.tls_depth + 1U;
}

[[nodiscard]] bool sense_owner_pair_valid(const OwnerCallProvenance& outer,
                                          const OwnerCallProvenance& nested) noexcept {
    return valid_owner_provenance(outer, kSenseOuterRva, 0U, 0U)
           && valid_owner_provenance(nested,
                                     kSenseDeltaRva,
                                     kSenseOuterRva,
                                     kSenseOuterRecoveredBytes)
           && nested.parent_call_id == outer.call_id
           && nested.producer_thread_id == outer.producer_thread_id
           && nested.tls_depth == outer.tls_depth + 1U;
}

[[nodiscard]] bool frozen_sense_valid(const SenseCapture& capture) noexcept {
    if (capture.capture_format_version != kCaptureFormatVersion
        || !valid_sense_build(capture.build) || !valid_exact_context(capture.context)
        || !valid_sense_sensor(capture.sensor) || capture.writer_copy != CopyOutcome::complete
        || !sense_owner_pair_valid(capture.outer_owner, capture.delta_owner)
        || capture.actual_delta_schema_argument != kSceneSenseSchema
        || capture.current_source_address == 0U || capture.prior_source_address == 0U
        || capture.writer_buffer_address == 0U || capture.writer_capacity_bytes == 0U
        || capture.writer_pending_bits > 64U || capture.writer_error != 0U
        || capture.writer_completed_bits
               > (std::numeric_limits<std::size_t>::max)() - capture.writer_pending_bits
        || capture.writer_logical_end
               != capture.writer_completed_bits + capture.writer_pending_bits
        || capture.writer_capacity_bytes > (std::numeric_limits<std::size_t>::max)() / 8U
        || capture.writer_logical_end > capture.writer_capacity_bytes * 8U
        || capture.writer_logical_end != capture.body_end
        || capture.body_end < capture.body_start
        || capture.delta_end < capture.body_start || capture.body_end < capture.delta_end
        || capture.body_bits != capture.body_end - capture.body_start
        || capture.delta_bits != capture.delta_end - capture.body_start
        || capture.body_end - capture.delta_end != kSenseRevisionBits
        || capture.body_bits > kSenseCaptureStorageBits
        || capture.body_hash != bounded_hash(capture.body, capture.body_bits)
        || capture.delta_hash != bounded_hash(capture.body, capture.delta_bits)
        || !unused_low_bits_zero(capture.body, capture.body_bits)
        || capture.next_revision != capture.accepted_revision + 1U) {
        return false;
    }
    return read_msb_u32(capture.body, capture.delta_bits) == capture.next_revision;
}

[[nodiscard]] bool source_is_fault(CopyOutcome first, CopyOutcome second) noexcept {
    return first == CopyOutcome::fault || second == CopyOutcome::fault;
}

[[nodiscard]] std::uint32_t read_msb_u32(std::span<const std::byte> bytes,
                                        std::size_t startBit) noexcept {
    std::uint32_t value{};
    for (std::size_t index = 0U; index < 32U; ++index) {
        const std::size_t bit = startBit + index;
        const auto octet = std::to_integer<std::uint8_t>(bytes[bit / 8U]);
        value = static_cast<std::uint32_t>(
            (value << 1U) | ((octet >> (7U - (bit % 8U))) & 1U));
    }
    return value;
}

[[nodiscard]] std::uint64_t read_msb_u64(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value{};
    for (std::size_t index = 0U; index < 8U; ++index) {
        value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[index]);
    }
    return value;
}

[[nodiscard]] bool valid_queue_observation(const ScalarHashObservation& observation) noexcept {
    if (observation.capture_format_version != kCaptureFormatVersion
        || observation.build_fingerprint != kPinnedUnpackedPcSha256
        || !valid_exact_context(observation.context) || observation.sequence != 0U
        || observation.committed_record_sequence == 0U || observation.owner_generation == 0U
        || observation.owner_generation != observation.owner_generation_at_exit
        || observation.sensor_record_generation == 0U
        || observation.sensor_record_address_hash == 0U || observation.body_bits == 0U
        || observation.body_hash == 0U || observation.producer_thread_id == 0U
        || observation.owner_entry_rva == 0U || observation.native_caller_rva == 0U
        || observation.sensor_registry != kSceneRegistry || observation.sensor_type != kSceneType
        || observation.sensor_index != kSceneIndex
        || observation.status != ObservationStatus::complete) {
        return false;
    }
    switch (observation.kind) {
    case ObservationKind::authority_decode:
        return observation.actual_schema_argument == kSceneAuthoritySchema
               && observation.decoded_hash != 0U && observation.delta_bits == 0U
               && observation.next_revision == 0U && observation.component_generation == 0U
               && observation.apply_owner_generation == 0U;
    case ObservationKind::authority_apply:
    case ObservationKind::authority_oracle:
        return observation.actual_schema_argument == kSceneAuthoritySchema
               && observation.decoded_hash != 0U && observation.component_generation != 0U
               && observation.component_address_hash != 0U
               && observation.component_definition == kSceneDefinition
               && observation.apply_owner_generation != 0U && observation.delta_bits == 0U
               && observation.next_revision == 0U;
    case ObservationKind::sense:
        return observation.actual_schema_argument == kSceneSenseSchema
               && observation.delta_bits + kSenseRevisionBits == observation.body_bits
               && observation.delta_bits != 0U
               && observation.component_generation == 0U
               && observation.apply_owner_generation == 0U;
    case ObservationKind::legacy_prefix:
    case ObservationKind::retirement:
        return false;
    default:
        return false;
    }
}

[[nodiscard]] bool same_without_sequence(const ScalarHashObservation& left,
                                         const ScalarHashObservation& right) noexcept {
    ScalarHashObservation normalizedLeft = left;
    ScalarHashObservation normalizedRight = right;
    normalizedLeft.sequence = 0U;
    normalizedRight.sequence = 0U;
    return normalizedLeft == normalizedRight;
}

} // namespace

bool valid_authority_build(const BuildEvidence& build) noexcept {
    return build.unpacked_pc_sha256 == kPinnedUnpackedPcSha256 && build.image_identity_valid
           && build.receive_outer_prefix_valid && build.authority_decode_prefix_valid
           && build.scene_apply_prefix_valid && build.component_start_prefix_valid;
}

bool valid_sense_build(const BuildEvidence& build) noexcept {
    return build.unpacked_pc_sha256 == kPinnedUnpackedPcSha256 && build.image_identity_valid
           && build.sense_outer_prefix_valid && build.sense_delta_prefix_valid;
}

bool valid_owner_provenance(const OwnerCallProvenance& provenance,
                            std::uintptr_t expectedEntryRva,
                            std::uintptr_t callerRangeStart,
                            std::size_t callerRangeBytes) noexcept {
    if (provenance.owner_id == 0U || provenance.generation_at_entry == 0U
        || provenance.generation_at_entry != provenance.generation_at_exit
        || provenance.call_id == 0U || provenance.producer_thread_id == 0U
        || provenance.tls_depth >= kOwnerTlsMaximumDepth
        || provenance.owner_entry_rva != expectedEntryRva
        || provenance.native_caller_rva == 0U) {
        return false;
    }
    if (callerRangeBytes == 0U) {
        return true;
    }
    if (callerRangeStart > (std::numeric_limits<std::uintptr_t>::max)() - callerRangeBytes) {
        return false;
    }
    return provenance.native_caller_rva >= callerRangeStart
           && provenance.native_caller_rva < callerRangeStart + callerRangeBytes;
}

bool valid_exact_context(const ExactCaptureContext& context) noexcept {
    return context.capture_epoch != 0U && nonzero(context.activation)
           && context.activity.session_id != 0U && context.activity.activity_id != 0U
           && context.activity.activity_generation != 0U && context.session.session_id != 0U
           && context.session.created_revision != 0U && context.session.record_revision != 0U
           && context.activity.session_id == context.session.session_id && context.run_token != 0U
           && context.correlation_token != 0U && context.authority_generation != 0U;
}

bool valid_authority_sensor(const SensorRecordLineage& sensor) noexcept {
    return sensor.record_address != 0U && sensor.record_generation != 0U
           && exact_scene_identity(sensor.identity)
           && sensor.identity.authority_schema == kSceneAuthoritySchema
           && sensor.expected_received_authority_destination != 0U;
}

bool valid_sense_sensor(const SensorRecordLineage& sensor) noexcept {
    return sensor.record_address != 0U && sensor.record_generation != 0U
           && exact_scene_identity(sensor.identity)
           && sensor.identity.sense_schema == kSceneSenseSchema;
}

bool valid_scene_component(const SceneComponentLineage& component,
                           const ExactCaptureContext& context) noexcept {
    return component.entry_address != 0U && component.descriptor_address != 0U
           && component.component_address != 0U && component.handler_address != 0U
           && component.component_generation != 0U
           && component.definition == kSceneDefinition && component.owner_object != 0U
           && component.encoded_component_size != 0U && nonzero(component.activation)
           && component.activation == context.activation;
}

IntervalResult normalize_msb_interval(std::span<const std::byte> backing,
                                      std::size_t capacityBytes,
                                      std::uint32_t startBit,
                                      std::uint32_t endBit,
                                      std::span<std::byte> output) noexcept {
    std::fill(output.begin(), output.end(), std::byte{0U});
    if (endBit < startBit) {
        return IntervalResult::decreasing_cursor;
    }
    if (capacityBytes > (std::numeric_limits<std::size_t>::max)() / 8U
        || static_cast<std::size_t>(endBit) > capacityBytes * 8U) {
        return IntervalResult::capacity_overflow;
    }
    if (capacityBytes > backing.size()
        || static_cast<std::size_t>(endBit) > backing.size() * 8U) {
        return IntervalResult::backing_too_small;
    }
    const std::size_t bitCount = static_cast<std::size_t>(endBit - startBit);
    if (bitCount > output.size() * 8U) {
        return IntervalResult::output_too_small;
    }
    for (std::size_t index = 0U; index < bitCount; ++index) {
        const std::size_t sourceBit = static_cast<std::size_t>(startBit) + index;
        const std::uint8_t source = std::to_integer<std::uint8_t>(backing[sourceBit / 8U]);
        if ((source & static_cast<std::uint8_t>(0x80U >> (sourceBit % 8U))) != 0U) {
            output[index / 8U] |= std::byte{static_cast<std::uint8_t>(0x80U >> (index % 8U))};
        }
    }
    return IntervalResult::success;
}

bool significant_bits_equal(std::span<const std::byte> left,
                            std::span<const std::byte> right,
                            std::size_t significantBits) noexcept {
    const std::size_t wholeBytes = significantBits / 8U;
    const std::size_t neededBytes = wholeBytes + ((significantBits % 8U) != 0U ? 1U : 0U);
    if (left.size() < neededBytes || right.size() < neededBytes) {
        return false;
    }
    if (!std::equal(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(wholeBytes),
                    right.begin())) {
        return false;
    }
    const std::size_t trailingBits = significantBits % 8U;
    if (trailingBits == 0U) {
        return true;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(0xFFU << (8U - trailingBits));
    return (std::to_integer<std::uint8_t>(left[wholeBytes]) & mask)
           == (std::to_integer<std::uint8_t>(right[wholeBytes]) & mask);
}

bool unused_low_bits_zero(std::span<const std::byte> bytes,
                          std::size_t significantBits) noexcept {
    const std::size_t neededBytes = (significantBits + 7U) / 8U;
    if (bytes.size() < neededBytes) {
        return false;
    }
    const std::size_t trailingBits = significantBits % 8U;
    if (trailingBits == 0U || neededBytes == 0U) {
        return true;
    }
    const std::uint8_t unusedMask = static_cast<std::uint8_t>((1U << (8U - trailingBits)) - 1U);
    return (std::to_integer<std::uint8_t>(bytes[neededBytes - 1U]) & unusedMask) == 0U;
}

std::uint64_t bounded_hash(std::span<const std::byte> bytes,
                           std::size_t significantBits) noexcept {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    const std::size_t neededBytes = (significantBits + 7U) / 8U;
    if (bytes.size() < neededBytes) {
        return 0U;
    }
    std::uint64_t hash = kOffset;
    for (std::size_t index = 0U; index < neededBytes; ++index) {
        std::uint8_t value = std::to_integer<std::uint8_t>(bytes[index]);
        if (index + 1U == neededBytes && (significantBits % 8U) != 0U) {
            value &= static_cast<std::uint8_t>(0xFFU << (8U - (significantBits % 8U)));
        }
        hash ^= value;
        hash *= kPrime;
    }
    hash ^= static_cast<std::uint64_t>(significantBits);
    hash *= kPrime;
    return hash;
}

ShapeResult authority_shape(const AuthorityDecoded& decoded, AuthorityShape& output) noexcept {
    output = {};
    const std::uint32_t entryCount = read_value<std::uint32_t>(decoded.data() + 0x08U);
    const std::uint32_t wordCount = read_value<std::uint32_t>(decoded.data() + 0x50U);
    if (entryCount > kAuthorityEntryMaximum) {
        return ShapeResult::entry_count_overflow;
    }
    if (wordCount > kAuthorityWordMaximum) {
        return ShapeResult::word_count_overflow;
    }
    constexpr std::size_t kBaseBits = 74U;
    constexpr std::size_t kEntryBits = 55U;
    constexpr std::size_t kWordBits = 32U;
    const std::size_t bits = kBaseBits + kEntryBits * entryCount + kWordBits * wordCount;
    if (bits < kBaseBits || bits > kAuthorityMaximumBits) {
        return ShapeResult::arithmetic_overflow;
    }
    output.entry_count = entryCount;
    output.word_count = wordCount;
    output.significant_bits = bits;
    return ShapeResult::valid;
}

AuthorityCaptureResult capture_authority_decode(const AuthorityDecodeFanoutRecord& input,
                                                AuthorityDecodeCapture& output) noexcept {
    output = {};
    if (!valid_authority_build(input.build)) {
        return AuthorityCaptureResult::wrong_build;
    }
    if (!valid_exact_context(input.context)) {
        return AuthorityCaptureResult::incomplete_context;
    }
    if (!exact_scene_identity(input.sensor.identity)) {
        return AuthorityCaptureResult::wrong_sensor;
    }
    if (!valid_authority_sensor(input.sensor)) {
        return AuthorityCaptureResult::stale_sensor;
    }
    if (!authority_owner_pair_valid(input.outer_owner, input.decode_owner)) {
        return AuthorityCaptureResult::owner_pair_mismatch;
    }
    if (input.actual_schema_argument != kSceneAuthoritySchema || !input.mode_argument_captured) {
        return AuthorityCaptureResult::wrong_sensor;
    }
    if (input.destination_address == 0U
        || input.destination_address != input.sensor.expected_received_authority_destination) {
        return AuthorityCaptureResult::wrong_destination;
    }
    if (input.inout_bytes_before != static_cast<std::int32_t>(kAuthorityDecodedBytes)
        || input.inout_bytes_after != static_cast<std::int32_t>(kAuthorityDecodedBytes)) {
        return AuthorityCaptureResult::wrong_size;
    }
    if (!input.decoder_returned_true) {
        return AuthorityCaptureResult::native_failure;
    }
    if (source_is_fault(input.reader.backing_copy, input.decoded_copy)) {
        return AuthorityCaptureResult::source_fault;
    }
    if (source_is_partial(input.reader.backing_copy, input.decoded_copy)) {
        return AuthorityCaptureResult::source_partial;
    }
    if (input.source_copy_timing != CopyTiming::after_original_before_owner_exit) {
        return AuthorityCaptureResult::source_fault;
    }
    if (!input.reader.nested_reflected_body_interval) {
        return AuthorityCaptureResult::owner_pair_mismatch;
    }
    if (input.reader.error_before != 0U || input.reader.error_after != 0U) {
        return AuthorityCaptureResult::reader_error;
    }
    if (input.reader.buffer_address == 0U || input.reader.capacity_bytes == 0U) {
        return AuthorityCaptureResult::reader_bounds;
    }
    if (input.reader.end_bit < input.reader.start_bit) {
        return AuthorityCaptureResult::reader_bounds;
    }
    const std::size_t bodyBits =
        static_cast<std::size_t>(input.reader.end_bit - input.reader.start_bit);
    if (bodyBits < kAuthorityMinimumBits) {
        return AuthorityCaptureResult::body_too_small;
    }
    if (bodyBits > kAuthorityMaximumBits) {
        return AuthorityCaptureResult::body_too_large;
    }
    AuthorityShape shape{};
    if (authority_shape(input.decoded, shape) != ShapeResult::valid) {
        return AuthorityCaptureResult::illegal_shape;
    }
    if (shape.significant_bits != bodyBits) {
        return AuthorityCaptureResult::length_mismatch;
    }
    AuthorityRaw raw{};
    if (normalize_msb_interval(input.reader.backing,
                               input.reader.capacity_bytes,
                               input.reader.start_bit,
                               input.reader.end_bit,
                               raw) != IntervalResult::success) {
        return AuthorityCaptureResult::reader_bounds;
    }

    output.capture_format_version = kCaptureFormatVersion;
    output.build = input.build;
    output.context = input.context;
    output.sensor = input.sensor;
    output.outer_owner = input.outer_owner;
    output.decode_owner = input.decode_owner;
    output.actual_schema_argument = input.actual_schema_argument;
    output.actual_mode_argument = input.actual_mode_argument;
    output.mode_argument_captured = input.mode_argument_captured;
    output.destination_address = input.destination_address;
    output.reader_buffer_address = input.reader.buffer_address;
    output.reader_capacity_bytes = input.reader.capacity_bytes;
    output.start_bit = input.reader.start_bit;
    output.end_bit = input.reader.end_bit;
    output.error_before = input.reader.error_before;
    output.error_after = input.reader.error_after;
    output.decoder_returned_true = input.decoder_returned_true;
    output.inout_bytes_before = input.inout_bytes_before;
    output.inout_bytes_after = input.inout_bytes_after;
    output.raw = raw;
    output.body_bits = bodyBits;
    output.raw_hash = bounded_hash(output.raw, bodyBits);
    output.decoded = input.decoded;
    output.decoded_hash = bounded_hash(output.decoded, output.decoded.size() * 8U);
    output.shape = shape;
    output.reader_copy = input.reader.backing_copy;
    output.decoded_copy = input.decoded_copy;
    output.source_copy_timing = input.source_copy_timing;
    return AuthorityCaptureResult::complete;
}

AuthorityCaptureResult capture_live_authority_decode(
    const LiveAuthorityDecodeFanoutRecord& input,
    AuthorityDecodeCapture& output) noexcept {
    output = {};
    const AuthorityReaderView& reader = input.metadata.reader;
    if (reader.end_bit < reader.start_bit) {
        return AuthorityCaptureResult::reader_bounds;
    }
    const std::size_t bodyBits = static_cast<std::size_t>(reader.end_bit - reader.start_bit);
    if (bodyBits < kAuthorityMinimumBits) {
        return AuthorityCaptureResult::body_too_small;
    }
    if (bodyBits > kAuthorityMaximumBits) {
        return AuthorityCaptureResult::body_too_large;
    }
    const std::size_t firstByte = reader.start_bit / 8U;
    const std::size_t finalByte = (static_cast<std::size_t>(reader.end_bit) + 7U) / 8U;
    if (finalByte < firstByte || finalByte - firstByte > kAuthorityMaximumBytes + 1U
        || input.live_reader_readable_bytes < finalByte
        || input.live_decoded_readable_bytes < kAuthorityDecodedBytes) {
        return AuthorityCaptureResult::source_partial;
    }
    if (input.live_reader_buffer == nullptr || input.live_decoded_source == nullptr
        || reader.buffer_address != reinterpret_cast<std::uintptr_t>(input.live_reader_buffer)
        || input.metadata.destination_address
               != reinterpret_cast<std::uintptr_t>(input.live_decoded_source)
        || firstByte > (std::numeric_limits<std::uintptr_t>::max)()
                           - reinterpret_cast<std::uintptr_t>(input.live_reader_buffer)) {
        return AuthorityCaptureResult::source_fault;
    }
    std::array<std::byte, kAuthorityMaximumBytes + 1U> readerWindow{};
    AuthorityDecoded decoded{};
    const std::size_t readerBytes = finalByte - firstByte;
    if (!seh_copy_exact_bytes(readerWindow.data(),
                              input.live_reader_buffer + firstByte,
                              readerBytes)
        || !seh_copy_exact_bytes(decoded.data(),
                                 input.live_decoded_source,
                                 decoded.size())) {
        return AuthorityCaptureResult::source_fault;
    }

    AuthorityDecodeFanoutRecord owned = input.metadata;
    owned.reader.backing = {readerWindow.data(), readerBytes};
    owned.reader.capacity_bytes = readerBytes;
    owned.reader.start_bit = reader.start_bit % 8U;
    owned.reader.end_bit = static_cast<std::uint32_t>(owned.reader.start_bit + bodyBits);
    owned.reader.backing_copy = CopyOutcome::complete;
    owned.decoded = decoded;
    owned.decoded_copy = CopyOutcome::complete;
    owned.source_copy_timing = CopyTiming::after_original_before_owner_exit;
    const AuthorityCaptureResult result = capture_authority_decode(owned, output);
    if (result == AuthorityCaptureResult::complete) {
        output.reader_buffer_address = reader.buffer_address;
        output.reader_capacity_bytes = reader.capacity_bytes;
        output.start_bit = reader.start_bit;
        output.end_bit = reader.end_bit;
    }
    return result;
}

ApplyCorrelationResult correlate_scene_apply(const AuthorityDecodeCapture& decode,
                                             const SceneApplyFanoutRecord& apply,
                                             AuthorityEvidenceRecord& output) noexcept {
    output = {};
    if (!valid_authority_build(apply.build) || apply.build != decode.build) {
        return ApplyCorrelationResult::wrong_build;
    }
    if (!valid_exact_context(apply.context) || apply.context != decode.context) {
        return ApplyCorrelationResult::incomplete_context;
    }
    if (apply.component.definition != kSceneDefinition) {
        return ApplyCorrelationResult::wrong_definition;
    }
    if (!valid_scene_component(apply.component, apply.context)
        || !valid_owner_provenance(apply.apply_owner, kSceneApplyRva, 0U, 0U)
        || !valid_owner_provenance(apply.component_owner, kComponentStartRva, 0U, 0U)) {
        return ApplyCorrelationResult::stale_component;
    }
    if (apply.decoded_source_copy == CopyOutcome::fault) {
        return ApplyCorrelationResult::source_fault;
    }
    if (apply.decoded_source_copy == CopyOutcome::partial) {
        return ApplyCorrelationResult::source_partial;
    }
    if (apply.component_before_copy == CopyOutcome::fault
        || apply.component_after_copy == CopyOutcome::fault) {
        return ApplyCorrelationResult::component_fault;
    }
    if (apply.component_before_copy == CopyOutcome::partial
        || apply.component_after_copy == CopyOutcome::partial) {
        return ApplyCorrelationResult::component_partial;
    }
    if (apply.decoded_source_copy_timing != CopyTiming::before_original
        || apply.component_before_copy_timing != CopyTiming::before_original
        || apply.component_after_copy_timing != CopyTiming::after_original_before_owner_exit) {
        return ApplyCorrelationResult::source_fault;
    }
    const std::uint32_t stateSchema = read_value<std::uint32_t>(apply.state_key.data());
    const std::uint64_t stateSource = read_value<std::uint64_t>(apply.state_key.data() + 0x08U);
    if (stateSchema != kSceneAuthoritySchema || stateSource != decode.destination_address) {
        return ApplyCorrelationResult::wrong_state_key;
    }
    const std::uint64_t sourceHash =
        bounded_hash(apply.decoded_source, apply.decoded_source.size() * 8U);
    if (apply.decoded_source_address != decode.destination_address
        || sourceHash != decode.decoded_hash || apply.decoded_source != decode.decoded) {
        return ApplyCorrelationResult::source_mismatch;
    }
    if (!original_called_exactly_once(apply.original_call_audit)) {
        return ApplyCorrelationResult::original_count_mismatch;
    }
    const std::uint32_t expectedScalar =
        read_value<std::uint32_t>(decode.decoded.data() + 0x4CU);
    const std::uint8_t expectedActive = decode.shape.entry_count != 0U ? 1U : 0U;
    if (apply.derived_scalar_after != expectedScalar
        || apply.derived_active_after != expectedActive) {
        return ApplyCorrelationResult::apply_semantic_mismatch;
    }

    output.capture_format_version = kCaptureFormatVersion;
    output.decode = decode;
    output.component = apply.component;
    output.apply_owner = apply.apply_owner;
    output.component_owner = apply.component_owner;
    output.state_key = apply.state_key;
    output.apply_source_address = apply.decoded_source_address;
    output.apply_source = apply.decoded_source;
    output.apply_source_hash = sourceHash;
    output.apply_source_copy = apply.decoded_source_copy;
    output.apply_source_copy_timing = apply.decoded_source_copy_timing;
    output.component_180_19f_before = apply.component_180_19f_before;
    output.component_180_19f_after = apply.component_180_19f_after;
    output.component_before_copy = apply.component_before_copy;
    output.component_after_copy = apply.component_after_copy;
    output.component_before_copy_timing = apply.component_before_copy_timing;
    output.component_after_copy_timing = apply.component_after_copy_timing;
    output.derived_scalar_before = apply.derived_scalar_before;
    output.derived_scalar_after = apply.derived_scalar_after;
    output.derived_active_before = apply.derived_active_before;
    output.derived_active_after = apply.derived_active_after;
    output.original_call_audit = apply.original_call_audit;
    return ApplyCorrelationResult::complete;
}

ApplyCorrelationResult prepare_live_scene_apply(const AuthorityDecodeCapture& decode,
                                                const LiveSceneApplyEntry& input,
                                                PendingLiveSceneApply& pending) noexcept {
    pending = {};
    if (!valid_authority_build(input.metadata.build)
        || input.metadata.build != decode.build) {
        return ApplyCorrelationResult::wrong_build;
    }
    if (!valid_exact_context(input.metadata.context)
        || input.metadata.context != decode.context) {
        return ApplyCorrelationResult::incomplete_context;
    }
    if (input.metadata.component.definition != kSceneDefinition) {
        return ApplyCorrelationResult::wrong_definition;
    }
    if (!valid_scene_component(input.metadata.component, input.metadata.context)
        || !valid_owner_provenance(input.metadata.apply_owner, kSceneApplyRva, 0U, 0U)
        || !valid_owner_provenance(input.metadata.component_owner,
                                   kComponentStartRva,
                                   0U,
                                   0U)) {
        return ApplyCorrelationResult::stale_component;
    }
    if (input.live_state_key == nullptr || input.live_decoded_source == nullptr
        || input.live_component == nullptr
        || reinterpret_cast<std::uintptr_t>(input.live_decoded_source)
               != input.metadata.decoded_source_address
        || reinterpret_cast<std::uintptr_t>(input.live_component)
               != input.metadata.component.component_address) {
        return ApplyCorrelationResult::source_fault;
    }
    if (input.live_state_key_readable_bytes < kStateKeyBytes
        || input.live_decoded_readable_bytes < kAuthorityDecodedBytes
        || input.live_component_readable_bytes <= kComponentDerivedActiveOffset) {
        return ApplyCorrelationResult::source_partial;
    }

    SceneApplyFanoutRecord owned = input.metadata;
    if (!seh_copy_exact_bytes(owned.state_key.data(), input.live_state_key, owned.state_key.size())
        || !seh_copy_exact_bytes(owned.decoded_source.data(),
                                 input.live_decoded_source,
                                 owned.decoded_source.size())) {
        return ApplyCorrelationResult::source_fault;
    }
    if (!seh_copy_exact_bytes(owned.component_180_19f_before.data(),
                              input.live_component + kComponentCommittedPrefixOffset,
                              owned.component_180_19f_before.size())
        || !seh_copy_exact_bytes(&owned.derived_scalar_before,
                                 input.live_component + kComponentDerivedScalarOffset,
                                 sizeof owned.derived_scalar_before)
        || !seh_copy_exact_bytes(&owned.derived_active_before,
                                 input.live_component + kComponentDerivedActiveOffset,
                                 sizeof owned.derived_active_before)) {
        return ApplyCorrelationResult::component_fault;
    }
    owned.decoded_source_copy = CopyOutcome::complete;
    owned.decoded_source_copy_timing = CopyTiming::before_original;
    owned.component_before_copy = CopyOutcome::complete;
    owned.component_before_copy_timing = CopyTiming::before_original;
    pending.owned = owned;
    pending.ready = true;
    return ApplyCorrelationResult::complete;
}

ApplyCorrelationResult finish_live_scene_apply(const AuthorityDecodeCapture& decode,
                                               PendingLiveSceneApply& pending,
                                               const std::byte* liveComponentAfter,
                                               std::size_t liveComponentReadableBytes,
                                               OriginalCallAudit originalCallAudit,
                                               AuthorityEvidenceRecord& output) noexcept {
    output = {};
    if (!pending.ready || liveComponentAfter == nullptr
        || reinterpret_cast<std::uintptr_t>(liveComponentAfter)
               != pending.owned.component.component_address) {
        return ApplyCorrelationResult::component_fault;
    }
    if (liveComponentReadableBytes <= kComponentDerivedActiveOffset) {
        return ApplyCorrelationResult::component_partial;
    }
    if (!seh_copy_exact_bytes(pending.owned.component_180_19f_after.data(),
                              liveComponentAfter + kComponentCommittedPrefixOffset,
                              pending.owned.component_180_19f_after.size())
        || !seh_copy_exact_bytes(&pending.owned.derived_scalar_after,
                                 liveComponentAfter + kComponentDerivedScalarOffset,
                                 sizeof pending.owned.derived_scalar_after)
        || !seh_copy_exact_bytes(&pending.owned.derived_active_after,
                                 liveComponentAfter + kComponentDerivedActiveOffset,
                                 sizeof pending.owned.derived_active_after)) {
        return ApplyCorrelationResult::component_fault;
    }
    pending.owned.component_after_copy = CopyOutcome::complete;
    pending.owned.component_after_copy_timing =
        CopyTiming::after_original_before_owner_exit;
    pending.owned.original_call_audit = originalCallAudit;
    const ApplyCorrelationResult result = correlate_scene_apply(decode, pending.owned, output);
    if (result == ApplyCorrelationResult::complete) {
        pending = {};
    }
    return result;
}

FrozenAuthorityValidation validate_frozen_authority_decode(
    const AuthorityDecodeCapture& decode) noexcept {
    if (decode.capture_format_version != kCaptureFormatVersion) {
        return FrozenAuthorityValidation::wrong_format;
    }
    if (!valid_authority_build(decode.build)) {
        return FrozenAuthorityValidation::wrong_build;
    }
    if (!valid_exact_context(decode.context)) {
        return FrozenAuthorityValidation::incomplete_context;
    }
    if (!exact_scene_identity(decode.sensor.identity)) {
        return FrozenAuthorityValidation::wrong_sensor;
    }
    if (!valid_authority_sensor(decode.sensor)) {
        return FrozenAuthorityValidation::stale_sensor;
    }
    if (!authority_owner_pair_valid(decode.outer_owner, decode.decode_owner)) {
        return FrozenAuthorityValidation::owner_provenance_mismatch;
    }
    if (decode.actual_schema_argument != kSceneAuthoritySchema
        || !decode.mode_argument_captured) {
        return FrozenAuthorityValidation::wrong_native_schema;
    }
    if (decode.destination_address == 0U
        || decode.destination_address
               != decode.sensor.expected_received_authority_destination) {
        return FrozenAuthorityValidation::wrong_destination;
    }
    if (decode.reader_buffer_address == 0U || decode.reader_capacity_bytes == 0U
        || decode.end_bit < decode.start_bit
        || decode.reader_capacity_bytes > (std::numeric_limits<std::size_t>::max)() / 8U
        || static_cast<std::size_t>(decode.end_bit) > decode.reader_capacity_bytes * 8U
        || decode.error_before != 0U || decode.error_after != 0U
        || decode.reader_copy != CopyOutcome::complete
        || decode.source_copy_timing != CopyTiming::after_original_before_owner_exit) {
        return FrozenAuthorityValidation::reader_invalid;
    }
    if (!decode.decoder_returned_true
        || decode.inout_bytes_before != static_cast<std::int32_t>(kAuthorityDecodedBytes)
        || decode.inout_bytes_after != static_cast<std::int32_t>(kAuthorityDecodedBytes)
        || decode.decoded_copy != CopyOutcome::complete) {
        return FrozenAuthorityValidation::decoder_invalid;
    }
    const std::size_t bodyBits = static_cast<std::size_t>(decode.end_bit - decode.start_bit);
    if (decode.body_bits != bodyBits || bodyBits < kAuthorityMinimumBits
        || bodyBits > kAuthorityMaximumBits
        || decode.raw_hash != bounded_hash(decode.raw, bodyBits)) {
        return FrozenAuthorityValidation::raw_hash_mismatch;
    }
    if (decode.decoded_hash != bounded_hash(decode.decoded, decode.decoded.size() * 8U)) {
        return FrozenAuthorityValidation::decoded_hash_mismatch;
    }
    AuthorityShape shape{};
    if (authority_shape(decode.decoded, shape) != ShapeResult::valid
        || shape.entry_count != decode.shape.entry_count
        || shape.word_count != decode.shape.word_count
        || shape.significant_bits != decode.shape.significant_bits
        || shape.significant_bits != bodyBits) {
        return FrozenAuthorityValidation::shape_mismatch;
    }
    if (!unused_low_bits_zero(decode.raw, bodyBits)) {
        return FrozenAuthorityValidation::padding_failure;
    }
    return FrozenAuthorityValidation::valid;
}

FrozenAuthorityValidation validate_frozen_authority_evidence(
    const AuthorityEvidenceRecord& record) noexcept {
    const AuthorityDecodeCapture& decode = record.decode;
    if (record.capture_format_version != kCaptureFormatVersion) {
        return FrozenAuthorityValidation::wrong_format;
    }
    const FrozenAuthorityValidation decodeValidation =
        validate_frozen_authority_decode(decode);
    if (decodeValidation != FrozenAuthorityValidation::valid) {
        return decodeValidation;
    }
    if (!valid_owner_provenance(record.apply_owner, kSceneApplyRva, 0U, 0U)
        || !valid_owner_provenance(record.component_owner,
                                   kComponentStartRva,
                                   0U,
                                   0U)) {
        return FrozenAuthorityValidation::owner_provenance_mismatch;
    }
    const std::uint32_t stateSchema = read_value<std::uint32_t>(record.state_key.data());
    const std::uint64_t stateSource = read_value<std::uint64_t>(record.state_key.data() + 0x08U);
    if (stateSchema != kSceneAuthoritySchema || stateSource != decode.destination_address) {
        return FrozenAuthorityValidation::wrong_state_key;
    }
    if (record.apply_source_address != decode.destination_address
        || record.apply_source != decode.decoded
        || record.apply_source_hash
               != bounded_hash(record.apply_source, record.apply_source.size() * 8U)
        || record.apply_source_hash != decode.decoded_hash) {
        return FrozenAuthorityValidation::source_mismatch;
    }
    if (!valid_scene_component(record.component, decode.context)) {
        return FrozenAuthorityValidation::stale_component;
    }
    if (record.apply_source_copy != CopyOutcome::complete
        || record.apply_source_copy_timing != CopyTiming::before_original) {
        return FrozenAuthorityValidation::source_copy_invalid;
    }
    if (record.component_before_copy != CopyOutcome::complete
        || record.component_after_copy != CopyOutcome::complete
        || record.component_before_copy_timing != CopyTiming::before_original
        || record.component_after_copy_timing
               != CopyTiming::after_original_before_owner_exit) {
        return FrozenAuthorityValidation::component_copy_invalid;
    }
    if (!original_called_exactly_once(record.original_call_audit)) {
        return FrozenAuthorityValidation::original_count_mismatch;
    }
    const std::uint32_t expectedScalar =
        read_value<std::uint32_t>(decode.decoded.data() + 0x4CU);
    const std::uint8_t expectedActive =
        read_value<std::uint32_t>(decode.decoded.data() + 0x08U) != 0U ? 1U : 0U;
    if (record.derived_scalar_after != expectedScalar
        || record.derived_active_after != expectedActive) {
        return FrozenAuthorityValidation::apply_semantic_mismatch;
    }
    return FrozenAuthorityValidation::valid;
}

OracleResult validate_native_oracle(const AuthorityEvidenceRecord& record,
                                    const NativeOracleObservation& observation) noexcept {
    if (validate_frozen_authority_evidence(record) != FrozenAuthorityValidation::valid) {
        return OracleResult::incomplete_record;
    }
    if (observation.publication_or_packet_call_observed) {
        return OracleResult::publication_attempted;
    }
    if (!observation.measurement_succeeded) {
        return OracleResult::measurement_failure;
    }
    if (!observation.measurement_private_storage_only) {
        return OracleResult::measurement_not_private;
    }
    const std::int32_t rounded =
        static_cast<std::int32_t>((record.decode.body_bits + 7U) / 8U);
    if (observation.rounded_bytes != rounded) {
        return OracleResult::rounded_size_mismatch;
    }
    if (!observation.first.writer_status_good || !observation.second.writer_status_good) {
        return OracleResult::writer_failure;
    }
    if (!observation.first.private_storage_only || !observation.second.private_storage_only) {
        return OracleResult::writer_not_private;
    }
    if (observation.first.cursor_bits != record.decode.body_bits
        || observation.second.cursor_bits != record.decode.body_bits) {
        return OracleResult::cursor_mismatch;
    }
    if (!unused_low_bits_zero(observation.first.bytes, record.decode.body_bits)
        || !unused_low_bits_zero(observation.second.bytes, record.decode.body_bits)) {
        return OracleResult::padding_failure;
    }
    if (!significant_bits_equal(observation.first.bytes,
                                observation.second.bytes,
                                record.decode.body_bits)) {
        return OracleResult::nondeterministic;
    }
    if (!significant_bits_equal(observation.first.bytes,
                                record.decode.raw,
                                record.decode.body_bits)) {
        return OracleResult::significant_bit_mismatch;
    }
    return OracleResult::accepted_observation;
}

std::size_t writer_logical_cursor(const WriterBitView& writer) noexcept {
    if (writer.pending_bits > 64U
        || writer.completed_bits > (std::numeric_limits<std::size_t>::max)() - writer.pending_bits) {
        return (std::numeric_limits<std::size_t>::max)();
    }
    return writer.completed_bits + writer.pending_bits;
}

IntervalResult normalize_writer_interval(const WriterBitView& writer,
                                         std::size_t startBit,
                                         std::size_t endBit,
                                         std::span<std::byte> output) noexcept {
    std::fill(output.begin(), output.end(), std::byte{0U});
    if (endBit < startBit) {
        return IntervalResult::decreasing_cursor;
    }
    if (writer.pending_bits > 64U
        || writer.capacity_bytes > (std::numeric_limits<std::size_t>::max)() / 8U
        || writer.completed_bits > writer.capacity_bytes * 8U) {
        return IntervalResult::capacity_overflow;
    }
    if (writer.completed_bits > writer.backing.size() * 8U) {
        return IntervalResult::backing_too_small;
    }
    const std::size_t cursor = writer_logical_cursor(writer);
    if (cursor == (std::numeric_limits<std::size_t>::max)()
        || cursor > writer.capacity_bytes * 8U || endBit > cursor
        || writer.buffer_address == 0U) {
        return IntervalResult::capacity_overflow;
    }
    const std::size_t bitCount = endBit - startBit;
    if (bitCount > output.size() * 8U) {
        return IntervalResult::output_too_small;
    }
    for (std::size_t index = 0U; index < bitCount; ++index) {
        const std::size_t sourceBit = startBit + index;
        bool set{};
        if (sourceBit < writer.completed_bits) {
            const std::uint8_t octet =
                std::to_integer<std::uint8_t>(writer.backing[sourceBit / 8U]);
            set = (octet & static_cast<std::uint8_t>(0x80U >> (sourceBit % 8U))) != 0U;
        } else {
            const std::size_t pendingIndex = sourceBit - writer.completed_bits;
            if (pendingIndex >= writer.pending_bits) {
                return IntervalResult::capacity_overflow;
            }
            set = ((writer.pending_word >> (writer.pending_bits - 1U - pendingIndex)) & 1U) != 0U;
        }
        if (set) {
            output[index / 8U] |= std::byte{static_cast<std::uint8_t>(0x80U >> (index % 8U))};
        }
    }
    return IntervalResult::success;
}

SenseCaptureResult capture_scene_sense(const SenseFanoutRecord& input,
                                       SenseCapture& output) noexcept {
    output = {};
    if (!valid_sense_build(input.build)) {
        return SenseCaptureResult::wrong_build;
    }
    if (!valid_exact_context(input.context)) {
        return SenseCaptureResult::incomplete_context;
    }
    if (!exact_scene_identity(input.sensor.identity)) {
        return SenseCaptureResult::wrong_sensor;
    }
    if (!valid_sense_sensor(input.sensor)) {
        return SenseCaptureResult::stale_sensor;
    }
    if (!sense_owner_pair_valid(input.outer_owner, input.delta_owner)) {
        if (input.outer_owner.producer_thread_id != input.delta_owner.producer_thread_id) {
            return SenseCaptureResult::wrong_thread;
        }
        return SenseCaptureResult::owner_pair_mismatch;
    }
    if (input.actual_delta_schema_argument != kSceneSenseSchema
        || input.current_source_address == 0U || input.prior_source_address == 0U) {
        return SenseCaptureResult::wrong_sensor;
    }
    if (input.writer_at_outer_return.backing_copy == CopyOutcome::fault) {
        return SenseCaptureResult::source_fault;
    }
    if (input.writer_at_outer_return.backing_copy == CopyOutcome::partial) {
        return SenseCaptureResult::source_partial;
    }
    if (input.writer_at_outer_return.error != 0U) {
        return SenseCaptureResult::writer_error;
    }
    if (writer_logical_cursor(input.writer_at_outer_return) != input.body_end) {
        return SenseCaptureResult::cursor_mismatch;
    }
    if (input.delta_end < input.body_start || input.body_end < input.delta_end) {
        return SenseCaptureResult::decreasing_cursor;
    }
    const std::size_t bodyBits = input.body_end - input.body_start;
    const std::size_t deltaBits = input.delta_end - input.body_start;
    if (bodyBits > kSenseCaptureStorageBits) {
        return SenseCaptureResult::storage_exhausted;
    }
    if (bodyBits <= kSenseRevisionBits || input.body_end - input.delta_end != kSenseRevisionBits) {
        return SenseCaptureResult::missing_revision_trailer;
    }
    SenseRaw body{};
    const IntervalResult interval = normalize_writer_interval(input.writer_at_outer_return,
                                                              input.body_start,
                                                              input.body_end,
                                                              body);
    if (interval == IntervalResult::output_too_small) {
        return SenseCaptureResult::storage_exhausted;
    }
    if (interval != IntervalResult::success) {
        return SenseCaptureResult::cursor_mismatch;
    }
    const std::uint32_t nextRevision = read_msb_u32(body, deltaBits);
    if (nextRevision != input.accepted_revision + 1U) {
        return SenseCaptureResult::revision_mismatch;
    }

    output.capture_format_version = kCaptureFormatVersion;
    output.build = input.build;
    output.context = input.context;
    output.sensor = input.sensor;
    output.outer_owner = input.outer_owner;
    output.delta_owner = input.delta_owner;
    output.actual_delta_schema_argument = input.actual_delta_schema_argument;
    output.current_source_address = input.current_source_address;
    output.prior_source_address = input.prior_source_address;
    output.writer_buffer_address = input.writer_at_outer_return.buffer_address;
    output.writer_capacity_bytes = input.writer_at_outer_return.capacity_bytes;
    output.writer_completed_bits = input.writer_at_outer_return.completed_bits;
    output.writer_pending_bits = input.writer_at_outer_return.pending_bits;
    output.writer_error = input.writer_at_outer_return.error;
    output.writer_logical_end = writer_logical_cursor(input.writer_at_outer_return);
    output.body_start = input.body_start;
    output.delta_end = input.delta_end;
    output.body_end = input.body_end;
    output.body = body;
    output.body_bits = bodyBits;
    output.delta_bits = deltaBits;
    output.body_hash = bounded_hash(output.body, bodyBits);
    output.delta_hash = bounded_hash(output.body, deltaBits);
    output.delta_any = (std::to_integer<std::uint8_t>(output.body[0]) & 0x80U) != 0U;
    output.accepted_revision = input.accepted_revision;
    output.next_revision = nextRevision;
    output.writer_copy = input.writer_at_outer_return.backing_copy;
    return SenseCaptureResult::complete;
}

SenseCaptureResult capture_live_scene_sense(const LiveSenseFanoutRecord& input,
                                            SenseCapture& output) noexcept {
    output = {};
    const WriterBitView& writer = input.metadata.writer_at_outer_return;
    if (input.metadata.body_end < input.metadata.body_start
        || input.live_writer_buffer == nullptr
        || writer.buffer_address
               != reinterpret_cast<std::uintptr_t>(input.live_writer_buffer)) {
        return SenseCaptureResult::source_fault;
    }
    const std::size_t firstByte = (std::min)(input.metadata.body_start / 8U,
                                             writer.completed_bits / 8U);
    if (writer.capacity_bytes < firstByte || input.live_writer_readable_bytes < firstByte) {
        return SenseCaptureResult::source_partial;
    }
    const std::size_t baseBits = firstByte * 8U;
    const std::size_t completedAfterBase =
        writer.completed_bits > baseBits ? writer.completed_bits - baseBits : 0U;
    const std::size_t copyBytes = (completedAfterBase + 7U) / 8U;
    if (copyBytes > kSenseCaptureStorageBytes + 1U
        || copyBytes > input.live_writer_readable_bytes - firstByte) {
        return SenseCaptureResult::source_partial;
    }
    std::array<std::byte, kSenseCaptureStorageBytes + 1U> backing{};
    if (copyBytes != 0U
        && !seh_copy_exact_bytes(backing.data(),
                                 input.live_writer_buffer + firstByte,
                                 copyBytes)) {
        return SenseCaptureResult::source_fault;
    }
    SenseFanoutRecord owned = input.metadata;
    owned.body_start -= baseBits;
    owned.delta_end -= baseBits;
    owned.body_end -= baseBits;
    owned.writer_at_outer_return.backing = {backing.data(), copyBytes};
    owned.writer_at_outer_return.buffer_address =
        reinterpret_cast<std::uintptr_t>(input.live_writer_buffer + firstByte);
    owned.writer_at_outer_return.capacity_bytes = writer.capacity_bytes - firstByte;
    owned.writer_at_outer_return.completed_bits = completedAfterBase;
    owned.writer_at_outer_return.backing_copy = CopyOutcome::complete;
    const SenseCaptureResult result = capture_scene_sense(owned, output);
    if (result == SenseCaptureResult::complete) {
        output.writer_buffer_address = writer.buffer_address;
        output.writer_capacity_bytes = writer.capacity_bytes;
        output.writer_completed_bits = writer.completed_bits;
        output.writer_pending_bits = writer.pending_bits;
        output.writer_error = writer.error;
        output.writer_logical_end = writer_logical_cursor(writer);
        output.body_start = input.metadata.body_start;
        output.delta_end = input.metadata.delta_end;
        output.body_end = input.metadata.body_end;
    }
    return result;
}

bool matches_frozen_140_fixture(const SenseCapture& capture) noexcept {
    return capture.body_bits == kFrozenSenseFixtureBits && capture.delta_bits == 108U
           && capture.next_revision == 10U
           && significant_bits_equal(capture.body,
                                     kFrozenSenseFixture140,
                                     kFrozenSenseFixtureBits);
}

bool split_scene_sense_service_observation(const SceneIdentity& identity,
                                           std::span<const std::byte> normalizedBody,
                                           std::size_t bodyBits,
                                           SceneSenseServiceSplit& output) noexcept {
    output = {};
    if (!exact_scene_identity(identity) || bodyBits <= kSenseRevisionBits
        || bodyBits > normalizedBody.size() * 8U) {
        return false;
    }
    const bool retained = bodyBits == 75U || bodyBits == 108U || bodyBits == 140U;
    if (!retained) {
        return false;
    }
    output.body_bits = bodyBits;
    output.delta_bits = bodyBits - kSenseRevisionBits;
    output.next_revision = read_msb_u32(normalizedBody, output.delta_bits);
    output.retained_fixture_width = true;
    output.exact_140_fixture =
        bodyBits == kFrozenSenseFixtureBits
        && significant_bits_equal(normalizedBody,
                                  kFrozenSenseFixture140,
                                  kFrozenSenseFixtureBits);
    return true;
}

LegacyPrefixObservation observe_legacy_64_of_140(std::span<const std::byte> normalizedBody,
                                                std::size_t bodyBits) noexcept {
    LegacyPrefixObservation output{};
    output.observed_bits = bodyBits;
    output.width_is_140 = bodyBits == kFrozenSenseFixtureBits;
    if (bodyBits >= 64U && normalizedBody.size() >= 8U) {
        output.observed_first64 = read_msb_u64(normalizedBody);
        output.first64_matches = output.observed_first64 == kLegacyFirst64;
    }
    output.full_fixture_matches =
        output.width_is_140
        && significant_bits_equal(normalizedBody,
                                  kFrozenSenseFixture140,
                                  kFrozenSenseFixtureBits);
    output.known_stateful_consumer_mask = kLegacyStatefulConsumerMask;
    output.authorizes_authority = false;
    output.authorizes_completion = false;
    output.mutates_state = false;
    return output;
}

bool FullEvidenceRing::lock_once() noexcept {
    return !lock_.test_and_set(std::memory_order_acquire);
}

void FullEvidenceRing::unlock() noexcept {
    lock_.clear(std::memory_order_release);
}

FullEvidenceCommitResult FullEvidenceRing::commit(FullEvidenceRecord record) noexcept {
    if (!lock_once()) {
        dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
        return FullEvidenceCommitResult::busy;
    }
    FullEvidenceCommitResult result = FullEvidenceCommitResult::committed;
    if (size_ == kFullEvidenceRingCapacity) {
        dropped_full_.fetch_add(1U, std::memory_order_relaxed);
        result = FullEvidenceCommitResult::full;
    } else if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
        dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
        result = FullEvidenceCommitResult::sequence_exhausted;
    } else {
        const std::size_t slot = (head_ + size_) % kFullEvidenceRingCapacity;
        record.capture_format_version = kCaptureFormatVersion;
        record.committed_sequence = next_sequence_;
        records_[slot] = record;
        ++next_sequence_;
        ++size_;
        committed_.fetch_add(1U, std::memory_order_relaxed);
    }
    unlock();
    return result;
}

FullEvidenceCommitResult FullEvidenceRing::try_commit(
    const AuthorityDecodeCapture& authorityDecode) noexcept {
    if (validate_frozen_authority_decode(authorityDecode)
        != FrozenAuthorityValidation::valid) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return FullEvidenceCommitResult::rejected;
    }
    FullEvidenceRecord record{};
    record.kind = FullEvidenceKind::authority_decode;
    record.authority_decode = authorityDecode;
    return commit(record);
}

FullEvidenceCommitResult FullEvidenceRing::try_commit(
    const AuthorityEvidenceRecord& authority) noexcept {
    if (validate_frozen_authority_evidence(authority) != FrozenAuthorityValidation::valid) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return FullEvidenceCommitResult::rejected;
    }
    FullEvidenceRecord record{};
    record.kind = FullEvidenceKind::authority;
    record.authority = authority;
    return commit(record);
}

FullEvidenceCommitResult FullEvidenceRing::try_commit(const SenseCapture& sense) noexcept {
    if (!frozen_sense_valid(sense)) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return FullEvidenceCommitResult::rejected;
    }
    FullEvidenceRecord record{};
    record.kind = FullEvidenceKind::sense;
    record.sense = sense;
    return commit(record);
}

FullEvidenceReadResult FullEvidenceRing::try_pop(FullEvidenceRecord& output) noexcept {
    if (!lock_once()) {
        return FullEvidenceReadResult::busy;
    }
    if (size_ == 0U) {
        unlock();
        return FullEvidenceReadResult::empty;
    }
    output = records_[head_];
    records_[head_] = {};
    head_ = (head_ + 1U) % kFullEvidenceRingCapacity;
    --size_;
    unlock();
    return FullEvidenceReadResult::success;
}

void FullEvidenceRing::account_source_outcome(CopyOutcome outcome) noexcept {
    if (outcome == CopyOutcome::partial) {
        source_partial_.fetch_add(1U, std::memory_order_relaxed);
    } else if (outcome == CopyOutcome::fault) {
        source_fault_.fetch_add(1U, std::memory_order_relaxed);
    }
}

FullEvidenceCounters FullEvidenceRing::counters() const noexcept {
    return {committed_.load(std::memory_order_relaxed),
            rejected_.load(std::memory_order_relaxed),
            dropped_full_.load(std::memory_order_relaxed),
            dropped_busy_.load(std::memory_order_relaxed),
            dropped_sequence_exhausted_.load(std::memory_order_relaxed),
            source_partial_.load(std::memory_order_relaxed),
            source_fault_.load(std::memory_order_relaxed)};
}

bool FullEvidenceRing::testing_lock() noexcept {
    return lock_once();
}

void FullEvidenceRing::testing_unlock() noexcept {
    unlock();
}

void FullEvidenceRing::testing_set_next_sequence(std::uint64_t sequence) noexcept {
    if (!lock_once()) {
        return;
    }
    next_sequence_ = sequence;
    unlock();
}

TlsStackResult OwnerTlsStack::push(const OwnerCallProvenance& frame) noexcept {
    if (depth_ == kOwnerTlsMaximumDepth) {
        return TlsStackResult::overflow;
    }
    if (frame.producer_thread_id == 0U
        || (thread_id_ != 0U && frame.producer_thread_id != thread_id_)) {
        return TlsStackResult::wrong_thread;
    }
    const std::uint64_t expectedParent = depth_ == 0U ? 0U : frames_[depth_ - 1U].call_id;
    if (frame.parent_call_id != expectedParent || frame.tls_depth != depth_) {
        return TlsStackResult::wrong_parent;
    }
    if (thread_id_ == 0U) {
        thread_id_ = frame.producer_thread_id;
    }
    frames_[depth_] = frame;
    ++depth_;
    return TlsStackResult::success;
}

TlsStackResult OwnerTlsStack::pop(std::uint64_t callId,
                                  std::uint32_t producerThreadId) noexcept {
    if (depth_ == 0U) {
        return TlsStackResult::wrong_frame;
    }
    if (producerThreadId != thread_id_) {
        return TlsStackResult::wrong_thread;
    }
    if (frames_[depth_ - 1U].call_id != callId) {
        return TlsStackResult::wrong_frame;
    }
    --depth_;
    frames_[depth_] = {};
    if (depth_ == 0U) {
        thread_id_ = 0U;
    }
    return TlsStackResult::success;
}

const OwnerCallProvenance* OwnerTlsStack::top(std::uint32_t producerThreadId) const noexcept {
    if (depth_ == 0U || producerThreadId != thread_id_) {
        return nullptr;
    }
    return &frames_[depth_ - 1U];
}

bool SoleOwnerClaim::try_claim(std::uint64_t ownerId) noexcept {
    if (ownerId == 0U) {
        return false;
    }
    std::uint64_t expected{};
    return owner_id_.compare_exchange_strong(expected,
                                             ownerId,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
}

bool SoleOwnerClaim::release(std::uint64_t ownerId) noexcept {
    if (ownerId == 0U) {
        return false;
    }
    return owner_id_.compare_exchange_strong(ownerId,
                                             0U,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
}

std::uint64_t SoleOwnerClaim::owner_id() const noexcept {
    return owner_id_.load(std::memory_order_acquire);
}

bool FanoutOwnerParticipant::configure_attached(std::uintptr_t hookHandle,
                                                std::uintptr_t trampoline,
                                                std::uint64_t ownerGeneration,
                                                std::uint64_t evidenceGeneration) noexcept {
    if (hookHandle == 0U || trampoline == 0U || ownerGeneration == 0U
        || evidenceGeneration == 0U || attached_.load(std::memory_order_acquire)
        || ownerGeneration <= last_owner_generation_.load(std::memory_order_acquire)
        || evidenceGeneration <= last_evidence_generation_.load(std::memory_order_acquire)) {
        return false;
    }
    hook_handle_.store(hookHandle, std::memory_order_relaxed);
    trampoline_.store(trampoline, std::memory_order_relaxed);
    owner_generation_.store(ownerGeneration, std::memory_order_relaxed);
    evidence_generation_.store(evidenceGeneration, std::memory_order_relaxed);
    last_owner_generation_.store(ownerGeneration, std::memory_order_release);
    last_evidence_generation_.store(evidenceGeneration, std::memory_order_release);
    attached_.store(true, std::memory_order_release);
    admission_open_.store(true, std::memory_order_release);
    return true;
}

bool FanoutOwnerParticipant::try_enter() noexcept {
    if (!admission_open_.load(std::memory_order_acquire)
        || !attached_.load(std::memory_order_acquire)) {
        return false;
    }
    in_flight_.fetch_add(1U, std::memory_order_acq_rel);
    if (!admission_open_.load(std::memory_order_acquire)
        || !attached_.load(std::memory_order_acquire)) {
        in_flight_.fetch_sub(1U, std::memory_order_acq_rel);
        return false;
    }
    return true;
}

void FanoutOwnerParticipant::leave() noexcept {
    const std::uint32_t before = in_flight_.load(std::memory_order_acquire);
    if (before != 0U) {
        in_flight_.fetch_sub(1U, std::memory_order_acq_rel);
    }
}

void FanoutOwnerParticipant::begin_quiesce() noexcept {
    admission_open_.store(false, std::memory_order_release);
}

OwnerDetachResult FanoutOwnerParticipant::try_finish_detach(
    bool nativeDetachSucceeded) noexcept {
    if (!attached_.load(std::memory_order_acquire)) {
        return OwnerDetachResult::not_attached;
    }
    admission_open_.store(false, std::memory_order_release);
    if (in_flight_.load(std::memory_order_acquire) != 0U) {
        return OwnerDetachResult::deferred_inflight;
    }
    if (!nativeDetachSucceeded) {
        return OwnerDetachResult::failed_retained;
    }
    attached_.store(false, std::memory_order_release);
    hook_handle_.store(0U, std::memory_order_release);
    trampoline_.store(0U, std::memory_order_release);
    owner_generation_.store(0U, std::memory_order_release);
    evidence_generation_.store(0U, std::memory_order_release);
    return OwnerDetachResult::detached;
}

OwnerRetainedState FanoutOwnerParticipant::retained_state() const noexcept {
    return {hook_handle_.load(std::memory_order_acquire),
            trampoline_.load(std::memory_order_acquire),
            owner_generation_.load(std::memory_order_acquire),
            evidence_generation_.load(std::memory_order_acquire),
            in_flight_.load(std::memory_order_acquire),
            admission_open_.load(std::memory_order_acquire),
            attached_.load(std::memory_order_acquire)};
}

void invoke_owner_original_once(FanoutOwnerParticipant& participant,
                                void* context,
                                OwnerCallback original,
                                OwnerCallback beforeObservation,
                                OwnerCallback afterObservation,
                                OriginalCallAudit& audit) noexcept {
    audit = {};
    audit.owner_entries = 1U;
    const bool admitted = participant.try_enter();
    if (admitted && beforeObservation != nullptr) {
        beforeObservation(context);
    }
    if (original != nullptr) {
        original(context);
        audit.original_calls = 1U;
    }
    if (admitted && afterObservation != nullptr) {
        afterObservation(context);
    }
    if (admitted) {
        participant.leave();
    }
    audit.owner_exits = 1U;
}

namespace {

[[nodiscard]] std::uint64_t address_fingerprint(std::uintptr_t address,
                                                std::uint64_t domain) noexcept {
    std::array<std::byte, sizeof address + sizeof domain> bytes{};
    std::memcpy(bytes.data(), &address, sizeof address);
    std::memcpy(bytes.data() + sizeof address, &domain, sizeof domain);
    return bounded_hash(bytes, bytes.size() * 8U);
}

} // namespace

ScalarHashObservation telemetry(const AuthorityDecodeCapture& capture) noexcept {
    ScalarHashObservation output{};
    output.capture_format_version = capture.capture_format_version;
    output.build_fingerprint = capture.build.unpacked_pc_sha256;
    output.context = capture.context;
    output.owner_generation = capture.decode_owner.generation_at_entry;
    output.owner_generation_at_exit = capture.decode_owner.generation_at_exit;
    output.sensor_record_generation = capture.sensor.record_generation;
    output.sensor_record_address_hash =
        address_fingerprint(capture.sensor.record_address, 0x53454E534F52ULL);
    output.body_hash = capture.raw_hash;
    output.decoded_hash = capture.decoded_hash;
    output.body_bits = capture.body_bits;
    output.actual_schema_argument = capture.actual_schema_argument;
    output.producer_thread_id = capture.decode_owner.producer_thread_id;
    output.owner_entry_rva = capture.decode_owner.owner_entry_rva;
    output.native_caller_rva = capture.decode_owner.native_caller_rva;
    output.sensor_registry = capture.sensor.identity.registry;
    output.sensor_type = capture.sensor.identity.type;
    output.sensor_index = capture.sensor.identity.index;
    output.kind = ObservationKind::authority_decode;
    output.status = ObservationStatus::complete;
    return output;
}

ScalarHashObservation telemetry(const AuthorityEvidenceRecord& record) noexcept {
    ScalarHashObservation output = telemetry(record.decode);
    output.apply_owner_generation = record.apply_owner.generation_at_entry;
    output.component_generation = record.component.component_generation;
    output.component_address_hash =
        address_fingerprint(record.component.component_address, 0x434F4D504F4E454EULL);
    output.component_definition = record.component.definition;
    output.kind = ObservationKind::authority_apply;
    return output;
}

ScalarHashObservation telemetry(const SenseCapture& capture) noexcept {
    ScalarHashObservation output{};
    output.capture_format_version = capture.capture_format_version;
    output.build_fingerprint = capture.build.unpacked_pc_sha256;
    output.context = capture.context;
    output.owner_generation = capture.delta_owner.generation_at_entry;
    output.owner_generation_at_exit = capture.delta_owner.generation_at_exit;
    output.sensor_record_generation = capture.sensor.record_generation;
    output.sensor_record_address_hash =
        address_fingerprint(capture.sensor.record_address, 0x53454E534F52ULL);
    output.body_hash = capture.body_hash;
    output.decoded_hash = capture.delta_hash;
    output.body_bits = capture.body_bits;
    output.delta_bits = capture.delta_bits;
    output.actual_schema_argument = capture.actual_delta_schema_argument;
    output.next_revision = capture.next_revision;
    output.producer_thread_id = capture.delta_owner.producer_thread_id;
    output.owner_entry_rva = capture.delta_owner.owner_entry_rva;
    output.native_caller_rva = capture.delta_owner.native_caller_rva;
    output.sensor_registry = capture.sensor.identity.registry;
    output.sensor_type = capture.sensor.identity.type;
    output.sensor_index = capture.sensor.identity.index;
    output.kind = ObservationKind::sense;
    output.status = ObservationStatus::complete;
    return output;
}

bool telemetry(const FullEvidenceRecord& record, ScalarHashObservation& output) noexcept {
    output = {};
    if (record.capture_format_version != kCaptureFormatVersion
        || record.committed_sequence == 0U) {
        return false;
    }
    if (record.kind == FullEvidenceKind::authority) {
        if (validate_frozen_authority_evidence(record.authority)
            != FrozenAuthorityValidation::valid) {
            return false;
        }
        output = telemetry(record.authority);
    } else if (record.kind == FullEvidenceKind::authority_decode) {
        if (validate_frozen_authority_decode(record.authority_decode)
            != FrozenAuthorityValidation::valid) {
            return false;
        }
        output = telemetry(record.authority_decode);
    } else if (record.kind == FullEvidenceKind::sense) {
        if (!frozen_sense_valid(record.sense)) {
            return false;
        }
        output = telemetry(record.sense);
    } else {
        return false;
    }
    output.committed_record_sequence = record.committed_sequence;
    return true;
}

bool ObservationQueue::lock_once() noexcept {
    return !lock_.test_and_set(std::memory_order_acquire);
}

void ObservationQueue::unlock() noexcept {
    lock_.clear(std::memory_order_release);
}

QueuePushResult ObservationQueue::try_push(const ScalarHashObservation& observation) noexcept {
    if (!valid_queue_observation(observation)) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::rejected;
    }
    if (!lock_once()) {
        dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::busy;
    }
    QueuePushResult result = QueuePushResult::enqueued;
    bool duplicate{};
    for (std::size_t index = 0U; index < size_; ++index) {
        const std::size_t slot = (head_ + index) % kObservationQueueCapacity;
        if (same_without_sequence(records_[slot], observation)) {
            duplicate = true;
            break;
        }
    }
    if (duplicate) {
        duplicates_.fetch_add(1U, std::memory_order_relaxed);
        result = QueuePushResult::duplicate;
    } else if (size_ == kObservationQueueCapacity) {
        dropped_full_.fetch_add(1U, std::memory_order_relaxed);
        result = QueuePushResult::full;
    } else if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
        dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
        result = QueuePushResult::sequence_exhausted;
    } else {
        const std::size_t slot = (head_ + size_) % kObservationQueueCapacity;
        records_[slot] = observation;
        records_[slot].sequence = next_sequence_;
        ++next_sequence_;
        ++size_;
        accepted_.fetch_add(1U, std::memory_order_relaxed);
    }
    unlock();
    return result;
}

QueuePopResult ObservationQueue::try_pop(ScalarHashObservation& output) noexcept {
    if (!lock_once()) {
        return QueuePopResult::busy;
    }
    if (size_ == 0U) {
        unlock();
        return QueuePopResult::empty;
    }
    output = records_[head_];
    records_[head_] = {};
    head_ = (head_ + 1U) % kObservationQueueCapacity;
    --size_;
    unlock();
    return QueuePopResult::success;
}

QueueCounters ObservationQueue::counters() const noexcept {
    return {accepted_.load(std::memory_order_relaxed),
            duplicates_.load(std::memory_order_relaxed),
            rejected_.load(std::memory_order_relaxed),
            dropped_full_.load(std::memory_order_relaxed),
            dropped_busy_.load(std::memory_order_relaxed),
            dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
}

bool ObservationQueue::testing_lock() noexcept {
    return lock_once();
}

void ObservationQueue::testing_unlock() noexcept {
    unlock();
}

void ObservationQueue::testing_set_next_sequence(std::uint64_t sequence) noexcept {
    if (!lock_once()) {
        return;
    }
    next_sequence_ = sequence;
    unlock();
}

} // namespace dawn::client::hooks::bootflow::opening_authority::scene_capture
