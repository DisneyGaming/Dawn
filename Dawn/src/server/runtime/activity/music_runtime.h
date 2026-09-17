#pragma once
#include "../../../middleware/bap/activity_message/native/music_authority.h"
#include "../../../state/activity/coo/executor.h"
#include "../../../state/activity/lifecycle_generation.h"
#include "registry_admission.h"
#include <algorithm>

namespace dawn::server::runtime::activity::music {
namespace wire = middleware::bap::activity_message::native::music;
namespace coo = state::activity::coo;
using Owner = state::activity::ActivityInstanceKey;
struct Selection final {
    std::uint32_t key{};
    const coo::Definition *graph{};
};
// Candidate order is package-authored priority, not a phase name. One selector
// group is supported here. The native component owns audio and transitions.
struct Definition final {
    const registry::Definition *registry{};
    std::uint16_t slot{};
    std::uint32_t group{};
    std::span<const std::uint32_t> candidates{};
    std::span<const Selection> selections{};
};
struct Context final {
    Owner owner{};
    std::uint64_t boot{}, definitionRevision{}, selectionRevision{}, event{};
    std::int16_t activity{-1};
    std::uint32_t bubble{UINT32_MAX};
    bool arrived{}, admitted{};
};
class Runtime final {
  public:
    [[nodiscard]] static bool valid(const Definition &d) noexcept {
        if (!d.registry || !registry::valid(*d.registry) || !key(d.group) || d.candidates.empty() ||
            d.candidates.size() > wire::kCapacity || d.selections.size() != d.candidates.size() + 1)
            return false;
        const auto a = asset(d);
        if (a == coo::Asset{})
            return false;
        for (std::size_t i = 0; i < d.candidates.size(); ++i) {
            if (!key(d.candidates[i]))
                return false;
            for (std::size_t j = 0; j < i; ++j)
                if (d.candidates[j] == d.candidates[i])
                    return false;
        }
        for (std::size_t i = 0; i < d.selections.size(); ++i) {
            const auto &s = d.selections[i];
            if (!s.graph || !coo::Executor::valid(*s.graph) || s.graph->schema != coo::Schema::otherMissions ||
                s.graph->steps.size() != 1 || s.graph->steps[0].dependencies || s.graph->steps[0].commands.size() != 1)
                return false;
            if (s.key && std::find(d.candidates.begin(), d.candidates.end(), s.key) == d.candidates.end())
                return false;
            for (std::size_t j = 0; j < i; ++j)
                if (d.selections[j].key == s.key)
                    return false;
            const auto &c = s.graph->steps[0].commands[0];
            if (c.operation != coo::Operation::device || c.asset != a || c.argument != s.key ||
                c.wait != coo::Wait::requested)
                return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(const Definition &d, const Context &c) noexcept {
        if (definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision || !c.selectionRevision ||
            !c.event || c.activity < 0 || c.bubble != d.registry->bubble)
            return false;
        request_.registry = d.registry->key;
        request_.slot = d.slot;
        request_.scope = d.registry->topLevel ? UINT32_MAX : d.registry->bubble;
        request_.candidateCount = static_cast<std::uint16_t>(d.candidates.size());
        if (!wire::valid(request_))
            return false;
        definition_ = &d;
        context_ = c;
        return true;
    }
    [[nodiscard]] bool request(std::uint32_t selection, std::uint64_t sequence, const Context &c) noexcept {
        if (!same(c) || failed_ || !eligible(c) || !sequence)
            return false;
        if (sequence == sequence_)
            return selection == selection_;
        if (sequence_ == UINT64_MAX || sequence != sequence_ + 1 ||
            (sequence_ && executor_.diagnostics().phase != coo::Phase::complete))
            return false;
        for (const auto &s : definition_->selections)
            if (s.key == selection) {
                executor_ = {};
                if (!executor_.start(*s.graph, sequence))
                    return false;
                sequence_ = sequence;
                selection_ = selection;
                return true;
            }
        return false;
    }
    [[nodiscard]] bool update(const Context &c) noexcept {
        if (!same(c) || failed_)
            return false;
        if (!sequence_ || !eligible(c))
            return true;
        Driver driver(*this);
        executor_.update(driver);
        failed_ = executor_.diagnostics().phase == coo::Phase::failed;
        return !failed_;
    }
    [[nodiscard]] bool append(wire::Batch &batch) const noexcept {
        if (!definition_ || failed_ || batch.count > batch.entries.size())
            return false;
        if (!published_)
            return true;
        if (batch.count == batch.entries.size())
            return false;
        for (std::size_t i = 0; i < batch.count; ++i)
            if (batch.entries[i].registry == request_.registry && batch.entries[i].slot == request_.slot)
                return false;
        batch.entries[batch.count++] = request_;
        return true;
    }
    // Publication only: no rendered/audio-ready receipt is manufactured.
    [[nodiscard]] bool requested() const noexcept {
        return sequence_ && executor_.diagnostics().phase == coo::Phase::complete;
    }
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }

  private:
    [[nodiscard]] static bool key(std::uint32_t k) noexcept { return k && k != UINT32_MAX && k != 0x811C9DC5; }
    [[nodiscard]] static coo::Asset asset(const Definition &d) noexcept {
        coo::Asset result{};
        unsigned n{};
        for (const auto &s : d.registry->slots)
            if (s.index == d.slot) {
                if (s.type != 11 || s.componentClass != 0x80804E8E || s.authSchema != wire::kSchema)
                    return {};
                result = {d.registry->key, s.descriptorTag, s.type, s.index};
                ++n;
            }
        return n == 1 ? result : coo::Asset{};
    }
    [[nodiscard]] bool same(const Context &c) const noexcept {
        return definition_ && c.owner == context_.owner && c.boot == context_.boot &&
               c.definitionRevision == context_.definitionRevision &&
               c.selectionRevision == context_.selectionRevision && c.event == context_.event &&
               c.activity == context_.activity;
    }
    [[nodiscard]] bool eligible(const Context &c) const noexcept {
        return c.arrived && c.admitted && c.bubble == definition_->registry->bubble;
    }
    struct Driver final : coo::Services {
        Runtime &owner;
        explicit Driver(Runtime &o) : owner(o) {}
        bool publish(const coo::Command &c) noexcept override {
            owner.request_.active = {};
            if (c.spec.argument) {
                const auto &candidates = owner.definition_->candidates;
                const auto at = std::find(candidates.begin(), candidates.end(), c.spec.argument);
                if (at == candidates.end() ||
                    !wire::select(owner.request_, static_cast<std::size_t>(at - candidates.begin())))
                    return false;
            }
            owner.published_ = true;
            return true;
        }
        void cancel(const coo::Command &) noexcept override {}
    };
    const Definition *definition_{};
    Context context_{};
    wire::Request request_{};
    coo::Executor executor_{};
    std::uint64_t sequence_{};
    std::uint32_t selection_{};
    bool published_{}, failed_{};
};
} // namespace dawn::server::runtime::activity::music
