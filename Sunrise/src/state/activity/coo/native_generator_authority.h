// Type-37 map generator authority: schema 80805007 -> body 80805008, the publication that makes
// the Infinite Forest actually generate. Every width, bias and override bit below is transcribed
// from the accepted Sunrise host's proven encoder
//   middleware/bap/activity_message/activity_scriptable_auth_fixed_body_codec.cpp
//     encode_type37_body / decode_type37_body
// and from its only two callers
//   server/activity/mission/mission_script_lua_slot_api.cpp
//     slot_set_map_generator_enabled / slot_set_map_generator_activation.
// Nothing is inferred from the client: that host encodes this body, the client accepts it, and the
// strike generates on it. Dawn's parked stub (activity_sensor_auth_bodies_other_missions.cpp,
// write_generator_500b) instead serializes all 100 tiles of each record as a fixed array, which
// totals 11,350 bits with a zero tile count; the host decoder recomputes the expected width from
// that count and rejects any body whose length disagrees, so the stub's shape can never be
// accepted. The 8080500C tile array is variable: only tileCount entries are on the wire.
#pragma once
#include "../../../middleware/bap/activity_message/native/forest_generator_route.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
namespace sunrise::state::activity::coo::native_generator {
namespace wire=middleware::bap::activity_message::native::forest_generator;

// Publication gate. The host refuses any slot that is not an exact type-37 generator, so a caller
// must match all four before writing (mission_script_lua_slot_api.cpp), and the SDK export carries
// the same numbers on every map_generator_sensor row.
inline constexpr std::uint8_t kSlotType=37;
inline constexpr std::uint32_t kComponentClass=0x80804EF6U,kSenseSchema=0x80805006U,kAuthSchema=0x80805007U;

// Shape of body 80805008: two 8080500B records then one 80805009 activation tail.
inline constexpr std::size_t kRecordCount=2,kAnchorCount=4,kValueCount=5,kTopologyCount=2;
inline constexpr std::size_t kRegionCount=32,kGroupCount=64,kTileCapacity=100;

inline constexpr std::uint8_t kSeedWidth=32,kModeWidth=8,kCoordinateWidth=8,kRealWidth=32,kFlagWidth=1,
    kOverrideWidth=7,kValueWidth=32,kTileCountWidth=7,kTileWidth=16,kByteWidth=8;
// Signed schema fields store zero at the middle of their unsigned wire range.
inline constexpr std::uint32_t kCoordinateBias=128U,kModeBias=128U,kValueBias=0x80000000U,kTileBias=0x8000U;
// Per-record tile entry is three biased i16; the count prefix decides how many are serialized.
inline constexpr std::size_t kTileBits=3U*kTileWidth;
inline constexpr std::size_t kRecordBits=kSeedWidth+kModeWidth
    +kAnchorCount*(2U*kCoordinateWidth+kRealWidth+kFlagWidth)
    +kOverrideWidth+kFlagWidth+kTopologyCount*kRealWidth+kValueCount*kValueWidth+kTileCountWidth;
inline constexpr std::size_t kTailBits=kSeedWidth+(kRegionCount+kGroupCount)*kByteWidth;
// The width write_activation must produce; it checks itself against this before returning true.
inline constexpr std::size_t kActivationBits=kRecordCount*kRecordBits+kTailBits;
inline constexpr std::size_t kActivationBytes=(kActivationBits+7U)/8U;
// The SDK export gives auth_min_bits 1750 / auth_max_bits 11350 on every map_generator_sensor row;
// the minimum is exactly the no-tile body, the maximum exactly both records' tile arrays filled.
inline constexpr std::size_t kMinimumBits=1750,kMaximumBits=11350;
static_assert(kRecordBits==475 && kTailBits==800 && kActivationBits==kMinimumBits);
static_assert(kActivationBits+kRecordCount*kTileCapacity*kTileBits==kMaximumBits);
static_assert(kActivationBytes==219);

// Override mask at record +0x2C. A clear bit leaves that input to the authored worker definition,
// so unselected fields still travel on the wire carrying their neutral value.
inline constexpr std::uint8_t kOverrideSeedInput=0x01U; // worker reads record +0x00 only when set
inline constexpr std::uint8_t kOverrideAnchors=0x04U;   // the four 8080500F endpoints replace the recipe
inline constexpr std::uint8_t kOverrideEnabled=0x08U;   // the record's enabled bool drives the worker
inline constexpr std::uint8_t kOverrideMaximum=0x7FU;   // the field is 7 bits; the encoder refuses more
// UNRESOLVED: override bits 0x02, 0x10, 0x20 and 0x40 select something in the worker definition, but
// no proven Sunrise host path ever sets them, so their inputs are unknown. Left clear.

// -1 is the in-band "keep the authored default" sentinel for the five i32 record values; they carry
// no override bit of their own (slot_set_map_generator_activation).
inline constexpr std::int32_t kKeepAuthoredValue=-1;
// The authored-default sentinel for the two f32 topology inputs. Native10059A0
// reads record+30/+34, commits worker+950/+954 and passes them to FF2F80.
inline constexpr float kAuthoredTopology=-1.0F;
inline constexpr std::array<float,kTopologyCount> kAuthoredTopologies{kAuthoredTopology,kAuthoredTopology};

// Both the host encoder and its decoder refuse non-finite floats. std::isfinite is not constexpr and
// would pull in <cmath>; an all-ones exponent is the whole test.
[[nodiscard]] constexpr bool finite(float value) noexcept {
    return (std::bit_cast<std::uint32_t>(value)&0x7F800000U)!=0x7F800000U;
}

/** One 8080500F connection endpoint. Defaults are the encoder's authored-absent form. */
struct Anchor final {
    // UNRESOLVED: the grid frame these two signed bytes address. Only their wire form (i8, bias 128)
    // and their role as the endpoint's two selectors are established; the proven strike copies the
    // authored Forest B endpoint values rather than deriving them.
    std::int8_t column{-1};
    std::int8_t height{-1};
    // Route progress the solver assigns this endpoint. strike_pact.lua gives the exit the higher
    // progress so native navigation walks forward instead of treating every encounter as the goal.
    float progress{};
    bool enabled{};
};

/** The authored-absent anchor recipe and value bank, written whenever their override bit is clear. */
inline constexpr std::array<Anchor,kAnchorCount> kAuthoredAnchors{};
inline constexpr std::array<std::int32_t,kValueCount> kAuthoredValues{
    kKeepAuthoredValue,kKeepAuthoredValue,kKeepAuthoredValue,kKeepAuthoredValue,kKeepAuthoredValue};

/** One activation publication for an exact type-37 generator slot. */
struct Request final {
    // Record 0's enabled bool, always selected by kOverrideEnabled. False parks the worker.
    bool enabled{true};
    // The activation tail's seed is always published; it is what the client mirrors back on the
    // generator-state event, which is how a mission recognises its own layout.
    std::uint32_t seed{};
    // Also drive record +0x00 with that seed. Clear only to reproduce the host's plain
    // slot_set_map_generator_enabled, which runs the worker on its own recipe: the activation tail
    // alone cannot select a new layout.
    bool selectSeed{true};
    // Five biased i32 worker inputs. values[0] is the solver's encounter placement budget; the
    // Forest generators author it as zero, so a host that leaves it at -1 places no encounter.
    // UNRESOLVED: what values[1..4] select; no proven host path sets them.
    std::array<std::int32_t,kValueCount> values{kAuthoredValues};
    // Garden selects zero for both native solver inputs; other missions keep
    // the authored-density sentinels. This does not alter anchors or ownership.
    std::array<float,kTopologyCount> topology{kAuthoredTopologies};
    // Native +X, -X, +Y, -Y order (slot_set_map_generator_activation). All four or none.
    std::array<Anchor,kAnchorCount> anchors{kAuthoredAnchors};
    bool selectAnchors{};
    // 80805009 tail. The client mirrors it back as one completion byte per generated area and one
    // open byte per gateway (strike_pact.lua on_event_generator_state). Every proven publication
    // leaves both banks zero and reads the client's copy.
    // UNRESOLVED: the effect of publishing a non-zero bank; the host only ever sends zeros.
    std::array<std::uint8_t,kRegionCount> regions{};
    std::array<std::uint8_t,kGroupCount> groups{};

