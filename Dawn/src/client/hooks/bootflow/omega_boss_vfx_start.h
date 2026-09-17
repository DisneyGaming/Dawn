#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

/**
 * Panoptes red-eye steady-state initialization (lane J, 2026-09-06).
 *
 * The authored glow signal is the float scalar CE0BA42D owned by the Panoptes
 * animation component whose definition is package tag 80F6690A. The boss
 * controller 80F6695E binds that scalar to its input 0 and computes 20AA7FC1
 * (index 32) from it; 20AA7FC1 drives the eye material 80F45358, the eye-area
 * particle 80F66915 and the eye lights 80F451B9/C3/C4/C5/C6. The assets supply
 * the red look; this code only enables the authored scalar through the original
 * setter so the original dirty notification reaches every bound consumer.
 *
 * Native evidence (D:\Dawn-work\decomp, runtime image base 7FF68E9A0000):
 *  - A0FE60(component, const uint32* name, float value): reads component+0 as
 *    the definition handle and component+8 as the definition body offset,
 *    binary-searches body+0x320 (count) / body+0x328 (self-relative array,
 *    +0x10 header skip) rows of 0x30 bytes by the name at row+0x28, then calls
 *    A10180(component + 0x1340 + *(int64*)(component+0x1330) + ordinal*0x30, value).
 *    The value arrives in xmm2 and is moved to xmm1 (movaps xmm1,xmm2).
 *  - A10180(row, value): returns when |row+0x20 - value| < 0.001, otherwise
 *    resolves row+0 / row+8 (definition record reference), follows the
 *    relative chain at row+0x10, stores the float at row+0x20 and tail-jumps to
 *    5906A0(resolve(node+0x24), definition+record+0x10, 1) - the dirty
 *    notification that wakes bound consumers.
 *  - A0FFA0 indexes the actor table (image+1F9D7F8, stride image+1F9D800) with
 *    component+0x1470 & 0x1FFF: +0x1470 is the owning AI actor handle.
 *
 * Asset evidence (scripts/pkg.py, offline): 80F6690A is 17600 bytes, body at
 * file 0x3038 with prefix {80F6690A,80807EE1,0x90}, 47 sorted provider rows at
 * 0x3BC0 whose references point at the provider records 0x2760+i*0x30, and
 * CE0BA42D is ordinal 40 (allocated provider record 0x2EE0). That record's
 * +8 points back to sorted definition row0x4340. The native runtime copies
 * the provider record, so its +8 remains0x4340, not0x2EE0.
 *
 * Everything here is pure so the unit fixture can exercise it against the real
 * asset bytes and the unchanged native A0FE60/A10180 instructions.
 */
