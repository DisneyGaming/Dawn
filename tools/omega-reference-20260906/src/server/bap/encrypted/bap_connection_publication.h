#pragma once

#include <cstdint>

#include "../internal.h"
#include "internal.h"
#include "queuez/queuez_state_validation.h"
#include "transactions/definition.h"

namespace dawn::server::bap::encrypted {

/** Connection fields one request may publish, captured before its transaction commits. */
struct ConnectionFields {
    middleware::bap::activity_message::patch_epoch::PatchEpoch patchEpoch{};
    /** The join carries the only member key the client ever sends. */
    std::uint64_t joinMemberKey{};
    /** The join also names the character the player signed in on. */
    std::uint64_t joinCharacterSoid{};
    bool retainsPatchEpoch{};
    /** Set by a join or a transition-token change, which are the client starting a load. */
    bool opensTransitionWindow{};
    /** Set by a join alone, which re-arms the roster warm-up the new container needs. */
    bool joinsActivity{};
    /** This committed plan came from the client's type-22 authoritative activity delta. */
    bool clientAuthoritative{};
    /** The type-22 delta carried the client's native teleport-state branch. */
    bool teleportPresent{};
    /** The type-22 delta moved the client to a different authored region. */
    bool regionMoved{};
};

/**
 * Captures the connection fields one service outcome carries.
 * @param outcome Prepared outcome, still holding its uncommitted mutations.
 * @return The fields to publish once the transaction commits.
 */
[[nodiscard]] ConnectionFields connection_fields(const ServiceOutcome& outcome) noexcept;

/**
 * Publishes the captured connection fields after a successful commit.
 * @param session Connection-owned activity binding and epoch.
 * @param stagedBinding Exact next binding key, absent when no binding is published.
 * @param publication Committed State bindings.
 * @param fields Fields captured before the commit.
 */
[[nodiscard]] bool can_publish_connection_fields(
    const Session& session,
    state::activity::BindingKey stagedBinding,
    const transactions::Publication& publication) noexcept;

[[nodiscard]] bool publish_connection_fields(
    Session& session,
    state::activity::BindingKey stagedBinding,
    const transactions::Publication& publication,
    const ConnectionFields& fields) noexcept;

/**
 * Arms the owed Family-4 and banner re-pushes when the queuez publication asks for them.
 * @param session Connection-owned re-push timers.
 * @param queuezPublication Staged queuez publication.
 */
void arm_repushes(Session& session, const queuez::StagedPublication& queuezPublication) noexcept;

} // namespace dawn::server::bap::encrypted
