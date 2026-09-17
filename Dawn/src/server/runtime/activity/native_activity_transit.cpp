#include "native_activity_transit.h"

#include <array>
#include <mutex>

namespace dawn::server::runtime::activity::native_activity_transit {
namespace {

constexpr std::size_t kOwnerCapacity = 16;
constexpr std::uint64_t kMembershipIntervalMs = 250;

enum class RespawnPhase : std::uint8_t { pending,requesting,arrived,released };
struct Respawn final {
    membership::SpawnState host{};
    RespawnPhase phase{};
};

struct Slot final {
    membership_transit::Service service{};
    Owner owner{};
    std::uint64_t boot{};
    std::array<Destination, membership_transit::kDestinationCapacity> destinations{};
    std::size_t destinationCount{};
    std::array<std::uint64_t, membership_transit::kMemberCapacity> observedMembers{};
    std::size_t observedCount{};
    std::array<std::uint64_t, membership_transit::kMemberCapacity> cohortMembers{};
    std::array<std::uint64_t, membership_transit::kMemberCapacity> cohortRequests{};
    std::size_t cohortCount{};
    std::uint64_t cohortId{};
    std::uint32_t destinationId{};
    Destination respawnDestination{};
    std::uint64_t nextMembershipDue{};
    bool hasCohort{};
    bool membershipPending{};
    bool hasRespawnDestination{};
    bool respawning{};
    std::array<Respawn,membership_transit::kMemberCapacity> respawns{};
};

std::mutex mutex;
std::array<Slot, kOwnerCapacity> slots{};

[[nodiscard]] Slot* exact_slot(Owner owner, std::uint64_t boot) noexcept {
    for (auto& slot : slots) {
        if (slot.owner == owner && slot.boot == boot && slot.service.owner()) {
            return &slot;
        }
    }
    return nullptr;
}

[[nodiscard]] Slot* owner_slot(Owner owner) noexcept {
    for (auto& slot : slots) {
        if (slot.owner == owner && slot.service.owner()) {
            return &slot;
        }
    }
    return nullptr;
}

[[nodiscard]] const Slot* exact_slot_const(Owner owner, std::uint64_t boot) noexcept {
    for (const auto& slot : slots) {
        if (slot.owner == owner && slot.boot == boot && slot.service.owner()) {
            return &slot;
        }
    }
    return nullptr;
}

[[nodiscard]] bool same_destinations(const Slot& slot,
                                     std::span<const Destination> destinations) noexcept {
    if (slot.destinationCount != destinations.size()) {
        return false;
    }
    for (std::size_t index = 0; index < destinations.size(); ++index) {
        if (slot.destinations[index] != destinations[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Slot* empty_slot() noexcept {
    for (auto& slot : slots) {
        if (!slot.service.owner()) {
            return &slot;
        }
    }
    return nullptr;
}

void copy_destinations(Slot& slot, std::span<const Destination> destinations) noexcept {
    slot.destinationCount = destinations.size();
    for (std::size_t index = 0; index < destinations.size(); ++index) {
        slot.destinations[index] = destinations[index];
    }
}

[[nodiscard]] bool contains_member(const Slot& slot, std::uint64_t member) noexcept {
    for (std::size_t index = 0; index < slot.observedCount; ++index) {
        if (slot.observedMembers[index] == member) {
            return true;
        }
    }
    return false;
}

void record_member(Slot& slot, std::uint64_t member) noexcept {
    if (!member || contains_member(slot, member)
        || slot.observedCount >= slot.observedMembers.size()) {
        return;
    }
    slot.observedMembers[slot.observedCount++] = member;
}

[[nodiscard]] bool accepted_observation(membership_transit::ObservationResult result) noexcept {
    return result == membership_transit::ObservationResult::observed
        || result == membership_transit::ObservationResult::arrived
        || result == membership_transit::ObservationResult::released
        || result == membership_transit::ObservationResult::duplicate;
}

[[nodiscard]] bool exact_cohort_member(const Slot& slot, std::size_t index,
                                       membership_transit::Projection projection) noexcept {
    return projection.member == slot.cohortMembers[index]
        && projection.requestId == slot.cohortRequests[index];
}

[[nodiscard]] bool respawns_released(const Slot& slot) noexcept {
    for(std::size_t i=0;i<slot.cohortCount;++i)
        if(slot.respawns[i].phase!=RespawnPhase::released)return false;
    return slot.cohortCount!=0;
}

} // namespace

bool bind(Owner owner, std::uint64_t boot,
          std::span<const Destination> destinations) noexcept {
    if (!owner || boot == 0 || !membership_transit::Service::valid(destinations)) {
        return false;
    }
    const std::lock_guard lock(mutex);
    if (auto* existing = owner_slot(owner)) {
        return existing->boot == boot && same_destinations(*existing, destinations);
    }
    auto* slot = empty_slot();
    if (!slot || !slot->service.begin(owner, boot, destinations)) {
        return false;
    }
    slot->owner = owner;
    slot->boot = boot;
    copy_destinations(*slot, destinations);
    return true;
}

void release(Owner owner, std::uint64_t boot) noexcept {
    const std::lock_guard lock(mutex);
    if (auto* slot = exact_slot(owner, boot)) {
        *slot = {};
    }
}

bool request_all(Owner owner, std::uint64_t boot, std::uint64_t cohortId,
                std::uint32_t destinationId) noexcept {
    if (cohortId == 0) {
        return false;
    }
    const std::lock_guard lock(mutex);
    auto* live = exact_slot(owner, boot);
    if (!live) {
        return false;
    }
    if (live->hasCohort) {
        if (cohortId == live->cohortId && destinationId == live->destinationId) {
            return !live->respawning;
        }
        if (cohortId <= live->cohortId) {
            return false;
        }
    }
    if (live->observedCount == 0) {
        return false;
    }
    if(live->respawning && !respawns_released(*live))return false;

    // Admission happens entirely on a value copy. A failed member request therefore cannot leave
    // a partially requested live cohort or advance the wrapped service revision.
    Slot staged = *live;
    if(staged.respawning) {staged.hasRespawnDestination=false;staged.respawnDestination={};}
    staged.respawning=false;staged.respawns={};
    for (std::size_t index = 0; index < staged.observedCount; ++index) {
        const auto projection = staged.service.project(
            owner, boot, staged.observedMembers[index]);
        if (projection.phase != membership_transit::Phase::idle
            && projection.phase != membership_transit::Phase::released) {
            return false;
        }
        const membership_transit::Command command{
            owner, boot, staged.service.revision(), cohortId, destinationId,
            staged.observedMembers[index]};
        if (staged.service.request(command) != membership_transit::RequestResult::accepted) {
            return false;
        }
    }

    staged.cohortId = cohortId;
    staged.destinationId = destinationId;
    staged.cohortCount = staged.observedCount;
    for (std::size_t index = 0; index < staged.cohortCount; ++index) {
        staged.cohortMembers[index] = staged.observedMembers[index];
        staged.cohortRequests[index] = cohortId;
    }
    staged.hasCohort = true;
    staged.membershipPending = true;
    staged.nextMembershipDue = 0;
    *live = staged;
    return true;
}

bool request_respawn_all(Owner owner,std::uint64_t boot,std::uint64_t cohortId,
    std::uint32_t destinationId) noexcept {
    if(!cohortId)return false;
    const std::lock_guard lock(mutex);
    auto* slot=exact_slot(owner,boot);
    if(!slot || !slot->observedCount)return false;
    if(slot->hasCohort) {
        if(cohortId==slot->cohortId)
            return slot->respawning && destinationId==slot->destinationId;
        if(cohortId<slot->cohortId || (slot->respawning && !respawns_released(*slot)))return false;
    }
    const Destination* destination{};
    for(std::size_t i=0;i<slot->destinationCount;++i)
        if(slot->destinations[i].id==destinationId)destination=&slot->destinations[i];
    if(!destination)return false;
    for(std::size_t i=0;i<slot->observedCount;++i) {
        const auto phase=slot->service.project(owner,boot,slot->observedMembers[i]).phase;
        if(phase!=membership_transit::Phase::idle && phase!=membership_transit::Phase::released)return false;
    }
    slot->cohortId=cohortId;slot->destinationId=destinationId;
    slot->cohortCount=slot->observedCount;slot->cohortMembers=slot->observedMembers;
    slot->respawns={};slot->respawning=true;slot->hasCohort=true;
    slot->respawnDestination=*destination;slot->hasRespawnDestination=true;
    slot->membershipPending=true;slot->nextMembershipDue=0;
    return true;
}

bool project_respawn(Owner owner,std::uint64_t memberKey,membership::SpawnState local,
    std::int32_t actualRegion,membership::SpawnState& output) noexcept {
    output=local;
    const std::lock_guard lock(mutex);
    auto* slot=owner_slot(owner);
    if(!slot || !slot->respawning || actualRegion!=slot->respawnDestination.region)return false;
    for(std::size_t i=0;i<slot->cohortCount;++i) {
        if(slot->cohortMembers[i]!=memberKey)continue;
        auto& recovery=slot->respawns[i];
        const auto before=recovery.phase;
        // The same membership spawn exchange used by 1AU recovery:
        // host 1 -> local 2/4 -> host 4 -> local 0. The reward environment and
        // spawn override are already published; no mission checkpoint is reset.
        if(recovery.phase==RespawnPhase::pending && local.state==0) {
            recovery.host={1,static_cast<std::uint8_t>(local.opaqueByte+1U),0};
            recovery.phase=RespawnPhase::requesting;
        } else if(local.opaqueByte==recovery.host.opaqueByte) {
            if(recovery.phase==RespawnPhase::requesting && local.state==4) {
                recovery.host.state=4;recovery.phase=RespawnPhase::arrived;
            } else if(recovery.phase==RespawnPhase::arrived && local.state==0) {
                recovery.host.state=0;recovery.phase=RespawnPhase::released;
            }
        }
        if(before!=recovery.phase) {slot->membershipPending=true;slot->nextMembershipDue=0;}
        if(recovery.phase==RespawnPhase::pending || recovery.phase==RespawnPhase::released)return false;
        output=recovery.host;return true;
    }
    return false;
}

Status snapshot(Owner owner, std::uint64_t boot, std::uint64_t cohortId) noexcept {
    const std::lock_guard lock(mutex);
    const auto* slot = exact_slot_const(owner, boot);
    if (!slot) {
        return {};
    }
    Status result{};
    result.bound = true;
    if (!slot->hasCohort || cohortId != slot->cohortId) {
        return result;
    }
    result.requested = true;
    result.members = slot->cohortCount;
    result.arrived = result.members != 0;
    result.released = result.members != 0;
    if(slot->respawning) {
        for(std::size_t i=0;i<slot->cohortCount;++i) {
            const auto phase=slot->respawns[i].phase;
            result.arrived &= phase==RespawnPhase::arrived || phase==RespawnPhase::released;
            result.released &= phase==RespawnPhase::released;
        }
        return result;
    }
    for (std::size_t index = 0; index < slot->cohortCount; ++index) {
        const auto projection = slot->service.project(
            owner, boot, slot->cohortMembers[index]);
        const bool exact = exact_cohort_member(*slot, index, projection);
        result.arrived = result.arrived && exact
            && projection.phase == membership_transit::Phase::arrived
            && projection.host.state == membership_transit::kArrivedState
            && projection.destination.region == projection.host.sliceSetIndex;
        result.released = result.released && exact
            && projection.phase == membership_transit::Phase::released
            && projection.host.state == 0;
    }
    return result;
}

bool set_respawn_destination(Owner owner,std::uint64_t boot,std::uint32_t destinationId) noexcept {
    const std::lock_guard lock(mutex);
    auto* slot=exact_slot(owner,boot);
    if(!slot || !destinationId)return false;
    for(std::size_t i=0;i<slot->destinationCount;++i) {
        if(slot->destinations[i].id!=destinationId)continue;
        slot->respawnDestination=slot->destinations[i];slot->hasRespawnDestination=true;return true;
    }
    return false;
}

bool destination_bound(Owner owner,std::uint64_t boot,std::uint32_t destinationId) noexcept {
    const std::lock_guard lock(mutex);
    const auto* slot=exact_slot_const(owner,boot);
    if(!slot || !destinationId)return false;
    for(std::size_t i=0;i<slot->destinationCount;++i)
        if(slot->destinations[i].id==destinationId)return true;
    return false;
}

void clear_respawn_destination(Owner owner,std::uint64_t boot) noexcept {
    const std::lock_guard lock(mutex);
    if(auto* slot=exact_slot(owner,boot)) {
        slot->respawnDestination={};slot->hasRespawnDestination=false;
    }
}

bool respawn_latched(Owner owner) noexcept {
    const std::lock_guard lock(mutex);
    const auto* slot=owner_slot(owner);
    return slot && slot->hasRespawnDestination;
}

bool spawn_destination(Owner owner, Destination& output) noexcept {
    output = {};
    const std::lock_guard lock(mutex);
    const auto* slot = owner_slot(owner);
    if (!slot) return false;
    if(slot->hasRespawnDestination) { output=slot->respawnDestination;return true; }
    if (!slot->hasCohort || !slot->cohortCount) return false;
    Destination destination{};
    for (std::size_t index = 0; index < slot->cohortCount; ++index) {
        const auto projection = slot->service.project(owner, slot->boot, slot->cohortMembers[index]);
        if (!exact_cohort_member(*slot, index, projection)
            || projection.destination.id != slot->destinationId) return false;
        if (index && projection.destination != destination) return false;
        destination = projection.destination;
    }
    output = destination;
    return true;
}

membership_transit::Projection project(Owner owner, std::uint64_t memberKey,
                                        bool hasTeleportReceipt,
                                        membership::TeleportState local,
                                        std::int32_t actualRegion) noexcept {
    const std::lock_guard lock(mutex);
    auto* slot = owner_slot(owner);
    if (!slot || slot->respawning || !memberKey || memberKey == membership::kInvalidOpaqueSoid) {
        return {};
    }
    const auto prior = slot->service.project(owner, slot->boot, memberKey);
    // Native message 22 may omit the untouched idle tuple until the first host
    // command (the same bootstrap contract used by omega_ending_transit).
    // Admit only that exact default from an existing membership/region snapshot;
    // arrivals and releases still require an explicit, matching native receipt.
    const bool initialIdle = !prior.requestId && !hasTeleportReceipt
        && local.state == 0 && local.token == 0
        && local.sliceSetIndex == state::activity::membership::kAbsentSliceSetIndex
        && local.sliceSetHash == 0;
    if ((hasTeleportReceipt || initialIdle) && actualRegion >= 0) {
        const membership_transit::Receipt receipt{
            owner, slot->boot, prior.present ? prior.requestId : 0, memberKey,
            local, actualRegion};
        const auto result = slot->service.observe(receipt);
        if (accepted_observation(result)) {
            record_member(*slot, memberKey);
            if (result == membership_transit::ObservationResult::arrived
                || result == membership_transit::ObservationResult::released) {
                slot->membershipPending = true;
                slot->nextMembershipDue = 0;
            }
        }
    }
    return slot->service.project(owner, slot->boot, memberKey);
}

bool membership_due(Owner owner, std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    const auto* slot = owner_slot(owner);
    if (!slot) {
        return false;
    }
    // Binding can follow the initial roster. Refresh it to discover the real
    // member before request_all freezes a cohort; otherwise each waits on the
    // other forever. Stop bootstrap refreshes once membership is known.
    if (!slot->hasCohort || slot->cohortCount == 0) {
        return slot->observedCount == 0 && now >= slot->nextMembershipDue;
    }
    if (slot->membershipPending) {
        return true;
    }
    if(slot->respawning)return !respawns_released(*slot) && now>=slot->nextMembershipDue;
    bool active = false;
    for (std::size_t index = 0; index < slot->cohortCount; ++index) {
        const auto projection = slot->service.project(
            owner, slot->boot, slot->cohortMembers[index]);
        if (projection.requestId != slot->cohortRequests[index]
            || projection.phase != membership_transit::Phase::released) {
            active = true;
        }
    }
    return active && now >= slot->nextMembershipDue;
}

void note_membership_published(Owner owner, std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    auto* slot = owner_slot(owner);
    if (!slot) {
        return;
    }
    slot->membershipPending = false;
    slot->nextMembershipDue = now + kMembershipIntervalMs;
}

std::size_t observed_member_count(Owner owner,std::uint64_t boot) noexcept {
    const std::lock_guard lock(mutex);
    const auto* slot=exact_slot_const(owner,boot);
    return slot?slot->observedCount:0;
}

} // namespace dawn::server::runtime::activity::native_activity_transit
