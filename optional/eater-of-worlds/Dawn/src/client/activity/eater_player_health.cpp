#include "eater_player_health.h"
#include "eater_player_health_binding.h"
#include "nightfall_player.h"
#include "../../state/activity/eater_of_worlds/runtime.h"
namespace dawn::client::activity::eater_player_health {
namespace native_fallback_runtime {
namespace native=hooks::bootflow::gateway_native;

inline constexpr std::array<unsigned char,16> kPlayerPrefix{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48};
inline constexpr std::array<unsigned char,16> kWeakPrefix{0x4C,0x8B,0xD1,0x83,0xFA,0xFF,0x74,0x5B,0x4C,0x8B,0x0D,0xD1,0x7F,0x0E,0x02,0x44};

[[nodiscard]] bool local_player(std::uintptr_t image,std::uint32_t& player) noexcept {
    native::Read read{image};
    std::array<unsigned char,16> prefix{};player=UINT32_MAX;
    if(!read.value(image+0x4B2260,prefix) || prefix!=kPlayerPrefix) return false;
    const auto local=reinterpret_cast<void(__fastcall*)(std::uint32_t*)>(image+0x4B2260);
    __try { local(&player);return player!=UINT32_MAX; }
    __except(EXCEPTION_EXECUTE_HANDLER) { player=UINT32_MAX;return false; }
}

[[nodiscard]] bool capture(std::uintptr_t image,std::uint32_t player,
                           nightfall_player::Observation& out) noexcept {
    native::Read read{image};std::array<unsigned char,16> prefix{};
    if(player==UINT32_MAX || !read.value(image+0x351C90,prefix) || prefix!=kWeakPrefix) return false;
    const auto weak=reinterpret_cast<void(__fastcall*)(native::Weak*,std::uint32_t)>(image+0x351C90);
    std::uint32_t again{UINT32_MAX};
    __try {
        const auto makeWeak=[weak](native::Weak& result,std::uint32_t handle) noexcept { weak(&result,handle); };
        if(!native_fallback::discover(read,player,makeWeak,out)) return false;
        return local_player(image,again) && again==player && read.weak(out.health);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace native_fallback_runtime

void poll() noexcept {
    namespace eater=state::activity::eater_of_worlds;
    const auto run=eater::native_run();
    // This function runs on the existing local-player game-frame poll. Reuse
    // the proved salted player health reader, including its retained corpse
    // lease while native control moves to the spectator Ghost.
    static nightfall_player::Cursor cursor{};
    static native_fallback::DiscoveryGate fallbackGate{};
    cursor.bind(run,0);fallbackGate.bind(run);if(!run) return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto now=GetTickCount64();
    std::uint32_t currentPlayer{UINT32_MAX};
    const bool currentPlayerKnown=native_fallback_runtime::local_player(image,currentPlayer);
    bool dead{};
    const bool retained=cursor.liveQualified && nightfall_player::retained(image,cursor.previous,dead);
    const auto retainedState=native_fallback::retained_decision(cursor.liveQualified,retained,dead,
        cursor.previous.entity,currentPlayer);
    if(retainedState.publish)
        eater::observe_player_health(run,cursor.previous.entity,retainedState.dead);
    nightfall_player::Observation observed{};
    if(nightfall_player::capture(image,observed)
        || (retainedState.discover && currentPlayerKnown && fallbackGate.claim(now)
            && native_fallback_runtime::capture(image,currentPlayer,observed))) {
        cursor.previous=observed;
        if(cursor.accepts(observed.dead)) eater::observe_player_health(run,observed.entity,observed.dead);
    }
}
}
