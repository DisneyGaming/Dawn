#pragma once
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <charconv>
#include <string_view>
#include "../../../middleware/bap/activity_message/sense_update.h"

namespace sunrise::server::runtime::activity::population {
namespace wire=middleware::bap::activity_message::native::population;
namespace codec=middleware::bap::activity_message::native::combatant_source;
using Owner=state::activity::ActivityInstanceKey;
struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{},rule{};
    codec::TacticalGroup tactical{};
    bool hasRule{true};
};
[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)) return false;
    unsigned source{},rule{},tactical{};
    for(const auto& slot:capability.registry->slots) {
        if(slot.index==capability.slot && slot.type==1 && slot.authSchema==0x80807EC9) ++source;
        if(slot.index==capability.rule && slot.type==66) ++rule;
        if(slot.index==capability.tactical.slot && slot.type==3 && slot.authSchema==0x80807F0C) ++tactical;
    }
    codec::Source request{capability.registry->key,1,capability.rule,1,capability.tactical};
    request.hasSpawnRule=capability.hasRule;
    return source==1 && (!capability.hasRule || rule==1)
        && (capability.tactical.row<0 || (capability.tactical.registry==capability.registry->key && tactical==1))
        && codec::valid(request);
}
struct Command final {
    Owner owner{};
    std::uint64_t expectedRevision{},request{};
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t requested{};
    std::uint64_t boot{};
};
enum class Result : std::uint8_t { accepted, invalid, stale, duplicate, unsupported, decrease, unchanged, exhausted };
// Explicit local development format. Absolute cumulative targets are idempotent;
// no reset, delete, teleport, arbitrary pointer or fabricated receipt operation.
// v2 BOOT_HEX OWNER_HEX INCARNATION REVISION REQUEST REGISTRY_HEX SLOT TARGET
[[nodiscard]] inline bool parse(std::string_view text,Command& output) noexcept {
    std::array<std::string_view,9> tokens{};std::size_t used{};
    while(!text.empty()) {
        const auto begin=text.find_first_not_of(" \t\r\n");
        if(begin==std::string_view::npos) break;
        text.remove_prefix(begin);
        const auto end=text.find_first_of(" \t\r\n");
        if(used==tokens.size()) return false;
        tokens[used++]=text.substr(0,end);
        if(end==std::string_view::npos) break;
        text.remove_prefix(end);
    }
    if(used!=9 || tokens[0]!="v2") return false;
    std::uint64_t boot{};
    const auto parsedBoot=std::from_chars(tokens[1].data(),tokens[1].data()+tokens[1].size(),boot,16);
    if(parsedBoot.ec!=std::errc{} || parsedBoot.ptr!=tokens[1].data()+tokens[1].size() || !boot) return false;
    std::array<std::uint64_t,7> values{};
    for(std::size_t i=0;i<values.size();++i) {
        const auto token=tokens[i+2];
        const auto result=std::from_chars(token.data(),token.data()+token.size(),values[i],(i==0 || i==4)?16:10);
        if(result.ec!=std::errc{} || result.ptr!=token.data()+token.size()) return false;
    }
    if(!values[0] || !values[1] || !values[2] || !values[3] || values[4]>UINT32_MAX
        || values[5]>32767 || values[6]==0 || values[6]>63) return false;
    output={{values[0],{values[1]}},values[2],values[3],static_cast<std::uint32_t>(values[4]),
        static_cast<std::uint16_t>(values[5]),static_cast<std::uint8_t>(values[6]),boot};
    return true;
}
class Service final {
public:
    struct Observation {
        std::array<std::uint32_t,6> scalar{};
        std::array<std::int32_t,8> consumed{};
        std::uint32_t revision{};
        std::uint8_t known{},consumedCount{};
        bool seen{},consumedKnown{};
    };
    [[nodiscard]] bool begin(Owner owner,std::span<const Capability> definitions,std::uint64_t boot=1) noexcept {
        if(owner_ || !owner || !boot || owner.incarnation.value>0x7FFFFFFFU
            || definitions.size()>targets_.size()) return false;
        for(std::size_t i=0;i<definitions.size();++i) {
            if(!valid(definitions[i])) return false;
            for(std::size_t j=0;j<i;++j)
                if(definitions[i].registry->key==definitions[j].registry->key && definitions[i].slot==definitions[j].slot) return false;
        }
        owner_=owner;capabilities_=definitions;revision_=1;boot_=boot;return true;
    }
    [[nodiscard]] Result request(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || command.boot!=boot_ || command.owner!=owner_ || command.expectedRevision!=revision_) return Result::stale;
        if(!command.request || command.request<=lastRequest_) return Result::duplicate;
        if(!command.requested || command.requested>63) return Result::invalid;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(command.registry!=capability.registry->key || command.slot!=capability.slot) continue;
            if(bubble!=capability.registry->bubble) return Result::stale;
            if(command.requested<targets_[i]) return Result::decrease;
            if(command.requested==targets_[i]) return Result::unchanged;
            if(revision_==UINT64_MAX) return Result::exhausted;
            targets_[i]=command.requested;lastRequest_=command.request;++revision_;return Result::accepted;
        }
        return Result::unsupported;
    }
    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept {
        wire::Batch batch{};
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(!targets_[i] || capability.registry->bubble!=bubble) continue;
            auto& row=batch.entries[batch.count++];
            row.bubble=capability.registry->bubble;row.slot=capability.slot;
            row.source={capability.registry->key,static_cast<std::uint32_t>(owner_.incarnation.value),
                capability.rule,targets_[i],capability.tactical};
            row.source.hasSpawnRule=capability.hasRule;
        }
        return batch;
    }
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    // Transport/session/epoch qualification belongs to the caller. Only a
    // generation-qualified native source delta can establish this mirror.
    [[nodiscard]] const Observation* observe(std::uint32_t bubble,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        if(!owner_ || !object.hasNativeSchema || object.nativeSchema!=0x80807ECC
            || object.slotType!=1) return nullptr;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(!targets_[i] || capability.registry->bubble!=bubble
                || capability.registry->key!=object.registryKey || capability.slot!=object.slotIndex) continue;
            auto& mirror=observations_[i];
            if(mirror.seen && (object.nativeRevision==mirror.revision
                || object.nativeRevision-mirror.revision>=0x80000000U)) return nullptr;
            const auto& delta=object.sourceDelta;
            if(object.hasRootDelta && (delta.present&1U)) {
                if(delta.scalar[0]!=owner_.incarnation.value) return nullptr;
            } else if(!(mirror.known&1U)) return nullptr;
            if(delta.consumedCount>delta.consumed.size() || (delta.present&0xC0U)) return nullptr;
            mirror.seen=true;mirror.revision=object.nativeRevision;
            if(object.hasRootDelta) {
                for(unsigned j=0;j<delta.scalar.size();++j)
                    if(delta.present&(1U<<j)) mirror.scalar[j]=delta.scalar[j];
                mirror.known|=delta.present;
                if(delta.consumedPresent) {
                    mirror.consumed=delta.consumed;mirror.consumedCount=delta.consumedCount;mirror.consumedKnown=true;
                }
            }
            return &mirror;
        }
        return nullptr;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint64_t last_request() const noexcept { return lastRequest_; }
private:
    Owner owner_{};
    // Registered capabilities have static lifetime. Dynamic definitions must
    // retain their owning revision lease before using this service.
    std::span<const Capability> capabilities_{};
    std::array<std::uint8_t,32> targets_{};
    std::array<Observation,32> observations_{};
    std::uint64_t revision_{},lastRequest_{},boot_{};
};
} // namespace sunrise::server::runtime::activity::population
