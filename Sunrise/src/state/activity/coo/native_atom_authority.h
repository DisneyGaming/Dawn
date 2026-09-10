// Type-2 client-atom programs (schema 80807DA1, field .6) and the type-24 authored object-channel
// body (schema 80804F40). Dawn has neither. Every width, bias, presence bit and field order below
// is transcribed from the accepted Sunrise host's proven encoders
//   middleware/bap/activity_message/scriptable_auth_body.h            (Type2Body, the ten lane
//     payload shapes, kType2KeyedLaneMaximumBitCount, kType2AtomCapacity, Type24Body)
//   middleware/bap/activity_message/activity_scriptable_auth_body_codec.cpp
//     (write_type2_root, write_type2_lane, write_lane_client_ref, encode_type2_body)
//   middleware/bap/activity_message/activity_scriptable_auth_fixed_body_codec.cpp (encode_type24)
//   middleware/bap/activity_message/combatant_auth.h                  (write_root, encode_spawn)
// and the selector-to-payload naming from its only caller
//   server/activity/mission/mission_script_lua_slot_api.cpp
//     (slot_run_atoms and the ten atom readers; slot_set_authored_channels).
// Nothing is inferred from the client: that host encodes these bodies, the client accepts them,
// and the strike's Thresher flies and its boss-room lasers cycle on them.
#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include "native_combatant_authority.h"
namespace sunrise::state::activity::coo::native_atom {

// ---- Type 2: the client-atom program -----------------------------------------------------------

// Publication gate. The host refuses any slot that is not an exact type-2 combatant, so a caller
// must match all four before writing; the SDK export carries the same numbers on every
// combatant_sensor row (sq_harvester__pilot: sense 2155904418, auth 2155904417).
inline constexpr std::uint8_t kSlotType=2;
inline constexpr std::uint32_t kComponentClass=0x8080834EU,kSenseSchema=0x80807DA2U,kAuthSchema=0x80807DA1U;
// SDK export auth_min_bits / auth_max_bits on every combatant_sensor row; the host's own field
// reader (read_type2_fields) refuses anything outside the same window.
inline constexpr std::size_t kMinimumAuthBits=11,kMaximumAuthBits=8228;

/** Canonical FNV sentinel of an unset registry key, shared by every absent nested ClientRef. */
inline constexpr std::uint32_t kAbsent=0x811C9DC5U;
/** Generations and revisions are 31-bit counters the client accepts only while positive. */
inline constexpr std::uint32_t kMaximumCounter=0x7FFFFFFFU;
/** .6.1 resume lane and .6.2 lane count are six bits each; the client runner holds 32 lanes. */
inline constexpr std::size_t kAtomCapacity=32;
inline constexpr std::uint8_t kMaximumSeed=0x3FU;

/** Exact 55-bit lane ClientRef: 32-bit key, 7-bit type biased by one, 16-bit index biased 32768. */
struct Ref final { std::uint32_t registry{kAbsent}; std::int8_t type{-1}; std::int16_t index{-1}; };

/**
 * Zero-based tag of the ten primary lane schemas; the wire tag is the enumerator plus one. This
 * order is Type2LanePrimary's own variant order in scriptable_auth_body.h, which is what selects
 * the child schema: RefByte, U32, Real32, RefByteBool, Empty, U6, U32Bool, U32Real32,
 * AlternateRefByte, TripleRef. mission_script_lua_slot_api.cpp binds the two reference-and-byte
 * schemas in that order too: atom_snap_to builds a RefByte (tag 0, client A9C140, which applies
 * the authored point's complete transform at once) and atom_face builds an AlternateRefByte
 * (tag 8, client A9BD70, which turns toward the path direction and keeps the actor's position).
 */
enum class Selector : std::uint8_t { snapTo,sequence,sleep,moveTo,trivial,controlFlag,
    setTemperament,setChannel,face,ability };
// snapTo and face carry identical payloads, so transposing them passes every width and bit-count
// check in this file and shows up only in game, as an actor that looks at a transform instead of
// being placed on it. Pin both tags to the host's variant indices.
static_assert(static_cast<std::uint8_t>(Selector::snapTo)==0
    && static_cast<std::uint8_t>(Selector::face)==8);
/** Child width of each primary schema in tag order (the host's kType2LanePrimaryBits). */
inline constexpr std::array<std::size_t,10> kSelectorBits{63,32,32,64,0,6,33,64,63,162};

/** Fixed bits of the .6 block: program generation, resume lane, lane count. No presence bit. */
inline constexpr std::size_t kProgramBlockBits=43;
/** Complete root around the .6 block when .5 carries the actor-control record, both of whose row
 * counts are zero. Includes the .0/.4 absence bits, the .6 presence bit and the .7 absence bit. */
inline constexpr std::size_t kControlRootBits=149;
/** Complete root around the .6 block when the body instead starts the actor: combatant_auth
 * write_root plus four absent optionals, the exact image encode_spawn produces. */
inline constexpr std::size_t kSpawnRootBits=42;
// Dawn's already-ported squad-member binding is the same enabled root with every optional absent:
// 1 + 31 + 2 + 3 + 1 + 4 = 42 bits, with .1 wire 0 and .2 wire 2. The squad-member control root
// written below repeats that same .1 and .2, so a bound member and the program that drives it
// agree bit for bit on the ownership prefix.
static_assert(native_combatant::kBindBits==kSpawnRootBits);
/** Widest lane, both tags and the quantized child included, without its presence bit. */
inline constexpr std::size_t kMaximumLaneBits=6U+162U+11U;
inline constexpr std::size_t kMaximumProgramBits=
    kControlRootBits+kProgramBlockBits+kAtomCapacity*(1U+kMaximumLaneBits);
inline constexpr std::size_t kMaximumProgramBytes=(kMaximumProgramBits+7U)/8U;
static_assert(kMaximumLaneBits==179 && kMaximumProgramBits==5952
    && kMaximumProgramBits<=kMaximumAuthBits);

/** .2 ownership wire, bias one. Wire 1 (logical 0) is the type-2-owned creation path and belongs
 * only to the spawn root; a program that binds an authored squad member writes wire 2. */
enum class Binding : std::uint8_t { selfOwned=0,squadMember=2 };

/**
 * One keyed lane. Only the fields the chosen selector reads reach the wire, so an Atom stays a
 * plain constexpr aggregate instead of a variant. A negative `quantized` leaves the secondary
 * child on its first empty schema, the only one the proven mission API ever selects.
 */
struct Atom final {
    Selector selector{Selector::trivial};
    Ref target{};                         // snapTo, moveTo, face, ability
    std::uint32_t value{};                // ref byte, sequence, control flag, temperament, channel
    float real{};                         // sleep seconds, set_channel value
    std::array<std::uint32_t,3> values{}; // ability identities
    std::int8_t mode{-1};                 // ability, wire is mode + 1
    std::int8_t marker{-1};               // ability, wire is marker + 128
    bool enabled{};                       // moveTo, setTemperament
    std::int16_t quantized{-1};
};

/** @return True when the value is not an infinity or a NaN, without pulling in <cmath>. */
[[nodiscard]] constexpr bool finite(float value) noexcept {
    return (std::bit_cast<std::uint32_t>(value)&0x7F800000U)!=0x7F800000U;
}
[[nodiscard]] constexpr bool valid(const Ref& ref) noexcept { return ref.type>=-1 && ref.type<=126; }
[[nodiscard]] constexpr bool valid(const Atom& atom) noexcept {
    if(static_cast<std::size_t>(atom.selector)>=kSelectorBits.size() || atom.quantized>0x7FF) { return false; }
    switch(atom.selector) {
    case Selector::snapTo: case Selector::face: case Selector::moveTo:
        return valid(atom.target) && atom.value<=0xFFU;
    case Selector::sleep: case Selector::setChannel: return finite(atom.real);
    case Selector::controlFlag: return atom.value<=0x3FU;
    case Selector::ability: return valid(atom.target) && atom.mode>=-1 && atom.mode<=6;
    default: return true; // sequence and setTemperament take the whole 32-bit field
    }
}
/** @return Meaningful width of one lane, both tags included but not its presence bit. */
[[nodiscard]] constexpr std::size_t lane_bits(const Atom& atom) noexcept {
    return 6U+kSelectorBits[static_cast<std::size_t>(atom.selector)]+(atom.quantized<0?0U:11U);
}

// Native selector 0 immediately applies the authored point's complete transform (client A9C140).
// This is the atom that puts the dropship on its drop-off marker.
[[nodiscard]] constexpr Atom snap_to(const Ref& target,std::uint8_t value=0) noexcept {
    Atom atom{}; atom.selector=Selector::snapTo; atom.target=target; atom.value=value; return atom;
}
[[nodiscard]] constexpr Atom move_to(const Ref& target,std::uint8_t value,bool enabled) noexcept {
    Atom atom{}; atom.selector=Selector::moveTo; atom.target=target; atom.value=value;
    atom.enabled=enabled; return atom;
}
// Native selector 8 turns toward the path direction while retaining position (client A9BD70).
[[nodiscard]] constexpr Atom face(const Ref& target,std::uint8_t value=0) noexcept {
    Atom atom{}; atom.selector=Selector::face; atom.target=target; atom.value=value; return atom;
}
[[nodiscard]] constexpr Atom sequence(std::uint32_t value) noexcept {
    Atom atom{}; atom.selector=Selector::sequence; atom.value=value; return atom;
}
[[nodiscard]] constexpr Atom sleep(float seconds) noexcept {
    Atom atom{}; atom.selector=Selector::sleep; atom.real=seconds; return atom;
}
[[nodiscard]] constexpr Atom set_channel(std::uint32_t channel,float value) noexcept {
    Atom atom{}; atom.selector=Selector::setChannel; atom.value=channel; atom.real=value; return atom;
}
[[nodiscard]] constexpr Atom set_temperament(std::uint32_t identity,bool enabled) noexcept {
    Atom atom{}; atom.selector=Selector::setTemperament; atom.value=identity; atom.enabled=enabled;
    return atom;
}
[[nodiscard]] constexpr Atom control_flag(std::uint8_t value) noexcept {
    Atom atom{}; atom.selector=Selector::controlFlag; atom.value=value; return atom;
}
[[nodiscard]] constexpr Atom trivial() noexcept { return Atom{}; }
/** Custom action with no spatial target. atom_ability leaves the reference absent, the mode at -1
 * and, with no marker supplied either, the marker at -1; the third identity is the same sentinel
 * the proven mission passes literally. */
[[nodiscard]] constexpr Atom ability(std::uint32_t group,std::uint32_t name) noexcept {
    Atom atom{}; atom.selector=Selector::ability; atom.values={group,name,kAbsent}; return atom;
}
/** Custom action anchored on a target. atom_ability defaults a targeted action's mode to 0 and
 * takes its marker from the same 8-bit field the other reference lanes use, biased by 128. */
[[nodiscard]] constexpr Atom ability_at(std::uint32_t group,std::uint32_t name,const Ref& target,
    std::int8_t mode=0,std::int8_t marker=-1) noexcept {
    Atom atom{}; atom.selector=Selector::ability; atom.values={group,name,kAbsent};
    atom.target=target; atom.mode=mode; atom.marker=marker; return atom;
}

/**
 * A complete 32-lane program. `generation` is the .5 actor-control revision and `revision` the .6
 * program generation, exactly as slot_run_atoms splits them: a bare `generation` seeds both, and a
 * rising `revision` restarts the program without recreating its actor. `spawn` replaces the control
 * root with the creation root, the only body that can start the actor and its first program in one
 * publication; that creation root carries the same `generation`, so the two never diverge.
 */
struct Program final {
    std::array<Atom,kAtomCapacity> atoms{};
    std::uint32_t generation{};
    std::uint32_t revision{};
    std::uint8_t count{};
    std::uint8_t progressSeed{};
    Binding binding{Binding::selfOwned};
    bool spawn{};
};

template<class Writer>[[nodiscard]] bool write_atom(Writer& writer,const Atom& atom) noexcept {
    if(!valid(atom)) { return false; }
    const auto begin=writer.bit_count();
    const auto reference=[&writer](const Ref& ref) noexcept {
        return writer.write(ref.registry,32)
            && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.type)+1),7)
            && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.index)+32768),16);
    };
    // Presence bit, primary tag, then the secondary tag: wire 1 is the first empty child, wire 3
    // selects the 11-bit quantized child that trails the primary body.
    bool ok=writer.write(1,1)
        && writer.write(static_cast<std::uint32_t>(atom.selector)+1U,4)
        && writer.write(atom.quantized<0?1U:3U,2);
    switch(atom.selector) {
    case Selector::snapTo: case Selector::face:
        ok=ok && reference(atom.target) && writer.write(atom.value,8); break;
    case Selector::moveTo:
        ok=ok && reference(atom.target) && writer.write(atom.value,8)
            && writer.write(atom.enabled?1U:0U,1); break;
    case Selector::sequence: ok=ok && writer.write(atom.value,32); break;
    case Selector::sleep: ok=ok && writer.write(std::bit_cast<std::uint32_t>(atom.real),32); break;
    case Selector::trivial: break;
    case Selector::controlFlag: ok=ok && writer.write(atom.value,6); break;
    case Selector::setTemperament:
        ok=ok && writer.write(atom.value,32) && writer.write(atom.enabled?1U:0U,1); break;
    case Selector::setChannel:
        ok=ok && writer.write(atom.value,32)
            && writer.write(std::bit_cast<std::uint32_t>(atom.real),32); break;
    case Selector::ability:
        for(const auto identity:atom.values) { ok=ok && writer.write(identity,32); }
        ok=ok && reference(atom.target)
            && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(atom.mode)+1),3)
            && writer.write(static_cast<std::uint32_t>(static_cast<std::int32_t>(atom.marker)+128),8);
        break;
    default: return false;
    }
    if(ok && atom.quantized>=0) { ok=writer.write(static_cast<std::uint32_t>(atom.quantized),11); }
    return ok && writer.bit_count()-begin==1U+lane_bits(atom);
}

