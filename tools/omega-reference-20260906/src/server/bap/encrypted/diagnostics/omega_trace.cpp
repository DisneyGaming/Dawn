#include "omega_trace.h"

#include <Windows.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "../../../../core/filesystem/path.h"
#include "../../../../core/logging/log.h"
#include "../../../../core/settings/settings.h"
#include "../../../../state/build_data/scenarios/omega_schema_catalog.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace sunrise::server::bap::encrypted::diagnostics::omega_trace {
namespace {

namespace path = core::path;
namespace schemas = state::build_data::scenarios;

constexpr std::wstring_view kExportDirectorySuffix = L"\\exports";
constexpr std::wstring_view kTraceFileSuffix = L"\\omega_trace.jsonl";
constexpr std::uint32_t kOmegaRuntimeKey = 0x4786C0E0U;
constexpr std::uint8_t kActivityScriptType = 18;
constexpr std::uint8_t kMissionDirectorType = 35;
constexpr std::uint8_t kParticipationType = 13;
constexpr std::uint32_t kOmegaOpeningKey = 0xD00142CFU;
constexpr std::uint8_t kOmegaSceneType = 43;
constexpr std::uint16_t kOmegaSceneIndex = 1;
constexpr std::uint8_t kOmegaPortalVisualType = 4;
constexpr std::uint16_t kOmegaPortalVisualFirstIndex = 2;
constexpr std::uint16_t kOmegaPortalVisualLastIndex = 4;

SRWLOCK g_lock = SRWLOCK_INIT;
bool g_started{};
std::uint64_t g_runId{};
std::uint64_t g_eventSequence{};
std::uint64_t g_publicationSequence{};

void append_format(std::string& output, const char* format, ...) {
    std::array<char, 2048> buffer{};
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    if (written <= 0) {
        return;
    }
    const std::size_t count =
        static_cast<std::size_t>(written) < buffer.size() ? static_cast<std::size_t>(written)
                                                         : buffer.size() - 1;
    output.append(buffer.data(), count);
}

[[nodiscard]] std::uint64_t hash_payload(std::span<const std::byte> payload) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::byte byte : payload) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void append_hex(std::string& output, std::span<const std::byte> payload) {
    constexpr char kHex[] = "0123456789ABCDEF";
    output.reserve(output.size() + payload.size() * 2);
    for (const std::byte byte : payload) {
        const unsigned value = std::to_integer<unsigned char>(byte);
        output.push_back(kHex[value >> 4U]);
        output.push_back(kHex[value & 0xFU]);
    }
}

[[nodiscard]] const char* recovered_symbol(std::uint32_t key,
                                           std::uint16_t index) noexcept {
    if (key == 0x4786C0E0U && index == 1) {
        return "omega.mission_director";
    }
    if (key == 0x4786C0E0U && index == 2) {
        return "omega.activity_script_manager";
    }
    if (key == 0x82FB58B7U && index == 0) {
        return "omega.directive_objective_sensor";
    }
    if (key == 0x82FB58B7U && index == 1) {
        return "omega.dialogue_sensor";
    }
    if (key == 0x82FB58B7U && index == 2) {
        return "omega.music_sensor";
    }
    if (key == 0xD00142CFU && index == 0) {
        return "omega.ikora_spawner";
    }
    if (key == 0xD00142CFU && index == 1) {
        return "omega.opening_scene";
    }
    if (key == 0xD00142CFU && index == 20) {
        return "omega.opening_trigger_volume";
    }
    if (key == 0xD00142CFU && index == 24) {
        return "omega.forest_entrance_monitor";
    }
    return nullptr;
}

[[nodiscard]] std::string symbol_for(std::uint32_t key,
                                     std::uint8_t type,
                                     std::uint16_t index) {
    if (const char* recovered = recovered_symbol(key, index); recovered != nullptr) {
        return recovered;
    }
    std::array<char, 96> value{};
    const int written = std::snprintf(value.data(),
                                      value.size(),
                                      "omega.component.%08X.type_%u.slot_%u",
                                      key,
                                      static_cast<unsigned>(type),
                                      static_cast<unsigned>(index));
    return written > 0 ? std::string(value.data(), static_cast<std::size_t>(written))
                       : std::string("omega.component.unknown");
}

void append_time_prefix(std::string& output, const char* event) {
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    if (!g_started) {
        g_runId = (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32U) ^ GetTickCount64();
        g_started = true;
    }
    ++g_eventSequence;
    append_format(output,
                  "{\"event\":\"%s\",\"event_sequence\":%llu,\"run_id\":\"0x%016llX\","
                  "\"time_utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\",\"tick_ms\":%llu",
                  event,
                  static_cast<unsigned long long>(g_eventSequence),
                  static_cast<unsigned long long>(g_runId),
                  static_cast<unsigned>(utc.wYear),
                  static_cast<unsigned>(utc.wMonth),
                  static_cast<unsigned>(utc.wDay),
                  static_cast<unsigned>(utc.wHour),
                  static_cast<unsigned>(utc.wMinute),
                  static_cast<unsigned>(utc.wSecond),
                  static_cast<unsigned>(utc.wMilliseconds),
                  static_cast<unsigned long long>(GetTickCount64()));
}

