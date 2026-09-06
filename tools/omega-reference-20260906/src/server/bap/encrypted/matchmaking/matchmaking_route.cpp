#include "matchmaking_route.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <string_view>

#include "../../../../core/filesystem/path.h"
#include "../../../../core/logging/log.h"
#include "../../../../middleware/bap/matchmaking/request/matchmaking_request_parser.h"
#include "../../../../middleware/bap/matchmaking/response/matchmaking_response_encoder.h"
#include "../../../../middleware/protobuf/codec.h"
#include "../../../../state/activity/forced/activity_forced_destination.h"

namespace sunrise::server::bap::encrypted::matchmaking {
namespace {

namespace service = middleware::bap::matchmaking;

/** Middleware parsing and State storage must accept the same runtime descriptor size. */
static_assert(service::kJoinDescriptorSize == state::matchmaking::kDescriptorSize);

/** Bounds private local descriptor captures even if the client repeatedly republishes a lobby. */
std::atomic_uint32_t g_descriptorCaptureCount{};

/** Returns true only while the operator is testing the Homecoming override. */
[[nodiscard]] bool homecoming_is_forced() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    return state::activity::forced::override_active()
           && std::string_view(forced.packageName.data(), forced.packageNameLength)
                  == "mission_towerfall";
}

/** Computes a non-secret identifier for correlating a request capture with its response copy. */
[[nodiscard]] std::uint64_t descriptor_fingerprint(std::span<const std::byte> descriptor) noexcept {
    std::uint64_t value = 1469598103934665603ULL;
    for (const std::byte item : descriptor) {
        value ^= static_cast<std::uint8_t>(item);
        value *= 1099511628211ULL;
    }
    return value;
}

/**
 * Saves one opaque join descriptor to the module-local private analysis directory. Raw bytes are
 * deliberately excluded from the shared log because they may contain session-bearing material.
 */
void capture_join_descriptor(const wchar_t* phase,
                             std::span<const std::byte> descriptor,
                             std::uint64_t variant,
                             std::uint64_t advertisementId) noexcept {
    if (phase == nullptr || descriptor.size() != service::kJoinDescriptorSize
        || !homecoming_is_forced()) {
        return;
    }
    const std::uint32_t capture =
        g_descriptorCaptureCount.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (capture > 32U) {
        return;
    }
    const HMODULE sunrise = GetModuleHandleW(L"steam_api64.dll");
    core::path::Buffer path{};
    if (sunrise == nullptr || !core::path::artifact_directory(sunrise, path)
        || !core::path::append(path, L"\\analysis")) {
        return;
    }
    if (CreateDirectoryW(path.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }
    const std::uint64_t fingerprint = descriptor_fingerprint(descriptor);
    std::array<wchar_t, 192> filename{};
    const int nameLength = std::swprintf(filename.data(),
                                         filename.size(),
                                         L"\\matchmaking_join_descriptor.%ls.%03u.variant_%016llX.ad_%016llX.hash_%016llX.bin",
                                         phase,
                                         capture,
                                         static_cast<unsigned long long>(variant),
                                         static_cast<unsigned long long>(advertisementId),
                                         static_cast<unsigned long long>(fingerprint));
    if (nameLength <= 0 || !core::path::append(path, filename.data())) {
        return;
    }
    const HANDLE file = CreateFileW(path.chars.data(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const bool complete =
        WriteFile(file,
                  descriptor.data(),
                  static_cast<DWORD>(descriptor.size()),
                  &written,
                  nullptr)
            != FALSE
        && written == descriptor.size() && FlushFileBuffers(file) != FALSE;
    (void)CloseHandle(file);

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=matchmaking stage=join_descriptor_capture phase=%ls n=%u bytes=%lu variant=0x%llX advertisement=0x%llX fingerprint=0x%llX result=%s",
        phase,
        capture,
        static_cast<unsigned long>(written),
        static_cast<unsigned long long>(variant),
        static_cast<unsigned long long>(advertisementId),
        static_cast<unsigned long long>(fingerprint),
        complete ? "ok" : "write");
    if (length > 0) {
        core::log::write(core::log::Channel::server,
                         complete ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Appends the scalar values and byte lengths from one known matchmaking protobuf scope. */
void append_scope(std::array<char, core::log::kLineCapacity>& line,
                  std::size_t& used,
                  const char* scope,
                  std::span<const std::byte> input) noexcept {
    middleware::protobuf::Reader reader(input);
    middleware::protobuf::Field field;
    while (reader.remaining() != 0 && reader.next(field) && used < line.size()) {
        const unsigned wire = static_cast<unsigned>(field.wireType);
        int appended = 0;
        if (field.wireType == middleware::protobuf::WireType::lengthDelimited) {
            appended = std::snprintf(line.data() + used,
                                     line.size() - used,
                                     " %sf%u/w%u/len%zu",
                                     scope,
                                     field.fieldNumber,
                                     wire,
                                     field.bytes.size());
        } else {
            appended = std::snprintf(line.data() + used,
                                     line.size() - used,
                                     " %sf%u/w%u/v0x%llX",
                                     scope,
                                     field.fieldNumber,
                                     wire,
                                     static_cast<unsigned long long>(field.value));
        }
        if (appended <= 0 || static_cast<std::size_t>(appended) >= line.size() - used) {
            used = line.size();
            return;
        }
        used += static_cast<std::size_t>(appended);
    }
}

/**
 * Reports only protobuf field metadata and scalar values. The opaque 128-byte join descriptor is
 * never serialized into the log.
 */
void report_request_shape(std::span<const std::byte> input) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int prefix = std::snprintf(
        line.data(), line.size(), "ev=matchmaking stage=request_shape bytes=%zu", input.size());
    if (prefix <= 0 || static_cast<std::size_t>(prefix) >= line.size()) {
        return;
    }
    std::size_t used = static_cast<std::size_t>(prefix);
    append_scope(line, used, "r", input);

    middleware::protobuf::Reader root(input);
    middleware::protobuf::Field rootField;
    while (root.remaining() != 0 && root.next(rootField)) {
        if (rootField.fieldNumber != 8
            || rootField.wireType != middleware::protobuf::WireType::lengthDelimited) {
            continue;
        }
        append_scope(line, used, "a", rootField.bytes);
        middleware::protobuf::Reader advertisement(rootField.bytes);
        middleware::protobuf::Field advertisementField;
        while (advertisement.remaining() != 0 && advertisement.next(advertisementField)) {
            if (advertisementField.fieldNumber != 2
                || advertisementField.wireType
                       != middleware::protobuf::WireType::lengthDelimited) {
                continue;
            }
            append_scope(line, used, "d", advertisementField.bytes);
            break;
        }
        break;
    }
    if (used < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::info,
                         {line.data(), used});
    }
}

/** Reports the parsed request nonce beside the response body size without exposing body bytes. */
void report_response_shape(const service::Request& request,
                           const service::Response& response,
                           bool encoded,
                           std::size_t written) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=matchmaking stage=response_shape kind=%u has_request_nonce=%u request_nonce=0x%llX advertisement=0x%llX descriptor_bytes=%zu response_bytes=%zu result=%s",
        static_cast<unsigned>(request.kind),
        request.hasRequestNonce ? 1U : 0U,
        static_cast<unsigned long long>(request.requestNonce),
        static_cast<unsigned long long>(response.advertisementId),
        response.descriptor.size(),
        written,
        encoded ? "ok" : "encode");
    if (length > 0) {
        core::log::write(core::log::Channel::server,
                         encoded ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/**
 * Finds the State-backed fields for the request kinds that are not static.
 * @param context Active logical matchmaking context.
 * @param request Fully validated request fields with a borrowed descriptor.
 * @param response Receives the response shape and any id State picked.
 * @param mutation Receives a prepared update without committing persistent State.
 * @return True when the request is static or all required State fields are available.
 */
[[nodiscard]] bool prepare_fields(state::matchmaking::ContextHandle context,
                                  const service::Request& request,
                                  service::Response& response,
                                  state::matchmaking::PendingMutation& mutation) noexcept {
    response.kind = request.kind;
    switch (request.kind) {
    case service::RequestKind::advertisementUpdate:
        return state::matchmaking::prepare_variant_update(context,
                                                          request.advertisement.existingId,
                                                          request.advertisement.variantKey,
                                                          request.advertisement.hasDescriptor,
                                                          request.advertisement.descriptor,
                                                          response.advertisementId,
                                                          mutation);
    case service::RequestKind::rejoinAdvertisementUpdate:
        return state::matchmaking::prepare_initial_latest(
            context, response.advertisementId, mutation);
    case service::RequestKind::none:
    case service::RequestKind::sessionSearch:
    case service::RequestKind::advertisementDelete:
    case service::RequestKind::configuration:
    case service::RequestKind::rejoinAdvertisementDelete:
    case service::RequestKind::locateSession:
    case service::RequestKind::liveStats:
        return true;
    }
    return false;
}

} // namespace

/** Prepares and encodes one kind-specific svc-43 response transaction. */
bool encode_response(state::matchmaking::ContextHandle context,
                     std::span<const std::byte> requestBody,
                     std::span<std::byte> output,
                     std::size_t& written,
                     state::matchmaking::PendingMutation& mutation,
                     bool& hasMutation) noexcept {
    written = 0;
    hasMutation = false;
    SecureZeroMemory(&mutation, sizeof mutation);
    report_request_shape(requestBody);
    const service::Request request = service::request::parse(requestBody);
    if (request.kind == service::RequestKind::advertisementUpdate
        && request.advertisement.hasDescriptor) {
        capture_join_descriptor(L"update",
                                request.advertisement.descriptor,
                                request.advertisement.variantKey,
                                request.advertisement.existingId);
    }
    service::Response response{};
    if (!prepare_fields(context, request, response, mutation)) {
        // A correlated empty fallback clears the pending client task without publishing State.
        SecureZeroMemory(&mutation, sizeof mutation);
        response = {};
    }

    state::matchmaking::LatestSnapshot latest{};
    if (response.kind == service::RequestKind::locateSession
        && state::matchmaking::latest_snapshot(context, latest)) {
        response.advertisementId = latest.advertisementId;
        if (latest.hasDescriptor) {
            response.descriptor = std::span(latest.descriptor);
            capture_join_descriptor(
                L"locate", response.descriptor, 0, response.advertisementId);
        }
    }
    const bool encoded = service::response::encode(response, output, written);
    report_response_shape(request, response, encoded, written);
    state::matchmaking::erase_snapshot(latest);
    if (!encoded) {
        written = 0;
        SecureZeroMemory(&mutation, sizeof mutation);
        return false;
    }
    hasMutation = mutation.kind != state::matchmaking::MutationKind::none;
    return true;
}

} // namespace sunrise::server::bap::encrypted::matchmaking
