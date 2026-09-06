#pragma once

#include <cmath>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "../../../state/activity/omega_first_lair_encounter.h"

namespace sunrise::client::hooks::bootflow::omega_boss_health {

inline constexpr std::uint32_t kBodyRegion=0x6DFE676DU;
inline constexpr std::uint32_t kEyeRegion=0x720A5B5BU;
inline constexpr std::uint32_t kFirstCheckpoint=0x46047267U;
inline constexpr std::uint32_t kSecondCheckpoint=0x46047264U;
inline constexpr std::uint32_t kKillCheckpoint=0x1678EC10U;
inline constexpr std::uint32_t kEyeRefillCheckpoint=0x95A36FFEU;
inline constexpr float kEyeThreshold=0.90F;
/** B8B0C0 returns the decrypted stored fraction, so an authored checkpoint set
 * through the named 1F action normally round-trips exactly (3F0CCCCD for .55).
 * The setter itself lives behind asset-driven dispatch that was not decompiled;
 * if it ever stores max*fraction/max the value can differ by a few ULP. This
 * tolerance (0.01% health) absorbs that while still rejecting any unrelated
 * lower value such as .54 or .09. */
inline constexpr float kCheckpointTolerance=1e-4F;
inline constexpr std::size_t kCharacterBytes=0x300;
inline constexpr std::size_t kHealthBytes=0x340;
inline constexpr std::size_t kMemberBytes=0x220;
// Original CD6C20 starts its next instruction with REX.B (41), not44.
// Validated against the archived exact-build PE; PID11564 logged rejection.
inline constexpr std::array<std::uint8_t,16> kFractionGetterPrefix{
    0x48,0x83,0xEC,0x68,0x44,0x8B,0x09,0x4C,
    0x8B,0xD1,0x48,0x89,0x4C,0x24,0x28,0x41};
[[nodiscard]] inline bool fraction_getter_prefix(std::span<const std::byte> bytes) noexcept {
    return bytes.size()>=kFractionGetterPrefix.size()
        && std::memcmp(bytes.data(),kFractionGetterPrefix.data(),kFractionGetterPrefix.size())==0;
}

namespace detail {
template<class T> [[nodiscard]] inline T read(std::span<const std::byte> b,std::size_t off) noexcept {
    T value{};
    if(off<=b.size() && sizeof(T)<=b.size()-off) { std::memcpy(&value,b.data()+off,sizeof(T)); }
    return value;
}
} // namespace detail

/** Native typed reference: {handle, kind, offset}. Character +2E8 holds the
 * health reference in this layout, and every runtime component begins with the
 * same 16-byte shape naming its own definition (CD6C20 reads that prefix as
 * handle at +0 and offset at +8 to reach the authored region table). */
struct Reference final {
    std::uint32_t handle{UINT32_MAX},kind{};
    std::int64_t offset{};
};
static_assert(sizeof(Reference)==16,"native reference is 16 bytes");
static_assert(offsetof(Reference,handle)==0 && offsetof(Reference,kind)==4 && offsetof(Reference,offset)==8,
    "native reference layout: handle +0, kind +4, offset +8");

/** The boss roster member is an inline registry component, not a world
 * component. Its full typed self reference is at +48; +24 is unused zero.
 * Authored 80F4756D runtime begins at raw70: rawB8 is the reciprocal
 * {80F4756D,8080834E,70} reference. Native relocation makes it {handle,kind,0}.
 * Captured PID17384: +48={4DF90199,8080834E,0}, matching actor row+60. */
[[nodiscard]] inline bool member_identity(std::span<const std::byte> bytes,
    const state::activity::omega_first_lair::Boss& boss,const Reference& member) noexcept {
    using detail::read;
    return boss.valid() && bytes.size()>=kMemberBytes && member.handle!=UINT32_MAX
        && member.kind==0x8080834EU && member.offset==0
        && read<std::uint32_t>(bytes,0)==0x80F4756DU
        && read<std::uint32_t>(bytes,4)==0x80807D9DU
        && read<std::uint64_t>(bytes,8)==0xB58U
        && read<std::uint32_t>(bytes,0x48)==member.handle
        && read<std::uint32_t>(bytes,0x4C)==member.kind
        && read<std::int64_t>(bytes,0x50)==member.offset
        && read<std::uint32_t>(bytes,0x180)==boss.generation
        && read<std::uint32_t>(bytes,0x190)==boss.revision
        && read<std::uint8_t>(bytes,0x1D4)==0
        && read<std::uint32_t>(bytes,0x21C)==boss.actor;
}

[[nodiscard]] inline bool character_identity(std::span<const std::byte> bytes,
    const state::activity::omega_first_lair::Boss& boss,Reference& health) noexcept {
    using detail::read;
    if(!boss.valid() || bytes.size()<kCharacterBytes
        || read<std::uint32_t>(bytes,0)!=0x80F6690BU
        || read<std::uint32_t>(bytes,4)!=0x80806832U
        || read<std::uint64_t>(bytes,8)!=0x738U
        || read<std::uint32_t>(bytes,0x24)!=boss.character
        || read<std::uint32_t>(bytes,0x2C)!=boss.entity
        || read<std::uint32_t>(bytes,0xC0)!=boss.actor) { return false; }
    health={read<std::uint32_t>(bytes,0x2E8),read<std::uint32_t>(bytes,0x2EC),
        read<std::int64_t>(bytes,0x2F0)};
    return health.handle!=UINT32_MAX && health.kind==0x80804BEEU && health.offset==0;
}

[[nodiscard]] inline bool health_identity(std::span<const std::byte> bytes,
    const state::activity::omega_first_lair::Boss& boss,const Reference& health) noexcept {
    using detail::read;
    // 815B5A40 runtime +90 points to definition +F98; the reciprocal definition
    // binds the six authored regions in the original Panoptes asset.
    return boss.valid() && bytes.size()>=kHealthBytes && health.handle!=UINT32_MAX
        && health.kind==0x80804BEEU && health.offset==0
        && read<std::uint32_t>(bytes,0)==0x815B5A40U
        && read<std::uint32_t>(bytes,4)==0x80804B8AU
        && read<std::uint64_t>(bytes,8)==0xF98U
        && read<std::uint32_t>(bytes,0x24)==health.handle
        && read<std::uint32_t>(bytes,0x2C)==boss.entity;
}

[[nodiscard]] inline bool fraction_valid(float value) noexcept {
    return std::isfinite(value) && value>=0.F && value<=1.F;
}

/** These values come from 815B5A40's named native checkpoints. They are not
 * reconstructed thirds of the HUD bar. Near-exact checkpoint equality avoids
 * treating an unrelated lower-health state as completion of this cycle. */
[[nodiscard]] inline bool checkpoint_fraction(std::uint8_t cycle,float& fraction) noexcept {
    if(cycle==1) { fraction=0.55F;return true; }
    if(cycle==2) { fraction=0.10F;return true; }
    if(cycle==3) { fraction=0.F;return true; }
    return false;
}
[[nodiscard]] inline bool checkpoint_reached(std::uint8_t cycle,float body) noexcept {
    float expected{};
    return fraction_valid(body) && checkpoint_fraction(cycle,expected)
        && std::fabs(body-expected)<=kCheckpointTolerance;
}

/** Original D54AF0, mode0: old > threshold && new <= threshold. CDF520
 * evaluates this for the eye's named 90-percent checkpoint. A first sample
 * below that checkpoint, a recycled health allocation, or another command
 * cannot masquerade as a new downward crossing. The animation driver retains
 * the issued token and samples throughout eyeOpening and eyeDps. */
class EyeThresholdTracker final {
public:
    [[nodiscard]] bool observe(const state::activity::omega_first_lair::CrownToken& token,
        std::uint32_t health,float eye) noexcept {
        if(!token.valid() || token.boss.actionEpoch==0 || health==UINT32_MAX || !fraction_valid(eye)) {
            return false;
        }
        if(!token_.valid()) { token_=token;health_=health;previous_=eye;return false; }
        if(token!=token_ || health!=health_ || crossed_) { return false; }
        crossed_=previous_>kEyeThreshold && eye<=kEyeThreshold;
        previous_=eye;return crossed_;
    }
    [[nodiscard]] bool crossed() const noexcept { return crossed_; }
    /** True once an old-side baseline has been retained for the current lease. */
    [[nodiscard]] bool armed() const noexcept { return token_.valid(); }
    [[nodiscard]] float previous() const noexcept { return previous_; }
    [[nodiscard]] std::uint32_t health() const noexcept { return health_; }
    void reset() noexcept { *this={}; }
private:
    state::activity::omega_first_lair::CrownToken token_{};
    std::uint32_t health_{UINT32_MAX};
    float previous_{};
    bool crossed_{};
};

} // namespace sunrise::client::hooks::bootflow::omega_boss_health
