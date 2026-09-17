#pragma once
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../middleware/bap/activity_message/native/activity_clock_authority.h"

namespace dawn::server::runtime::activity::activity_clock {
namespace wire=middleware::bap::activity_message::native::activity_clock;
using Owner=state::activity::ActivityInstanceKey;
struct Policy final {
    std::uint32_t scenario{};
    std::uint8_t bubble{};
    wire::Configuration configuration{};
};
struct Domain final {
    Owner owner{};
    std::uint64_t boot{},epoch{};
    std::uint32_t scenario{};
    std::uint8_t bubble{};
    friend constexpr bool operator==(const Domain&,const Domain&)=default;
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(owner) && boot && epoch && scenario && bubble<=63;
    }
};
struct Publication final {
    Domain domain{};
    wire::Configuration configuration{};
    std::uint64_t elapsedTicks{};
    [[nodiscard]] explicit constexpr operator bool() const noexcept {return static_cast<bool>(domain);}
};
// Owner-thread policy only. This owns the activity's synchronized elapsed time,
// never a native completion receipt. The caller supplies its monotonic host
// clock in milliseconds; native wire conversion happens in the typed codec.
// Unconfigured activities do not create a domain or change their existing bytes.
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::uint64_t epoch,
        const Policy& policy,std::uint64_t now) noexcept {
        if(domain_ || !owner || !boot || !epoch || epoch<=lastEpoch_ || !policy.scenario || policy.scenario==UINT32_MAX
            || policy.bubble>63 || !wire::valid(policy.configuration)
            || policy.configuration.timing<=0.0F)return false;
        // Epoch comes from the runtime's boot-scoped allocator, not an Entry
        // constructor that could repeat after hot document replacement.
        domain_={owner,boot,epoch,policy.scenario,policy.bubble};lastEpoch_=epoch;
        configuration_=policy.configuration;origin_=lastNow_=now;return true;
    }
    [[nodiscard]] bool project(Owner owner,std::uint64_t boot,std::uint32_t bubble,
        std::uint64_t now,Publication& output) noexcept {
        if(!domain_ || owner!=domain_.owner || boot!=domain_.boot || bubble!=domain_.bubble
            || now<lastNow_)return false;
        std::uint64_t ticks{};
        if(!wire::from_milliseconds(now-origin_,ticks) || ticks==UINT64_MAX)return false;
        output={domain_,configuration_,ticks};lastNow_=now;return true;
    }
    // An admitted persistent owner may move through a separately qualified
    // native regional route. Retain the exact admission domain and epoch;
    // callers must prove that route before requesting this continuation.
    [[nodiscard]] bool project_retained(Owner owner,std::uint64_t boot,
        const Domain& admitted,std::uint64_t now,Publication& output) noexcept {
        if(!admitted || admitted!=domain_)return false;
        return project(owner,boot,admitted.bubble,now,output);
    }
    void retire() noexcept {domain_={};configuration_={};origin_=lastNow_=0;}
    [[nodiscard]] Domain domain() const noexcept {return domain_;}
private:
    Domain domain_{};wire::Configuration configuration_{};
    std::uint64_t origin_{},lastNow_{},lastEpoch_{};
};
}
