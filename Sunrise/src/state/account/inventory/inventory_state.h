#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sunrise::state::account::inventory {

/** Authored equipment exposes the 16 named slots the first State supports. */
enum class EquipmentSlot : std::uint8_t {
    kinetic,
    energy,
    heavy,
    helmet,
    gauntlets,
    chest,
    legs,
    classItem,
    ghost,
    vehicle,
    ship,
    subclass,
    clanBanner,
    emblem,
    emote,
    finisher,
    count,
};

/** One fixed entry exists for every semantic equipment slot. */
inline constexpr std::size_t kEquipmentSlotCount = static_cast<std::size_t>(EquipmentSlot::count);
/** An authored item can pick at most 12 ordinary socket lanes. */
inline constexpr std::size_t kPlugCapacity = 12;
/** An item instance carries 8 native random-roll bytes. */
inline constexpr std::size_t kRandomRollCapacity = 8;
/** The engine no-definition hash cannot identify an authored item or plug. */
inline constexpr std::uint32_t kNoDefinitionHash = 0x811C9DC5U;

/** Says whether Middleware uses native socket defaults or authored lanes. */
enum class SocketPolicy : std::uint8_t {
    nativeDefaults,
    authored,
};

/** Fixed authored socket choices. An empty optional is an explicit empty lane. */
struct Sockets {
    SocketPolicy policy{SocketPolicy::nativeDefaults};
    std::array<std::optional<std::uint32_t>, kPlugCapacity> plugs{};
    std::size_t plugCount{};
    friend bool operator==(const Sockets&, const Sockets&) = default;
};

/** Account-wide stacks can occupy every row of the native 701-row profile inventory. */
inline constexpr std::size_t kProfileItemCapacity = 701;
/** The supported mod and shader profile bucket runs reserve 50 action-source rows each. */
inline constexpr std::size_t kProfileActionSourceCapacity = 100;
/** Runtime-owned SOIDs for profile stacks use a namespace separate from created item instances. */
inline constexpr std::uint64_t kFirstProfileItemInstanceSoid = 0x5000000000000001ULL;
/**
 * Native item-state bit the Client sets to lock one item against destruction.
 * Confirmed against the installed build by three observed lock and unlock transitions.
 */
inline constexpr std::uint32_t kLockedItemFlag = 0x1;
/** Native character rows include equipment, Pursuits, engrams and recovery. */
inline constexpr std::size_t kCharacterItemCapacity = 334;
inline constexpr std::uint8_t kPostmasterBucket = 34;

/** One authored account-wide item, placed by the inventory bucket its definition names. */
struct ProfileItem {
    /** Stable runtime identity required to materialize this row as an inventory action source. */
    std::uint64_t instanceSoid{};
    std::uint32_t definitionHash{};
    std::int32_t quantity{};
    /** Rising generation copied into the native row and matched by acquisition feedback. */
    std::int32_t mutationSerial{};
    friend bool operator==(const ProfileItem&, const ProfileItem&) = default;
};

/** One authored equipment item without native table or wire-layout fields. */
struct Item {
    std::uint64_t instanceSoid{};
    std::uint32_t definitionHash{};
    std::int32_t level{};
    std::int32_t quantity{};
    /** Rising per-character generation assigned whenever this item changes inventory rows. */
    std::int32_t mutationSerial{};
    /** Native accumulated item-state bits such as the finisher favorite marker. */
    std::uint32_t flags{};
    Sockets sockets;
    /** Recovery location; vector order preserves arrival age independently of mutations. */
    bool postmaster{};
    /**
     * Per-instance entropy the Client reduces to pick a plug for every socket whose plug set is
     * randomized: it takes one of these bytes, chosen by the socket entry's own selector byte,
     * modulo the plug set's row count. All-zero therefore pins every randomized socket to its
     * first plug, which is what a curated Collections pull wants.
     */
    std::array<std::uint8_t, kRandomRollCapacity> randomRoll{};
    /**
     * Bit per ordinary socket lane this instance rolled rather than taking defaults from its
     * definition. A rolled plug is drawn from the socket's randomized set, which the definition's
     * allowed pool never carries, so the socket route offers a rolled lane the rows named by
     * availablePlugRows on top of that pool. That is what lets a rolled lane be swapped to a
     * curated choice and back to the rolled plug, matching what the inspection grid offers.
     */
    std::uint16_t rolledLaneMask{};
    /**
     * Per-lane bitmask of the rows of that socket's randomized plug set this instance owns, one
     * bit per row in the lane's published pool order. Trait lanes own one row; other perk
     * columns may offer two. Zero retains the definition's curated choices.
     * The Client walks these bits before it falls back to the definition's own plugs, so this is
     * what keeps the hover preview on the same perk the inspection screen reads.
     */
    std::array<std::uint64_t, kPlugCapacity> availablePlugRows{};
    friend bool operator==(const Item&, const Item&) = default;
};

/** Ordered unequipped items placed into their native character-inventory bucket ranges. */
struct CharacterItems {
    std::array<Item, kCharacterItemCapacity> values{};
    std::size_t count{};
    friend bool operator==(const CharacterItems&, const CharacterItems&) = default;
};

/** One optional authored item for every semantic equipment slot. */
struct Equipment {
    std::array<std::optional<Item>, kEquipmentSlotCount> slots{};
    friend bool operator==(const Equipment&, const Equipment&) = default;
};

/**
 * Finds the semantic State slot for one exact JSON equipment name.
 * @param name Borrowed case-sensitive equipment name.
 * @return Matching slot, or no value for an unknown name.
 */
[[nodiscard]] std::optional<EquipmentSlot> slot_from_name(std::string_view name) noexcept;

/**
 * Checks the canonical socket policy and every authored plug hash.
 * @return True when the policy, count and fixed tail agree.
 */
[[nodiscard]] bool valid(const Sockets& sockets) noexcept;

/**
 * Checks one whole authored item without reading installed build data.
 * @return True when the id, scalar and socket fields are valid.
 */
[[nodiscard]] bool valid(const Item& item) noexcept;

/**
 * Checks every item present in the fixed semantic equipment array.
 * @return True when every used slot holds a whole item.
 */
[[nodiscard]] bool valid(const Equipment& equipment) noexcept;

/** Checks the used prefix and empty tail of one character's unequipped item array. */
[[nodiscard]] bool valid(const CharacterItems& items) noexcept;

} // namespace sunrise::state::account::inventory
