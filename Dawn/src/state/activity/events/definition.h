#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::state::activity::events {

/**
 * The Tower's seasonal events, as the operator picks them.
 *
 * Each event is a handful of per-bubble roster groups, one registry key per area it dresses. The
 * map below ties each key to its event and area. It was attributed on 2026-09-04 by publishing one
 * carrier per area at a time and walking the Tower.
 */
enum class Event : std::uint8_t {
    festivalOfTheLost,
    dawning,
    ironBanner,
    crimsonDays,
    solstice,
    trialsSaint14,
    count,
};

inline constexpr std::size_t kEventCount = static_cast<std::size_t>(Event::count);

/** Name of each event, in `Event` order. */
inline constexpr std::array<const char*, kEventCount> kEventNames = {
    "Festival of the Lost",
    "The Dawning",
    "Iron Banner",
    "Crimson Days",
    "Solstice of Heroes",
    "Trials of Osiris / Saint-14",
};

/** Where each event shows, in `Event` order. */
inline constexpr std::array<const char*, kEventCount> kEventAreas = {
    "Courtyard, Bazaar, Hangar and Annex.",
    "Courtyard, snow in the Bazaar and Hangar, and the Hangar rink with its ball game, goals and "
    "scoreboards. Bungie dressed no Dawning in the Annex. Also dresses the Farm.",
    "Courtyard only.",
    "Courtyard, and the Farm.",
    "Courtyard only.",
    "The Saint-14 ship in the Hangar.",
};

/** One registry key, the event it carries and the area it dresses. */
struct EventKey {
    std::uint32_t key{};
    Event event{};
    const char* area{};
};

/** Every event registry key that the authoritative Tower/Farm roster may withhold. */
inline constexpr std::array<EventKey, 14> kEventKeys = {{
    {0x7C6DE64FU, Event::festivalOfTheLost, "Courtyard"},
    {0xFC6B8707U, Event::festivalOfTheLost, "Bazaar"},
    {0xEE34BBABU, Event::festivalOfTheLost, "Hangar"},
    {0x6D3740C6U, Event::festivalOfTheLost, "Annex"},
    {0x00ACD208U, Event::dawning, "Courtyard"},
    {0x2F2B8D00U, Event::dawning, "Bazaar snow"},
    {0x6E087824U, Event::dawning, "Hangar snow"},
    {0xDA989AA3U, Event::dawning, "Hangar rink"},
    {0xC140FF19U, Event::dawning, "Farm"},
    {0x4488AD94U, Event::crimsonDays, "Farm"},
    {0x27060E6CU, Event::ironBanner, "Courtyard"},
    {0x6CEFCC01U, Event::crimsonDays, "Courtyard"},
    {0xD5B68262U, Event::solstice, "Courtyard"},
    {0x9052672CU, Event::trialsSaint14, "Hangar"},
}};

/** One family-5 flag slot associated with an event. */
struct EventFlag {
    std::uint16_t slot{};
    Event event{};
};

/** One family-5 value slot associated with an event. */
struct EventValue {
    std::uint16_t slot{};
    std::int32_t value{};
    Event event{};
};

/** Logical flag value the client reads as set. */
inline constexpr std::uint8_t kFlagLive = 2;

inline constexpr std::array<EventFlag, 8> kEventFlags = {{
    {946, Event::festivalOfTheLost},
    {947, Event::festivalOfTheLost},
    {950, Event::festivalOfTheLost},
    {958, Event::festivalOfTheLost}, // Tower graph81327CF7 node4680F506:946 AND958 ANDpool133.
    {779, Event::dawning},
    {777, Event::crimsonDays},
    {7201, Event::crimsonDays},
    {884, Event::solstice},
}};

/** Guardian Games has an identity flag but no admitted dressing group. */
inline constexpr std::uint16_t kGuardianGamesFlag = 992;

inline constexpr std::array<EventValue, 3> kEventValues = {{
    {8137, 1, Event::festivalOfTheLost},
    {12496, 1, Event::dawning},
    {8482, 1, Event::crimsonDays},
}};

/** Which event theme the Tower is asked to use. */
enum class Music : std::uint8_t {
    followEvents,
    festivalOfTheLost,
    dawning,
    crimsonDays,
    solstice,
    guardianGames,
    count,
};

inline constexpr std::size_t kMusicCount = static_cast<std::size_t>(Music::count);

/** Name of each music choice, in `Music` order. */
inline constexpr std::array<const char*, kMusicCount> kMusicNames = {
    "Follow the shown events",
    "Festival of the Lost",
    "The Dawning",
    "Crimson Days",
    "Solstice of Heroes",
    "Guardian Games",
};

/** The token each music choice writes to `event_music.txt`, in `Music` order. */
inline constexpr std::array<const char*, kMusicCount> kMusicTokens = {
    "auto", "festival", "dawning", "crimson", "solstice", "guardian_games",
};

/** @return The identity flag that carries an event's theme, or 0 for an event without one. */
[[nodiscard]] constexpr std::uint16_t identity_flag(Event event) noexcept {
    switch (event) {
    case Event::festivalOfTheLost: return 946;
    case Event::dawning: return 779;
    case Event::crimsonDays: return 777;
    case Event::solstice: return 884;
    default: return 0;
    }
}

/** @return The event whose theme a choice names, or `Event::count` for `followEvents`. */
[[nodiscard]] constexpr Event music_event(Music music) noexcept {
    switch (music) {
    case Music::festivalOfTheLost: return Event::festivalOfTheLost;
    case Music::dawning: return Event::dawning;
    case Music::crimsonDays: return Event::crimsonDays;
    case Music::solstice: return Event::solstice;
    default: return Event::count;
    }
}

