#include <Windows.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string_view>

#include "core/logging/log.h"
#include "middleware/bap/activity_message/incident.h"
#include "middleware/bap/activity_message/loot_pickup.h"
#include "server/bap/encrypted/activity_message/festival_pickups.h"
#include "server/bap/encrypted/queuez/queuez_outcome_staging.h"
#include "state/activity/destination/activity_destination_snapshot.h"
#include "state/activity/events/activity_event_selection.h"
#include "state/build_data/collectibles/collectible_catalog.h"
#include "state/runtime/runtime.h"

namespace stubs {
std::uint64_t accountSoid{};
bool hidden{};
bool tower{true};
bool activityLive{true};
bool encode{true};
bool commit{true};
std::uint64_t character{};
std::size_t commits{};
}

namespace sunrise::core::log {
void write(Channel, Level, std::string_view) noexcept {}
}

namespace sunrise::state {
AccountState account_snapshot() noexcept {
    AccountState value{};
    value.primarySoid = stubs::accountSoid;
    return value;
}
const BapState& bap() noexcept {
    static BapState value{};
    return value;
}
namespace account {
std::uint64_t selected_character_soid(const AccountState&) noexcept { return stubs::character; }
}
bool prepare_profile_item_acquisition(
    std::uint16_t collectible,
    std::uint32_t definition,
    PendingProfileItemAcquisition& mutation,
    std::span<const build_data::material_requirements::Requirement> cost) noexcept {
    if (collectible != build_data::collectibles::kNoCollectibleIndex || !cost.empty()) return false;
    mutation = {};
    mutation.accountSoid = stubs::accountSoid;
    mutation.acquiredDefinitionHash = definition;
    mutation.prepared = true;
    return true;
}
bool commit_profile_item_acquisition(PendingProfileItemAcquisition& mutation) noexcept {
    if (!stubs::commit || !mutation.prepared) return false;
    mutation = {};
    ++stubs::commits;
    return true;
}
namespace activity {
bool contains(ActivityInstanceKey) noexcept { return stubs::activityLive; }
namespace destination {
bool snapshot(ActivityInstanceKey, DestinationSelection& output) noexcept {
    output = {};
    if (!stubs::tower) return false;
    constexpr std::string_view name = "city_tower_social_d2";
    output.packageNameLength = static_cast<std::uint8_t>(name.size());
    for (std::size_t i = 0; i < name.size(); ++i) {
        output.packageName[i] = static_cast<std::int8_t>(name[i]);
    }
    return true;
}
}
namespace events {
bool withheld(std::uint32_t) noexcept { return stubs::hidden; }
}
}
}

namespace sunrise::server::bap::encrypted::queuez {
bool stage_profile_item_acquisition(const SessionState&, std::uint64_t, std::uint64_t,
                                    bool, bool, ProfileItemAcquisition&) noexcept { return true; }
bool stage_service_outcome(Scratch&, const SessionState& before, const ServiceOutcome&,
                           std::span<const std::byte, state::kAesKeySize>,
                           std::array<std::byte, state::kBapNonceSize>& nonce,
                           std::span<std::byte> response, std::size_t& written,
                           StagedPublication& publication) noexcept {
    if (!stubs::encode || response.empty()) return false;
    response[0] = std::byte{0x42};
    written = 1;
    nonce[0] = static_cast<std::byte>(std::to_integer<unsigned>(nonce[0]) + 1U);
    publication = {};
    publication.after = before;
    ++publication.after.family4Version;
    publication.hasState = true;
    return true;
}
}

namespace {
namespace incident = sunrise::middleware::bap::activity_message::incident;
namespace loot = sunrise::middleware::bap::activity_message::loot_pickup;
namespace pickups = sunrise::server::bap::encrypted::festival_pickups;

class Bits final {
public:
    explicit Bits(std::span<std::byte> bytes) : bytes_(bytes) {}
    void put(std::uint64_t value, std::size_t width) {
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t bit = used_ + i;
            const std::byte mask = static_cast<std::byte>(1U << (7U - bit % 8U));
            if ((value >> (width - i - 1U)) & 1U) bytes_[bit / 8U] |= mask;
        }
        used_ += width;
    }
private:
    std::span<std::byte> bytes_;
    std::size_t used_{};
};

std::array<std::byte, 80> make_pickup(std::uint32_t source, std::int32_t bubble,
                                      std::uint32_t nonce, std::uint64_t account,
                                      std::uint64_t character) {
    std::array<std::byte, 80> bytes{};
    Bits bits(bytes);
    bits.put(nonce, 32); bits.put(2, 3); bits.put(character, 64); bits.put(18, 6); bits.put(0, 5);
    bits.put(0x811C9DC5U, 32); bits.put(0x811C9DC5U, 32); bits.put(0x811C9DC5U, 32);
    bits.put(character, 64); bits.put(3, 6); bits.put(account, 64); bits.put(1, 3); bits.put(1, 2);
    bits.put(character, 64); bits.put(0x80000001U, 32); bits.put(source, 32);
    bits.put(std::bit_cast<std::uint32_t>(13.5F), 32);
    bits.put(std::bit_cast<std::uint32_t>(-2.25F), 32);
    bits.put(std::bit_cast<std::uint32_t>(7.0F), 32);
    bits.put(static_cast<std::uint32_t>(static_cast<std::int64_t>(bubble) + 0x80000000LL), 32);
    bits.put(0x811C9DC5U, 32); bits.put(0, 7);
    return bytes;
}

