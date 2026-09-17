#include "activity_global_state_push.h"
#include "../../../../../state/activity/strike_bond/runtime.h"

#include <Windows.h>

#include <algorithm>
#include <string_view>

#include "../../../../../middleware/bap/activity_message/activity_global_state_encoder.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../state/activity/defaults/activity_defaults_snapshot.h"
#include "../../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../../state/activity/destination/activity_destination_spawn_binding.h"
#include "../../../../../state/build_data/runtime.h"
#include "../../../../../state/activity/runtime.h"
#include "../../../../../state/activity/omega_ending.h"
#include "../../../../../state/activity/omega_presentation.h"
#include "activity_arrival.h"
#include "activity_notification_frame.h"

namespace dawn::server::bap::encrypted::push::activity {

namespace message = middleware::bap::activity_message::global_activity_state;

namespace {

/** The byte a bubble carries when its first slice-set state is enabled. */
constexpr std::uint8_t kBubbleEnabledByte = 0x80;

/**
 * Copies one destination name into the fixed message field.
 * @param selection Destination carrying the package name.
 * @param state Receives the name and its length.
 */
void copy_name(const state::activity::destination::DestinationSelection& selection,
               message::GlobalActivityState& state) noexcept {
    // The last element is reserved for the null, so a full-width name loses its last byte instead
    // of failing the encode.
    const std::size_t length = selection.packageNameLength < message::kNameCapacity
                                   ? selection.packageNameLength
                                   : message::kNameCapacity - 1;
    for (std::size_t index = 0; index < length; ++index) {
        state.name[index] = static_cast<char>(selection.packageName[index]);
    }
    state.nameLength = length;
}

} // namespace

/** Builds the whole message body input for one session. */
[[nodiscard]] bool
resolve_state_for_selection(state::activity::ActivityInstanceKey activity,
              message::GlobalActivityState& output,
              const state::activity::destination::DestinationSelection& selection) noexcept {
    output = {};
    state::activity::defaults::ActivityDefaults defaults{};
    state::activity::defaults::snapshot(defaults);
    const state::activity::defaults::FallbackPolicy& fallback =
        defaults.defaultDestination.fallback;

    copy_name(selection, output);
    // The descriptor view points into caller storage that outlives the encode.
    output.descriptorBits = std::span<const std::byte>(selection.descriptorBits);
    output.descriptorBitLength = selection.descriptorBitLength;
    output.reason = selection.reason;
    output.fromActivityIndex = selection.previousActivityIndex;
    output.activityIndex = selection.activityIndex;
    output.spawnSetHash =
        state::activity::destination::attachable_spawn_set_hash(selection, fallback.spawnSetHash);

    // The extracted layout wins where the packages carry one. The count and the output array must
    // come from the same source: a count from one and states from another is how uniform values
    // reach the wire and look as though they worked.
    const std::string_view name(output.name.data(), output.nameLength);
    ::dawn::state::build_data::scenarios::Definition layout{};
    if (::dawn::state::build_data::find_scenario_layout(name, layout)) {
        output.bubbleCount = layout.bubbleCount;
        std::copy(
            layout.bubbleStates.begin(), layout.bubbleStates.end(), output.bubbleStates.begin());
        output.hasSliceSet = true;
        output.sliceSetIndex =
            arrival_slice_set(defaults.defaultDestination, selection, name, layout);
        if(name=="mission_bond" && layout.tag==0x80F47445U
            && activity==state::activity::newest_joined_activity()) {
            const auto garden=state::activity::strike_bond::request();
            if(garden.frame.campaign && garden.frame.endingFlow.retired && output.bubbleCount>3) {
                output.bubbleStates[3]=0x81;output.sliceSetIndex=25;
            }
        }
        if(name=="mission_scot" && layout.tag==0x80F47522U
            && activity==state::activity::newest_joined_activity()
            && state::activity::mission_seed_armed()) {
            const auto nav=state::activity::omega_presentation::navigation();
            const auto ending=state::activity::omega_ending::authority(nav.run,GetTickCount64());
            if(nav.enabled && ending.bookendState && ending.token.valid()
                && output.bubbleCount>state::activity::omega_ending::kBubble) {
                // The alternate state loads its authored bookend placement. Transport
                // is a separate native transition; changing the initial slice is not a hop.
                output.bubbleStates[state::activity::omega_ending::kBubble]=0x81U;
                output.sliceSetIndex=state::activity::omega_ending::kSlice;
            }
        }
        return true;
    }
    output.bubbleCount = fallback.bubbleCount;
    for (std::size_t index = 0; index < message::kBubbleStateCount; ++index) {
        const bool stateful = (fallback.statefulBubbleMask >> index & 1U) != 0;
        output.bubbleStates[index] = stateful ? kBubbleEnabledByte : message::kBubbleStateNone;
    }
    output.hasSliceSet = true;
    // No layout means no bubble array to derive an arrival from, so the authored index stands.
    output.sliceSetIndex = fallback.initialSliceSet;
    return true;
}

/** Appends one global-activity-state svc9 notification and advances its local nonce. */
bool append_global_state_notification(Scratch& scratch,
                                      state::activity::ActivityInstanceKey activity,
                                      const state::activity::destination::DestinationSelection& selection,
                                      std::span<const std::byte, state::kAesKeySize> key,
                                      std::array<std::byte, state::kBapNonceSize>& nonce,
                                      std::span<std::byte> response,
                                      std::size_t& written) noexcept {
    message::GlobalActivityState body{};
    if (!static_cast<bool>(activity) || written > response.size()
        || !resolve_state_for_selection(activity, body, selection)) {
        return false;
    }

    const std::size_t initialWritten = written;
    auto initialNonce = nonce;
    std::size_t messageSize = 0;
    const bool encoded =
        message::encode_global_activity_state(body, scratch.responseBody, messageSize)
        && append_notification_frame(scratch,
                                     activity.sessionId,
                                     message::kMessageType,
                                     std::span(scratch.responseBody).first(messageSize),
                                     key,
                                     nonce,
                                     response,
                                     written);
    // A replayed descriptor makes the body wider than the minimum encoding, so the clear uses the
    // size actually produced, not that minimum.
    SecureZeroMemory(scratch.responseBody.data(), messageSize);
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
    } else {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
    }
    SecureZeroMemory(&initialNonce, sizeof initialNonce);
    SecureZeroMemory(&body, sizeof body);
    return encoded;
}

bool resolve_state(state::activity::ActivityInstanceKey activity,
                   message::GlobalActivityState& output,
                   state::activity::destination::DestinationSelection& selection) noexcept {
    output={};
    return state::activity::destination::snapshot(activity,selection)
        && resolve_state_for_selection(activity,output,selection);
}

bool append_global_state_notification(Scratch& scratch,
                                      state::activity::ActivityInstanceKey activity,
                                      std::span<const std::byte,state::kAesKeySize> key,
                                      std::array<std::byte,state::kBapNonceSize>& nonce,
                                      std::span<std::byte> response,std::size_t& written) noexcept {
    state::activity::destination::DestinationSelection selection{};
    return state::activity::destination::snapshot(activity,selection)
        && append_global_state_notification(scratch,activity,selection,key,nonce,response,written);
}

} // namespace dawn::server::bap::encrypted::push::activity