namespace dawn::client::hooks::bootflow::omega_boss_vfx {

inline constexpr std::uint32_t kSource=0x80F6690AU;
inline constexpr std::uint32_t kSourceBytes=0x44C0U;
inline constexpr std::uint64_t kBodyOffset=0x3038U;
inline constexpr std::uint32_t kBodyKind=0x80807EE1U;
inline constexpr std::uint64_t kBodyPrefixOffset=0x90U;
inline constexpr std::uint64_t kAnimationStateOffset=0x300U;   // body-relative
inline constexpr std::uint32_t kAnimationState=0x80F56184U;    // character animation config owner
inline constexpr std::uint64_t kProviderCountOffset=0x320U;    // body-relative, int64
inline constexpr std::uint64_t kProviderArrayOffset=0x328U;    // body-relative, self-relative int64
inline constexpr std::uint64_t kProviderArrayHeader=0x10U;
inline constexpr std::size_t kProviderCount=47;
inline constexpr std::size_t kProviderRowBytes=0x30;
inline constexpr std::uint32_t kProviderRowKind=0x80807EEAU;
inline constexpr std::uint32_t kProviderType=0x80BFDDA6U;      // row+0x18
inline constexpr std::size_t kProviderNameOffset=0x28;
inline constexpr std::uint64_t kProviderRecordBase=0x2760U;    // record i = base + i*0x30
inline constexpr std::uint64_t kSortedProviderBase=0x3BC0U;    // referenced by runtime row+8
inline constexpr std::uint32_t kProviderRecordKind=0x80807EEBU;
inline constexpr std::uint32_t kRedEye=0xCE0BA42DU;
inline constexpr std::size_t kRedEyeOrdinal=40;
inline constexpr std::uint64_t kRedEyeRecord=kProviderRecordBase+kRedEyeOrdinal*kProviderRowBytes; // 0x2EE0
inline constexpr float kRedEyeOff=0.F;
inline constexpr float kRedEyeOn=1.F;
/** Native near-equality used by A10180 before it writes or notifies. */
inline constexpr float kNativeEpsilon=0.001F;
inline constexpr std::uint32_t kController=0x80F6695EU;
inline constexpr std::uint32_t kControllerInput=0;              // CE0BA42D -> input 0
inline constexpr std::uint32_t kControllerOutput=0x20AA7FC1U;
inline constexpr std::uint32_t kControllerOutputIndex=32;
/** Transient illumination input toggled by the summon; not the red-eye control. */
inline constexpr std::uint32_t kSummonIllumination=0xF4ECD569U;

/** Sorted provider names of 80F6690A (file 0x3BC0, name at row+0x28). */
inline constexpr std::array<std::uint32_t,kProviderCount> kProviderNames{{
    0x0A79AB27U,0x1B8DFB01U,0x21B65565U,0x24A2F803U,0x2E74B239U,0x2EC0C771U,0x3A4A5921U,0x49E64CCAU,
    0x4EFEB846U,0x50212DEBU,0x51033A3AU,0x59E3C920U,0x5D899AA5U,0x627BF470U,0x63F0B333U,0x6739872DU,
    0x6CC74E6AU,0x710C8E67U,0x73F8A1EFU,0x77AE1C61U,0x7A3DCA8EU,0x7AFC4613U,0x7E2BAB57U,0x82796D77U,
    0x8A7B19DBU,0x8FD1C2DBU,0x9084609DU,0x9264A5E9U,0x93129F83U,0x98E0AF17U,0x9907CE50U,0x9C86B806U,
    0xA86678F5U,0xAAEDA332U,0xADDE487BU,0xB893488AU,0xB92A5217U,0xBF39E12BU,0xBFE08F26U,0xCCC1292EU,
    0xCE0BA42DU,0xCF9087D3U,0xD3B52196U,0xDA085811U,0xEB9AFC50U,0xEE07AEFBU,0xF480B268U}};
static_assert(kProviderNames[kRedEyeOrdinal]==kRedEye,"CE0BA42D is provider ordinal 40");

// Runtime animation component (owner of the 80F6690A definition).
inline constexpr std::size_t kSelfOffset=0x24;
inline constexpr std::size_t kRuntimeRelativeOffset=0x1330;      // int64, relative to +0x1340
inline constexpr std::size_t kRuntimeBase=0x1340;
inline constexpr std::size_t kRuntimeRowBytes=0x30;
inline constexpr std::size_t kRuntimeValueOffset=0x20;
inline constexpr std::size_t kRuntimeChainOffset=0x10;
inline constexpr std::size_t kRuntimeOwnerOffset=0x24;
inline constexpr std::size_t kActorOffset=0x1470;
inline constexpr std::size_t kComponentBytes=0x1478;
inline constexpr std::int64_t kRuntimeRelativeLimit=0x100000;

// Native code identities gated on their unchanged prologues.
inline constexpr std::uintptr_t kSetterRva=0xA0FE60;
inline constexpr std::uintptr_t kWriterRva=0xA10180;
inline constexpr std::uintptr_t kNotifyRva=0x5906A0;
inline constexpr std::array<std::uint8_t,16> kSetterPrologue{{
    0x40,0x53,0x48,0x83,0xEC,0x20,0x44,0x8B,0x09,0x48,0x8B,0xD9,0x41,0x8B,0xC1,0x4C}};
inline constexpr std::array<std::uint8_t,16> kWriterPrologue{{
    0x48,0x83,0xEC,0x28,0xF3,0x0F,0x10,0x41,0x20,0x4C,0x8D,0x51,0x20,0xF3,0x0F,0x5C}};
inline constexpr std::array<std::uint8_t,16> kNotifyPrologue{{
    0x48,0x89,0x54,0x24,0x10,0x48,0x83,0xEC,0x28,0x45,0x0F,0xB6,0xC8,0x48,0x8D,0x54}};
/** A0FE60 calls A10180 with rel32 at +0x12F; A10180 tail-jumps to 5906A0 at +0x41C of the same image. */
inline constexpr std::size_t kSetterCallWriterOffset=0x12F;
inline constexpr std::size_t kWriterJumpNotifyOffset=0x41C;

enum class Reject : std::uint8_t {
    none,
    gateClosed,          // hook quiesced / side effects not accepted
    runChanged,          // navigation run differs from the captured owner
    ownerInvalid,        // intro graph owner (character/entity) not validated
    memberDisabled,      // authored member disabled
    queueOccupied,       // native command queue not idle
    setterUnavailable,   // A0FE60/A10180 prologue mismatch
    actorRow,            // actor table row self/entity mismatch
    parentHandle,        // actor +0x50 link invalid
    parentMismatch,      // A8CB20 context parent differs from the actor +0x50 link
    componentRead,
    componentIdentity,   // +0 source, +8 body offset, +0x24 self
    componentActor,      // +0x1470 does not point back to the actor
    runtimeRelative,     // +0x1330 relative array out of bounds
    definitionRead,
    definitionLayout,    // 80F6690A bytes differ from the verified layout
    rowRead,
    rowIdentity,         // runtime row reference/owner invalid
    ownerUnresolved,     // notify owner handle does not resolve
    intermediateValue,   // native animation in progress; left alone
    alreadyAttempted,    // one attempt per owner and run
    uncertain            // owner or run changed around the native write
};

[[nodiscard]] constexpr const char* reject_name(Reject reason) noexcept {
    switch(reason) {
    case Reject::none:return "none";
    case Reject::gateClosed:return "gate_closed";
    case Reject::runChanged:return "run_changed";
    case Reject::ownerInvalid:return "owner_invalid";
    case Reject::memberDisabled:return "member_disabled";
    case Reject::queueOccupied:return "queue_occupied";
    case Reject::setterUnavailable:return "setter_unavailable";
    case Reject::actorRow:return "actor_row";
    case Reject::parentHandle:return "parent_handle";
    case Reject::parentMismatch:return "parent_mismatch";
    case Reject::componentRead:return "component_read";
    case Reject::componentIdentity:return "component_identity";
    case Reject::componentActor:return "component_actor";
    case Reject::runtimeRelative:return "runtime_relative";
    case Reject::definitionRead:return "definition_read";
    case Reject::definitionLayout:return "definition_layout";
    case Reject::rowRead:return "row_read";
    case Reject::rowIdentity:return "row_identity";
    case Reject::ownerUnresolved:return "owner_unresolved";
    case Reject::intermediateValue:return "intermediate_value";
    case Reject::alreadyAttempted:return "already_attempted";
    case Reject::uncertain:return "uncertain";
    }
    return "unknown";
}

namespace detail {
template<class T> [[nodiscard]] bool read(std::span<const std::byte> bytes,std::uint64_t offset,T& value) noexcept {
    if(offset>bytes.size() || bytes.size()-offset<sizeof(T)) { return false; }
    std::memcpy(&value,bytes.data()+offset,sizeof(T));return true;
}
inline void set(Reject* reason,Reject value) noexcept { if(reason!=nullptr) { *reason=value; } }
} // namespace detail

/**
 * Validates the exact 80F6690A source layout A0FE60 walks: total size, body
 * prefix, 47 sorted provider rows with their record references and types, the
 * verified name table and CE0BA42D at ordinal 40. The loaded definition is the
 * file image (relative pointers are file-relative), so this also runs against
 * the offline package bytes.
 */
[[nodiscard]] inline bool validate_definition_body(std::span<const std::byte> definition,Reject* reason=nullptr) noexcept {
    detail::set(reason,Reject::definitionLayout);
    std::uint32_t tag{},kind{};std::uint64_t offset{};std::int64_t count{},relative{};
    if(definition.size()<kSourceBytes) { return false; }
    if(!detail::read(definition,kBodyOffset,tag) || !detail::read(definition,kBodyOffset+4,kind)
        || !detail::read(definition,kBodyOffset+8,offset)
        || tag!=kSource || kind!=kBodyKind || offset!=kBodyPrefixOffset) { return false; }
    if(!detail::read(definition,kBodyOffset+kAnimationStateOffset,tag) || tag!=kAnimationState) { return false; }
    if(!detail::read(definition,kBodyOffset+kProviderCountOffset,count) || count!=static_cast<std::int64_t>(kProviderCount)) { return false; }
    if(!detail::read(definition,kBodyOffset+kProviderArrayOffset,relative) || relative<=0) { return false; }
    const auto rows=kBodyOffset+kProviderArrayOffset+static_cast<std::uint64_t>(relative)+kProviderArrayHeader;
    if(rows!=kSortedProviderBase || rows>kSourceBytes
        || kSourceBytes-rows<kProviderCount*kProviderRowBytes) { return false; }
    std::uint32_t previous{};
    for(std::size_t i=0;i<kProviderCount;++i) {
        const auto row=rows+i*kProviderRowBytes;
        std::uint32_t rowTag{},rowKind{},type{},name{},pad{};std::uint64_t record{};
        if(!detail::read(definition,row,rowTag) || !detail::read(definition,row+4,rowKind)
            || !detail::read(definition,row+8,record) || !detail::read(definition,row+0x18,type)
            || !detail::read(definition,row+kProviderNameOffset,name)
            || !detail::read(definition,row+kProviderNameOffset+4,pad)) { return false; }
        if(rowTag!=kSource || rowKind!=kProviderRowKind || record!=kProviderRecordBase+i*kProviderRowBytes
            || type!=kProviderType || pad!=0 || name!=kProviderNames[i] || (i!=0 && name<=previous)) { return false; }
        previous=name;
        std::uint32_t recordTag{},recordKind{};
        if(!detail::read(definition,record,recordTag) || !detail::read(definition,record+4,recordKind)
            || recordTag!=kSource || recordKind!=kProviderRecordKind) { return false; }
    }
    detail::set(reason,Reject::none);return true;
}
/** Body validation plus the package header size word (file image; the loaded copy keeps it too). */
[[nodiscard]] inline bool validate_definition(std::span<const std::byte> definition,Reject* reason=nullptr) noexcept {
    std::uint32_t size{};
    if(!detail::read(definition,0,size) || size!=kSourceBytes) { detail::set(reason,Reject::definitionLayout);return false; }
    return validate_definition_body(definition,reason);
}

/** Component-relative offset of the runtime provider row A0FE60 hands to A10180. */
[[nodiscard]] inline std::optional<std::uint64_t> runtime_row_offset(std::span<const std::byte> component,std::size_t ordinal) noexcept {
    std::int64_t relative{};
    if(ordinal>=kProviderCount || !detail::read(component,kRuntimeRelativeOffset,relative)
        || relative<0 || relative>kRuntimeRelativeLimit) { return std::nullopt; }
    return kRuntimeBase+static_cast<std::uint64_t>(relative)+ordinal*kRuntimeRowBytes;
}

/** Component prefix identity: definition handle, body offset, self handle and the AI actor backlink. */
[[nodiscard]] inline bool validate_component(std::span<const std::byte> component,std::uint32_t self,
                                             std::uint32_t actor,Reject* reason=nullptr) noexcept {
    std::uint32_t tag{},observedSelf{},observedActor{};std::uint64_t body{};
    if(component.size()<kComponentBytes || self==UINT32_MAX || actor==UINT32_MAX) {
        detail::set(reason,Reject::componentRead);return false;
    }
    if(!detail::read(component,0,tag) || !detail::read(component,8,body) || !detail::read(component,kSelfOffset,observedSelf)
        || tag!=kSource || body!=kBodyOffset || observedSelf!=self) {
        detail::set(reason,Reject::componentIdentity);return false;
    }
    if(!detail::read(component,kActorOffset,observedActor) || observedActor!=actor) {
        detail::set(reason,Reject::componentActor);return false;
    }
    if(!runtime_row_offset(component,kRedEyeOrdinal)) { detail::set(reason,Reject::runtimeRelative);return false; }
    detail::set(reason,Reject::none);return true;
}

struct RuntimeRow final {
    std::uint32_t source{UINT32_MAX};
    std::int64_t record{-1};
    std::int64_t chain{};
    float value{};
    std::uint32_t owner{UINT32_MAX};
};

/**
 * Decodes the 0x30-byte runtime row A10180 writes: the definition record
 * reference at +0/+8 must name this source and this ordinal's sorted definition
 * row. The +24 owner is meaningful only at the terminal of its relative chain.
 */
[[nodiscard]] inline bool decode_runtime_row(std::span<const std::byte> row,std::size_t ordinal,RuntimeRow& out) noexcept {
    if(row.size()<kRuntimeRowBytes || ordinal>=kProviderCount) { return false; }
    RuntimeRow decoded{};std::uint32_t kind{};
    if(!detail::read(row,0,decoded.source) || !detail::read(row,8,decoded.record)
        || !detail::read(row,4,kind)
        || !detail::read(row,kRuntimeChainOffset,decoded.chain) || !detail::read(row,kRuntimeValueOffset,decoded.value)
        || !detail::read(row,kRuntimeOwnerOffset,decoded.owner)) { return false; }
    out=decoded;
    return decoded.source==kSource && kind==kProviderRecordKind
        && decoded.record==static_cast<std::int64_t>(kSortedProviderBase+ordinal*kProviderRowBytes);
}

/** Accept the exact authored parent chain, or an already terminal self-owned
 * row. Live47 rows copy the package provider records and point from row+10
 * directly back to component+0; parent+10 is0 and parent+24 is the full owner.
 * Reject external, cyclic and multi-hop links rather than letting A10180 follow
 * an unvalidated pointer. Row+24 is not consulted for a linked row.
 */
[[nodiscard]] inline bool runtime_notify_owner(std::span<const std::byte> component,
    std::uint64_t rowOffset,const RuntimeRow& row,std::uint32_t self,std::uint32_t& owner) noexcept {
    owner=UINT32_MAX;
    if(self==UINT32_MAX || rowOffset>static_cast<std::uint64_t>(INT64_MAX)-kRuntimeChainOffset) { return false; }
    if(row.chain==0) {
        if(row.owner!=self) { return false; }
        owner=self;return true;
    }
    if(row.chain!=-static_cast<std::int64_t>(rowOffset+kRuntimeChainOffset)) { return false; }
    std::int64_t terminalChain{};std::uint32_t terminalOwner{};
    if(!detail::read(component,kRuntimeChainOffset,terminalChain) || terminalChain!=0
        || !detail::read(component,kSelfOffset,terminalOwner) || terminalOwner!=self) { return false; }
    owner=terminalOwner;return true;
}

enum class Decision : std::uint8_t { write,acceptExisting,leaveIntermediate };

/** 0.0 is the authored default and gets the one write; an existing 1.0 is accepted; anything else is native animation. */
[[nodiscard]] constexpr Decision decide(float current) noexcept {
    if(current==kRedEyeOff) { return Decision::write; }
    if(current==kRedEyeOn) { return Decision::acceptExisting; }
    return Decision::leaveIntermediate;
}
/** Success requires the exact requested value, not the native 0.001 band. */
[[nodiscard]] constexpr bool confirmed(float after) noexcept { return after==kRedEyeOn; }
/** Mirrors A10180's early return: |current-requested| < 0.001 writes and notifies nothing. */
[[nodiscard]] inline bool native_skips(float current,float requested) noexcept {
    return std::fabs(current-requested)<kNativeEpsilon;
}

struct Owner final {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},component{UINT32_MAX},generation{},revision{};
    [[nodiscard]] constexpr bool valid() const noexcept { return run!=0 && actor!=UINT32_MAX && component!=UINT32_MAX; }
    [[nodiscard]] constexpr bool operator==(const Owner&) const noexcept=default;
};

