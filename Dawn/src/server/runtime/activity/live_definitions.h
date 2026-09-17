#pragma once

#include "../../../state/activity/coo/mission_script.h"
#include <memory>
#include <mutex>

namespace dawn::server::runtime::activity {

using ActivityDocument = state::activity::coo::script::MissionDocument;
using DocumentPtr = std::shared_ptr<const ActivityDocument>;

struct ActivityOwner final {
    std::uint64_t instance{}, run{};
    std::uint32_t bubble{};
    [[nodiscard]] bool valid() const noexcept { return instance!=0 && run!=0 && bubble<64; }
    friend bool operator==(const ActivityOwner&,const ActivityOwner&)=default;
};
struct DefinitionRevision final {
    ActivityOwner owner{};
    std::uint64_t number{};
    friend bool operator==(const DefinitionRevision&,const DefinitionRevision&)=default;
};
// Consumers retain this lease for the entire lifetime of an issued native operation.
// An executor retains its graph lease across policy edits: changing policy does not
// replace the Definition pointer pinned by Executor::start().
struct DefinitionLease final {
    DefinitionRevision revision{};
    DocumentPtr document;
    [[nodiscard]] explicit operator bool() const noexcept { return document!=nullptr; }
};
enum class Change : std::uint8_t { unchanged, futureRequests, restartRequired, incompatible };
enum class EditResult : std::uint8_t { accepted, stale, busy, invalid, duplicate, restartRequired, unchanged, empty };
struct StagedRevision final {
    DefinitionRevision expected{};
    std::uint64_t serial{};
    friend bool operator==(const StagedRevision&,const StagedRevision&)=default;
};
struct StageResult final { EditResult result{EditResult::invalid}; Change change{Change::incompatible}; StagedRevision token{}; };
struct DefinitionStatus final {
    DefinitionRevision active{};
    std::uint64_t fingerprint{}, stagedFingerprint{}, lastRequest{};
    Change stagedChange{Change::unchanged};
    bool queued{};
};

// The server activity owner alone calls begin(), apply_pending(), and end(). UI/file
// tooling may stage validated documents and enqueue apply requests. No native work,
// file IO, middleware encoding, or callbacks run while this mailbox's lock is held.
// Keep this owning object outside the memcpy/secure-wipe session structures.
class LiveDefinitions final {
public:
    [[nodiscard]] bool begin(ActivityOwner owner,DocumentPtr document) {
        std::lock_guard lock(mutex_);
        if(active_ || !owner.valid() || !document || !document->views().valid || serial_==UINT64_MAX) { return false; }
        active_={{owner,++serial_},std::move(document)};
        return true;
    }
    [[nodiscard]] DefinitionLease acquire() const {
        std::lock_guard lock(mutex_);return active_;
    }
    [[nodiscard]] DefinitionLease previous() const {
        std::lock_guard lock(mutex_);return previous_;
    }
    [[nodiscard]] static Change classify(const ActivityDocument& active,const ActivityDocument& candidate) noexcept {
        const auto& a=active.views();const auto& b=candidate.views();
        if(!a.valid || !b.valid || a.missionId!=b.missionId || a.profileId!=b.profileId
            || a.mission.sequence.schema!=b.mission.sequence.schema) { return Change::incompatible; }
        if(!active.same_structure(candidate) || a.parameters.size()!=b.parameters.size()) { return Change::restartRequired; }
        bool different{};
        for(const auto& before:a.parameters) {
            const auto* after=b.parameter(before.id);
            if(!after || before.liveEditable!=after->liveEditable) { return Change::restartRequired; }
            if(before.value!=after->value) {
                if(!before.liveEditable) { return Change::restartRequired; }
                different=true;
            }
        }
        return different?Change::futureRequests:Change::unchanged;
    }
    [[nodiscard]] StageResult stage(DefinitionRevision expected,DocumentPtr candidate) {
        std::lock_guard lock(mutex_);
        if(!active_ || expected!=active_.revision) { return {EditResult::stale}; }
        if(queued_) { return {EditResult::busy}; }
        if(!candidate || !candidate->views().valid || stageSerial_==UINT64_MAX) { return {EditResult::invalid}; }
        const auto change=classify(*active_.document,*candidate);
        if(change==Change::incompatible) { return {EditResult::invalid,change}; }
        staged_=std::move(candidate);stagedChange_=change;stagedToken_={expected,++stageSerial_};
        return {EditResult::accepted,change,stagedToken_};
    }
    [[nodiscard]] EditResult request_apply(StagedRevision token,std::uint64_t request) {
        std::lock_guard lock(mutex_);
        if(request==0) { return EditResult::invalid; }
        if(request<=lastRequest_) { return EditResult::duplicate; }
        if(queued_) { return EditResult::busy; }
        if(!active_ || !staged_ || token!=stagedToken_ || token.expected!=active_.revision) { return EditResult::stale; }
        if(stagedChange_==Change::restartRequired) { return EditResult::restartRequired; }
        if(stagedChange_==Change::unchanged) { return EditResult::unchanged; }
        lastRequest_=request;queued_=true;return EditResult::accepted;
    }
    // Call at the server update boundary, before issuing NEW native operations.
    // Never reissue prior commands or synthesize readiness during this operation.
    [[nodiscard]] EditResult apply_pending(DefinitionRevision expected) {
        std::lock_guard lock(mutex_);
        if(!active_ || expected!=active_.revision) { return EditResult::stale; }
        if(!queued_) { return EditResult::empty; }
        if(serial_==UINT64_MAX) { return EditResult::invalid; }
        previous_=active_;
        active_={{expected.owner,++serial_},std::move(staged_)};
        queued_=false;stagedToken_={};stagedChange_=Change::unchanged;
        return EditResult::accepted;
    }
    // The activity owner must retire its services first. This only closes the
    // configuration mailbox; leases held by outstanding consumers stay valid.
    [[nodiscard]] bool end(DefinitionRevision expected) {
        std::lock_guard lock(mutex_);
        if(!active_ || expected!=active_.revision) { return false; }
        active_={};previous_={};staged_.reset();stagedToken_={};queued_=false;stagedChange_=Change::unchanged;
        return true;
    }
    [[nodiscard]] DefinitionStatus status() const {
        std::lock_guard lock(mutex_);
        return {active_.revision,active_?active_.document->fingerprint():0,
            staged_?staged_->fingerprint():0,lastRequest_,stagedChange_,queued_};
    }
private:
    mutable std::mutex mutex_;
    DefinitionLease active_,previous_;
    DocumentPtr staged_;
    StagedRevision stagedToken_{};
    std::uint64_t serial_{},stageSerial_{},lastRequest_{};
    Change stagedChange_{Change::unchanged};
    bool queued_{};
};
} // namespace dawn::server::runtime::activity