/** Writes the complete eight-field type-2 body: the root first, then the .6 atom program. */
template<class Writer>[[nodiscard]] bool write_program(Writer& writer,const Program& program,
    std::span<const Ref> cargo={},std::uint32_t deliveryRevision=0) noexcept {
    // slot_run_atoms: one to 32 lanes, both counters positive 31-bit, the resume lane inside the
    // program, and a spawning program only ever on the self binding.
    if(program.count==0 || program.count>kAtomCapacity
        || program.generation==0 || program.generation>kMaximumCounter
        || program.revision==0 || program.revision>kMaximumCounter
        || program.progressSeed>kMaximumSeed || program.progressSeed>program.count
        || (program.spawn && program.binding!=Binding::selfOwned)
        || cargo.size()>8 || (cargo.empty()!=(deliveryRevision==0))
        || deliveryRevision>kMaximumCounter) { return false; }
    for(std::size_t index=0;index<cargo.size();++index) {
        if(cargo[index].registry==0 || cargo[index].registry==kAbsent
            || cargo[index].type!=1 || cargo[index].index<0) { return false; }
        for(std::size_t prior=0;prior<index;++prior) {
            if(cargo[index].registry==cargo[prior].registry
                && cargo[index].index==cargo[prior].index) { return false; }
        }
    }
    std::size_t lanes=0;
    for(std::size_t index=0;index<program.count;++index) {
        if(!valid(program.atoms[index])) { return false; }
        lanes+=1U+lane_bits(program.atoms[index]);
    }
    const auto begin=writer.bit_count();
    bool ok=false;
    if(program.spawn) {
        // combatant_auth write_root plus encode_spawn's tail: .0 present with the generation, .1
        // wire 1, .2 wire 1 (the type-2-owned creation path), .3 enabled, .4 and .5 absent. The
        // host publishes creation and program as one composed body that keeps every creation field
        // but .6, so a spawning program carries no control block of its own.
        ok=writer.write(1,1) && writer.write(program.generation,31)
            && writer.write(1,2) && writer.write(1,3) && writer.write(1,1)
            && writer.write(0,1) && writer.write(0,1);
    } else {
        // .0 absent, .1 wire 0 (logical -1, which keeps the actor), .2 the ownership wire, .3 the
        // actor-backed master enable, .4 absent, .5 present. run_atoms builds a default channel
        // state, so both row counts are zero and the nested placement ClientRef stays unset.
        ok=writer.write(0,1) && writer.write(0,2)
            && writer.write(static_cast<std::uint32_t>(program.binding),3)
            && writer.write(1,1) && writer.write(0,1)
            && writer.write(1,1) && writer.write(program.generation,31)
            && writer.write(0,6) && writer.write(0,6) && writer.write(0,3)
            && writer.write(kAbsent,32) && writer.write(0,7) && writer.write(32767U,16)
            && writer.write(0,32) && writer.write(0,5);
    }
    ok=ok && writer.write(1,1) && writer.write(program.revision,31)
        && writer.write(program.progressSeed,6) && writer.write(program.count,6);
    for(std::size_t index=0;ok && index<program.count;++index) {
        ok=write_atom(writer,program.atoms[index]);
    }
    // A replacement body retains both fields: clearing .6 while publishing .7 cancels the path.
    ok=ok && writer.write(cargo.empty()?0U:1U,1);
    if(!cargo.empty()) {
        ok=ok && writer.write(cargo.size(),4);
        for(const auto& squad:cargo) {
            ok=ok && writer.write(squad.registry,32) && writer.write(2,7)
                && writer.write(static_cast<std::uint32_t>(squad.index)+32768U,16);
        }
        ok=ok && writer.write(deliveryRevision,31);
    }
    return ok
        && writer.bit_count()-begin
            ==(program.spawn?kSpawnRootBits:kControlRootBits)+kProgramBlockBits+lanes
                +(cargo.empty()?0U:35U+55U*cargo.size());
}