std::array<std::byte, 84> make_incident(std::span<const std::byte, 80> pickup) {
    std::array<std::byte, 84> bytes{};
    Bits bits(bytes);
    bits.put(loot::kIncidentTarget, incident::kTargetWidth);
    bits.put(0, incident::kExtraCountWidth);
    bits.put(0, incident::kSelectorPresenceWidth);
    bits.put(0, incident::kOptionalPresenceWidth);
    bits.put(80, incident::kPayloadLengthWidth);
    for (const std::byte value : pickup) bits.put(std::to_integer<std::uint8_t>(value), 8);
    return bytes;
}

sunrise::middleware::bap::activity_message::Request request_for(std::span<const std::byte> body,
                                                                  std::uint64_t sessionId) {
    sunrise::middleware::bap::activity_message::Request request{};
    request.accountHandle = sessionId;
    request.messageType = incident::kMessageType;
    request.payload = body;
    return request;
}

std::unique_ptr<sunrise::server::bap::Session> live_session(std::uint64_t sessionId,
                                                             std::uint64_t character) {
    using namespace sunrise::state::activity;
    auto session = std::make_unique<sunrise::server::bap::Session>();
    session->authenticated = true;
    session->connectionKey = {17, ConnectionGeneration{1}};
    session->authenticationClock = AuthenticationGeneration{1};
    session->authenticationKey = {session->connectionKey, AuthenticationGeneration{1}};
    session->activityBindingClock = BindingGeneration{1};
    session->activity.key = {session->authenticationKey, BindingGeneration{1}};
    session->activity.instance = {sessionId, ActivityIncarnation{1}};
    session->activity.characterSoid = character;
    session->queuez.family4Active = true;
    session->queuez.family4RootSoid = stubs::accountSoid;
    return session;
}

void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}
}

int main() {
    constexpr std::uint64_t account = 0x1000000000000001ULL;
    constexpr std::uint64_t character = 0x2000000000000001ULL;
    stubs::accountSoid = account;
    stubs::character = character;

    auto pickup = make_pickup(0xE86DC710U, 6, 0xAABBCCDDU, account, character);
    loot::Pickup parsed{};
    if (!loot::parse(pickup, parsed) || parsed.sourceHash != 0xE86DC710U
        || parsed.bubble != 6 || parsed.position[0] != 13.5F) fail("synthetic parser");
    for (std::size_t length = 0; length < pickup.size(); ++length) {
        if (loot::parse(std::span(pickup).first(length), parsed)) fail("truncated parser input");
    }
    auto session = live_session(44, character);
    auto scratch = std::make_unique<sunrise::server::bap::Scratch>();
    std::array<std::byte, 128> response{};
    bool touched = false;
    std::size_t written = 0;
    auto body = make_incident(pickup);
    auto request = request_for(body, 44);
    stubs::hidden = true;
    pickups::receive(*session, request);
    if (pickups::consume(*session, *scratch, response, written, touched)) fail("hidden event reward");
    stubs::hidden = false;

    pickups::receive(*session, request);
    pickups::receive(*session, request);
    if (!pickups::consume(*session, *scratch, response, written, touched) || stubs::commits != 1)
        fail("first deduplicated unit");
    for (std::size_t unit = 1; unit < 50; ++unit) {
        if (!pickups::consume(*session, *scratch, response, written, touched)) fail("payout units");
    }
    if (stubs::commits != 50 || pickups::consume(*session, *scratch, response, written, touched))
        fail("placed-source deduplication");

    auto wrong = make_incident(make_pickup(0xE86DC710U, 1, 2, account, character));
    pickups::receive(*session, request_for(wrong, 44));
    if (pickups::consume(*session, *scratch, response, written, touched)) fail("wrong source scope");

    auto retry = make_incident(make_pickup(0xB6D6DC5AU, 1, 3, account, character));
    pickups::receive(*session, request_for(retry, 44));
    stubs::encode = false;
    if (pickups::consume(*session, *scratch, response, written, touched)) fail("failed queuez accepted");
    stubs::encode = true;
    Sleep(1010);
    if (!pickups::consume(*session, *scratch, response, written, touched)) fail("pending retry lost");
    std::puts("PASS: synthetic parser, source scope, visibility, dedupe, and retry claim contracts");
    return 0;
}
