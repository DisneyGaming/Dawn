#include <Windows.h>
#include <array>
#include <cstdio>
#include "omega_ending.h"
#include "coo/omega_ending_controller.h"
#include "omega_presentation.h"
#include "omega_first_lair_runtime.h"
#include "runtime.h"
#include "../../core/logging/log.h"

namespace dawn::state::activity::omega_ending {
namespace {
SRWLOCK g_lock=SRWLOCK_INIT;
coo::ending::Controller g_ending{};
omega_ending_transit::Transaction g_transit{};
ActivityInstanceKey g_transitActivity{};
std::uint64_t g_transitMember{};
std::uint64_t g_transitPublished{};
bool current(std::uint64_t run) noexcept {
    return run!=0 && run==mission_run_generation() && mission_seed_armed()
        && !omega_authority_quiesced() && world_phase()!=WorldPhase::idle;
}
void log_executor() noexcept {
    if(!g_ending.selected()) { return; }
    static coo::Diagnostics previous{};
    const auto d=g_ending.diagnostics();
    if(d.run==previous.run && d.incarnation==previous.incarnation && d.active==previous.active
        && d.complete==previous.complete && d.phase==previous.phase && d.failure==previous.failure) { return; }
    previous=d;
    const auto waiting=g_ending.waiting();
    std::array<char,384> line{};
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=coo_ending run=%llu incarnation=%llu phase=%u active=%08X complete=%08X failure=%u waiting=\"%.*s\"",
        static_cast<unsigned long long>(d.run),static_cast<unsigned long long>(d.incarnation),
        static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),
        static_cast<int>(waiting.size()),waiting.data());
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
}
void changed(const char* event) noexcept {
    log_executor();
    const auto state=g_ending.authority();
    std::array<char,384> text{};
    const int size=std::snprintf(text.data(),text.size(),
        "ev=omega_ending stage=%s run=%llu epoch=%llu actor=%08X generation=%u phase=%u revision=%u state121=%u play=%u complete=%u failed=%u arrived=%u retire=%u origin=%u handoff=%u",
        event,static_cast<unsigned long long>(state.token.run),static_cast<unsigned long long>(state.token.epoch),
        state.token.actor,state.token.generation,static_cast<unsigned>(g_ending.phase()),state.revision,
        state.bookendState?1U:0U,state.play?1U:0U,state.complete?1U:0U,state.failed?1U:0U,state.arrived?1U:0U,
        state.retireRoster?1U:0U,static_cast<unsigned>(state.token.origin),static_cast<unsigned>(g_ending.handoff()));
    if(size>0 && static_cast<std::size_t>(size)<text.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,
            {text.data(),static_cast<std::size_t>(size)});
    }
}

/** Bounded rejection diagnostics for the silent early returns below. Each reason is
 * written once per mission run (the table resets with the run), so a stalled ending
 * names its first blocker without flooding the log on every tick or publication. */