/** Disables an actor on a fresh spawn revision and clears its retained delivery subscription. */
template<class Writer>[[nodiscard]] bool write_retirement(Writer& writer,
    std::uint32_t generation) noexcept {
    if(generation==0 || generation>kMaximumCounter) { return false; }
    return writer.write(1,1) && writer.write(generation,31) && writer.write(1,2)
        && writer.write(1,3) && writer.write(0,1) && writer.write(0,1)
        && writer.write(0,1) && writer.write(0,1) && writer.write(1,1)
        && writer.write(0,4) && writer.write(generation,31);
}

// ---- Type 24: the authored object-channel body -------------------------------------------------

// Publication gate, from slot_set_authored_channels and matched by every channel_sensor row of the
// SDK export (ch_laser_rm1_b1[0]: sense 2155892541, auth 2155892544, min 3 bits, max 387).
inline constexpr std::uint8_t kChannelSlotType=24;
inline constexpr std::uint32_t kChannelComponentClass=0x80804F3BU,kChannelSenseSchema=0x80804F3DU,
    kChannelAuthSchema=0x80804F40U;
inline constexpr std::size_t kChannelCapacity=4;
inline constexpr std::size_t kMaximumChannelBits=3U+96U*kChannelCapacity;
inline constexpr std::size_t kMaximumChannelBytes=(kMaximumChannelBits+7U)/8U;
static_assert(kMaximumChannelBits==387); // the SDK export's auth_max_bits on every ch_ row
/** The authored value range the host admits. */
inline constexpr float kMinimumChannelValue=-100.F,kMaximumChannelValue=100.F;

