#include "server/bap/encrypted/push/activity/activity_clock_push.h"
#include "server/bap/encrypted/push/activity/activity_notification_frame.h"
#include "server/runtime/activity/native_activity_runtime.h"
#include "middleware/bap/activity_message/native/activity_clock_sync.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>
namespace bap=dawn::server::bap;
namespace push=bap::encrypted::push::activity;
namespace activity=dawn::server::runtime::activity;
namespace wire=dawn::middleware::bap::activity_message::native::activity_clock;
namespace bits=dawn::middleware::encoding::bits;
namespace group=dawn::server::gameplay::group;
using Owner=dawn::state::activity::ActivityInstanceKey;
unsigned checks{},failures{};
#define CHECK(x) do {++checks;if(!(x)){++failures;std::printf("FAIL %d: %s\n",__LINE__,#x);}}while(false)
const Owner creator{0x9EAA300100200001ULL,{7}},recipient{0x9EAA300100200002ULL,{9}};
struct Recorded {std::uint64_t target{};std::uint32_t type{};std::vector<std::byte> bytes;};
std::vector<Recorded> calls;
activity::activity_clock::Publication clockValue{};
bool haveClock{},edgeCurrent{},acquireOk{};
unsigned failureCall{},invalidateCall{},snapshotCalls{},invalidateSnapshot{},releaseCount{};
static bap::Session session;
static bap::Scratch scratch;
void reset() {
    session.activity.joinedForeignSession=true;session.activity.instance=recipient;
    session.activity.lineage={recipient,creator,bap::RegionLineageKind::groupDerivedBorrow};
    session.activityPatchEpochSeen=true;session.activityPatchEpoch={0x1122334455667788ULL,0xAABBCCDDEEFF0011ULL};
    clockValue={{creator,83,14,0x80F4696A,15},{false,1000.0F/30},11444523};
    calls.clear();haveClock=edgeCurrent=acquireOk=true;
    failureCall=invalidateCall=snapshotCalls=invalidateSnapshot=releaseCount=0;
}
// These seams model transport capacity/failure and concurrent lifetime changes.
// Native acceptance is proved separately with the original executable; these
// stubs are never native observations or production authority.
namespace dawn::server::runtime::activity::native_activity {
bool snapshot_clock(Owner owner,activity_clock::Publication& output) noexcept {
    ++snapshotCalls;if(invalidateSnapshot==snapshotCalls)++clockValue.domain.epoch;
    output={};if(!haveClock || owner!=creator)return false;output=clockValue;return true;
}
}
namespace dawn::server::bap {
bool acquire_region_lineage_locked(const Session&,const RegionLineage& edge,
    gameplay::group::HostActivityLineageLease& lease) noexcept {
    if(!acquireOk || edge.source!=creator || edge.bound!=recipient)return false;
    lease={71,recipient,creator,120,19,3,true};return true;
}
bool retained_region_lineage_is_current_locked(const Session& value,const RegionLineage& edge,
    const gameplay::group::HostActivityLineageLease& lease) noexcept {
    return edgeCurrent && value.activity.instance==recipient && value.activity.lineage==edge
        && lease.pinned && lease.rowGeneration==19 && lease.rowSlot==3;
}
}
namespace dawn::server::gameplay::group {
void release_host_activity_lineage(HostActivityLineageLease& lease) noexcept {
    if(lease.pinned)++releaseCount;lease={};
}
}
namespace dawn::middleware::secure_channel {
void advance_nonce(std::span<std::byte,state::kBapNonceSize> nonce) noexcept {
    for(auto& byte:nonce){byte=static_cast<std::byte>(std::to_integer<unsigned>(byte)+1);if(byte!=std::byte{})break;}
}
}
namespace dawn::server::bap::encrypted::push::activity {
bool append_notification_frame(Scratch&,std::uint64_t target,std::uint32_t type,
    std::span<const std::byte> body,std::span<const std::byte,state::kAesKeySize>,
    std::span<const std::byte,state::kBapNonceSize>,std::span<std::byte> response,std::size_t& written) noexcept {
    calls.push_back({target,type,{body.begin(),body.end()}});
    if(invalidateCall==calls.size())edgeCurrent=false;
    if(failureCall==calls.size() || written>response.size() || body.size()>response.size()-written)return false;
    std::copy(body.begin(),body.end(),response.begin()+written);written+=body.size();return true;
}
}
void wire_tests(const char* output) {
    const std::array<std::uint64_t,7> values{0,1,673200,11444523,0x0102030405060708ULL,UINT64_MAX-1,UINT64_MAX};
    for(const auto ticks:values)for(std::size_t capacity=0;capacity<=wire::kSynchronizationBytes;++capacity) {
        std::array<std::byte,wire::kSynchronizationBytes> buffer{};
        bits::Writer writer(std::span(buffer).first(capacity));
        const auto ok=wire::write_synchronization(writer,session.activityPatchEpoch,ticks);
        CHECK(ok==(capacity==buffer.size() && ticks!=UINT64_MAX));
        if(!ok)continue;
        CHECK(writer.bit_count()==205);bits::Reader reader(buffer);std::uint64_t field{};
        CHECK(reader.read(8,field)&&field==0);
        CHECK(reader.read(64,field)&&field==session.activityPatchEpoch.first);
        CHECK(reader.read(64,field)&&field==session.activityPatchEpoch.second);
        CHECK(reader.read(1,field)&&field==0);
        for(unsigned shift=0;shift<64;shift+=8)CHECK(reader.read(8,field)&&field==((ticks>>shift)&255));
        CHECK(reader.read(4,field)&&field==0);
        CHECK(reader.read(3,field)&&field==0);
    }
    if(output) {
        std::array<std::byte,26> buffer{};bits::Writer writer(buffer);
        CHECK(wire::write_synchronization(writer,session.activityPatchEpoch,11444523));
        std::ofstream stream(output,std::ios::binary);stream.write(reinterpret_cast<const char*>(buffer.data()),buffer.size());CHECK(stream.good());
    }
}
void publication_tests() {
    std::array<std::byte,dawn::state::kAesKeySize> key{};
    for(unsigned mode=0;mode<24;++mode) {
        reset();std::array<std::byte,64> buffer{};buffer.fill(std::byte{0xAC});std::size_t written=3;
        std::array<std::byte,dawn::state::kBapNonceSize> nonce{};nonce[0]=std::byte{7};const auto initialNonce=nonce;
        bool expected=true,present=true;
        switch(mode) {
        case 1:session.activity.joinedForeignSession=false;present=false;break;
        case 2:session.activityPatchEpochSeen=false;present=false;break;
        case 3:session.activity.lineage.kind=bap::RegionLineageKind::ownedActivity;present=false;break;
        case 4:haveClock=false;present=false;break;
        case 5:acquireOk=false;expected=false;break;
        case 6:++session.activity.lineage.source.incarnation.value;expected=false;break;
        case 7:++session.activity.lineage.bound.incarnation.value;expected=false;break;
        case 8:session.activity.lineage.source=session.activity.lineage.bound;expected=false;break;
        case 9:++clockValue.domain.owner.incarnation.value;expected=false;break;
        case 10:edgeCurrent=false;expected=false;break;
        case 11:failureCall=1;expected=false;break;
        case 12:failureCall=2;expected=false;break;
        case 13:invalidateCall=1;expected=false;break;
        case 14:invalidateCall=2;expected=false;break;
        case 15:invalidateSnapshot=3;expected=false;break;
        case 16:clockValue.elapsedTicks=UINT64_MAX;expected=false;break;
        case 17:clockValue.configuration.timing=std::numeric_limits<float>::infinity();expected=false;break;
        case 18:clockValue.configuration.timing=-1;expected=false;break;
        case 19:session.activity.lineage.kind=bap::RegionLineageKind::none;present=false;break;
        case 20:++session.activity.instance.incarnation.value;expected=false;break;
        case 21:++session.activity.lineage.source.sessionId;expected=false;break;
        case 22:session.activity.lineage.bound={};expected=false;break;
        case 23:clockValue.domain={};expected=false;break;
        }
        push::BorrowedClockPublication publication{};
        CHECK(push::append_borrowed_clock_notifications(session,scratch,key,nonce,buffer,written,publication)==expected);
        if(expected && present) {
            CHECK(publication.present && publication.lease.pinned);CHECK(publication.domain==clockValue.domain);
            CHECK(calls.size()==2 && calls[0].type==2 && calls[1].type==5);
            CHECK(calls[0].target==recipient.sessionId && calls[1].target==recipient.sessionId);
            CHECK(calls[0].bytes.size()==5 && calls[1].bytes.size()==26);
            CHECK(written==34 && nonce[0]==std::byte{9});
            CHECK(push::borrowed_clock_publication_is_current(session,publication));
            ++clockValue.domain.epoch;CHECK(!push::borrowed_clock_publication_is_current(session,publication));
        } else {
            CHECK(written==3 && nonce==initialNonce);CHECK(!publication.present && !publication.lease.pinned);
            if(expected)CHECK(calls.empty());
            if(calls.size()==2)for(std::size_t i=3;i<8;++i)CHECK(buffer[i]==std::byte{});
        }
        CHECK(buffer[0]==std::byte{0xAC} && buffer[2]==std::byte{0xAC});
        push::release_borrowed_clock_publication(publication);CHECK(!publication.present && !publication.lease.pinned);
    }
    for(std::size_t cap=0;cap<35;++cap) {
        reset();std::array<std::byte,40> bytes{};std::size_t count=3;
        std::array<std::byte,dawn::state::kBapNonceSize> nonce{};push::BorrowedClockPublication publication{};
        const bool ok=push::append_borrowed_clock_notifications(session,scratch,key,nonce,std::span(bytes).first(cap),count,publication);
        CHECK(ok==(cap>=34));if(!ok)CHECK(count==3 && nonce[0]==std::byte{});
        push::release_borrowed_clock_publication(publication);
    }
}
int main(int argc,char** argv) {
    reset();wire_tests(argc>1?argv[1]:nullptr);publication_tests();
    std::printf("activity_clock_push: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
