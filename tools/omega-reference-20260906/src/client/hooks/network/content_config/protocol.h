#pragma once

#include <string_view>

namespace dawn::client::hooks::network::content_config {

/** Process-local URL accepted by the ContentConfig GET replacement. */
inline constexpr std::string_view kLocalUrl = "dawn://local/config";

} // namespace dawn::client::hooks::network::content_config