/**
 * Type-24 authored object channels: the row count, then that many revision/value/blend triples.
 * Every declared row of the target's native channel table is supplied together; there is no
 * implicit clearing of an omitted row, and the count belongs to the live published roster, not to
 * the caller, so it arrives as the span's length.
 * @param revision Host-owned per-slot output revision. The native controller compares revisions
 *        for equality, so a replayed value is ignored; the mission script sends a zero placeholder
 *        that host_runtime_scriptable overwrites with its own counter while staging.
 * @param blend Seconds the native controller takes to reach the value. boss.lua sends none.
 */
template<class Writer>[[nodiscard]] bool write_channels(Writer& writer,std::span<const float> values,
    std::int32_t revision,float blend=0.F) noexcept {
    if(values.empty() || values.size()>kChannelCapacity || revision<0
        || !finite(blend) || blend<0.F) { return false; }
    for(const auto value:values) {
        if(!finite(value) || value<kMinimumChannelValue || value>kMaximumChannelValue) { return false; }
    }
    const auto begin=writer.bit_count();
    bool ok=writer.write(static_cast<std::uint32_t>(values.size()),3);
    for(const auto value:values) {
        // The signed revision stores zero at the middle of its unsigned wire range.
        ok=ok && writer.write(static_cast<std::uint32_t>(revision)+0x80000000U,32)
            && writer.write(std::bit_cast<std::uint32_t>(value),32)
            && writer.write(std::bit_cast<std::uint32_t>(blend),32);
    }
    return ok && writer.bit_count()-begin==3U+96U*values.size();
}
/** @return Width one body of `count` rows occupies, for a caller sizing its own buffer. */
[[nodiscard]] constexpr std::size_t channel_bits(std::size_t count) noexcept { return 3U+96U*count; }

