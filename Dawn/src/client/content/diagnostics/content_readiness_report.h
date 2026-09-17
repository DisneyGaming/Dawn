#pragma once

namespace dawn::client::content::diagnostics {

/** Reports build-data domain readiness, once per change. */
void report_readiness() noexcept;

} // namespace dawn::client::content::diagnostics