enum class Reject : std::uint8_t {
    status,stage,not_current,token,observe_token,resource,active_before_offer,
    arrival_not_current,arrival_token,arrival_state,arrival_region,
    transit_scope,transit_activity,transit_member,transit_begin,transit_foreign,transit_region,
    timeout,retirement_token,retirement_not_requested,count
};
constexpr const char* reject_name(Reject reason) noexcept {
    switch(reason) {
    case Reject::status: return "status";
    case Reject::stage: return "stage";
    case Reject::not_current: return "not_current";
    case Reject::token: return "token";
    case Reject::observe_token: return "observe_token";
    case Reject::resource: return "resource_not_registered";
    case Reject::active_before_offer: return "active_before_offer";
    case Reject::arrival_not_current: return "arrival_not_current";
    case Reject::arrival_token: return "arrival_token";
    case Reject::arrival_state: return "arrival_state121_not_requested";
    case Reject::arrival_region: return "arrival_region";
    case Reject::transit_scope: return "transit_scope";
    case Reject::transit_activity: return "transit_activity";
    case Reject::transit_member: return "transit_member";
    case Reject::transit_begin: return "transit_begin";
    case Reject::transit_foreign: return "transit_foreign_teleport";
    case Reject::transit_region: return "transit_region";
    case Reject::timeout: return "timeout";
    case Reject::retirement_token: return "retirement_token";
    case Reject::retirement_not_requested: return "retirement_not_requested";
    default: return "unknown";
    }
}
static_assert(static_cast<unsigned>(Reject::count)<=32U,"reject mask is 32 bits");
SRWLOCK g_rejectLock=SRWLOCK_INIT;
std::uint64_t g_rejectRun{};
std::uint32_t g_rejectMask{};
/** `detail` is a preformatted "key=value ..." tail; the caller owns its content. */
void reject(std::uint64_t run,Reject reason,const char* detail) noexcept {
    const auto bit=1U<<static_cast<unsigned>(reason);
    AcquireSRWLockExclusive(&g_rejectLock);
    if(g_rejectRun!=run) { g_rejectRun=run;g_rejectMask=0; }
    const bool first=(g_rejectMask&bit)==0;
    g_rejectMask|=bit;
    ReleaseSRWLockExclusive(&g_rejectLock);
    if(!first) { return; }
    std::array<char,384> text{};
    const int size=std::snprintf(text.data(),text.size(),
        "ev=omega_ending stage=reject reason=%s run=%llu %s",reject_name(reason),
        static_cast<unsigned long long>(run),detail==nullptr?"":detail);
    if(size>0 && static_cast<std::size_t>(size)<text.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,
            {text.data(),static_cast<std::size_t>(size)});
    }
}
template<std::size_t N,class... Args>
const char* format(std::array<char,N>& buffer,const char* form,Args... args) noexcept {
    const int size=std::snprintf(buffer.data(),buffer.size(),form,args...);
    if(size<=0 || static_cast<std::size_t>(size)>=buffer.size()) { buffer[0]='\0'; }
    return buffer.data();
}
const char* current_detail(std::array<char,160>& buffer) noexcept {
    return format(buffer,"mission_run=%llu seed_armed=%u quiesced=%u world_phase=%u",
        static_cast<unsigned long long>(mission_run_generation()),mission_seed_armed()?1U:0U,
        omega_authority_quiesced()?1U:0U,static_cast<unsigned>(world_phase()));
}
const char* token_detail(std::array<char,192>& buffer,const char* label,Token token) noexcept {
    return format(buffer,"%s_epoch=%llu %s_actor=%08X %s_generation=%u",label,
        static_cast<unsigned long long>(token.epoch),label,token.actor,label,token.generation);
}
/** Reacquire the encounter's complete immutable token; never fabricate its
 * character/entity identity from the narrower cinematic token. */
bool matches(Token token,const omega_first_lair::Status& status) noexcept {
    const auto& native=status.token;
    return token.origin==Origin::encounter && status.enabled && !status.failed && native.valid() && native.cycle==3
        && native.boss.run==token.run && native.boss.actionEpoch==token.epoch
        && native.boss.actor==token.actor && native.boss.generation==token.generation;
}
void forward(Token token,bool finished) noexcept {
    const auto status=omega_first_lair::status(token.run);
    if(matches(token,status)) {
        (void)omega_first_lair::observe_ending(status.token,finished);
    }
}
}
bool preview_available() noexcept {
    const auto navigation=omega_presentation::navigation();
    if(!navigation.enabled || navigation.landmark!=omega_presentation::Landmark::lighthouse
        || !current(navigation.run) || world_phase()!=WorldPhase::arrived) { return false; }
    const auto encounter=omega_first_lair::status(navigation.run);
    if(encounter.enabled) { return false; }
    AcquireSRWLockShared(&g_lock);
    const bool available=g_ending.phase()==Phase::dormant || g_ending.token().run!=navigation.run;
    ReleaseSRWLockShared(&g_lock);
    return available;
}
bool request_preview() noexcept {
    if(!preview_available()) { return false; }
    const auto run=mission_run_generation();
    AcquireSRWLockExclusive(&g_lock);
    if(current(run) && g_ending.token().run!=run) {
        g_ending.reset();g_transit={};g_transitActivity={};g_transitMember=0;g_transitPublished=0;
    }
    const bool accepted=current(run) && g_ending.request_preview(run,GetTickCount64());
    if(accepted) { changed("preview_requested"); }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}
Handoff handoff_status(std::uint64_t run) noexcept {
    AcquireSRWLockShared(&g_lock);
    const auto result = run != 0 && g_ending.token().run == run ? g_ending.handoff() : Handoff::dormant;
    ReleaseSRWLockShared(&g_lock);
    return result;
}
Token handoff_request() noexcept {
    AcquireSRWLockShared(&g_lock);
    const auto token=g_ending.handoff_request();
    const auto activity=g_transitActivity;
    ReleaseSRWLockShared(&g_lock);
    return token.valid() && current(token.run) && activity && contains(activity) ? token : Token{};
}
bool claim_handoff(Token token) noexcept {
    if(!token.valid() || token!=handoff_request()) { return false; }
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=current(token.run) && g_ending.claim_handoff(token);
    if(accepted) { changed("handoff_claimed"); }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}
