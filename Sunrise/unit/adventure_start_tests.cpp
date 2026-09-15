#include "middleware/bap/activity_message/adventure_start_request.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/bap/activity_message/activity_global_state_encoder.h"
#include "server/runtime/activity/adventure_mercury_start_routes.h"
#include "state/activity/adventure_destination_transition.h"
#include "server/bap/encrypted/activity_message/adventure_start_route.h"
#include "server/runtime/activity/mercury_definition.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace wire=sunrise::middleware::bap::activity_message::adventure_start;
namespace host=sunrise::server::runtime::activity::adventure_start;
namespace mercury=sunrise::server::runtime::activity::adventure::mercury;
namespace route_fixture {
static sunrise::state::activity::ActivityState store{};
bool wrongAccount{},missingPlacements{};
std::string lastLog;
}
// External State/runtime boundaries are deterministic; the production adapter,
// parser, profile routes and State prepare/commit algorithm remain unmodified.
namespace sunrise::state {
AccountState account_snapshot() noexcept {
    AccountState account{};account.primarySoid=route_fixture::wrongAccount?1:0x9EAA300100100100ULL;return account;
}
}
namespace sunrise::state::activity::adventure_destination {
bool snapshot(ActivityInstanceKey owner,Snapshot& output) noexcept {
    output={};const auto index=activity::transactions::find_session(route_fixture::store,owner);
    if(index>=route_fixture::store.sessions.size() || !route_fixture::store.sessions[index].joined) return false;
    const auto& record=route_fixture::store.sessions[index];output={owner,record.recordRevision,record.destination};return true;
}
Result prepare(ActivityInstanceKey owner,std::uint64_t revision,const destination::DestinationSelection& selection,Pending& pending) noexcept {
    return prepare_from(route_fixture::store,owner,revision,selection,pending);
}
}
namespace sunrise::server::runtime::activity::native_activity {
bool snapshot_placements(Owner,const NativeActivityDefinition*& definition,placement::wire::Batch& output) noexcept {
    definition=&mercury::kActivity;output={};
    if(!route_fixture::missingPlacements) {
        output.count=3;for(std::uint16_t i=0;i<3;++i) output.entries[i]={0x2749BAAE,i,15};
    }
    return true;
}
}
namespace sunrise::core::log {
void write(Channel,Level,std::string_view text) noexcept {route_fixture::lastLog=text;}
bool append_hex(std::span<char> line,std::size_t& length,std::span<const std::byte> bytes) noexcept {
    constexpr char digits[]="0123456789ABCDEF";
    for(auto byte:bytes) {
        if(length+2>=line.size()) return false;
        const auto n=std::to_integer<unsigned>(byte);line[length++]=digits[n>>4];line[length++]=digits[n&15];
    }
    return true;
}
}
unsigned checks{};
#define CHECK(condition) do { ++checks; if(!(condition)) {std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#condition);std::exit(1);} } while(false)
struct Encoded {std::array<std::byte,232> data{};std::size_t bytes{},bits{};};
// Schema-authored synthetic request; the original capture contains only its
// first 64 bytes and must fail complete parsing. Native decoder proof is separate.
Encoded encode(std::int16_t activity=1076,std::uint8_t revision=3,
               std::string_view package="mercury_freeroam",std::uint8_t skulls=0,std::uint8_t pairs=0) {
    Encoded out{};
    sunrise::middleware::encoding::bits::Writer w(out.data);
    CHECK(w.write(2,4) && w.write(static_cast<unsigned>(activity+1),12) && w.write(static_cast<unsigned>(activity+1),12));
    CHECK(w.write(1,1) && w.write(0,9));
    CHECK(w.write(1,1) && w.write(0x9EAA300100100100ULL,64));
    CHECK(w.write(1,1) && w.write(0xA0A016D88F937C17ULL,64));
    CHECK(w.write(skulls,5));
    for(std::uint8_t i=0;i<skulls;++i) CHECK(w.write(1,2) && w.write(1,7));
    CHECK(w.write(revision,8) && w.write(1,1) && w.write(127,8));
    CHECK(w.write(1,1) && w.write(0x811C9DC5,32));
    CHECK(w.write(1,1) && w.write(0x811C9DC5,32) && w.write(1,1));
    for(std::size_t i=0;i<40;++i) CHECK(w.write(i<package.size()?static_cast<std::uint8_t>(package[i]+128):128,8));
    CHECK(w.write(0,1) && w.write(0,1) && w.write(0,1) && w.write(pairs?1:0,1));
    if(pairs) {CHECK(w.write(pairs,6));for(unsigned i=0;i<pairs;++i) CHECK(w.write(i,13));}
    out.bits=w.bit_count();CHECK(w.finish(out.bytes));return out;
}
wire::Request request(const Encoded& source) {
    wire::Request result{};CHECK(wire::parse(std::span(source.data).first(source.bytes),result));return result;
}
void adapter_routes(const Encoded& encoded) {
    namespace state=sunrise::state::activity;
    namespace bap=sunrise::server::bap::encrypted::activity_message;
    const auto current=request(encode(29,2));
    auto& record=route_fixture::store.sessions[0];record.occupied=true;record.joined=true;
    record.sessionId=42;record.lifecycle={{7},{9},false};record.recordRevision=18;
    route_fixture::store.stateRevision=22;
    auto& destination=record.destination;
    destination.activityIndex=29;destination.packageName=current.selection.packageName;
    destination.packageNameLength=current.selection.packageNameLength;
    destination.descriptorBits=current.selection.descriptorBits;
    destination.descriptorBitLength=static_cast<std::uint16_t>(current.selection.descriptorBitLength);
    destination.descriptorNameBit=static_cast<std::uint16_t>(current.selection.packageNameBitOffset);
    destination.hasDescriptorName=true;
    destination.hasArrivalBubbleHash=true;destination.arrivalBubbleHash=0xA83A9175;
    destination.hasSpawnSetHash=true;destination.spawnSetHash=0xD49C610E;
    sunrise::middleware::bap::activity_message::Request envelope{};
    envelope.messageType=11;envelope.accountHandle=42;envelope.payload=std::span(encoded.data).first(encoded.bytes);
    bap::ActivityPlan plan{};
    CHECK(bap::adventure_start::prepare({42,{7}},envelope,plan));
    CHECK(plan.instanceKey==(state::ActivityInstanceKey{42,{7}}) && plan.sessionId==42);
    CHECK(plan.mutationDomain==bap::MutationDomain::destination && plan.delivery==bap::Delivery::globalStateNotification);
    CHECK(plan.destinationMutation.prepared && plan.destinationMutation.after.activityIndex==1076);
    CHECK(plan.destinationMutation.after.hasArrivalBubbleHash && plan.destinationMutation.after.arrivalBubbleHash==0xA83A9175);
    CHECK(plan.destinationMutation.after.hasSpawnSetHash && plan.destinationMutation.after.spawnSetHash==0xD49C610E);
    CHECK(record.destination.activityIndex==29 && record.recordRevision==18);
    CHECK(route_fixture::lastLog.find("captured=73 body=24354358")!=std::string::npos);
    auto pending=plan.destinationMutation;
    CHECK(state::adventure_destination::commit_to(route_fixture::store,pending));
    CHECK(record.destination.activityIndex==1076);
    CHECK(!bap::adventure_start::prepare({42,{7}},envelope,plan));
    CHECK(!plan.destinationMutation.prepared && plan.mutationDomain==bap::MutationDomain::none);
    CHECK(route_fixture::lastLog.find("result=revision")!=std::string::npos);
    destination.descriptorBits=current.selection.descriptorBits;record.recordRevision=18;
    route_fixture::wrongAccount=true;
    CHECK(!bap::adventure_start::prepare({42,{7}},envelope,plan));
    CHECK(route_fixture::lastLog.find("result=account")!=std::string::npos);
    route_fixture::wrongAccount=false;route_fixture::missingPlacements=true;
    CHECK(!bap::adventure_start::prepare({42,{7}},envelope,plan));
    CHECK(route_fixture::lastLog.find("result=placement")!=std::string::npos);
    route_fixture::missingPlacements=false;
    envelope.payload=envelope.payload.first(64);
    CHECK(!bap::adventure_start::prepare({42,{7}},envelope,plan));
    CHECK(route_fixture::lastLog.find("result=malformed")!=std::string::npos);
}
void destination_transactions(const wire::Request& parsed) {
    namespace state=sunrise::state::activity;
    namespace change=state::adventure_destination;
    static state::ActivityState store{};
    auto& record=store.sessions[0];record.occupied=true;record.joined=true;
    record.sessionId=42;record.lifecycle={{7},{9},false};
    record.recordRevision=18;record.createdRevision=4;record.joinedRevision=6;
    record.memberKey=31337;record.heldEntitySlots[3]=std::byte{7};record.serverEntitySlots[6]=std::byte{9};
    record.membership.region={120,0xA83A9175};
    record.destination.packageNameLength=parsed.selection.packageNameLength;
    record.destination.packageName=parsed.selection.packageName;
    record.destination.activityIndex=29;record.destination.hasArrivalBubbleOverride=true;
    record.destination.arrivalBubbleOverride=15;
    record.destination.hasSliceSetOverride=true;record.destination.sliceSetOverride=120;
    store.stateRevision=22;
    auto selected=record.destination;
    selected.activityIndex=1076;selected.previousActivityIndex=1076;selected.reason=1;
    selected.descriptorBits=parsed.selection.descriptorBits;
    selected.descriptorBitLength=static_cast<std::uint16_t>(parsed.selection.descriptorBitLength);
    selected.descriptorNameBit=static_cast<std::uint16_t>(parsed.selection.packageNameBitOffset);
    selected.hasDescriptorName=true;
    change::Pending pending{};
    CHECK(change::prepare_from(store,{42,{7}},18,selected,pending)==change::Result::prepared);
    CHECK(record.destination.activityIndex==29 && record.recordRevision==18);
    auto stale=pending;record.recordRevision=19;
    CHECK(!change::commit_to(store,stale) && !stale.prepared);
    record.recordRevision=18;
    CHECK(change::commit_to(store,pending));
    CHECK(!pending.prepared && !change::commit_to(store,pending));
    CHECK(record.destination.activityIndex==1076 && record.recordRevision==23 && store.stateRevision==23);
    CHECK(record.lifecycle.incarnation.value==7 && record.lifecycle.hostRegion.value==9);
    CHECK(record.joined && record.createdRevision==4 && record.joinedRevision==6 && record.memberKey==31337);
    CHECK(record.heldEntitySlots[3]==std::byte{7} && record.serverEntitySlots[6]==std::byte{9});
    CHECK(record.membership.region.index==120 && record.membership.region.hash==0xA83A9175);
    CHECK(record.destination.hasArrivalBubbleOverride && record.destination.arrivalBubbleOverride==15
       && record.destination.hasSliceSetOverride && record.destination.sliceSetOverride==120);
    CHECK(record.destination.descriptorBits==selected.descriptorBits);
    CHECK(change::prepare_from(store,{42,{8}},23,selected,pending)==change::Result::unjoinedOwner);
    CHECK(change::prepare_from(store,{42,{7}},18,selected,pending)==change::Result::staleRevision);
    auto invalid=selected;invalid.packageName[0]='x';
    CHECK(change::prepare_from(store,{42,{7}},23,invalid,pending)==change::Result::invalidSelection);
    invalid=selected;invalid.descriptorBitLength=UINT16_MAX;
    CHECK(change::prepare_from(store,{42,{7}},23,invalid,pending)==change::Result::invalidSelection);
    store.stateRevision=state::kMaximumRevision;
    CHECK(change::prepare_from(store,{42,{7}},23,selected,pending)==change::Result::exhausted);
}
int main(int argc,char** argv) {
    const auto encoded=encode();const auto parsed=request(encoded);
    if(argc>2) {
        namespace global=sunrise::middleware::bap::activity_message::global_activity_state;
        global::GlobalActivityState state{};
        state.nameLength=15;std::memcpy(state.name.data(),"mercury_freeroam",15);
        state.bubbleCount=24;state.bubbleStates.fill(0x7f);state.bubbleStates[15]=0x80;
        state.hasSliceSet=true;state.sliceSetIndex=120;state.spawnSetHash=0xD49C610E;
        state.descriptorBits=parsed.selection.descriptorBits;
        state.descriptorBitLength=parsed.selection.descriptorBitLength;
        std::array<std::byte,400> bytes{};std::size_t size{};
        CHECK(global::encode_global_activity_state(state,bytes,size));
        FILE* file{};CHECK(fopen_s(&file,argv[2],"wb")==0 && file);
        CHECK(std::fwrite(bytes.data(),1,size,file)==size);CHECK(std::fclose(file)==0);
    }
    CHECK(parsed.selection.reason==1 && parsed.selection.sourceActivityIndex==1076 && parsed.selection.activityIndex==1076);
    CHECK(parsed.hasAccount && parsed.account==0x9EAA300100100100ULL);
    CHECK(parsed.hasNonce && parsed.nonce==0xA0A016D88F937C17ULL && parsed.revision==3);
    CHECK(parsed.selection.hasElementIndex && parsed.selection.elementIndex==-1);
    CHECK(parsed.selection.hasArrivalBubbleHash && parsed.selection.arrivalBubbleHash==0x811C9DC5);
    CHECK(parsed.selection.hasSpawnSetHash && parsed.selection.spawnSetHash==0x811C9DC5);
    CHECK(host::package(parsed)=="mercury_freeroam" && parsed.selection.descriptorBitLength==encoded.bits);
    CHECK(std::memcmp(parsed.selection.descriptorBits.data(),encoded.data.data(),encoded.bytes)==0);
    wire::Request rejected=parsed;
    for(std::size_t n=0;n<encoded.bytes;++n) {
        CHECK(!wire::parse(std::span(encoded.data).first(n),rejected));
        CHECK(rejected.selection.descriptorBitLength==0 && !rejected.hasNonce);
    }
    auto malformed=encoded;malformed.data[encoded.bytes-1]|=std::byte{1};
    CHECK(!wire::parse(std::span(malformed.data).first(encoded.bytes),rejected));
    CHECK(!wire::parse(std::span(encoded.data).first(encoded.bytes+1),rejected));
    auto excessive=encode(1076,3,"mercury_freeroam",17);
    CHECK(!wire::parse(std::span(excessive.data).first(excessive.bytes),rejected));
    excessive=encode(1076,3,"mercury_freeroam",0,33);
    CHECK(!wire::parse(std::span(excessive.data).first(excessive.bytes),rejected));
    const auto widestPairs=encode(1076,3,"mercury_freeroam",0,32);
    CHECK(wire::parse(std::span(widestPairs.data).first(widestPairs.bytes),rejected));
    auto badName=encode(1076,3,"mercury_freeroam/evil");
    CHECK(!wire::parse(std::span(badName.data).first(badName.bytes),rejected));
    badName=encode(1076,3,"abcdefghijklmnopqrstuvwxyzabcdefghijklmn");
    CHECK(!wire::parse(std::span(badName.data).first(badName.bytes),rejected));
    // Exact first 64 captured bytes. It must not be padded into an accepted message.
    constexpr char preview[]="24354358033D54600200200201A0A016D88F937C17001DFF02393B8B811C9DC5F6F2F971FAF97CEFF37972F2F977F0F6C0404040404040404040404040404040";
    std::array<std::byte,64> capture{};
    const auto nibble=[](char c) {return c<='9'?c-'0':c-'A'+10;};
    for(std::size_t i=0;i<capture.size();++i) capture[i]=std::byte((nibble(preview[i*2])<<4)|nibble(preview[i*2+1]));
    CHECK(std::memcmp(capture.data(),encoded.data.data(),capture.size())==0);
    CHECK(!wire::parse(capture,rejected));

    host::Context context{};context.owner={0x1234,{2}};context.scenario=0x80F4696A;
    context.expectedRecordRevision=7;context.current=request(encode(29,2));
    context.published.count=3;
    for(std::uint16_t i=0;i<3;++i) context.published.entries[i]={0x2749BAAE,i,15};
    host::Plan plan{};
    for(std::int16_t i=1076;i<=1078;++i) {
        const auto selected=request(encode(i));
        CHECK(host::prepare(context,selected,mercury::kStartRoutes,plan)==host::Result::accepted);
        CHECK(plan.owner==context.owner && plan.expectedRecordRevision==7 && plan.route.activity==i);
        CHECK(plan.request.selection.descriptorBitLength==selected.selection.descriptorBitLength);
    }
    auto changed=context;changed.owner.incarnation={};
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::invalidOwner);
    CHECK(!static_cast<bool>(plan.owner));
    changed=context;changed.current.nonce^=1;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::identityMismatch);
    changed=context;changed.current.account^=1;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::identityMismatch);
    changed=context;changed.current.revision=3;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::staleRevision);
    changed=context;changed.published.count=0;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::placementUnavailable);
    changed=context;changed.published.entries[0].bubble=14;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::placementUnavailable);
    changed=context;changed.published.entries[0].interactionMode=
        sunrise::middleware::bap::activity_message::native::interaction::Mode::disabled;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::placementUnavailable);
    changed=context;changed.scenario=0x80F47522;
    CHECK(host::prepare(changed,parsed,mercury::kStartRoutes,plan)==host::Result::unsupportedRoute);
    CHECK(host::prepare(context,request(encode(342)),mercury::kStartRoutes,plan)==host::Result::unsupportedRoute);
    auto duplicate=mercury::kStartRoutes;duplicate[1]=duplicate[0];
    CHECK(host::prepare(context,parsed,duplicate,plan)==host::Result::unsupportedRoute);
    changed=context;changed.current.revision=255;
    CHECK(host::prepare(changed,request(encode(1076,1)),mercury::kStartRoutes,plan)==host::Result::accepted);
    // Data chooses the route; there is no hardcoded Mercury branch in preparation.
    auto other=mercury::kStartRoutes;other[0].scenario=123;changed=context;changed.scenario=123;
    CHECK(host::prepare(changed,parsed,other,plan)==host::Result::accepted);
    destination_transactions(parsed);
    adapter_routes(encoded);
    if(argc>=2) {
        std::FILE* file{};CHECK(fopen_s(&file,argv[1],"wb")==0 && file);
        CHECK(std::fwrite(encoded.data.data(),1,encoded.bytes,file)==encoded.bytes);CHECK(std::fclose(file)==0);
    }
    std::printf("%u checks, 0 failures; synthetic native descriptor %zu bits/%zu bytes; captured prefix matched, truncated capture rejected\n",checks,encoded.bits,encoded.bytes);
}
