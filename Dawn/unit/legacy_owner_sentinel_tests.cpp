#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "client/hooks/bootflow/legacy_owner_quarantine_lifecycle.h"
#include "client/hooks/bootflow/legacy_owner_sentinel.h"

namespace {

namespace sentinel = dawn::client::hooks::bootflow::legacy_owner_sentinel;
namespace quarantine = dawn::client::hooks::bootflow::legacy_owner_quarantine;

int g_failureCount{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "unable to read production source: " << path << '\n';
        ++g_failureCount;
        return {};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] std::string marked_region(const std::string& source,
                                        std::string_view begin,
                                        std::string_view end) {
    const std::size_t first = source.find(begin);
    CHECK(first != std::string::npos);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = source.find(end, first + begin.size());
    CHECK(last != std::string::npos);
    if (last == std::string::npos) {
        return {};
    }
    return source.substr(first, last + end.size() - first);
}

[[nodiscard]] std::string function_region(const std::string& source,
                                          std::string_view functionName) {
    const std::size_t name = source.find(functionName);
    CHECK(name != std::string::npos);
    if (name == std::string::npos) {
        return {};
    }
    const std::size_t open = source.find('{', name + functionName.size());
    CHECK(open != std::string::npos);
    if (open == std::string::npos) {
        return {};
    }
    std::size_t depth = 0U;
    for (std::size_t cursor = open; cursor < source.size(); ++cursor) {
        if (source[cursor] == '{') {
            ++depth;
        } else if (source[cursor] == '}') {
            CHECK(depth != 0U);
            --depth;
            if (depth == 0U) {
                return source.substr(name, cursor + 1U - name);
            }
        }
    }
    CHECK(false);
    return {};
}

[[nodiscard]] std::size_t token_count(const std::string& source,
                                      const std::string& token) {
    const std::regex expression{"\\b" + token + "\\b"};
    return static_cast<std::size_t>(
        std::distance(std::sregex_iterator(source.begin(), source.end(), expression),
                      std::sregex_iterator{}));
}

[[nodiscard]] std::vector<std::string> captures(const std::string& source,
                                                const std::regex& expression) {
    std::vector<std::string> result;
    for (std::sregex_iterator current(source.begin(), source.end(), expression), end;
         current != end;
         ++current) {
        result.push_back((*current)[1].str());
    }
    return result;
}

[[nodiscard]] std::set<std::string> as_set(const std::vector<std::string>& values) {
    return {values.begin(), values.end()};
}

struct OwnerContract final {
    std::string_view markerName;
    std::string_view fileName;
    std::string_view apiName;
    std::size_t pairCount{};
    std::vector<std::string_view> claims{};
    std::vector<std::string_view> excludedTelemetry{};
    bool indexedHandleArray{};
    bool sharedType31TranslationUnit{};
};

const std::array<OwnerContract, 9> kOwners{{
    {"activity_selection",
     "activity_selection_probe.cpp",
     "activity_selection_probe_has_ownership",
     40U,
     {"g_selectionPublicationState0",
      "g_activitySelectionPumpTarget",
      "g_selectionStateOneWatchVectoredHandler",
      "g_selectionStateOneWatchStatus",
      "g_selectionStateOneWatchArmStarted",
      "g_selectionStateOneWatchArmedThreads",
      "g_selectionStateOneWatchAddress",
      "g_state5SnapshotStarted",
      "g_selectionRouteObjectBuilderSnapshotStarted"}},
    {"player_broadcast_create",
     "player_broadcast_create_probe.cpp",
     "player_broadcast_create_probe_has_ownership",
     8U,
     {"g_installAttempted"}},
    {"activity_feature_flag",
     "activity_feature_flag_probe.cpp",
     "activity_feature_flag_probe_has_ownership",
     1U,
     {"g_installed"}},
    {"activity_script_event",
     "activity_script_event_probe.cpp",
     "activity_script_event_probe_has_ownership",
     5U},
    {"activity_script_upstream",
     "activity_script_upstream_probe.cpp",
     "activity_script_upstream_probe_has_ownership",
     116U,
     {"g_currentSelectionPublishInstallAttempted",
      "g_embeddedRouteIdentityProviderLookupInstallAttempted",
      "g_currentSelectionManager",
      "g_currentSelectionOwner"},
     {"g_bootstrapRequestAttempted",
      "g_componentDispatchAttempted",
      "g_activityReceiverSnapshotAttempted",
      "g_launchProducerReadinessRequestAttempted",
      "g_omegaPendingConsumerAttempted"}},
    {"activity_notification_type1",
     "activity_notification_type1_apply_probe.cpp",
     "activity_notification_type1_apply_probe_has_ownership",
     1U},
    {"activity_behavior_condition",
     "activity_behavior_condition_probe.cpp",
     "activity_behavior_condition_probe_has_ownership",
     9U,
     {"g_installed", "g_image"},
     {},
     true},
    {"activity_schema_decode_legacy_bundle",
     "activity_schema_decode_probe.cpp",
     "activity_schema_decode_legacy_bundle_has_ownership",
     18U,
     {"g_serviceSevenCaptureActive", "g_serviceSevenReadCaptureActive"},
     {},
     false,
     true},
    {"group_initial_update_decode",
     "group_initial_update_decode_probe.cpp",
     "group_initial_update_decode_probe_has_ownership",
     3U},
}};

enum class LifecycleEvent {
    lockInstall,
    readInstalled,
    readAccepting,
    readLegacyOwnership,
    closeAdmission,
    unlockInstall,
    quiesce,
};

struct LifecycleOperations final {
    std::array<LifecycleEvent, 16> events{};
    std::size_t eventCount{};
    bool lockHeld{};
    bool installedState{};
    bool acceptingState{};
    bool legacyOwnership{};
    bool quiesced{};
    bool dependenciesPublished{true};
    bool type31Owned{};
    std::uint32_t installerCalls{};
    std::uint32_t detachCalls{};
    std::uint32_t legacyDetachCalls{};

