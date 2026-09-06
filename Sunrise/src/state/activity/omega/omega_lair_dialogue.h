#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace sunrise::state::activity::omega {
/** tv_dialog_mercury_m_scot_lair_010_vo, FNV1 4FF1DB4D, A3928C71/60/2. */
[[nodiscard]] inline bool in_lair_dialogue_010(float x, float y, float z) noexcept {
    struct Vertex { float x, y; };
    constexpr std::array<Vertex,4> vertices{{
        {-1506.17822265625F,488.1286315917969F},
        {-1507.4510498046875F,476.3022155761719F},
        {-1475.7196044921875F,476.0859680175781F},
        {-1477.4234619140625F,486.6628112792969F}}};
    constexpr std::array<std::array<unsigned,3>,2> triangles{{{3,0,1},{3,1,2}}};
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || z < -22.701810836791992F || z > 2.2981910705566406F) return false;
    const auto cross = [x,y](Vertex a, Vertex b) {
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
    };
    for (const auto& triangle : triangles) {
        const float a=cross(vertices[triangle[0]],vertices[triangle[1]]);
        const float b=cross(vertices[triangle[1]],vertices[triangle[2]]);
        const float c=cross(vertices[triangle[2]],vertices[triangle[0]]);
        if ((a>=0 && b>=0 && c>=0) || (a<=0 && b<=0 && c<=0)) return true;
    }
    return false;
}

/** tv_dialog_mercury_m_scot_lair_050_vo, FNV1 15684301, A3928C71/60/3.
 * Installed 80F47B42 stores the polygon at1170, triangles11E0, boundsCA0/CB0. */
[[nodiscard]] inline bool in_lair_dialogue_050(float x, float y, float z) noexcept {
    struct Vertex { float x, y; };
    constexpr std::array<Vertex,5> vertices{{
        {-1463.2938232421875F,266.00738525390625F},
        {-1546.319091796875F,264.1702575683594F},
        {-1483.7730712890625F,164.2411651611328F},
        {-1410.1800537109375F,147.92169189453125F},
        {-1407.2794189453125F,214.2381134033203F}}};
    constexpr std::array<std::array<unsigned,3>,3> triangles{{{2,3,4},{1,2,4},{4,0,1}}};
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || z < -56.49456024169922F || z > 3.505441665649414F) return false;
    const auto cross = [x,y](Vertex a, Vertex b) {
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
    };
    for (const auto& triangle : triangles) {
        const float a=cross(vertices[triangle[0]],vertices[triangle[1]]);
        const float b=cross(vertices[triangle[1]],vertices[triangle[2]]);
        const float c=cross(vertices[triangle[2]],vertices[triangle[0]]);
        if ((a>=0 && b>=0 && c>=0) || (a<=0 && b<=0 && c<=0)) return true;
    }
    return false;
}

/** A bounded two-cue queue. A timer can separate accepted cues but creates no event.
 * Native dispatch retires the current offer; historical generation remains1. */
class LairDialogue final {
public:
    static constexpr std::uint8_t kNone = 255;
    static constexpr std::uint32_t kIntroMask = 1U << 12;
    static constexpr std::uint32_t kArenaMask = 1U << 13;

    void cinematic_completed(std::uint64_t completedAt) noexcept {
        if (cinematicSeen_ || (offered_ & kArenaMask) != 0) return;
        cinematicSeen_ = true;
        cinematicCompletedAt_ = completedAt;
        requested_ |= kIntroMask;
    }

    void dispatched(std::int32_t row, std::uint64_t now) noexcept {
        if ((row != 12 && row != 13) || row != pending_) return;
        const auto mask=1U << static_cast<unsigned>(row);
        if ((offered_ & mask) == 0 || (delivered_ & mask) != 0) return;
        delivered_ |= mask;
        pending_ = kNone;
        if (row == 12) { introDispatchedAt_=now; introDispatched_=true; }
    }

    void update(std::uint64_t now, std::uint32_t loadedBubble, bool present,
                float x, float y, float z) noexcept {
        if (loadedBubble == 14 && present) {
            if (objective_ == 0 && in_lair_dialogue_010(x,y,z)) objective_=0x3517D4D5U;
            if (in_lair_dialogue_050(x,y,z)) {
                objective_=0x31A51CEBU;
                requested_ |= kArenaMask;
            }
        }
        if (pending_ != kNone) return;
        if ((requested_ & kIntroMask) != 0 && (delivered_ & kIntroMask) == 0) {
            if (now >= cinematicCompletedAt_ && now-cinematicCompletedAt_ >= 250U)
                offer(12);
            return;
        }
        if ((requested_ & kArenaMask) == 0 || (delivered_ & kArenaMask) != 0) return;
        // Bank80F1FD07 row12 duration2.017580032348633s plus250ms presentation gap.
        if (introDispatched_ && (now < introDispatchedAt_ || now-introDispatchedAt_ < 2268U)) return;
        offer(13);
    }

    [[nodiscard]] std::uint32_t requested_mask() const noexcept { return requested_; }
    [[nodiscard]] std::uint8_t pending_row() const noexcept { return pending_; }
    [[nodiscard]] std::uint32_t objective_event() const noexcept { return objective_; }
private:
    void offer(std::uint8_t row) noexcept { pending_=row; offered_ |= 1U << row; }
    std::uint32_t requested_{}, offered_{}, delivered_{};
    std::uint32_t objective_{};
    std::uint64_t cinematicCompletedAt_{}, introDispatchedAt_{};
    std::uint8_t pending_{kNone};
    bool cinematicSeen_{}, introDispatched_{};
};
} // namespace sunrise::state::activity::omega
