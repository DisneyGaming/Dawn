// Panoptes red-eye steady-state initialization (lane J, 2026-09-06).
//
// Included by omega_lair_cinematic.cpp inside its anonymous namespace, after
// try_summon_event and before observe_boss_member, so it shares that file's
// copy/at/log helpers, the resolver/actor-context originals, the CallGate and
// the BossOwner validation. See omega_boss_vfx_start.h for the evidence.
//
// Behaviour: immediately before the intro graph is queued, read CE0BA42D on the
// Panoptes animation component (AI actor +0x50 -> component; component+0x1470
// must point back). If it is the authored default 0.0, claim the single attempt
// for this owner and run and call the original setter A0FE60 with 1.0, then
// re-resolve the whole chain and read the provider back. 1.0 already present is
// accepted without a write; any other value is native animation and is left
// alone. Nothing here gates or suppresses the intro.

namespace vfx=omega_boss_vfx;
using SetAnimationScalar=void(__fastcall*)(std::byte*,const std::uint32_t*,float) noexcept;
void* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& prefix) noexcept;
std::atomic<SetAnimationScalar> g_setAnimationScalar{};
SRWLOCK g_vfxLock=SRWLOCK_INIT;
vfx::Ledger g_vfxLedger{};
vfx::Trace g_vfxTrace{};
vfx::RejectLimiter g_vfxRejects{};
// (actor<<32)|entity of the owner whose readback is traced; UINT64_MAX when none.
std::atomic_uint64_t g_vfxTraceKey{UINT64_MAX};

/** Original A0FE60, accepted only while its prologue, A10180's prologue, the
 * notification's prologue and both relative branches are unchanged. */
SetAnimationScalar animation_scalar_setter() noexcept {
    auto setter=g_setAnimationScalar.load(std::memory_order_acquire);
    if(setter!=nullptr) { return setter; }
    auto* code=static_cast<std::byte*>(target(vfx::kSetterRva,vfx::kSetterPrologue));
    if(code==nullptr || target(vfx::kWriterRva,vfx::kWriterPrologue)==nullptr
        || target(vfx::kNotifyRva,vfx::kNotifyPrologue)==nullptr) { return nullptr; }
    std::array<std::byte,5> call{},jump{};
    if(!copy(code+vfx::kSetterCallWriterOffset,call) || !copy(code+vfx::kWriterJumpNotifyOffset,jump)
        || call[0]!=std::byte{0xE8} || jump[0]!=std::byte{0xE9}) { return nullptr; }
    const auto callTarget=static_cast<std::int64_t>(vfx::kSetterRva+vfx::kSetterCallWriterOffset+5)
        +at<std::int32_t>(call.data()+1);
    const auto jumpTarget=static_cast<std::int64_t>(vfx::kSetterRva+vfx::kWriterJumpNotifyOffset+5)
        +at<std::int32_t>(jump.data()+1);
    if(callTarget!=static_cast<std::int64_t>(vfx::kWriterRva)
        || jumpTarget!=static_cast<std::int64_t>(vfx::kNotifyRva)) { return nullptr; }
    setter=reinterpret_cast<SetAnimationScalar>(code);
    g_setAnimationScalar.store(setter,std::memory_order_release);
    return setter;
}

void report_vfx_reject(std::uint64_t run,vfx::Reject reason,std::uint32_t actor,std::uint32_t a=0,
                       std::uint32_t b=0,double value=0.0) noexcept {
    AcquireSRWLockExclusive(&g_vfxLock);
    const bool emit=g_vfxRejects.should_log(run,reason);
    ReleaseSRWLockExclusive(&g_vfxLock);
    if(!emit) { return; }
    std::array<char,288> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=vfx_reject run=%llu reason=%s source=80F6690A scalar=CE0BA42D actor=%08X a=%08X b=%08X value=%g",
        static_cast<unsigned long long>(run),vfx::reject_name(reason),actor,a,b,value));
}

struct VfxParent final {
    std::byte* component{};
    std::uint32_t handle{UINT32_MAX};
    std::uint32_t runtimeClass{};
    std::uint64_t rowOffset{};
    vfx::RuntimeRow row{};
    vfx::Reject reject{vfx::Reject::parentHandle};
    std::uint32_t detail{},detail2{};
};