    void record(LifecycleEvent event) noexcept {
        if (eventCount < events.size()) {
            events[eventCount++] = event;
        }
    }

    void lock_install() noexcept {
        record(LifecycleEvent::lockInstall);
        lockHeld = true;
    }

    void unlock_install() noexcept {
        record(LifecycleEvent::unlockInstall);
        lockHeld = false;
    }

    [[nodiscard]] bool installed() noexcept {
        record(LifecycleEvent::readInstalled);
        return installedState;
    }

    [[nodiscard]] bool accepting() noexcept {
        record(LifecycleEvent::readAccepting);
        return acceptingState;
    }

    [[nodiscard]] bool quarantined_legacy_ownership() noexcept {
        record(LifecycleEvent::readLegacyOwnership);
        return legacyOwnership;
    }

    void close_late_admission() noexcept {
        record(LifecycleEvent::closeAdmission);
        acceptingState = false;
    }

    void quiesce() noexcept {
        record(LifecycleEvent::quiesce);
        acceptingState = false;
        quiesced = true;
    }
};

[[nodiscard]] std::size_t event_position(const LifecycleOperations& operations,
                                         LifecycleEvent expected) noexcept {
    for (std::size_t index = 0; index < operations.eventCount; ++index) {
        if (operations.events[index] == expected) {
            return index;
        }
    }
    return operations.events.size();
}

class DynamicAdmissionState final {
public:
    void close() noexcept {
        closeAttempted_.store(true, std::memory_order_release);
        closeAttempted_.notify_all();
        const std::unique_lock lock(lock_);
        accepting_ = false;
        closeComplete_.store(true, std::memory_order_release);
        closeComplete_.notify_all();
    }

    void wait_until_close_attempted() const noexcept {
        closeAttempted_.wait(false, std::memory_order_acquire);
    }

    [[nodiscard]] bool close_complete() const noexcept {
        return closeComplete_.load(std::memory_order_acquire);
    }

private:
    friend class DynamicAdmissionGuard;

    std::shared_mutex lock_{};
    bool accepting_{true};
    std::atomic_bool closeAttempted_{};
    std::atomic_bool closeComplete_{};
};

class DynamicAdmissionGuard final {
public:
    explicit DynamicAdmissionGuard(DynamicAdmissionState& state) noexcept
        : state_(state), lock_(state.lock_) {
        accepted_ = state_.accepting_;
        if (!accepted_) {
            lock_.unlock();
        }
    }

    ~DynamicAdmissionGuard() = default;
    DynamicAdmissionGuard(const DynamicAdmissionGuard&) = delete;
    DynamicAdmissionGuard& operator=(const DynamicAdmissionGuard&) = delete;

