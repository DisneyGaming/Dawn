#pragma once
#include "omega_script_views.h"
#include "mission_script.h"
#include <filesystem>
#include <memory>
#include <string>

namespace sunrise::state::activity::coo::script {
[[nodiscard]] const Profile& omega_profile();
// Parsing does not publish. The caller can inspect/validate a candidate without
// affecting the running mission. activate() succeeds only once per process.
class Document final {
public:
    ~Document();
    Document(const Document&)=delete;
    Document& operator=(const Document&)=delete;
    [[nodiscard]] static std::unique_ptr<Document> parse_lua(std::string_view text,std::string& error) noexcept;
    [[nodiscard]] static std::unique_ptr<Document> read(const std::filesystem::path& path,std::string& error) noexcept;
    [[nodiscard]] const Views& views() const noexcept;
    [[nodiscard]] bool activate() const noexcept { return publish(views()); }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept;
private:
    Document();
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
} // namespace sunrise::state::activity::coo::script