/** Actor table row (image+1F9D7F8, stride image+1F9D800): +48 self, +4C entity,
 * +50 the generic parent that A0FFA0 indexes back to from component+0x1470. */
bool actor_parent(std::uint32_t actor,std::uint32_t entity,std::uint32_t& parent,std::uint32_t& observed) noexcept {
    std::array<std::byte,8> baseBytes{};std::array<std::byte,4> strideBytes{};
    if(g_image==0 || !copy(reinterpret_cast<const void*>(g_image+0x1F9D7F8),baseBytes)
        || !copy(reinterpret_cast<const void*>(g_image+0x1F9D800),strideBytes)) { return false; }
    const auto base=at<std::uintptr_t>(baseBytes.data());
    const auto stride=at<std::int32_t>(strideBytes.data());
    if(base<0x10000 || stride<0x70 || stride>0x100000) { return false; }
    const auto delta=std::uintptr_t{actor&0x1FFFU}*static_cast<std::uintptr_t>(stride);
    if(delta>UINTPTR_MAX-base) { return false; }
    std::array<std::byte,0x70> row{};
    if(!copy(reinterpret_cast<const void*>(base+delta),row)) { return false; }
    observed=at<std::uint32_t>(row.data()+0x48);
    if(observed!=actor || at<std::uint32_t>(row.data()+0x4C)!=entity) { return false; }
    parent=at<std::uint32_t>(row.data()+0x50);
    return parent!=UINT32_MAX;
}

/** Resolves the animation component from the AI actor and validates the exact
 * runtime layout A0FE60/A10180 will walk. Pointers live only in this callback. */
VfxParent resolve_vfx_parent(std::uint32_t actor,std::uint32_t entity,
    std::size_t ordinal=vfx::kRedEyeOrdinal) noexcept {
    VfxParent result{};
    if(ordinal>=vfx::kProviderCount) { result.reject=vfx::Reject::rowIdentity;return result; }
    const auto context=g_actorContext.load(std::memory_order_acquire);
    const auto resolve=g_resolve.load(std::memory_order_acquire);
    if(context==nullptr || resolve==nullptr || actor==UINT32_MAX || entity==UINT32_MAX) { return result; }
    std::uint32_t parent{UINT32_MAX},observed{UINT32_MAX};
    if(!actor_parent(actor,entity,parent,observed)) { result.reject=vfx::Reject::actorRow;result.detail=observed;return result; }
    // The original A8CB20 accessor returns the same record+50 link; both must agree.
    std::array<std::byte,24> actorContext{};
    context(actorContext.data(),actor);
    const auto contextParent=at<std::uint32_t>(actorContext.data());
    if(contextParent!=parent || at<std::uint32_t>(actorContext.data()+4)!=actor) {
        result.reject=vfx::Reject::parentMismatch;result.detail=parent;result.detail2=contextParent;return result;
    }
    auto* component=static_cast<std::byte*>(resolve(parent));
    std::array<std::byte,vfx::kComponentBytes> bytes{};
    if(component==nullptr || !copy(component,bytes)) { result.reject=vfx::Reject::componentRead;result.detail=parent;return result; }
    result.handle=parent;result.runtimeClass=at<std::uint32_t>(bytes.data()+4);
    if(!vfx::validate_component(bytes,parent,actor,&result.reject)) {
        result.detail=at<std::uint32_t>(bytes.data());result.detail2=at<std::uint32_t>(bytes.data()+vfx::kActorOffset);return result;
    }
    const auto offset=vfx::runtime_row_offset(bytes,ordinal);
    if(!offset) { result.reject=vfx::Reject::runtimeRelative;return result; }
    result.rowOffset=*offset;
    std::array<std::byte,vfx::kRuntimeRowBytes> row{};
    if(!copy(component+*offset,row)) { result.reject=vfx::Reject::rowRead;return result; }
    if(!vfx::decode_runtime_row(row,ordinal,result.row)) {
        result.reject=vfx::Reject::rowIdentity;result.detail=result.row.source;
        result.detail2=static_cast<std::uint32_t>(result.row.record);return result;
    }
    // A10180 follows row+10 before reading +24. The live copied provider rows
    // point back to the parent; their own +24 is not the notification owner.
    std::uint32_t notifyOwner{};
    if(!vfx::runtime_notify_owner(bytes,*offset,result.row,result.handle,notifyOwner)
        || resolve(notifyOwner)!=component) {
        result.reject=vfx::Reject::ownerUnresolved;result.detail=notifyOwner;return result;
    }
    result.row.owner=notifyOwner;
    result.component=component;result.reject=vfx::Reject::none;return result;
}

