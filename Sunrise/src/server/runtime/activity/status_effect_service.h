#pragma once

#include "registry_admission.h"
#include "../../../middleware/bap/activity_message/native/status_effect_authority.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace sunrise::server::runtime::activity::status_effect {

namespace wire=middleware::bap::activity_message::native::status_effect;
using Owner=state::activity::ActivityInstanceKey;

inline constexpr std::size_t kBindingCapacity=wire::kCapacity;

/** An immutable authored type-26 status-effect slot admitted by the activity. */
struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{};
};

[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry))return false;
    unsigned matches{};
    for(const auto& slot:capability.registry->slots) {
        if(slot.index!=capability.slot)continue;
        if(slot.type!=wire::kType || slot.flags()!=3 || slot.componentClass!=wire::kComponentClass
            || slot.authSchema!=wire::kSchema)return false;
        ++matches;
    }
    return matches==1;
}

struct Command final {
    Owner owner{};
    std::uint64_t boot{};
    std::uint64_t expectedRevision{};
    std::uint64_t request{};
    std::size_t capabilityIndex{};
    bool enabled{};
};

enum class Result : std::uint8_t { accepted, unchanged, stale, duplicate, unsupported, exhausted };

struct Snapshot final {
    Owner owner{};
    std::uint64_t boot{};
    std::uint64_t revision{};
    std::uint64_t lastRequest{};
};

/** Server-owned generic publication state for authored type-26 effects. */
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,
        std::span<const Capability> capabilities) noexcept {
        if(owner_ || !owner || !boot || capabilities.size()>kBindingCapacity)return false;
        for(std::size_t i=0;i<capabilities.size();++i) {
            if(!valid(capabilities[i]))return false;
            for(std::size_t j=0;j<i;++j)
                if(capabilities[j].registry->key==capabilities[i].registry->key
                    && capabilities[j].slot==capabilities[i].slot)return false;
        }
        owner_=owner;boot_=boot;revision_=1;lastRequest_=0;count_=capabilities.size();
        for(std::size_t i=0;i<count_;++i) {
            bindings_[i]={capabilities[i],{},false};
            bindings_[i].request.registry=capabilities[i].registry->key;
            bindings_[i].request.slot=capabilities[i].slot;
            bindings_[i].request.bubble=capabilities[i].registry->bubble;
        }
        return true;
    }

    [[nodiscard]] Result request(const Command& command) noexcept {
        if(command.capabilityIndex>=count_)return Result::unsupported;
        if(!matches_session(command))return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        auto& binding=bindings_[command.capabilityIndex];
        if(!binding.published) {
            if(revision_==std::numeric_limits<std::uint64_t>::max())return Result::exhausted;
            binding.published=true;binding.request.enabled=command.enabled;
            lastRequest_=command.request;++revision_;return Result::accepted;
        }
        if(binding.request.enabled==command.enabled) {
            // The request number is consumed for replay protection; equal state does not
            // create another publication revision or alter the encoded record.
            lastRequest_=command.request;return Result::unchanged;
        }
        if(revision_==std::numeric_limits<std::uint64_t>::max())return Result::exhausted;
        binding.request.enabled=command.enabled;lastRequest_=command.request;++revision_;
        return Result::accepted;
    }

    [[nodiscard]] Result request(Owner owner,std::uint64_t boot,std::uint64_t expectedRevision,
        std::uint64_t monotonicRequest,std::size_t capabilityIndex,bool enabled) noexcept {
        return request({owner,boot,expectedRevision,monotonicRequest,capabilityIndex,enabled});
    }

    /** One native application per command; retained snapshots cannot restart it. */
    [[nodiscard]] Result pulse(const Command& command) noexcept {
        if(command.capabilityIndex>=count_ || !command.enabled)return Result::unsupported;
        if(!matches_session(command))return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        auto& binding=bindings_[command.capabilityIndex];
        if(revision_==UINT64_MAX || binding.request.selectionRevision==INT32_MAX)
            return Result::exhausted;
        binding.published=true;binding.request.enabled=true;binding.request.once=true;
        ++binding.request.selectionRevision;lastRequest_=command.request;++revision_;
        return Result::accepted;
    }

    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept {
        wire::Batch output{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& binding=bindings_[i];
            if(!binding.published || binding.request.bubble!=bubble
                || output.count==output.entries.size())continue;
            output.entries[output.count++]=binding.request;
        }
        return output;
    }

    [[nodiscard]] Snapshot snapshot() const noexcept { return {owner_,boot_,revision_,lastRequest_}; }
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint64_t last_request() const noexcept { return lastRequest_; }

    /** Drops all owner-scoped publication state so no request crosses a lifetime. */
    void release_owner() noexcept {*this={};}
    void release() noexcept {release_owner();}

private:
    struct Binding final {
        Capability capability{};
        wire::Request request{};
        bool published{};
    };

    [[nodiscard]] bool matches_session(const Command& command) const noexcept {
        return owner_ && command.owner==owner_ && command.boot==boot_
            && command.expectedRevision==revision_;
    }

    std::array<Binding,kBindingCapacity> bindings_{};
    std::size_t count_{};
    Owner owner_{};
    std::uint64_t boot_{},revision_{},lastRequest_{};
};

} // namespace sunrise::server::runtime::activity::status_effect
