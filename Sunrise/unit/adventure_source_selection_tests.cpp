#include "state/runtime/storage/internal.h"
#include "state/activity/membership/activity_membership_query.h"
#include "state/activity/transactions/internal.h"
#include "server/runtime/activity/mercury_definition.h"
#include "server/runtime/activity/adventure_opening_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace sunrise::state::runtime::storage { State g_state{}; SRWLOCK g_stateLock=SRWLOCK_INIT; }
namespace state=sunrise::state::activity;
namespace member=state::membership;
namespace storage=sunrise::state::runtime::storage;
namespace runtime=sunrise::server::runtime::activity;
namespace opening=runtime::adventure;
namespace wire=runtime::adventure_start::wire;
unsigned checks{};
void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL %u: %s\n",checks,why);std::exit(1);}}
std::vector<std::byte> read(const char* path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"full request fixture opens");
    const auto n=in.tellg();check(n>0 && n<256,"bounded raw request");
    std::vector<std::byte> bytes(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),n);
    check(bool(in),"full raw request read");return bytes;
}
wire::Request parse(const state::destination::DestinationSelection& d){
    wire::Request r{};check(d.descriptorBitLength && wire::parse(std::span(d.descriptorBits).first((d.descriptorBitLength+7U)/8U),r)
        && r.selection.descriptorBitLength==d.descriptorBitLength,"exact retained descriptor parses");return r;
}
int main(int argc,char** argv){
    if(argc!=4)return 2;
    const auto start=read(argv[1]);const auto abandon=read(argv[2]);wire::Request selected{},returned{};
    check(start.size()==90 && wire::parse(start,selected) && selected.selection.activityIndex==1076
        && selected.selection.reason==1 && selected.revision==5,"actual r12 start1076 revision5");
    check(abandon.size()==78 && wire::parse(abandon,returned) && returned.selection.activityIndex==29
        && returned.selection.reason==3,"actual r12 abandon29 reason3, no guessed tail");
    std::string error;const auto document=state::coo::script::MissionDocument::read_native_policy(argv[3],runtime::mercury::kProfile,error);
    if(!document)std::fprintf(stderr,"%s\n",error.c_str());check(bool(document),"current production Mercury profile");
    auto& world=storage::g_state.activity;world.stateRevision=91;
    const state::ActivityInstanceKey owner{0x9EAA300100200003ULL,{1}},child{0x9EAA300100200004ULL,{1}};
    const auto install=[&](std::size_t index,state::ActivityInstanceKey key,const wire::Request& r){
        auto& record=world.sessions[index];record.occupied=record.joined=true;record.sessionId=key.sessionId;
        record.lifecycle={key.incarnation,{3},false};record.joinedRevision=10+index;record.recordRevision=20+index;
        record.membership.region={96,0xB34BD815};record.membership.revision=7;record.membership.acknowledgedRevision=7;
        record.destination.packageName=r.selection.packageName;record.destination.packageNameLength=r.selection.packageNameLength;
        record.destination.activityIndex=r.selection.activityIndex;record.destination.reason=r.selection.reason;
        record.destination.descriptorBits=r.selection.descriptorBits;
        record.destination.descriptorBitLength=static_cast<std::uint16_t>(r.selection.descriptorBitLength);
        return &record;
    };
    auto* creator=install(0,owner,selected);auto* bound=install(1,child,returned);
    // The child's cached Mercury descriptor is independent of the later creator selection.
    const auto childBefore=bound->destination;const auto creatorBefore=creator->destination;
    std::memset(&bound->bubbleAuthority,0x23,sizeof bound->bubbleAuthority);
    const auto grants=bound->bubbleAuthority;
    member::RegionSnapshotInputs copied{};
    check(member::snapshot_region_inputs(child,owner,{owner,{3}},copied),"production joined child/creator query");
    check(std::memcmp(&copied.destination,&childBefore,sizeof childBefore)==0
        && std::memcmp(&copied.grantBefore,&grants,sizeof grants)==0,"bound descriptor and grants retained bit exact");
    check(std::memcmp(&copied.sourceDestination,&creatorBefore,sizeof creatorBefore)==0
        && copied.sourceMembership.region.index==96 && copied.sourceMembership.region.hash==0xB34BD815,
        "creator selected descriptor and actual opaque membership copied together");
    check(copied.boundRecordRevision==bound->recordRevision && copied.sourceRecordRevision==creator->recordRevision
        && copied.stateRevision==world.stateRevision,"both existing revision guards retained");
    opening::OpeningRuntime previous;
    check(previous.begin(owner,97,runtime::mercury::kActivity,*document),"old-path reproduction owner starts");
    check(previous.update(15,true,selected).requested,"old-path opening request published");
    check(previous.update(12,true,parse(copied.destination)).conflictingSelection,
        "OLD FAIL: bound public descriptor permanently poisons creator Adventure despite no creator change");
    opening::native_bridge::release(owner);opening::dialogue_bridge::release(owner);
    opening::OpeningRuntime fixed;
    check(fixed.begin(owner,98,runtime::mercury::kActivity,*document),"correct-path owner starts");
    check(fixed.update(15,true,parse(copied.sourceDestination)).requested,"creator descriptor starts exact Adventure");
    const auto cue=fixed.ticket();const auto dialogue=fixed.dialogue_ticket();
    for(const auto scope:{15U,12U,15U,12U}){
        check(member::snapshot_region_inputs(child,owner,{},copied),"interleaved public refresh qualified");
        check(!fixed.update(scope,true,parse(copied.sourceDestination)).conflictingSelection
            && fixed.ticket()==cue && fixed.dialogue_ticket()==dialogue,"refresh retains creator run and native tickets");
        check(member::snapshot_region_inputs(owner,owner,{},copied),"interleaved creator refresh qualified");
        check(!fixed.update(scope,true,parse(copied.sourceDestination)).conflictingSelection,"self refresh shares same selection");
    }
    member::PendingMutation pending{};member::PeriodicRegionRefresh periodic{};
    check(member::prepare_periodic_region_refresh(owner,96,pending,periodic)
        && std::memcmp(&periodic.inputs.destination,&periodic.inputs.sourceDestination,sizeof creatorBefore)==0,
        "actual periodic self-refresh populates both exact inputs");
    check(!member::snapshot_region_inputs(child,{owner.sessionId,{2}},{},copied)
        && !member::snapshot_region_inputs(child,owner,{owner,{4}},copied),"stale owner incarnation/region cannot read selection");
    bound->joined=false;check(!member::snapshot_region_inputs(child,owner,{},copied),"retired child cannot refresh creator");bound->joined=true;
    creator->destination.packageNameLength=41;check(!member::snapshot_region_inputs(child,owner,{},copied),"invalid source descriptor rejected");creator->destination=creatorBefore;
    // A genuine owner selection change remains rejected until an explicit return protocol is implemented.
    creator->destination=childBefore;++creator->recordRevision;
    check(member::snapshot_region_inputs(child,owner,{},copied)
        && fixed.update(12,true,parse(copied.sourceDestination)).conflictingSelection,
        "genuine creator abandon cannot impersonate an unchanged Adventure");
    check(fixed.ticket()==cue && fixed.dialogue_ticket()==dialogue,"conflict does not fabricate source retirement");
    check(state::transactions::retire_exact(world,child)==state::RetireResult::retired
        && state::transactions::find_session(world,owner)!=state::kInvalidSessionSlot,"child retirement preserves creator record");
    check(state::transactions::retire_exact(world,owner)==state::RetireResult::retired
        && !member::snapshot_region_inputs(owner,owner,{},copied),"real creator retirement revokes subsequent query");
    check(state::transactions::retire_exact(world,owner)==state::RetireResult::alreadyRetired,"owner retirement is idempotent");
    opening::native_bridge::release(owner);opening::dialogue_bridge::release(owner);
    check(!opening::native_bridge::lookup(cue.definition).epoch && !opening::dialogue_bridge::lookup(dialogue.authored.definition).epoch,
        "real owner retirement clears cue/dialogue receipts");
    std::printf("PASS %u production-query, old-fail/new-pass creator selection, raw abandon and owner retirement checks\n",checks);
}