enum class Outcome : std::uint8_t { none,confirmed,acceptedExisting,uncertain,intermediate };

/**
 * One attempt per Panoptes owner and mission run. The claim is recorded before
 * native code runs; an uncertain result is settled, never retried. A new run
 * or a new actor is a new owner.
 */
class Ledger final {
public:
    [[nodiscard]] bool attempted(std::uint64_t run,std::uint32_t actor) const noexcept {
        return owner_.valid() && owner_.run==run && owner_.actor==actor;
    }
    [[nodiscard]] bool claim(const Owner& owner) noexcept {
        if(!owner.valid() || attempted(owner.run,owner.actor)) { return false; }
        owner_=owner;outcome_=Outcome::none;return true;
    }
    void settle(const Owner& owner,Outcome outcome) noexcept {
        if(owner_==owner && outcome_==Outcome::none) { outcome_=outcome; }
    }
    [[nodiscard]] const Owner& owner() const noexcept { return owner_; }
    [[nodiscard]] Outcome outcome() const noexcept { return outcome_; }
    void reset() noexcept { owner_={};outcome_=Outcome::none; }
private:
    Owner owner_{};
    Outcome outcome_{Outcome::none};
};

/** Observed preconditions gathered by the hook before deciding anything. */
struct Facts final {
    bool accepting{};
    bool runMatches{};
    bool ownerValidated{};
    bool memberEnabled{};
    bool queueIdle{};
    bool attempted{};
    bool setterAvailable{};
    /** Result of resolving actor +0x50 -> component -> runtime row (none when resolved). */
    Reject resolution{Reject::none};
    bool definitionValid{};
    float current{};
};
struct Plan final {
    bool proceed{};
    Decision decision{Decision::leaveIntermediate};
    Reject reject{Reject::none};
};
/** True for the reject reasons a parent resolution can report. */
[[nodiscard]] constexpr bool resolution_reject(Reject reason) noexcept {
    switch(reason) {
    case Reject::actorRow:case Reject::parentHandle:case Reject::parentMismatch:
    case Reject::componentRead:case Reject::componentIdentity:case Reject::componentActor:
    case Reject::runtimeRelative:case Reject::rowRead:case Reject::rowIdentity:
    case Reject::ownerUnresolved:return true;
    default:return false;
    }
}
/** Pure gate order; the first failing fact names the reject reason. */
[[nodiscard]] constexpr Plan plan(const Facts& facts) noexcept {
    Plan result{};
    if(!facts.accepting) { result.reject=Reject::gateClosed;return result; }
    if(!facts.runMatches) { result.reject=Reject::runChanged;return result; }
    if(!facts.ownerValidated) { result.reject=Reject::ownerInvalid;return result; }
    if(!facts.memberEnabled) { result.reject=Reject::memberDisabled;return result; }
    if(!facts.queueIdle) { result.reject=Reject::queueOccupied;return result; }
    if(facts.attempted) { result.reject=Reject::alreadyAttempted;return result; }
    if(!facts.setterAvailable) { result.reject=Reject::setterUnavailable;return result; }
    if(facts.resolution!=Reject::none) {
        result.reject=resolution_reject(facts.resolution)?facts.resolution:Reject::parentHandle;return result;
    }
    if(!facts.definitionValid) { result.reject=Reject::definitionLayout;return result; }
    result.decision=decide(facts.current);
    if(result.decision==Decision::leaveIntermediate) { result.reject=Reject::intermediateValue;return result; }
    result.proceed=true;return result;
}