void note_handoff_result(Token token,bool queued) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(g_ending.note_handoff_result(token,queued,GetTickCount64())) { changed(queued?"handoff_queued":"handoff_failed"); }
    ReleaseSRWLockExclusive(&g_lock);
}
bool request(Token token,bool executorOwned) noexcept {
    const auto status=omega_first_lair::status(token.run);
    if(!matches(token,status)) {
        std::array<char,320> detail{};
        reject(token.run,Reject::status,format(detail,
            "enabled=%u failed=%u cycle=%u crown_stage=%u native_epoch=%u native_actor=%08X native_generation=%u epoch=%llu actor=%08X generation=%u",
            status.enabled?1U:0U,status.failed?1U:0U,static_cast<unsigned>(status.token.cycle),
            static_cast<unsigned>(status.crownStage),status.boss.actionEpoch,status.boss.actor,status.boss.generation,
            static_cast<unsigned long long>(token.epoch),token.actor,token.generation));
        return false;
    }
    if(status.crownStage!=omega_first_lair::CrownStage::ending) {
        std::array<char,96> detail{};
        reject(token.run,Reject::stage,format(detail,"crown_stage=%u action=%u phase=%u",
            static_cast<unsigned>(status.crownStage),static_cast<unsigned>(status.action),static_cast<unsigned>(status.phase)));
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    if(current(token.run) && g_ending.token().run!=token.run) {
        g_ending.reset();g_transit={};g_transitActivity={};g_transitMember=0;g_transitPublished=0;
    }
    const auto before=g_ending.phase();
    const bool live=current(token.run);
    const bool accepted=live && g_ending.request(token,GetTickCount64(),executorOwned);
    if(accepted && before!=g_ending.phase()) { changed("requested"); }
    const auto held=g_ending.token();
    ReleaseSRWLockExclusive(&g_lock);
    if(!live) {
        std::array<char,160> detail{};
        reject(token.run,Reject::not_current,current_detail(detail));
    } else if(!accepted) {
        std::array<char,192> detail{};
        reject(token.run,Reject::token,token_detail(detail,"held",held));
    }
    if(accepted && status.action==omega_first_lair::Action::finishEncounter) {
        // Claim the scheduled immutable epoch once; later snapshot requests see
        // mechanicRequested/Playing and leave that claim alone.
        (void)omega_first_lair::claim_action(status.boss,omega_first_lair::Action::finishEncounter);
    }
    return accepted;
}
Authority authority(std::uint64_t run,std::uint64_t now) noexcept {
    // Presentation and ending locks are never nested. No presentation cue or
    // native component pointer is retained by the ending controller.
    const bool dialogue=omega_presentation::ending_dialogue_finished(run,now);
    AcquireSRWLockExclusive(&g_lock);
    Authority result{};
    Phase before{},after{};
    bool advanced=false,started=false,finished=false;
    if(current(run) && run==g_ending.token().run) {
        before=g_ending.phase();
        const auto previous=g_ending.authority();
        const auto previousHandoff=g_ending.handoff();
        g_ending.advance(now,dialogue);
        after=g_ending.phase();
        const auto state=g_ending.authority();
        started=!previous.started && state.started;
        finished=!previous.complete && state.complete;
        advanced=before!=after || previous.revision!=state.revision || previous.arrived!=state.arrived
            || previousHandoff!=g_ending.handoff();
        if(advanced) { changed("advance"); }
        result=g_ending.authority();
    }
    ReleaseSRWLockExclusive(&g_lock);
    if(started && result.token.origin==Origin::encounter) {
        omega_presentation::note_encounter(result.token.run,omega_presentation::Encounter::cinematic,3);
        forward(result.token,false);
    }
    if(finished) { forward(result.token,true); }
    if(advanced && after==Phase::failed) {
        // The phase that timed out is the diagnostic; `changed` only shows the failure.
        std::array<char,96> detail{};
        reject(run,Reject::timeout,format(detail,"phase_before=%u dialogue_finished=%u",
            static_cast<unsigned>(before),dialogue?1U:0U));
    }
    return result;
}
void update() noexcept {
    AcquireSRWLockShared(&g_lock);
    const auto token=g_ending.token();const bool selected=g_ending.selected();
    ReleaseSRWLockShared(&g_lock);
    if(!selected || !token.valid()) { return; }
    const auto now=GetTickCount64();
    if(current(token.run)) { static_cast<void>(authority(token.run,now)); }
    else {
        // Native commit can end Omega before the next publisher samples its
        // receipt. Finish only the already claimed handoff; publish no new work.
        AcquireSRWLockExclusive(&g_lock);
        const auto before=g_ending.handoff();
        if(g_ending.token()==token) { g_ending.finish_handoff(now); }
        if(before!=g_ending.handoff()) { changed("handoff_result"); }
        ReleaseSRWLockExclusive(&g_lock);
    }
}
Token retirement_request(std::uint64_t run) noexcept {
    AcquireSRWLockShared(&g_lock);
    const auto result=current(run) && run==g_ending.token().run
        ? g_ending.retirement_request() : Token{};
    ReleaseSRWLockShared(&g_lock);
    return result;
}
bool observe_retirement(Token token) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool live=current(token.run);
    const auto before=g_ending.phase();
    const auto held=g_ending.token();
    const bool accepted=live && g_ending.observe_retirement(token,GetTickCount64());
    if(accepted && before!=g_ending.phase()) { changed("native_retirement"); }
    ReleaseSRWLockExclusive(&g_lock);
    if(!accepted) {
        std::array<char,192> detail{};
        if(!live || !token.valid() || token!=held) {
            reject(token.run,Reject::retirement_token,token_detail(detail,"held",held));
        } else {
            reject(token.run,Reject::retirement_not_requested,format(detail,"phase=%u",static_cast<unsigned>(before)));
        }
    }
    return accepted;
}
bool request_skip(std::uint64_t run) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=current(run) && g_ending.token().run==run && g_ending.skip(GetTickCount64());
    if(accepted) { changed("skip"); }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}