// ---- Tree of Probabilities (strike_pact, scenario 80F54AE7) -------------------------------------

// strike_pact/populations.lua: the pilot's action definition 80C0E599 puts these names in its exact
// "dropship" group. An empty group fails the native lookup; it is not a wildcard.
inline constexpr std::uint32_t kThresherActionGroup=0x07EBF354U,kThresherEnter=0x4482A76DU,
    kThresherExit=0x7D0D39A9U;
// strike_pact/populations.lua: the ship's own authored doors channel. The program drives it because
// the delivery component's optional door programs are unbound.
inline constexpr std::uint32_t kThresherDoors=0x80296344U;
// SDK export mission.Slot, object tag 80F54E07, whose published registry key is A5F083B5. The pilot
// is the type-2 slot these programs are published to; the two type-58 points are lane targets.
inline constexpr std::uint32_t kLedgeRegistry=0xA5F083B5U;
inline constexpr std::uint16_t kThresherPilotSlot=21;          // sq_harvester__pilot, type 2
inline constexpr Ref kThresherEntry{kLedgeRegistry,58,295};    // cps_harvester_entry
inline constexpr Ref kThresherExitPath{kLedgeRegistry,58,296}; // cps_harvester_exit

/**
 * strike_pact/populations.lua on_stage, stage ledge_final: generation 1, revision 1, self binding,
 * spawning, four lanes. Controller 80FE21CE rebases arrival root motion around the entry's terminal
 * transform, so the snap establishes that transform before the action rather than the program
 * moving there from a ground anchor. The doors close for the flight and open on arrival.
 */