    [[nodiscard]] constexpr std::uint8_t overrides() const noexcept {
        return static_cast<std::uint8_t>(kOverrideEnabled|(selectSeed?kOverrideSeedInput:0U)
            |(selectAnchors?kOverrideAnchors:0U));
    }
};

/** Build the legacy request form from the shared, explicitly selected route. */
[[nodiscard]] constexpr bool build_route_request(const Request& source,const wire::Route& route,
                                                 Request& destination) noexcept {
    wire::State state{};
    if(!wire::resolve_route(route,state))return false;
    auto resolved=source;
    for(std::size_t i=0;i<kAnchorCount;++i)
        resolved.anchors[i]={state.primary.anchors[i].a,state.primary.anchors[i].b,
            state.primary.anchors[i].weight,state.primary.anchors[i].active};
    resolved.selectAnchors=true;
    destination=resolved;
    return true;
}

/** One 8080500B record. Bias arithmetic wraps in 32 bits exactly as the Sunrise encoder's does;
 *  the explicit masks only make the writer's low-bit truncation visible at the call site. */
template<class Writer>
[[nodiscard]] bool write_record(Writer& writer,std::uint32_t seedInput,
                                const std::array<Anchor,kAnchorCount>& anchors,
                                const std::array<std::int32_t,kValueCount>& values,
                                std::uint8_t overrides,bool enabled,
                                const std::array<float,kTopologyCount>& topology=kAuthoredTopologies) noexcept {
    if(overrides>kOverrideMaximum) { return false; }
    for(const auto value:topology) { if(!finite(value)) { return false; } }
    const auto begin=writer.bit_count();
    // UNRESOLVED: the i8 mode at record +0x04. No proven host path sets it, so it is written as the
    // neutral 0 (wire 128) that both host callers leave in place.
    if(!writer.write(seedInput,kSeedWidth) || !writer.write(kModeBias,kModeWidth)) { return false; }
    for(const auto& anchor:anchors) {
        if(!finite(anchor.progress)
            || !writer.write((static_cast<std::uint32_t>(anchor.column)+kCoordinateBias)&0xFFU,kCoordinateWidth)
            || !writer.write((static_cast<std::uint32_t>(anchor.height)+kCoordinateBias)&0xFFU,kCoordinateWidth)
            || !writer.write(std::bit_cast<std::uint32_t>(anchor.progress),kRealWidth)
            || !writer.write(anchor.enabled?1U:0U,kFlagWidth)) { return false; }
    }
    if(!writer.write(overrides,kOverrideWidth) || !writer.write(enabled?1U:0U,kFlagWidth)) { return false; }
    for(std::size_t index=0;index<kTopologyCount;++index) {
        if(!writer.write(std::bit_cast<std::uint32_t>(topology[index]),kRealWidth)) { return false; }
    }
    for(const auto value:values) {
        if(!writer.write(std::bit_cast<std::uint32_t>(value)+kValueBias,kValueWidth)) { return false; }
    }
    // 8080500D's count governs its 8080500C array; zero means the array is absent from the wire.
    // No proven host publishes tiles, and the meaning of a tile's three i16 fields is UNRESOLVED,
    // so this encoder has no way to express one.
    return writer.write(0U,kTileCountWidth) && writer.bit_count()-begin==kRecordBits;
}

/** Publishes one complete 80805008 body: record 0 from the request, record 1 authored, then the tail. */
template<class Writer>
[[nodiscard]] bool write_activation(Writer& writer,const Request& request) noexcept {
    const auto begin=writer.bit_count();
    if(!write_record(writer,request.selectSeed?request.seed:0U,
                     request.selectAnchors?request.anchors:kAuthoredAnchors,request.values,
                     request.overrides(),request.enabled,request.topology)) { return false; }
    // The second record is transition-scoped. Neither proven host path touches it, so it goes out
    // with a clear mask and every input at its authored-absent value, leaving that transition
    // entirely native. UNRESOLVED: which transition selects record 1.
    if(!write_record(writer,0U,kAuthoredAnchors,kAuthoredValues,0U,false)) { return false; }
    if(!writer.write(request.seed,kSeedWidth)) { return false; }
    for(const auto region:request.regions) { if(!writer.write(region,kByteWidth)) { return false; } }
    for(const auto group:request.groups) { if(!writer.write(group,kByteWidth)) { return false; } }
    return writer.bit_count()-begin==kActivationBits;
}

// --- Tree of Probabilities (strike_pact, scenario 80F54AE7) -------------------------------------
// The one generator the proven route drives: map_generator_sensor of object 80F550B8, Infinite
// Forest B (SDK export slot/80f550b8/00001e/001e/0025, index 30). The export carries two further
// map_generator_sensor rows on other object tags; the proven mission never publishes to them.
inline constexpr std::uint32_t kForestGeneratorRegistry=0x2763EC91U;
inline constexpr std::uint16_t kForestGeneratorSlot=30;

// strike_pact.lua initialize_forest: one masked host seed per run, retained across reattachment and
// region streaming, and never zero.
[[nodiscard]] constexpr std::uint32_t mission_seed(std::uint32_t entropy) noexcept {
    const std::uint32_t seed=entropy&0x7FFFFFFFU;
    return seed==0U?1U:seed;
}

/** The exact activation strike_pact.lua initialize_forest publishes, seeded for this run. */
[[nodiscard]] constexpr Request forest_request(std::uint32_t seed) noexcept {
    Request request{};
    request.seed=seed;
    // Budget 6 encounters. The authored default is zero, so without this the Forest places none.
    request.values[0]=6;
    // Authored Forest B endpoint geometry, kept as-is; only the +Y exit's progress is raised so the
    // forward route is the goal. Order is native +X, -X, +Y, -Y.
    request.anchors={Anchor{2,0,0.0F,true},Anchor{0,2,0.0F,true},Anchor{2,0,1.0F,true},Anchor{0,2,0.0F,true}};
    request.selectAnchors=true;
    return request;
}

} // namespace sunrise::state::activity::coo::native_generator