/** The loaded 80F6690A definition must match the verified layout byte for byte
 * where A0FE60 reads it. The header word is reported, not required. */
bool vfx_definition_valid(std::uint32_t& header) noexcept {
    auto* definition=resolve_part(vfx::kSource,0);
    std::array<std::byte,vfx::kSourceBytes> bytes{};
    header=0;
    if(definition==nullptr || !copy(definition,bytes)) { return false; }
    header=at<std::uint32_t>(bytes.data());
    return vfx::validate_definition_body(bytes);
}

// CF is the upstream scalar source, not the overwritten computed vector exposed
// by576420. Keep native calls outside the lease lock and resolve the full parent
// every time. The graph's exposure clip supplies its own ramp; acquire only when
// the native eye loop has actually begun. The same source also feeds C7's
// eye-region state in cycle 3, without changing its different authored pose.
void start_eye_hold(const combat::Token& token) noexcept {
    if(!token.valid() || !g_gate.accepting()) { return; }
    const auto navigation=p::navigation();const auto state=lair::status(token.owner.run);
    if(!navigation.enabled || navigation.run!=token.owner.run || !state.enabled || state.failed
        || state.token!=encounter_token(token)
        || (state.crownStage!=lair::CrownStage::eyeOpening && state.crownStage!=lair::CrownStage::eyeDps)) { return; }
    const auto owner=boss_owner(token.owner.actor,false);const auto body=full_body(owner);
    const auto parent=resolve_vfx_parent(token.owner.actor,token.owner.entity,combat::kEyeHoldOrdinal);
    const auto setter=animation_scalar_setter();std::uint32_t header{};
    if(owner.handle!=token.owner.character || owner.entity!=token.owner.entity || body.object==nullptr
        || body.biped!=token.owner.biped || parent.reject!=vfx::Reject::none || setter==nullptr
        || !vfx_definition_valid(header)) { report_combat("eye_hold_owner_unconfirmed",token);return; }
    AcquireSRWLockExclusive(&g_combatLock);
    const bool claimed=g_combat.command_token()==token && g_combat.stage()==combat::Stage::eyeVulnerable
        && g_eyeHold.claim(token,parent.handle,combat::Stage::eyeVulnerable,parent.row.value);
    ReleaseSRWLockExclusive(&g_combatLock);
    if(!claimed) { report_combat("eye_hold_not_claimed",token);return; }
    setter(parent.component,&combat::kEyeHoldName,1.F);
    if(!g_gate.accepting()) { return; }
    const auto fresh=resolve_vfx_parent(token.owner.actor,token.owner.entity,combat::kEyeHoldOrdinal);
    const auto after=lair::status(token.owner.run);const auto afterNavigation=p::navigation();
    const bool confirmed=afterNavigation.enabled && afterNavigation.run==token.owner.run
        && after.enabled && !after.failed && after.token==encounter_token(token)
        && fresh.reject==vfx::Reject::none && fresh.handle==parent.handle
        && fresh.row.owner==parent.row.owner && fresh.row.value==1.F;
    report_combat(confirmed?"eye_hold_started":"eye_hold_start_unconfirmed",token,parent.handle);
}

void stop_eye_hold(const combat::Token& phase,bool retiring) noexcept {
    AcquireSRWLockExclusive(&g_combatLock);const auto lease=g_eyeHold.release(phase,retiring);
    ReleaseSRWLockExclusive(&g_combatLock);
    if(!lease.valid() || !g_gate.accepting()) { return; }
    const auto navigation=p::navigation();
    if(navigation.run!=lease.token.owner.run) { return; }
    const auto owner=boss_owner(lease.token.owner.actor,false);const auto body=full_body(owner);
    const auto parent=resolve_vfx_parent(lease.token.owner.actor,lease.token.owner.entity,combat::kEyeHoldOrdinal);
    const auto setter=animation_scalar_setter();std::uint32_t header{};
    if(owner.handle!=lease.token.owner.character || owner.entity!=lease.token.owner.entity
        || body.object==nullptr || body.biped!=lease.token.owner.biped || parent.reject!=vfx::Reject::none
        || setter==nullptr || !vfx_definition_valid(header)) {
        report_combat("eye_hold_cleanup_owner_unconfirmed",lease.token);return;
    }
    if(!lease.can_restore(parent.handle,parent.row.value)) {
        report_combat("eye_hold_cleanup_not_owned",lease.token,parent.handle);return;
    }
    setter(parent.component,&combat::kEyeHoldName,0.F);
    if(!g_gate.accepting()) { return; }
    const auto fresh=resolve_vfx_parent(lease.token.owner.actor,lease.token.owner.entity,combat::kEyeHoldOrdinal);
    const bool cleared=fresh.reject==vfx::Reject::none && fresh.handle==lease.parent && fresh.row.value==0.F;
    report_combat(cleared?"eye_hold_stopped":"eye_hold_stop_unconfirmed",lease.token,lease.parent);
}

