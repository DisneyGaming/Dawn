#pragma once

#include "definition.h"

namespace sunrise::server::bap::encrypted {

struct ServiceOutcome;

namespace transactions {

/**
 * Captures and validates any exact activity binding a successful commit will publish.
 * @param outcome Checked service result whose pending mutation is still intact.
 * @param publication Cleared first, then receives the exact expected publication.
 * @return True when the outcome is coherent, including one that publishes no binding.
 */
[[nodiscard]] bool prepare_publication(const ServiceOutcome& outcome,
                                       Publication& publication) noexcept;

/**
 * Commits at most one delayed State transaction.
 * @param outcome Checked service result whose pending transaction is used up.
 * @param publication Exact binding expectation captured before commit.
 * @return True when there is no mutation, or the one mutation commits.
 */
[[nodiscard]] bool commit(ServiceOutcome& outcome, Publication& publication) noexcept;

} // namespace transactions

} // namespace sunrise::server::bap::encrypted