/** Bounded readback trace: first sample and every change, at most kLines per run. */
class Trace final {
public:
    static constexpr unsigned kLines=16;
    [[nodiscard]] bool should_log(std::uint64_t run,float value) noexcept {
        if(run!=run_) { run_=run;lines_=0;sampled_=false;last_=0.F; }
        // NaN compares unequal to itself; treat repeated NaN as unchanged.
        const bool changed=!sampled_ || (std::isnan(value) ? !std::isnan(last_) : value!=last_);
        sampled_=true;last_=value;
        if(!changed || lines_>=kLines) { return false; }
        ++lines_;return true;
    }
    [[nodiscard]] unsigned lines() const noexcept { return lines_; }
    /** True once this run's budget is spent; callers skip the readback entirely. */
    [[nodiscard]] bool exhausted(std::uint64_t run) const noexcept { return run==run_ && lines_>=kLines; }
    void reset() noexcept { run_=0;lines_=0;sampled_=false;last_=0.F; }
private:
    std::uint64_t run_{};
    unsigned lines_{};
    bool sampled_{};
    float last_{};
};

/** Bounded reject diagnostics: one line per reason per run, at most kLines per run. */
class RejectLimiter final {
public:
    static constexpr unsigned kLines=16;
    [[nodiscard]] bool should_log(std::uint64_t run,Reject reason) noexcept {
        if(run!=run_) { run_=run;lines_=0;seen_={}; }
        const auto bit=std::uint32_t{1}<<static_cast<unsigned>(reason);
        if((seen_&bit)!=0 || lines_>=kLines) { return false; }
        seen_|=bit;++lines_;return true;
    }
    void reset() noexcept { run_=0;lines_=0;seen_=0; }
private:
    std::uint64_t run_{};
    unsigned lines_{};
    std::uint32_t seen_{};
};
static_assert(static_cast<unsigned>(Reject::uncertain)<32,"reject bitmask");

} // namespace dawn::client::hooks::bootflow::omega_boss_vfx