void report_vfx_start(std::uint64_t run,const vfx::Owner& owner,const VfxParent& parent,bool requested,
                      float before,double observed,bool confirmedResult,std::uint32_t header) noexcept {
    std::array<char,384> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=vfx_start source=80F6690A scalar=CE0BA42D requested=%u observed=%g confirmed=%u mode=steady_state_candidate authored_timing=0 run=%llu actor=%08X component=%08X class=%08X generation=%u revision=%u ordinal=40 before=%g owner=%08X header=%08X",
        requested?1U:0U,observed,confirmedResult?1U:0U,static_cast<unsigned long long>(run),owner.actor,
        owner.component,parent.runtimeClass,owner.generation,owner.revision,static_cast<double>(before),
        parent.row.owner,header));
}

/** Steady-state initialization of CE0BA42D immediately before the intro graph is
 * queued. One attempt per owner and run; the claim precedes the native call; an
 * uncertain result is settled, never retried; failure never suppresses the intro. */
void prepare_intro_vfx(std::uint64_t run,std::uint32_t actor,std::uint32_t generation,std::uint32_t revision,
                       const BossOwner& owner,bool disabled,std::int32_t head,std::int32_t count) noexcept {
    vfx::Facts facts{};
    facts.accepting=g_gate.accepting();
    const auto navigation=p::navigation();
    facts.runMatches=navigation.enabled && run!=0 && navigation.run==run;
    facts.ownerValidated=owner.object!=nullptr && owner.handle!=UINT32_MAX && owner.entity!=UINT32_MAX
        && actor!=UINT32_MAX;
    facts.memberEnabled=!disabled;
    facts.queueIdle=head>=0 && count>=0 && head>=count;
    AcquireSRWLockExclusive(&g_vfxLock);
    facts.attempted=g_vfxLedger.attempted(run,actor);
    ReleaseSRWLockExclusive(&g_vfxLock);
    const auto setter=animation_scalar_setter();
    facts.setterAvailable=setter!=nullptr;
    VfxParent parent{};
    std::uint32_t header{};
    if(facts.accepting && facts.runMatches && facts.ownerValidated) {
        parent=resolve_vfx_parent(actor,owner.entity);
        facts.resolution=parent.reject;
        facts.definitionValid=parent.reject==vfx::Reject::none && vfx_definition_valid(header);
        facts.current=parent.row.value;
    } else { facts.resolution=vfx::Reject::parentHandle; }
    const auto decided=vfx::plan(facts);
    const vfx::Owner vfxOwner{run,actor,parent.handle,generation,revision};
    if(!decided.proceed) {
        // definition_layout carries the loaded header word so a relocated or
        // patched definition is diagnosable from one run.
        report_vfx_reject(run,decided.reject,actor,
            decided.reject==vfx::Reject::definitionLayout?header:parent.detail,
            decided.reject==vfx::Reject::definitionLayout?parent.handle:parent.detail2,
            static_cast<double>(facts.current));
        if(decided.reject==vfx::Reject::intermediateValue) {
            // Native animation owns the value; consume this owner's attempt so a
            // later re-issue in the same run cannot write over it either.
            AcquireSRWLockExclusive(&g_vfxLock);
            if(g_vfxLedger.claim(vfxOwner)) { g_vfxLedger.settle(vfxOwner,vfx::Outcome::intermediate); }
            ReleaseSRWLockExclusive(&g_vfxLock);
        }
        return;
    }
    g_vfxTraceKey.store((static_cast<std::uint64_t>(actor)<<32)|owner.entity,std::memory_order_release);
    if(decided.decision==vfx::Decision::acceptExisting) {
        AcquireSRWLockExclusive(&g_vfxLock);
        if(g_vfxLedger.claim(vfxOwner)) { g_vfxLedger.settle(vfxOwner,vfx::Outcome::acceptedExisting); }
        ReleaseSRWLockExclusive(&g_vfxLock);
        report_vfx_start(run,vfxOwner,parent,false,facts.current,static_cast<double>(facts.current),true,header);
        return;
    }
    // Record the claim before native code runs. The void setter cannot be retried.
    AcquireSRWLockExclusive(&g_vfxLock);
    const bool claimed=g_vfxLedger.claim(vfxOwner);
    ReleaseSRWLockExclusive(&g_vfxLock);
    if(!claimed) { report_vfx_reject(run,vfx::Reject::alreadyAttempted,actor,parent.handle); return; }
    const std::uint32_t name=vfx::kRedEye;
    setter(parent.component,&name,vfx::kRedEyeOn);
    // Resolve and validate the complete owner and source chain again, then read
    // the provider back through the same path. Anything else is uncertain.
    const auto fresh=resolve_vfx_parent(actor,owner.entity);
    const auto after=p::navigation();
    std::uint32_t headerAfter{};
    const bool stable=g_gate.accepting() && fresh.reject==vfx::Reject::none && fresh.handle==parent.handle
        && fresh.component==parent.component && fresh.rowOffset==parent.rowOffset
        && fresh.row.owner==parent.row.owner && after.enabled && after.run==run
        && vfx_definition_valid(headerAfter);
    const bool confirmedResult=stable && vfx::confirmed(fresh.row.value);
    AcquireSRWLockExclusive(&g_vfxLock);
    g_vfxLedger.settle(vfxOwner,confirmedResult?vfx::Outcome::confirmed:vfx::Outcome::uncertain);
    ReleaseSRWLockExclusive(&g_vfxLock);
    report_vfx_start(run,vfxOwner,parent,true,facts.current,
        stable?static_cast<double>(fresh.row.value):std::numeric_limits<double>::quiet_NaN(),confirmedResult,header);
    if(!confirmedResult) {
        report_vfx_reject(run,vfx::Reject::uncertain,actor,fresh.handle,static_cast<std::uint32_t>(fresh.reject),
            stable?static_cast<double>(fresh.row.value):std::numeric_limits<double>::quiet_NaN());
    }
}