/** Room for every mapped key and for hand-edited keys that the map does not name. */
inline constexpr std::size_t kKeyCapacity = 64;

/** A small set of registry keys with no duplicates. Zero is not a key. */
struct KeySet {
    std::array<std::uint32_t, kKeyCapacity> keys{};
    std::size_t count{};

    /** @return True when the key is in the set. */
    [[nodiscard]] constexpr bool contains(std::uint32_t key) const noexcept {
        for (std::size_t index = 0; index < count; ++index) {
            if (keys[index] == key) {
                return true;
            }
        }
        return false;
    }

    /** @return True when the key is in the set afterwards. */
    constexpr bool insert(std::uint32_t key) noexcept {
        if (key == 0) {
            return false;
        }
        if (contains(key)) {
            return true;
        }
        if (count == keys.size()) {
            return false;
        }
        keys[count] = key;
        ++count;
        return true;
    }

    /** Takes one key out. Order is not kept. */
    constexpr void erase(std::uint32_t key) noexcept {
        for (std::size_t index = 0; index < count; ++index) {
            if (keys[index] == key) {
                --count;
                keys[index] = keys[count];
                keys[count] = 0;
                return;
            }
        }
    }
};

/** @return True when both sets hold the same keys, in any order. */
[[nodiscard]] constexpr bool same_keys(const KeySet& left, const KeySet& right) noexcept {
    if (left.count != right.count) {
        return false;
    }
    for (std::size_t index = 0; index < left.count; ++index) {
        if (!right.contains(left.keys[index])) {
            return false;
        }
    }
    return true;
}

/**
 * @param withheld Keys the roster withholds.
 * @param event Event asked about.
 * @return True while none of the event's keys are withheld.
 */
[[nodiscard]] constexpr bool shown(const KeySet& withheld, Event event) noexcept {
    for (const EventKey& entry : kEventKeys) {
        if (entry.event == event && withheld.contains(entry.key)) {
            return false;
        }
    }
    return true;
}

/** @return True when a mapped event flag is live for the selected roster and music. */
[[nodiscard]] constexpr bool flag_live(const KeySet& withheld,
                                       Music music,
                                       const EventFlag& entry) noexcept {
    const Event chosen = music_event(music);
    const bool identity = entry.slot == identity_flag(entry.event);
    return identity && chosen != Event::count ? entry.event == chosen
                                              : shown(withheld, entry.event);
}

/** @return True when a mapped event value is live for the selected roster. */
[[nodiscard]] constexpr bool value_live(const KeySet& withheld, const EventValue& entry) noexcept {
    return shown(withheld, entry.event);
}

/**
 * Takes every key of one event out of the withheld set, or puts them all in. Keys the map does not
 * name are left as they are.
 * @param withheld Keys the roster withholds, updated in place.
 * @param event Event to show or hide.
 * @param visible True to show the event.
 * @return True when every key of the event fitted the set.
 */
constexpr bool show(KeySet& withheld, Event event, bool visible) noexcept {
    bool complete = true;
    for (const EventKey& entry : kEventKeys) {
        if (entry.event != event) {
            continue;
        }
        if (visible) {
            withheld.erase(entry.key);
        } else {
            complete = withheld.insert(entry.key) && complete;
        }
    }
    return complete;
}

/** @param key A registry key. @return Its event map entry, or null when it is not an event key. */
[[nodiscard]] constexpr const EventKey* find_key(std::uint32_t key) noexcept {
    for (const EventKey& entry : kEventKeys) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

/**
 * Pure roster-selection predicate. Unknown hand-edited keys remain retained and never suppress a
 * group; only keys in the recovered event map are eligible for authoritative filtering.
 */
[[nodiscard]] constexpr bool should_withhold(const KeySet& withheld,
                                             std::uint32_t key) noexcept {
    return find_key(key) != nullptr && withheld.contains(key);
}

/** The missing-file default: only Festival of the Lost is shown. */
inline constexpr KeySet kDefaultWithheldKeys = [] {
    KeySet result{};
    for (std::size_t index = 0; index < kEventCount; ++index) {
        (void)show(result, static_cast<Event>(index), false);
    }
    (void)show(result, Event::festivalOfTheLost, true);
    return result;
}();

static_assert(kDefaultWithheldKeys.count == 10);
static_assert(!should_withhold(kDefaultWithheldKeys, 0x7C6DE64FU));
static_assert(should_withhold(kDefaultWithheldKeys, 0x00ACD208U));
static_assert(!should_withhold(kDefaultWithheldKeys, 0xE0000001U));
static_assert(identity_flag(Event::festivalOfTheLost) == 946);
static_assert(kEventFlags[0].slot == 946 && kEventFlags[1].slot == 947
              && kEventFlags[2].slot == 950);
static_assert(kEventValues[0].slot == 8137 && kEventValues[0].value == 1);
static_assert(flag_live(kDefaultWithheldKeys, Music::followEvents, kEventFlags[0]));
static_assert(flag_live(kDefaultWithheldKeys, Music::followEvents, kEventFlags[1]));
static_assert(flag_live(kDefaultWithheldKeys, Music::followEvents, kEventFlags[2]));
static_assert(value_live(kDefaultWithheldKeys, kEventValues[0]));

} // namespace dawn::state::activity::events
