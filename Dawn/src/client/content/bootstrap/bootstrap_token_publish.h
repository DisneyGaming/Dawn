#pragma once

namespace dawn::client::content::bootstrap {

/** Publishes the installed client's bootstrap content-id token into State. */
[[nodiscard]] bool publish_token() noexcept;

} // namespace dawn::client::content::bootstrap