void append_schema(std::string& output,
                   std::uint32_t key,
                   std::uint8_t type,
                   std::uint16_t index) {
    schemas::OmegaSchema schema{};
    if (schemas::find_omega_schema(key, type, index, schema)) {
        append_format(output,
                      ",\"schema_source\":\"package_descriptor\",\"component_class\":\"0x%08X\","
                      "\"auth_schema\":\"0x%08X\",\"sense_schema\":\"0x%08X\"",
                      schema.componentClass,
                      schema.authSchema,
                      schema.senseSchema);
    } else {
        output += ",\"schema_source\":\"unavailable\",\"component_class\":null,"
                  "\"auth_schema\":null,\"sense_schema\":null";
    }
}

[[nodiscard]] bool trace_path(path::Buffer& filePath) noexcept {
    path::Buffer directory{};
    if (!path::artifact_directory(reinterpret_cast<void*>(&__ImageBase), directory)
        || !path::append(directory, kExportDirectorySuffix)) {
        return false;
    }
    if (CreateDirectoryW(directory.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.chars.data());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return false;
    }
    filePath = directory;
    return path::append(filePath, kTraceFileSuffix);
}

[[nodiscard]] bool append_batch(std::string_view batch) noexcept {
    path::Buffer filePath{};
    if (!trace_path(filePath)) {
        return false;
    }
    const HANDLE file = CreateFileW(filePath.chars.data(),
                                    FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::size_t offset = 0;
    bool written = true;
    while (offset < batch.size()) {
        const std::size_t remaining = batch.size() - offset;
        const DWORD requested = remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
        DWORD transferred = 0;
        if (WriteFile(file, batch.data() + offset, requested, &transferred, nullptr) == FALSE
            || transferred == 0) {
            written = false;
            break;
        }
        offset += transferred;
    }
    written = written && FlushFileBuffers(file) != FALSE;
    written = CloseHandle(file) != FALSE && written;
    return written;
}

void report_write(bool written, const char* event, std::size_t records) noexcept {
    if (written) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(line.data(),
                                    line.size(),
                                    "ev=omega_trace result=write_failed event=%s records=%zu",
                                    event,
                                    records);
    if (count > 0) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

[[nodiscard]] bool phase_two_present(
    const message::sensor_auth_update::Snapshot& snapshot) noexcept {
    return !snapshot.phaseOneOnly
           && snapshot.omegaOpeningStage
                  != message::sensor_auth_update::kOmegaOpeningStageSettled
           && (!snapshot.preserveMissionAuthorityState
               || snapshot.omegaOpeningStage
                      != message::sensor_auth_update::kOmegaOpeningStageNone);
}

} // namespace

void record_sense(std::uint64_t sessionId,
                  std::uint64_t packetSequence,
                  std::uint64_t packetHash,
                  std::string_view validation,
                  std::string_view transition,
                  std::string_view hostAction,
                  bool rosterReady,
                  bool openingTriggered,
                  bool parsed,
                  const message::sense_update::SenseUpdate& update,
                  std::span<const std::byte> payload) noexcept {
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    std::string batch;
    batch.reserve(2048 + payload.size() * 2 + update.objectCount * 512);
    append_time_prefix(batch, "sense_packet");
    append_format(batch,
                  ",\"session\":\"0x%016llX\",\"packet_sequence\":%llu,"
                  "\"packet_hash\":\"0x%016llX\",\"validation\":\"%.*s\","
                  "\"transition\":\"%.*s\",\"host_action\":\"%.*s\","
                  "\"mission_phase\":\"%s\",\"parsed\":%s,\"bytes\":%zu,"
                  "\"groups\":%u,\"objects\":%u,\"raw_hex\":\"",
                  static_cast<unsigned long long>(sessionId),
                  static_cast<unsigned long long>(packetSequence),
                  static_cast<unsigned long long>(packetHash),
                  static_cast<int>(validation.size()),
                  validation.data(),
                  static_cast<int>(transition.size()),
                  transition.data(),
                  static_cast<int>(hostAction.size()),
                  hostAction.data(),
                  openingTriggered ? "opening_triggered"
                                   : (rosterReady ? "opening_ready" : "bootstrapping"),
                  parsed ? "true" : "false",
                  payload.size(),
                  static_cast<unsigned>(update.groupCount),
                  static_cast<unsigned>(update.objectCount));
    append_hex(batch, payload);
    batch += "\"}\n";
    std::size_t records = 1;
    if (parsed) {
        for (std::size_t index = 0; index < update.objectCount; ++index) {
            const message::sense_update::SenseObject& object = update.objects[index];
            const std::string symbol =
                symbol_for(object.registryKey, object.slotType, object.slotIndex);
            append_time_prefix(batch, "sense_object");
            append_format(batch,
                          ",\"session\":\"0x%016llX\",\"packet_sequence\":%llu,"
                          "\"packet_hash\":\"0x%016llX\",\"mission_phase\":\"%s\","
                          "\"symbol\":\"%s\",\"registry_key\":\"0x%08X\","
                          "\"slot_type\":%u,\"real_index\":%u,\"group_ordinal\":%u,"
                          "\"object_ordinal\":%u,\"body_bits\":%u,"
                          "\"body_words\":[\"0x%016llX\",\"0x%016llX\",\"0x%016llX\"],"
                          "\"body_hash\":\"0x%016llX\",\"body_set_bits\":%u",
                          static_cast<unsigned long long>(sessionId),
                          static_cast<unsigned long long>(packetSequence),
                          static_cast<unsigned long long>(packetHash),
                          openingTriggered ? "opening_triggered"
                                           : (rosterReady ? "opening_ready" : "bootstrapping"),
                          symbol.c_str(),
                          object.registryKey,
                          static_cast<unsigned>(object.slotType),
                          static_cast<unsigned>(object.slotIndex),
                          static_cast<unsigned>(object.groupOrdinal),
                          static_cast<unsigned>(object.objectOrdinal),
                          object.bodyBits,
                          static_cast<unsigned long long>(object.bodyFirst),
                          static_cast<unsigned long long>(object.bodySecond),
                          static_cast<unsigned long long>(object.bodyThird),
                          static_cast<unsigned long long>(object.bodyHash),
                          static_cast<unsigned>(object.bodySetBitCount));
            append_schema(batch, object.registryKey, object.slotType, object.slotIndex);
            batch += "}\n";
            ++records;
        }
    }
    report_write(append_batch(batch), "sense", records);
    ReleaseSRWLockExclusive(&g_lock);
}

std::uint64_t record_auth_staged(
    std::uint64_t sessionId,
    const message::sensor_auth_update::Snapshot& snapshot,
    std::span<const std::byte> payload) noexcept {
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics) {
        return 0;
    }
    AcquireSRWLockExclusive(&g_lock);
    const std::uint64_t publicationId = ++g_publicationSequence;
    std::string batch;
    batch.reserve(4096 + payload.size() * 2);
    append_time_prefix(batch, "auth_packet_staged");
    append_format(batch,
                  ",\"session\":\"0x%016llX\",\"publication_id\":%llu,"
                  "\"message_type\":5,\"packet_hash\":\"0x%016llX\",\"bytes\":%zu,"
                  "\"state_sequence\":%u,\"group_count\":%zu,\"top_level_groups\":%zu,"
                  "\"phase_two_present\":%s,\"phase_one_only\":%s,"
                  "\"preserve_mission_authority\":%s,\"opening_transition\":%s,"
                  "\"opening_stage\":%u,"
                  "\"activity_script_state\":%d,\"mission_director_active\":%s,"
                  "\"raw_hex\":\"",
                  static_cast<unsigned long long>(sessionId),
                  static_cast<unsigned long long>(publicationId),
                  static_cast<unsigned long long>(hash_payload(payload)),
                  payload.size(),
                  static_cast<unsigned>(snapshot.stateSequence),
                  snapshot.roster.groupCount,
                  snapshot.roster.topLevelGroupCount,
                  phase_two_present(snapshot) ? "true" : "false",
                  snapshot.phaseOneOnly ? "true" : "false",
                  snapshot.preserveMissionAuthorityState ? "true" : "false",
                  snapshot.publishOmegaOpeningTransition ? "true" : "false",
                  static_cast<unsigned>(snapshot.omegaOpeningStage),
                  snapshot.activityScriptState,
                  snapshot.missionDirectorActive ? "true" : "false");
    append_hex(batch, payload);
    batch += "\"}\n";
    std::size_t records = 1;

    if (phase_two_present(snapshot)) {
        bool keyPlaced = false;
        for (std::size_t groupIndex = 0; groupIndex < snapshot.roster.groupCount; ++groupIndex) {
            const message::sensor_auth_update::Group& group =
                snapshot.roster.groups[groupIndex];
            for (std::size_t slot = 0; slot < group.slotTypes.size(); ++slot) {
                const std::uint8_t type = group.slotTypes[slot];
                const std::uint8_t flags = group.slotFlags[slot];
                const std::uint16_t realIndex = group.slotIndices[slot];
                if ((flags & (message::sensor_auth_update::kSlotAuthFlag
                              | message::sensor_auth_update::kSlotSenseFlag))
                    == 0) {
                    continue;
                }
                const bool runtimePacket =
                    snapshot.omegaOpeningStage
                        == message::sensor_auth_update::kOmegaOpeningStageTriggered
                    || snapshot.omegaOpeningStage
                           == message::sensor_auth_update::kOmegaOpeningStageCompleted;
                if (runtimePacket
                    && (group.key != kOmegaRuntimeKey
                        || (type != kActivityScriptType && type != kMissionDirectorType))) {
                    continue;
                }
                const bool scenePacket =
                    snapshot.omegaOpeningStage
                        == message::sensor_auth_update::kOmegaOpeningStageScene
                    || snapshot.omegaOpeningStage
                           == message::sensor_auth_update::kOmegaOpeningStageReady;
                if (scenePacket
                    && (group.key != kOmegaOpeningKey || type != kOmegaSceneType
                        || realIndex != kOmegaSceneIndex)) {
                    continue;
                }
                const bool portalPacket =
                    snapshot.omegaOpeningStage
                    == message::sensor_auth_update::kOmegaOpeningStagePortal;
                if (portalPacket
                    && (group.key != kOmegaOpeningKey || type != kOmegaPortalVisualType
                        || realIndex < kOmegaPortalVisualFirstIndex
                        || realIndex > kOmegaPortalVisualLastIndex)) {
                    continue;
                }
                if (snapshot.omegaOpeningStage
                        == message::sensor_auth_update::kOmegaOpeningStageNone
                    && snapshot.preserveMissionAuthorityState
                    && (type == kActivityScriptType || type == kMissionDirectorType)) {
                    continue;
                }
                const bool firstOrEvery = !keyPlaced || snapshot.keyOnEveryParticipationSlot;
                const bool carriesPlayerKey =
                    type == kParticipationType && group.key == snapshot.roster.playerKeyGroup
                    && firstOrEvery;
                keyPlaced = keyPlaced || carriesPlayerKey;
                const std::size_t bodyBits = message::sensor_auth_update::auth_body_bits(
                    snapshot, group.key, type, realIndex, carriesPlayerKey);
                const std::string symbol = symbol_for(group.key, type, realIndex);
                append_time_prefix(batch, "auth_component_staged");
                append_format(batch,
                              ",\"session\":\"0x%016llX\",\"publication_id\":%llu,"
                              "\"symbol\":\"%s\",\"registry_key\":\"0x%08X\","
                              "\"slot_type\":%u,\"real_index\":%u,\"flags\":\"0x%02X\","
                              "\"auth_schema_present\":%s,\"sense_schema_present\":%s,"
                              "\"auth_body_bits\":%zu,\"carries_player_key\":%s,"
                              "\"activity_script_state\":%d,\"mission_director_active\":%s",
                              static_cast<unsigned long long>(sessionId),
                              static_cast<unsigned long long>(publicationId),
                              symbol.c_str(),
                              group.key,
                              static_cast<unsigned>(type),
                              static_cast<unsigned>(realIndex),
                              static_cast<unsigned>(flags),
                              (flags & message::sensor_auth_update::kSlotAuthFlag) != 0 ? "true"
                                                                                       : "false",
                              (flags & message::sensor_auth_update::kSlotSenseFlag) != 0 ? "true"
                                                                                        : "false",
                              bodyBits,
                              carriesPlayerKey ? "true" : "false",
                              snapshot.activityScriptState,
                              snapshot.missionDirectorActive ? "true" : "false");
                append_schema(batch, group.key, type, realIndex);
                batch += "}\n";
                ++records;
            }
        }
    }
    report_write(append_batch(batch), "auth_staged", records);
    ReleaseSRWLockExclusive(&g_lock);
    return publicationId;
}

void record_auth_delivery(std::uint64_t sessionId,
                          std::uint64_t publicationId,
                          std::uint8_t scriptState,
                          bool delivered) noexcept {
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics
        || publicationId == 0) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    std::string line;
    append_time_prefix(line, delivered ? "auth_packet_delivered" : "auth_packet_discarded");
    append_format(line,
                  ",\"session\":\"0x%016llX\",\"publication_id\":%llu,"
                  "\"activity_script_state\":%u,\"delivered\":%s}\n",
                  static_cast<unsigned long long>(sessionId),
                  static_cast<unsigned long long>(publicationId),
                  static_cast<unsigned>(scriptState),
                  delivered ? "true" : "false");
    report_write(append_batch(line), "auth_delivery", 1);
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace sunrise::server::bap::encrypted::diagnostics::omega_trace
