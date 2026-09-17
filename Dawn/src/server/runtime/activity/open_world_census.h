#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include "../../../state/activity/coo/native_population_ledger.h"

namespace dawn::server::runtime::activity::open_world_census {
namespace testing {
template<class T,std::size_t Capacity> class BoundedQueue final {
public:
    [[nodiscard]] bool push(const T& value) noexcept {
        if(count_==Capacity)return false;values_[(head_+count_)%Capacity]=value;++count_;return true;
    }
    [[nodiscard]] bool pop(T& value) noexcept {
        if(!count_)return false;value=values_[head_];head_=(head_+1)%Capacity;--count_;return true;
    }
    void clear() noexcept {head_=count_=0;}
    [[nodiscard]] std::size_t size() const noexcept{return count_;}
private:std::array<T,Capacity> values_{};std::size_t head_{},count_{};
};
}
enum class Event : std::uint8_t {runStart,boundary,sourceRequest,sourceObservation,taskSelection,actorAdmitted,actorDied,actorRetired,sourceRecreated,sourceSnapshot,renewalStarted,renewalBusy,renewalCancelled,renewalCommitted,failure};
struct SelectedMember final {
    bool known{};
    std::string_view evidence{"unresolved"};
    std::uint32_t categoryKey{},entity{},weight{},kind{};
    std::int64_t offset{};
    std::uint8_t category{},variant{};
    std::uint16_t choice{};
};
struct Record final {
    Event event{};std::string_view destination{},reason{};
    std::uint32_t scenario{},bubble{},sourceBubble{},priorBubble{};bool arrived{},priorArrived{},hasRule{};
    std::uint32_t registry{};std::uint16_t sourceSlot{},ruleSlot{},tacticalSlot{};
    std::uint32_t generation{};std::uint64_t requestSequence{};
    std::uint32_t requestedFirst{},requestedSecond{};std::int32_t consumedFirst{},consumedSecond{};bool consumedKnown{};
    std::uint32_t tacticalProvider{};std::int8_t tacticalRow{-1};std::uint32_t tacticalRevision{};
    state::activity::coo::PopulationCounts counts{};bool hasCounts{},failed{};
    std::uint32_t priorGeneration{},nextGeneration{};
    SelectedMember member{};
};
[[nodiscard]] bool format(const Record&,std::uint64_t sequence,std::uint64_t elapsedMs,std::span<char>,std::size_t& length) noexcept;
[[nodiscard]] bool enabled() noexcept;
struct Stats final {std::uint64_t dropped{},ioFailures{};};
[[nodiscard]] Stats stats() noexcept;
[[nodiscard]] bool initialize() noexcept;
void shutdown() noexcept;
void emit(const Record&) noexcept;
}