void observe(Token token,std::uint32_t revision,bool active,bool ready) noexcept {
    bool started{},finished{};
    Reject pending=Reject::count;
    AcquireSRWLockExclusive(&g_lock);
    const bool live=current(token.run);
    if(live) {
        const auto before=g_ending.phase();
        const auto state=g_ending.authority();
        if(token!=state.token || !token.valid()) { pending=Reject::observe_token; }
        else if(before==Phase::preparing && state.arrived && !ready) { pending=Reject::resource; }
        else if(before==Phase::preparing && state.arrived && ready && active) { pending=Reject::active_before_offer; }
        g_ending.observe(token,revision,active,ready,GetTickCount64());
        started=before!=Phase::playing && g_ending.phase()==Phase::playing;
        finished=before!=Phase::complete && g_ending.phase()==Phase::complete;
        if(before!=g_ending.phase()) { changed("native"); }
    }
    ReleaseSRWLockExclusive(&g_lock);
    if(!live) {
        std::array<char,160> detail{};
        reject(token.run,Reject::not_current,current_detail(detail));
    } else if(pending!=Reject::count) {
        std::array<char,160> detail{};
        reject(token.run,pending,format(detail,"revision=%u active=%u ready=%u",revision,active?1U:0U,ready?1U:0U));
    }
    if(started && token.origin==Origin::encounter) {
        omega_presentation::note_encounter(token.run,omega_presentation::Encounter::cinematic,3);
        forward(token,false);
    }
    if(finished) { forward(token,true); }
}
bool observe_arrival(Token token,std::int32_t region) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto state=g_ending.authority();
    const bool live=current(token.run);
    const bool accepted=live && g_ending.observe_arrival(token,region,GetTickCount64());
    if(accepted && !state.arrived) { changed("native_arrival"); }
    ReleaseSRWLockExclusive(&g_lock);
    if(!accepted) {
        std::array<char,192> detail{};
        if(!live) { reject(token.run,Reject::arrival_not_current,format(detail,"region=%d",region)); }
        else if(token!=state.token || !token.valid()) {
            reject(token.run,Reject::arrival_token,token_detail(detail,"held",state.token));
        } else if(!state.bookendState) { reject(token.run,Reject::arrival_state,format(detail,"region=%d",region)); }
        else if(region!=kSlice) { reject(token.run,Reject::arrival_region,format(detail,"region=%d expected=%u",region,static_cast<unsigned>(kSlice))); }
    }
    return accepted;
}
omega_ending_transit::Authority project_transit(const TransitInput& input) noexcept {
    omega_ending_transit::Authority result{};
    if(!input.validatedOmega || !input.activity || !contains(input.activity)
        || input.memberKey==0 || input.memberKey==UINT64_MAX || !current(input.run)) {
        // Only worth a line while an ending is actually pending for this run.
        AcquireSRWLockShared(&g_lock);
        const bool pending=g_ending.token().run==input.run && g_ending.authority().bookendState;
        ReleaseSRWLockShared(&g_lock);
        if(pending) {
            std::array<char,192> detail{};
            reject(input.run,Reject::transit_scope,format(detail,
                "validated=%u activity=%u contains=%u member=%016llX current=%u",
                input.validatedOmega?1U:0U,input.activity?1U:0U,
                input.activity && contains(input.activity)?1U:0U,
                static_cast<unsigned long long>(input.memberKey),current(input.run)?1U:0U));
        }
        return result;
    }
    const auto command=authority(input.run,GetTickCount64());
    if(!command.token.valid() || !command.bookendState || command.failed) { return result; }
    Reject pending=Reject::count;
    std::array<char,224> detail{};
    AcquireSRWLockExclusive(&g_lock);
    const auto held=g_ending.authority();
    if(current(input.run) && command.token==g_ending.token() && held.bookendState && !held.failed) {
        // Pin one exact activity incarnation/member for the ending. An unrelated
        // membership publication must not reset or steal an ongoing transaction.
        if(!g_transitActivity) { g_transitActivity=input.activity;g_transitMember=input.memberKey; }
        if(g_transitActivity!=input.activity) { pending=Reject::transit_activity; }
        else if(g_transitMember!=input.memberKey) {
            pending=Reject::transit_member;
            format(detail,"member=%016llX pinned=%016llX",
                static_cast<unsigned long long>(input.memberKey),static_cast<unsigned long long>(g_transitMember));
        } else {
            const omega_ending_transit::Scope scope{command.token,input.memberKey,true};
            const auto& local=input.native.local;
            if(!g_transit.begin(scope,{kSlice,kArrivalSpawnSet},input.native)) {
                // C72CE0 starts a new command from idle. The untouched tuple may
                // precede the first optional message22 receipt; a foreign active
                // command or inconsistent unreported tuple still blocks entry.
                pending=Reject::transit_begin;
                format(detail,"has_teleport=%u local_state=%d local_token=%u local_index=%d local_hash=%08X transit_phase=%u",
                    input.native.hasTeleport?1U:0U,static_cast<int>(local.state),static_cast<unsigned>(local.token),
                    local.sliceSetIndex,local.sliceSetHash,static_cast<unsigned>(g_transit.phase()));
            } else {
                if(g_transit.observe(scope,input.native)
                    && g_ending.observe_arrival(command.token,input.native.currentRegion,GetTickCount64())) {
                    changed("native_arrival");
                }
                result=g_transit.authority(scope);
                if(!result.arrived && input.native.hasTeleport && local.state!=0) {
                    const bool ours=local.token==result.host.token && local.sliceSetIndex==result.host.sliceSetIndex
                        && local.sliceSetHash==result.host.sliceSetHash;
                    if(!ours) {
                        pending=Reject::transit_foreign;
                        format(detail,"local_state=%d local_token=%u local_index=%d local_hash=%08X host_token=%u",
                            static_cast<int>(local.state),static_cast<unsigned>(local.token),local.sliceSetIndex,
                            local.sliceSetHash,static_cast<unsigned>(result.host.token));
                    } else if(local.state==3 && (!input.native.hasRegion || input.native.currentRegion!=kSlice)) {
                        pending=Reject::transit_region;
                        format(detail,"has_region=%u current_region=%d expected=%u",
                            input.native.hasRegion?1U:0U,input.native.currentRegion,static_cast<unsigned>(kSlice));
                    }
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    if(pending!=Reject::count) { reject(input.run,pending,detail.data()); }
    return result;
}
bool membership_due(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept {
    if(!activity || !contains(activity) || !current(run)) { return false; }
    const auto command=authority(run,now);
    if(!command.bookendState || command.failed) { return false; }
    AcquireSRWLockShared(&g_lock);
    const bool due=command.token==g_ending.token()
        && (!g_transitActivity || g_transitActivity==activity)
        && g_transit.phase()!=omega_ending_transit::Phase::complete
        && (g_transitPublished==0 || (now>=g_transitPublished && now-g_transitPublished>=500));
    ReleaseSRWLockShared(&g_lock);return due;
}
void note_membership_published(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(current(run) && run==g_ending.token().run && activity==g_transitActivity) { g_transitPublished=now; }
    ReleaseSRWLockExclusive(&g_lock);
}
void reset() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_ending.reset();g_transit={};g_transitActivity={};g_transitMember=0;g_transitPublished=0;
    ReleaseSRWLockExclusive(&g_lock);
    AcquireSRWLockExclusive(&g_rejectLock);
    g_rejectRun=0;g_rejectMask=0;
    ReleaseSRWLockExclusive(&g_rejectLock);
}
} // namespace dawn::state::activity::omega_ending