    [[nodiscard]] bool accepted() const noexcept {
        return accepted_;
    }

private:
    DynamicAdmissionState& state_;
    std::shared_lock<std::shared_mutex> lock_;
    bool accepted_{};
};

void production_primitive_truth_table() {
    CHECK(!sentinel::has_ownership({}));
    const std::size_t mappedCount = std::accumulate(
        sentinel::kOwnerHookCounts.begin(),
        sentinel::kOwnerHookCounts.end(),
        std::size_t{});
    CHECK(mappedCount == sentinel::mapped_hook_count());
    CHECK(mappedCount == sentinel::kCompleteLegacyHookCount);
    CHECK(mappedCount == 201U);

    for (const std::size_t count : sentinel::kOwnerHookCounts) {
        std::vector<sentinel::HookOwnership> hooks(count);
        CHECK(!sentinel::has_ownership(hooks));
        for (std::size_t slot = 0; slot < hooks.size(); ++slot) {
            hooks[slot].attached = true;
            CHECK(sentinel::has_ownership(hooks));
            hooks[slot].attached = false;

            hooks[slot].originalPublished = true;
            CHECK(sentinel::has_ownership(hooks));
            hooks[slot].originalPublished = false;
        }
    }

    for (const OwnerContract& owner : kOwners) {
        std::vector<sentinel::HookOwnership> hooks(owner.pairCount);
        std::array<bool, 9> claims{};
        CHECK(owner.claims.size() <= claims.size());
        const std::span<const bool> claimView{claims.data(), owner.claims.size()};
        CHECK(!sentinel::has_ownership(hooks, claimView));
        for (std::size_t index = 0; index < owner.claims.size(); ++index) {
            claims[index] = true;
            CHECK(sentinel::has_ownership(hooks, claimView));
            claims[index] = false;
        }
    }

    std::array<bool, sentinel::kQuarantinedOwnerCount> owners{};
    CHECK(!sentinel::any_quarantined_legacy_ownership(owners));
    for (std::size_t index = 0; index < owners.size(); ++index) {
        owners[index] = true;
        CHECK(sentinel::any_quarantined_legacy_ownership(owners));
        owners[index] = false;
    }
}

void production_lifecycle_behavior_contract() {
    LifecycleOperations dirtyRetry{};
    dirtyRetry.installedState = true;
    dirtyRetry.acceptingState = true;
    dirtyRetry.legacyOwnership = true;
    const quarantine::InstallDisposition dirtyDisposition =
        quarantine::begin_install(dirtyRetry);
    if (dirtyDisposition == quarantine::InstallDisposition::proceed_with_lock) {
        ++dirtyRetry.installerCalls;
    }
    CHECK(dirtyDisposition
          == quarantine::InstallDisposition::rejected_legacy_ownership);
    CHECK(dirtyRetry.installedState);
    CHECK(!dirtyRetry.acceptingState);
    CHECK(!dirtyRetry.lockHeld);
    CHECK(dirtyRetry.dependenciesPublished);
    CHECK(dirtyRetry.installerCalls == 0U);
    CHECK(event_position(dirtyRetry, LifecycleEvent::readLegacyOwnership)
          < event_position(dirtyRetry, LifecycleEvent::closeAdmission));
    CHECK(event_position(dirtyRetry, LifecycleEvent::closeAdmission)
          < event_position(dirtyRetry, LifecycleEvent::unlockInstall));

    LifecycleOperations dirtyFresh{};
    dirtyFresh.acceptingState = false;
    dirtyFresh.legacyOwnership = true;
    CHECK(quarantine::begin_install(dirtyFresh)
          == quarantine::InstallDisposition::rejected_legacy_ownership);
    CHECK(!dirtyFresh.installedState);
    CHECK(!dirtyFresh.acceptingState);
    CHECK(!dirtyFresh.lockHeld);
    CHECK(dirtyFresh.installerCalls == 0U);

    LifecycleOperations closedGeneration{};
    closedGeneration.installedState = true;
    closedGeneration.acceptingState = false;
    closedGeneration.legacyOwnership = true;
    CHECK(quarantine::begin_install(closedGeneration)
          == quarantine::InstallDisposition::rejected_closed);
    CHECK(event_position(closedGeneration, LifecycleEvent::readLegacyOwnership)
          == closedGeneration.events.size());
    CHECK(!closedGeneration.lockHeld);
    CHECK(closedGeneration.installedState);

    LifecycleOperations cleanRetry{};
    cleanRetry.installedState = true;
    cleanRetry.acceptingState = true;
    const quarantine::InstallDisposition cleanDisposition =
        quarantine::begin_install(cleanRetry);
    CHECK(cleanDisposition == quarantine::InstallDisposition::proceed_with_lock);
    CHECK(cleanRetry.lockHeld);
    if (cleanDisposition == quarantine::InstallDisposition::proceed_with_lock) {
        ++cleanRetry.installerCalls;
        cleanRetry.unlock_install();
    }
    CHECK(cleanRetry.installerCalls == 1U);
    CHECK(!cleanRetry.lockHeld);

    LifecycleOperations dirtyUninstall{};
    dirtyUninstall.installedState = true;
    dirtyUninstall.acceptingState = true;
    dirtyUninstall.legacyOwnership = true;
    const bool dirtyMayDetach = quarantine::begin_uninstall(dirtyUninstall);
    if (dirtyMayDetach) {
        ++dirtyUninstall.detachCalls;
    }
    CHECK(!dirtyMayDetach);
    CHECK(dirtyUninstall.quiesced);
    CHECK(!dirtyUninstall.acceptingState);
    CHECK(dirtyUninstall.installedState);
    CHECK(dirtyUninstall.dependenciesPublished);
    CHECK(dirtyUninstall.detachCalls == 0U);
    CHECK(dirtyUninstall.legacyDetachCalls == 0U);
    CHECK(event_position(dirtyUninstall, LifecycleEvent::quiesce)
          < event_position(dirtyUninstall, LifecycleEvent::readLegacyOwnership));

    LifecycleOperations type31Retained{};
    type31Retained.installedState = true;
    type31Retained.acceptingState = true;
    type31Retained.type31Owned = true;
    const bool type31MayDetach = quarantine::begin_uninstall(type31Retained);
    CHECK(type31MayDetach);
    CHECK(type31Retained.quiesced);
    CHECK(type31Retained.type31Owned);
    CHECK(type31Retained.legacyDetachCalls == 0U);
}

void production_dynamic_writer_behavior_contract() {
    DynamicAdmissionState admission;
    std::atomic_bool writerEntered{};
    std::atomic_bool releaseWriter{};
    std::atomic_uint32_t publications{};
    std::atomic_bool admittedResult{};

    std::thread writer([&] {
        const bool admitted = quarantine::execute_admitted_dynamic_writer(
            DynamicAdmissionGuard{admission},
            [&]() noexcept {
                writerEntered.store(true, std::memory_order_release);
                writerEntered.notify_all();
                releaseWriter.wait(false, std::memory_order_acquire);
                publications.fetch_add(1U, std::memory_order_release);
            });
        admittedResult.store(admitted, std::memory_order_release);
    });
    writerEntered.wait(false, std::memory_order_acquire);

    std::thread closer([&] { admission.close(); });
    admission.wait_until_close_attempted();
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    CHECK(!admission.close_complete());
    CHECK(publications.load(std::memory_order_acquire) == 0U);

    releaseWriter.store(true, std::memory_order_release);
    releaseWriter.notify_all();
    writer.join();
    closer.join();
    CHECK(admittedResult.load(std::memory_order_acquire));
    CHECK(admission.close_complete());
    CHECK(publications.load(std::memory_order_acquire) == 1U);

    const bool postCloseAdmitted = quarantine::execute_admitted_dynamic_writer(
        DynamicAdmissionGuard{admission},
        [&]() noexcept { publications.fetch_add(1U, std::memory_order_release); });
    CHECK(!postCloseAdmitted);
    CHECK(publications.load(std::memory_order_acquire) == 1U);
}

void production_source_coverage(const std::filesystem::path& bootflowRoot) {
    const std::regex handleDeclaration{R"(hooking::detour::Handle\s+(g_\w+)\s*\{)"};
    const std::regex originalDeclaration{
        R"(std::atomic\s*<[^>]+>\s*(g_(?:original|\w+Original))\s*\{)"};
    const std::regex claimToken{R"(\b(g_\w+)\b)"};
    const std::string header = read_text(bootflowRoot / "legacy_owner_sentinel.h");

    CHECK(kOwners.size() == sentinel::kOwnerHookCounts.size());
    for (std::size_t ownerIndex = 0; ownerIndex < kOwners.size(); ++ownerIndex) {
        const OwnerContract& owner = kOwners[ownerIndex];
        CHECK(owner.pairCount == sentinel::kOwnerHookCounts[ownerIndex]);
        CHECK(token_count(header, std::string{owner.apiName}) == 1U);

        const std::string source = read_text(bootflowRoot / owner.fileName);
        const std::string begin =
            "LEGACY_OWNER_SENTINEL_BEGIN(" + std::string{owner.markerName} + ", "
            + std::to_string(owner.pairCount) + ")";
        const std::string end =
            "LEGACY_OWNER_SENTINEL_END(" + std::string{owner.markerName} + ")";
        const std::string sentinelRegion = marked_region(source, begin, end);
        CHECK(token_count(source, std::string{owner.apiName}) == 1U);
        const std::string apiRegion = function_region(source, owner.apiName);
        CHECK(apiRegion.find(".store(") == std::string::npos);
        CHECK(apiRegion.find(".exchange(") == std::string::npos);
        CHECK(apiRegion.find("detour::uninstall") == std::string::npos);
        CHECK(sentinelRegion.find(".store(") == std::string::npos);
        CHECK(sentinelRegion.find(".exchange(") == std::string::npos);
        CHECK(sentinelRegion.find("detour::uninstall") == std::string::npos);

        std::vector<std::string> originals = captures(source, originalDeclaration);
        if (owner.sharedType31TranslationUnit) {
            CHECK(originals.size() == owner.pairCount + 1U);
            const auto point = std::find(
                originals.begin(), originals.end(), "g_omegaPointApplyOriginal");
            CHECK(point != originals.end());
            if (point != originals.end()) {
                originals.erase(point);
            }
        }
        CHECK(originals.size() == owner.pairCount);
        CHECK(as_set(originals).size() == originals.size());
        for (const std::string& original : originals) {
            CHECK(token_count(sentinelRegion, original) == 1U);
        }

        if (owner.indexedHandleArray) {
            CHECK(source.find("std::array<hooking::detour::Handle, kHookCount> g_handles")
                  != std::string::npos);
            CHECK(token_count(sentinelRegion, "g_handles") == owner.pairCount);
            const std::regex handleIndex{R"(\bg_handles\s*\[\s*(\d+)\s*\])"};
            const std::vector<std::string> indices = captures(sentinelRegion, handleIndex);
            CHECK(indices.size() == owner.pairCount);
            CHECK(as_set(indices).size() == owner.pairCount);
            std::set<std::string> expectedIndices;
            for (std::size_t index = 0; index < owner.pairCount; ++index) {
                expectedIndices.insert(std::to_string(index));
            }
            CHECK(as_set(indices) == expectedIndices);
        } else {
            std::vector<std::string> handles = captures(source, handleDeclaration);
            if (owner.sharedType31TranslationUnit) {
                CHECK(handles.size() == owner.pairCount + 1U);
                const auto point = std::find(
                    handles.begin(), handles.end(), "g_omegaPointApplyHandle");
                CHECK(point != handles.end());
                if (point != handles.end()) {
                    handles.erase(point);
                }
            }
            CHECK(handles.size() == owner.pairCount);
            CHECK(as_set(handles).size() == handles.size());
            for (const std::string& handle : handles) {
                CHECK(token_count(sentinelRegion, handle) == 1U);
            }
        }

        if (owner.claims.empty()) {
            const std::string absentClaimMarker =
                "LEGACY_OWNER_CLAIMS_BEGIN(" + std::string{owner.markerName};
            CHECK(source.find(absentClaimMarker) == std::string::npos);
        } else {
            const std::string claimBegin =
                "LEGACY_OWNER_CLAIMS_BEGIN(" + std::string{owner.markerName} + ", "
                + std::to_string(owner.claims.size()) + ")";
            const std::string claimEnd =
                "LEGACY_OWNER_CLAIMS_END(" + std::string{owner.markerName} + ")";
            const std::string claimRegion = marked_region(source, claimBegin, claimEnd);
            const std::set<std::string> actualClaims = as_set(captures(claimRegion, claimToken));
            std::set<std::string> expectedClaims;
            for (const std::string_view claim : owner.claims) {
                expectedClaims.emplace(claim);
                CHECK(token_count(claimRegion, std::string{claim}) == 1U);
            }
            CHECK(actualClaims == expectedClaims);
            CHECK(claimRegion.find(".store(") == std::string::npos);
            CHECK(claimRegion.find(".exchange(") == std::string::npos);
            CHECK(claimRegion.find("detour::uninstall") == std::string::npos);
            for (const std::string_view telemetry : owner.excludedTelemetry) {
                CHECK(token_count(claimRegion, std::string{telemetry}) == 0U);
            }
        }
    }
}

void type31_exclusion_contract(const std::filesystem::path& bootflowRoot) {
    const std::regex handleDeclaration{R"(hooking::detour::Handle\s+(g_\w+)\s*\{)"};
    const std::regex originalDeclaration{
        R"(std::atomic\s*<[^>]+>\s*(g_(?:original|\w+Original))\s*\{)"};
    const std::string schema = read_text(bootflowRoot / "activity_schema_decode_probe.cpp");
    const std::vector<std::string> handles = captures(schema, handleDeclaration);
    const std::vector<std::string> originals = captures(schema, originalDeclaration);
    CHECK(handles.size() == 19U);
    CHECK(originals.size() == 19U);
    CHECK(as_set(handles).contains("g_omegaPointApplyHandle"));
    CHECK(as_set(originals).contains("g_omegaPointApplyOriginal"));
    CHECK(handles.size() - 1U == 18U);
    CHECK(originals.size() - 1U == 18U);

    const std::string sentinelRegion = marked_region(
        schema,
        "LEGACY_OWNER_SENTINEL_BEGIN(activity_schema_decode_legacy_bundle, 18)",
        "LEGACY_OWNER_SENTINEL_END(activity_schema_decode_legacy_bundle)");
    const std::string claimRegion = marked_region(
        schema,
        "LEGACY_OWNER_CLAIMS_BEGIN(activity_schema_decode_legacy_bundle, 2)",
        "LEGACY_OWNER_CLAIMS_END(activity_schema_decode_legacy_bundle)");
    const std::string apiRegion = function_region(
        schema, "activity_schema_decode_legacy_bundle_has_ownership");
    const std::string legacyOwnerSource = sentinelRegion + claimRegion + apiRegion;
    CHECK(token_count(legacyOwnerSource, "g_omegaPointApplyHandle") == 0U);
    CHECK(token_count(legacyOwnerSource, "g_omegaPointApplyOriginal") == 0U);
    const std::regex type31State{R"(\b(g_type31\w*)\b)"};
    CHECK(captures(legacyOwnerSource, type31State).empty());

    std::vector<std::string> excludedHandles;
    for (const std::string& handle : handles) {
        if (token_count(sentinelRegion, handle) == 0U) {
            excludedHandles.push_back(handle);
        }
    }
    std::vector<std::string> excludedOriginals;
    for (const std::string& original : originals) {
        if (token_count(sentinelRegion, original) == 0U) {
            excludedOriginals.push_back(original);
        }
    }
    CHECK(excludedHandles
          == std::vector<std::string>{"g_omegaPointApplyHandle"});
    CHECK(excludedOriginals
          == std::vector<std::string>{"g_omegaPointApplyOriginal"});

    const std::string armRegion = function_region(
        schema, "arm_activity_host_manager_response_decode_probe");
    CHECK(token_count(armRegion, "LateInstallGuard") == 1U);
    CHECK(token_count(armRegion, "accepted") == 1U);
    CHECK(token_count(armRegion, "g_bitHandle") == 1U);
    CHECK(token_count(armRegion, "g_original") == 1U);
    const std::size_t armAdmission = armRegion.find("LateInstallGuard");
    const std::size_t firstArmStore = armRegion.find(".store(");
    CHECK(armAdmission != std::string::npos);
    CHECK(firstArmStore != std::string::npos);
    CHECK(armAdmission < firstArmStore);

    const std::size_t operationsBegin = schema.find("class Type31OwnerOperations final");
    const std::size_t operationsEnd =
        schema.find("bool install_type31_objective_capture() noexcept", operationsBegin);
    CHECK(operationsBegin != std::string::npos);
    CHECK(operationsEnd != std::string::npos);
    const std::string operations =
        operationsBegin != std::string::npos && operationsEnd != std::string::npos
            ? schema.substr(operationsBegin, operationsEnd - operationsBegin)
            : std::string{};
    CHECK(token_count(operations, "g_omegaPointApplyHandle") >= 1U);
    CHECK(token_count(operations, "g_omegaPointApplyOriginal") >= 1U);

    for (const OwnerContract& owner : kOwners) {
        if (owner.sharedType31TranslationUnit) {
            continue;
        }
        const std::string source = read_text(bootflowRoot / owner.fileName);
        CHECK(token_count(source, "g_omegaPointApplyHandle") == 0U);
        CHECK(token_count(source, "g_omegaPointApplyOriginal") == 0U);
    }
    const std::string header = read_text(bootflowRoot / "legacy_owner_sentinel.h");
    CHECK(token_count(header, "g_omegaPointApplyHandle") == 0U);
    CHECK(token_count(header, "g_omegaPointApplyOriginal") == 0U);

    const bool type31Owned = true;
    std::array<bool, sentinel::kQuarantinedOwnerCount> legacyOwners{};
    CHECK(type31Owned);
    CHECK(!sentinel::any_quarantined_legacy_ownership(legacyOwners));
    legacyOwners[7] = true;
    CHECK(sentinel::any_quarantined_legacy_ownership(legacyOwners));
}

void dynamic_writer_source_contract(const std::filesystem::path& bootflowRoot) {
    const std::string upstream = read_text(
        bootflowRoot / "activity_script_upstream_probe.cpp");
    const std::string selectionWrapper = function_region(
        upstream, "void ensure_current_selection_publish_probe");
    const std::string selectionWriter = function_region(
        upstream, "void current_selection_publish_probe_admitted");
    const std::string providerWrapper = function_region(
        upstream, "void ensure_embedded_route_identity_provider_lookup_probe");
    const std::string providerWriter = function_region(
        upstream, "void embedded_route_identity_provider_lookup_probe_admitted");

    for (const std::string* wrapper : {&selectionWrapper, &providerWrapper}) {
        CHECK(token_count(*wrapper, "execute_admitted_dynamic_writer") == 1U);
        CHECK(token_count(*wrapper, "LateInstallGuard") == 1U);
        CHECK(wrapper->find(".store(") == std::string::npos);
        CHECK(wrapper->find("compare_exchange") == std::string::npos);
        CHECK(wrapper->find("detour::install") == std::string::npos);
    }
    CHECK(token_count(selectionWrapper, "current_selection_publish_probe_admitted") == 1U);
    CHECK(token_count(providerWrapper,
                      "embedded_route_identity_provider_lookup_probe_admitted")
          == 1U);
    CHECK(token_count(upstream, "current_selection_publish_probe_admitted") == 2U);
    CHECK(token_count(upstream,
                      "embedded_route_identity_provider_lookup_probe_admitted")
          == 2U);
    CHECK(token_count(upstream, "ensure_current_selection_publish_probe") == 2U);
    CHECK(token_count(upstream,
                      "ensure_embedded_route_identity_provider_lookup_probe")
          == 2U);

    constexpr std::array<std::string_view, 3> kSelectionDynamicHandles{
        "g_currentSelectionPublishHandle",
        "g_upstreamSelectionPublishHandle",
        "g_upstreamSelectionUpdateHandle",
    };
    constexpr std::array<std::string_view, 3> kSelectionDynamicOriginals{
        "g_currentSelectionPublishOriginal",
        "g_upstreamSelectionPublishOriginal",
        "g_upstreamSelectionUpdateOriginal",
    };
    for (const std::string_view handle : kSelectionDynamicHandles) {
        CHECK(token_count(selectionWriter, std::string{handle}) >= 1U);
        CHECK(token_count(selectionWrapper, std::string{handle}) == 0U);
    }
    for (const std::string_view original : kSelectionDynamicOriginals) {
        CHECK(token_count(selectionWriter, std::string{original}) >= 1U);
    }
    CHECK(token_count(selectionWriter,
                      "g_currentSelectionPublishInstallAttempted")
          >= 1U);
    CHECK(token_count(providerWriter,
                      "g_embeddedRouteIdentityProviderLookupHandle")
          >= 1U);
    CHECK(token_count(providerWriter,
                      "g_embeddedRouteIdentityProviderLookupOriginal")
          >= 1U);
    CHECK(token_count(providerWriter,
                      "g_embeddedRouteIdentityProviderLookupInstallAttempted")
          >= 1U);

    const std::string snapshot = marked_region(
        upstream,
        "LEGACY_OWNER_SENTINEL_BEGIN(activity_script_upstream, 116)",
        "LEGACY_OWNER_SENTINEL_END(activity_script_upstream)");
    for (const std::string_view original : kSelectionDynamicOriginals) {
        CHECK(token_count(snapshot, std::string{original}) == 1U);
    }
    CHECK(token_count(snapshot,
                      "g_embeddedRouteIdentityProviderLookupOriginal")
          == 1U);
    const std::string snapshotFunction = function_region(
        upstream, "activity_script_upstream_hook_ownership");
    CHECK(snapshotFunction.find("std::memory_order_acquire") != std::string::npos);
    CHECK(selectionWriter.find("std::memory_order_release") != std::string::npos);
    CHECK(providerWriter.find("std::memory_order_release") != std::string::npos);

    const std::string lifecycle = read_text(bootflowRoot / "bootflow_hook_lifecycle.cpp");
    const std::string guardConstructor = function_region(
        lifecycle, "LateInstallGuard::LateInstallGuard");
    const std::string guardDestructor = function_region(
        lifecycle, "LateInstallGuard::~LateInstallGuard");
    CHECK(guardConstructor.find("AcquireSRWLockShared") != std::string::npos);
    CHECK(guardConstructor.find("g_acceptLateInstalls.load(std::memory_order_acquire)")
          != std::string::npos);
    CHECK(guardDestructor.find("ReleaseSRWLockShared") != std::string::npos);
    CHECK(lifecycle.find("AcquireSRWLockExclusive(&g_lateInstallLock)")
          != std::string::npos);
    CHECK(lifecycle.find("g_acceptLateInstalls.store(false, std::memory_order_release)")
          != std::string::npos);
}

void lifecycle_aggregate_contract(const std::filesystem::path& bootflowRoot) {
    const std::string lifecycle = read_text(bootflowRoot / "bootflow_hook_lifecycle.cpp");
    CHECK(token_count(lifecycle, "install_type31_objective_capture") == 1U);
    CHECK(token_count(lifecycle, "quiesce_type31_objective_capture") == 1U);
    CHECK(token_count(lifecycle, "uninstall_type31_objective_capture") == 1U);

    constexpr std::string_view kAggregate =
        "quarantined_legacy_groups_have_ownership";
    CHECK(token_count(lifecycle, std::string{kAggregate}) == 2U);
    const std::string aggregateRegion = function_region(lifecycle, kAggregate);
    CHECK(token_count(aggregateRegion, "any_quarantined_legacy_ownership") == 1U);
    CHECK(token_count(aggregateRegion, "g_omegaPointApplyHandle") == 0U);
    CHECK(token_count(aggregateRegion, "g_omegaPointApplyOriginal") == 0U);
    CHECK(aggregateRegion.find("type31") == std::string::npos);
    for (const OwnerContract& owner : kOwners) {
        CHECK(token_count(aggregateRegion, std::string{owner.apiName}) == 1U);
        CHECK(token_count(lifecycle, std::string{owner.apiName}) == 1U);
    }

    const std::string closeAdmissionRegion = function_region(
        lifecycle, "void close_late_admission");
    const std::string unlockInstallRegion = function_region(
        lifecycle, "void unlock_install");
    const std::string installedRegion = function_region(
        lifecycle, "bool installed() const noexcept");
    const std::string acceptingRegion = function_region(
        lifecycle, "bool accepting() const noexcept");
    CHECK(closeAdmissionRegion.find(
              "g_acceptLateInstalls.store(false, std::memory_order_release)")
          != std::string::npos);
    CHECK(unlockInstallRegion.find("ReleaseSRWLockExclusive") != std::string::npos);
    CHECK(installedRegion.find("g_installed.load(std::memory_order_acquire)")
          != std::string::npos);
    CHECK(acceptingRegion.find("g_acceptLateInstalls.load(std::memory_order_acquire)")
          != std::string::npos);

    constexpr std::array<std::string_view, 5> kRetiredAttachedApis{
        "activity_selection_probe_attached",
        "player_broadcast_create_probe_attached",
        "activity_feature_flag_probe_attached",
        "activity_script_event_probe_attached",
        "activity_script_upstream_probe_attached",
    };
    for (const std::string_view api : kRetiredAttachedApis) {
        CHECK(token_count(aggregateRegion, std::string{api}) == 0U);
    }

    constexpr std::array<std::string_view, 9> kUnsafeLegacyUninstallers{
        "uninstall_activity_selection_probe",
        "uninstall_player_broadcast_create_probe",
        "uninstall_activity_feature_flag_probe",
        "uninstall_activity_script_event_probe",
        "uninstall_activity_script_upstream_probe",
        "uninstall_activity_notification_type1_apply_probe",
        "uninstall_activity_behavior_condition_probe",
        "uninstall_activity_schema_decode_probe",
        "uninstall_group_initial_update_decode_probe",
    };
    for (const std::string_view uninstaller : kUnsafeLegacyUninstallers) {
        CHECK(token_count(lifecycle, std::string{uninstaller}) == 0U);
    }

    const std::string installRegion = function_region(lifecycle, "bool install() noexcept");
    const std::size_t installQuarantine = installRegion.find("begin_install");
    CHECK(installQuarantine != std::string::npos);
    CHECK(installQuarantine < installRegion.find("g_freshInstallOwner = true"));
    CHECK(installQuarantine < installRegion.find("install_character_select_hold"));
    CHECK(installRegion.find("quarantined_owner_state") != std::string::npos);
    CHECK(installRegion.find("if (!quarantineClean)") != std::string::npos);
    CHECK(token_count(installRegion, std::string{kAggregate}) == 0U);

    const std::size_t anyFixBegin = installRegion.find("const bool anyFix =");
    const std::size_t anyFixEnd = installRegion.find(';', anyFixBegin);
    CHECK(anyFixBegin != std::string::npos);
    CHECK(anyFixEnd != std::string::npos);
    const std::string anyFixExpression =
        anyFixBegin != std::string::npos && anyFixEnd != std::string::npos
            ? installRegion.substr(anyFixBegin, anyFixEnd + 1U - anyFixBegin)
            : std::string{};
    CHECK(token_count(anyFixExpression, "activityHost") == 0U);
    CHECK(token_count(anyFixExpression, "activitySpawnerChain") == 0U);
    CHECK(token_count(anyFixExpression, "activitySelection") == 0U);
    CHECK(token_count(anyFixExpression, "activitySchemaDecode") == 0U);
    CHECK(token_count(anyFixExpression, "groupInitialUpdateDecode") == 0U);
    CHECK(token_count(anyFixExpression, "type31CaptureInstalled") == 1U);
    CHECK(token_count(anyFixExpression, "prologueFiller") == 1U);
    CHECK(installRegion.find("g_acceptLateInstalls.store(anyFix") != std::string::npos);
    CHECK(installRegion.find("g_installed.store(anyFix") != std::string::npos);

    const std::string uninstallRegion = function_region(lifecycle, "bool uninstall() noexcept");
    const std::size_t uninstallQuarantine = uninstallRegion.find("begin_uninstall");
    const std::size_t firstDetach = uninstallRegion.find("uninstall_spawn_hold");
    CHECK(uninstallQuarantine != std::string::npos);
    CHECK(firstDetach != std::string::npos);
    CHECK(uninstallQuarantine < firstDetach);
    CHECK(uninstallRegion.find("if (!quarantineClean)") != std::string::npos);
    CHECK(token_count(uninstallRegion, std::string{kAggregate}) == 0U);

    const std::string internal = read_text(bootflowRoot / "internal.h");
    CHECK(token_count(internal, "activity_schema_decode_legacy_bundle_has_ownership") == 1U);
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path bootflowRoot =
        argc > 1 ? std::filesystem::path{argv[1]}
                 : std::filesystem::path{"Dawn/src/client/hooks/bootflow"};
    production_primitive_truth_table();
    production_lifecycle_behavior_contract();
    production_dynamic_writer_behavior_contract();
    production_source_coverage(bootflowRoot);
    type31_exclusion_contract(bootflowRoot);
    dynamic_writer_source_contract(bootflowRoot);
    lifecycle_aggregate_contract(bootflowRoot);
    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " legacy-owner sentinel checks failed\n";
        return 1;
    }
    std::cout << "legacy-owner sentinel checks passed: 201 pairs, nine owners, "
                 "dirty lifecycle and dynamic writers verified, Type31 excluded\n";
    return 0;
}
