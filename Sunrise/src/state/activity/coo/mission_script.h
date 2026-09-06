#pragma once
#include "script_views.h"
#include <filesystem>
#include <memory>
#include <string>

namespace sunrise::state::activity::coo::script {
// Trusted native capabilities. A document selects a registered profile; it
// cannot define executable code, invent a wire schema, or add native authority.
struct Capability final { std::string_view id,domain; CommandSpec spec{}; };
struct ModuleCapability final { std::string_view id; ModuleBinding binding{}; };
struct FactCapability final { std::string_view id; std::uint8_t fact{}; };
struct EventCapability final { std::string_view set,id; std::uint32_t event{}; std::uint8_t allowedCycles{}; };
struct Profile final {
    std::string_view id,schemaName;
    Schema schema{};
    std::span<const Capability> capabilities;
    std::span<const ModuleCapability> modules;
    std::span<const FactCapability> facts;
    DialogueDefinition dialogue{};
    std::span<const std::uint32_t> objectives;
    std::span<const EventCapability> events;
    std::span<const PresentationTable> tables;
};
// All spans in views() refer to storage owned by this immutable document.
// Parsing is transactional and never publishes, executes, or loads native code.
class MissionDocument final {
public:
    ~MissionDocument();
    MissionDocument(const MissionDocument&)=delete;
    MissionDocument& operator=(const MissionDocument&)=delete;
    [[nodiscard]] static std::unique_ptr<MissionDocument> parse(std::string_view text,const Profile& profile,std::string& error) noexcept;
    [[nodiscard]] static std::unique_ptr<MissionDocument> read(const std::filesystem::path& path,const Profile& profile,std::string& error) noexcept;
    [[nodiscard]] const Views& views() const noexcept;
    [[nodiscard]] std::uint64_t fingerprint() const noexcept;
private:
    MissionDocument();
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
} // namespace sunrise::state::activity::coo::script
