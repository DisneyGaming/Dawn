#pragma once
#include "../../../middleware/bap/activity_message/sense_update.h"

namespace dawn::server::runtime::activity::ambient_population {
namespace sense=middleware::bap::activity_message::sense_update;

struct MonitorDelta final {
    std::uint32_t revision{};
    std::int32_t selected{}, authorityToken{};
    bool root{}, any{}, all{};
};

// Native B1FC00 computes any=(selected-mask!=0), all=(selected-mask==filter),
// popcount(selected-mask), then echoes authority+188 into sense+194. Schema
// 80809531 carries bool/bool/s32/s32, preceded by root and followed by revision.
// The canonical parser already validates this exact schema and retains its bits.
// An empty filter can report all=true, any=false, selected=0; it is not occupied.
[[nodiscard]] inline bool decode_monitor(const sense::SenseObject& object,MonitorDelta& output) noexcept {
    if(!object.hasNativeSchema || object.nativeSchema!=0x80809531 || object.slotType!=30
        || object.inferredBodyWidth) return false;
    MonitorDelta result{};result.revision=object.nativeRevision;result.root=object.hasRootDelta;
    if(result.root) {
        if(object.bodyBits!=99 || !(object.bodyFirst&(std::uint64_t{1}<<63))
            || object.bodySecond>>35 || object.bodyThird || object.bodyFourth
            || static_cast<std::uint32_t>(object.bodySecond)!=result.revision) return false;
        result.any=(object.bodyFirst&(std::uint64_t{1}<<62))!=0;
        result.all=(object.bodyFirst&(std::uint64_t{1}<<61))!=0;
        const auto selected=static_cast<std::uint32_t>(object.bodyFirst>>29);
        const auto token=static_cast<std::uint32_t>(((object.bodyFirst&0x1FFFFFFFU)<<3)|(object.bodySecond>>32));
        result.selected=static_cast<std::int32_t>(static_cast<std::int64_t>(selected)-2147483648LL);
        result.authorityToken=static_cast<std::int32_t>(static_cast<std::int64_t>(token)-2147483648LL);
        if(result.selected<0 || result.selected>32 || result.any!=(result.selected!=0)) return false;
    } else if(object.bodyBits!=33 || object.bodyFirst!=result.revision || object.bodySecond
        || object.bodyThird || object.bodyFourth) return false;
    output=result;return true;
}

enum class MonitorIntake : std::uint8_t { accepted, unchanged, stale, unrelated, invalid };
class Monitor final {
public:
    // Caller has already authenticated sender/owner and matching patch epoch.
    [[nodiscard]] MonitorIntake observe(const sense::SenseObject& object,std::int32_t expectedToken) noexcept {
        MonitorDelta value{};
        if(!decode_monitor(object,value)) return MonitorIntake::invalid;
        if(value.root && value.authorityToken!=expectedToken) return MonitorIntake::unrelated;
        if(seen_ && (value.revision==revision_ || value.revision-revision_>=0x80000000U)) return MonitorIntake::stale;
        seen_=true;revision_=value.revision;
        if(!value.root) return MonitorIntake::unchanged;
        const bool changed=!known_ || value.any!=any_ || value.all!=all_ || value.selected!=selected_;
        known_=true;any_=value.any;all_=value.all;selected_=value.selected;
        return changed?MonitorIntake::accepted:MonitorIntake::unchanged;
    }
    // Clear occupancy when the zone observation epoch changes. This does not
    // retire a source or forget which initial activations have been published.
    void reset() noexcept { *this={}; }
    [[nodiscard]] bool known() const noexcept {return known_;}
    [[nodiscard]] bool occupied() const noexcept {return known_ && any_ && selected_>0;}
    [[nodiscard]] std::int32_t selected() const noexcept {return selected_;}
    [[nodiscard]] std::uint32_t revision() const noexcept {return revision_;}
private:
    std::uint32_t revision_{};
    std::int32_t selected_{};
    bool known_{},seen_{},any_{},all_{};
};
} // namespace dawn::server::runtime::activity::ambient_population