[[nodiscard]] constexpr Program thresher_arrival() noexcept {
    Program program{};
    program.atoms[0]=set_channel(kThresherDoors,0.F);
    program.atoms[1]=snap_to(kThresherEntry,0);
    program.atoms[2]=ability(kThresherActionGroup,kThresherEnter);
    program.atoms[3]=set_channel(kThresherDoors,1.F);
    program.count=4;
    program.generation=1;
    program.revision=1;
    program.spawn=true;
    return program;
}

/**
 * strike_pact/populations.lua depart: generation 1, revision 2, self binding, no spawn, three
 * lanes. Departure starts at the actor's current transform, so it moves to the exit path instead
 * of snapping to it.
 */
[[nodiscard]] constexpr Program thresher_departure() noexcept {
    Program program{};
    program.atoms[0]=set_channel(kThresherDoors,0.F);
    program.atoms[1]=move_to(kThresherExitPath,1,true);
    program.atoms[2]=ability(kThresherActionGroup,kThresherExit);
    program.count=3;
    program.generation=1;
    program.revision=2;
    return program;
}

// strike_pact/populations.lua on_combatant_state, the gates the two programs are watched on: only
// spawn revision 1 counts; arrival is program revision 1 reaching state 4, which releases the
// passenger delivery. Departure normally detaches during its last action (revision 2, state 2),
// before a state-3 completion can be reported. The actor service also retains that qualified
// detachment; the optional state-3 path requests retirement on generation 2. Departure itself is
// withheld until delivery completion and every reserved passenger's population receipt.
inline constexpr std::uint32_t kThresherSpawnRevision=1;
inline constexpr std::uint32_t kArrivalRevision=1,kDepartureRevision=2;
inline constexpr std::uint32_t kArrivalCompleteState=4,kDepartureCompleteState=3;
inline constexpr std::uint32_t kThresherRetireGeneration=2;

