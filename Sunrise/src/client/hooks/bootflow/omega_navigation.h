#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::client::hooks::bootflow::omega_navigation {
struct Point { float x{}, y{}, z{}; };
enum class Goal : std::uint8_t { ikora, portal, entrance, gates, lairApproach, lairEntry, complete };
struct Target { Point position{}; std::uint32_t bubble{}; bool present{}; };
// Package-verified ActivityPoints and lighthouse_teleport's authored placement.
inline constexpr Point kIkora{347.3338623F, 249.9882660F, 102.6479492F};
inline constexpr Point kPortal{366.6945496F, 249.9992828F, 100.5929337F};
inline constexpr Point kEntrance{-1357.0991F, 1126.9106F, -46.8116F};
inline constexpr Point kLairApproach{-1491.8749F, 519.2537F, -15.9310F};
inline constexpr Point kLairEntry{-1491.7739F, 415.2429F, -26.9186F};
[[nodiscard]] constexpr Target target(Goal goal) noexcept {
    switch (goal) {
    case Goal::ikora: return {kIkora, 15, true};
    case Goal::portal: return {kPortal, 15, true};
    case Goal::entrance: return {kEntrance, 11, true};
    case Goal::lairApproach: return {kLairApproach, 14, true};
    case Goal::lairEntry: return {kLairEntry, 14, true};
    default: return {};
    }
}
[[nodiscard]] inline bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
[[nodiscard]] inline bool within_target(Point a, Point b) noexcept {
    const float x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
    return finite(a) && x*x+y*y+z*z <= 49.0F;
}
class Route final {
public:
    void reset(std::uint64_t run) noexcept {
        if (run != run_) { run_=run; goal_=Goal::ikora; }
    }
    [[nodiscard]] Goal current() const noexcept { return goal_; }
    void advance(Goal goal) noexcept { if (goal > goal_) goal_=goal; }
    void terminal_gate() noexcept { if (goal_ == Goal::gates) goal_=Goal::lairApproach; }
    [[nodiscard]] Goal observe(std::uint64_t run, bool present, Point position,
                               std::uint32_t bubble) noexcept {
        reset(run);
        if (!present || !finite(position)) return goal_;
        // Actual loaded scenario ordinals may retire earlier approach goals.
        if (bubble == 11) advance(Goal::gates);
        if (bubble == 14) advance(Goal::lairEntry);
        if (within_target(position,kEntrance)) advance(Goal::gates);
        if (within_target(position,kLairApproach)) advance(Goal::lairEntry);
        if (within_target(position,kLairEntry)) advance(Goal::complete);
        if (goal_ == Goal::ikora && within_target(position,kIkora)) goal_=Goal::portal;
        if (goal_ == Goal::portal && within_target(position,kPortal)) goal_=Goal::entrance;
        return goal_;
    }
private:
    std::uint64_t run_{UINT64_MAX};
    Goal goal_{};
};
template<class T> [[nodiscard]] inline T read(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    if (offset <= bytes.size() && sizeof value <= bytes.size()-offset)
        std::memcpy(&value,bytes.data()+offset,sizeof value);
    return value;
}
template<class T> inline void write(std::span<std::byte> bytes, std::size_t offset, T value) noexcept {
    std::memcpy(bytes.data()+offset,&value,sizeof value);
}
inline constexpr std::size_t kComponentBytes = 0xB08;
inline constexpr std::size_t kFallback = 0xA80;
[[nodiscard]] inline bool component(std::span<const std::byte> bytes) noexcept {
    return bytes.size() >= kComponentBytes && read<std::uint32_t>(bytes,0)==0x80F47BD4U
        && read<std::uint32_t>(bytes,4)==0x80804F54U && read<std::int64_t>(bytes,8)==0xB88;
}
/** Only the objective's generic fallback changes. The other twelve native targets remain owned by native code. */
[[nodiscard]] inline bool sync(std::span<std::byte> bytes, Goal goal) noexcept {
    if (!component(bytes)) return false;
    const auto desired=target(goal);
    const std::uint8_t kind=desired.present ? 3U : 0U;
    bool changed=read<std::uint8_t>(bytes,kFallback+4)!=kind;
    if (desired.present) {
        changed=changed || read<std::uint8_t>(bytes,kFallback+12)!=2U
            || read<std::uint32_t>(bytes,kFallback+24)!=desired.bubble
            || std::memcmp(bytes.data()+kFallback+32,&desired.position,sizeof(Point))!=0;
    }
    if (!changed) return false;
    write(bytes,kFallback+4,kind);
    if (desired.present) {
        write<std::uint8_t>(bytes,kFallback+12,2);
        write(bytes,kFallback+24,desired.bubble);
        write(bytes,kFallback+32,desired.position);
    }
    return true;
}
/** Final-gate qualification follows original FFB850. The +355 byte alone is not a physical-open receipt. */
[[nodiscard]] inline bool terminal(std::uint32_t nativeResult, std::uint8_t kind,
                                   std::int32_t gate, std::int32_t gateCount,
                                   float progress, float maximum, std::uint8_t native355) noexcept {
    return nativeResult==0 && kind==3 && gate>=0 && gateCount>0 && gateCount<=256
        && gate<gateCount && progress==1.0F && maximum==1.0F && native355==1;
}
} // namespace sunrise::client::hooks::bootflow::omega_navigation