/** Bounded readback of the native provider (first sample and changes, 16 lines
 * per run). Observe-only: the value is read through the validated chain and never written. */
void trace_intro_vfx(std::uint64_t run,std::uint32_t actor) noexcept {
    const auto key=g_vfxTraceKey.load(std::memory_order_acquire);
    if(key==UINT64_MAX || static_cast<std::uint32_t>(key>>32)!=actor || !g_gate.accepting()) { return; }
    AcquireSRWLockExclusive(&g_vfxLock);
    const bool exhausted=g_vfxTrace.exhausted(run);
    ReleaseSRWLockExclusive(&g_vfxLock);
    if(exhausted) { return; }
    const auto parent=resolve_vfx_parent(actor,static_cast<std::uint32_t>(key));
    if(parent.reject!=vfx::Reject::none) { return; }
    AcquireSRWLockExclusive(&g_vfxLock);
    const bool emit=g_vfxTrace.should_log(run,parent.row.value);
    const auto lines=g_vfxTrace.lines();
    ReleaseSRWLockExclusive(&g_vfxLock);
    if(!emit) { return; }
    std::array<char,256> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=vfx_values run=%llu actor=%08X component=%08X animation_CE0BA42D=%g ordinal=40 line=%u",
        static_cast<unsigned long long>(run),actor,parent.handle,static_cast<double>(parent.row.value),lines));
}

void reset_intro_vfx() noexcept {
    AcquireSRWLockExclusive(&g_vfxLock);
    g_vfxLedger.reset();g_vfxTrace.reset();g_vfxRejects.reset();
    ReleaseSRWLockExclusive(&g_vfxLock);
    g_vfxTraceKey.store(UINT64_MAX,std::memory_order_release);
    g_setAnimationScalar.store(nullptr,std::memory_order_release);
}