// strike_pact/boss.lua: channel values span -100..100, full drive on and none off.
inline constexpr float kLaserOn=100.F,kLaserOff=0.F;
// strike_pact/boss.lua: reconstructed host intervals, not constants recovered from the client
// profile. High is the native 2.5s warmup plus 7s fully red; low is the 1.5s fade plus a 2s dark
// hold. One durable host clock drives both banks of every prepared room together, so a room that
// joins while another already owns the cycle waits for the next rising edge and keeps its full
// warning interval; both banks of a room always receive the same value.
inline constexpr std::uint32_t kLaserHighMs=2500+7000,kLaserLowMs=1500+2000;
inline constexpr std::size_t kLaserRooms=3,kLaserBanks=2;
// SDK export: the forty ch_laser rows of object 80F54E07 are contiguous, registry A5F083B5, slot
// type 24, indices 189..228. Rooms 1 and 2 hold five slots per bank, room 3 holds ten.
struct LaserBank final { std::uint16_t first,count; };
inline constexpr std::array<std::array<LaserBank,kLaserBanks>,kLaserRooms> kLaserBankSlots{{
    {{{189,5},{194,5}}},   // ch_laser_rm1_b1[0..4], ch_laser_rm1_b2[0..4]
    {{{199,5},{204,5}}},   // ch_laser_rm2_b1[0..4], ch_laser_rm2_b2[0..4]
    {{{209,10},{219,10}}}, // ch_laser_rm3_b1[0..9], ch_laser_rm3_b2[0..9]
}};

// Strike laser membership and per-slot row counts are recovered in strike_pact/catalog_boss.h.
// Other missions must supply their own verified native channel counts.
// UNRESOLVED: the per-slot type-24 row count for every other channel target, for the same reason.
// UNRESOLVED: the second empty secondary lane child (secondary wire tag 2). The mission atom API
// only ever emits wire tag 1 or the quantized wire tag 3, so nothing establishes its effect, and
// Atom therefore cannot select it.
// UNRESOLVED: what the 8-bit byte of the snapTo, moveTo and face lanes selects, and what ability
// modes 0 through 6 mean. The proven Lua passes 0 for the Thresher's entry snap, 1 for its exit
// move, and only ever the untargeted mode -1; the Sunrise source fixes the field widths and biases
// but not what the values choose.
// UNRESOLVED: the runtime effect of the trivial, controlFlag and setTemperament selectors. Their
// payload shapes are established by the codec, but no lane in this strike uses them and no client
// address is recorded for any of the three.

} // namespace sunrise::state::activity::coo::native_atom
