#include "../../../core/settings/settings.h"
#include "../internal.h"

namespace dawn::steam::interfaces::methods {

/** @return Persona name from settings. It lasts for the whole process. */
const char* persona_name([[maybe_unused]] void* self) noexcept {
    return core::settings::get().steam.user.personaName.data();
}

/** Dawn has no remote Steam recipient capable of accepting an invitation. */
bool invite_user_to_game([[maybe_unused]] void* self,
                         [[maybe_unused]] std::uint64_t friendSteamId,
                         [[maybe_unused]] const char* connectString) noexcept {
    return false;
}

} // namespace dawn::steam::interfaces::methods
