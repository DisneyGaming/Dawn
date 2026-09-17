#include "server/runtime/activity/public_event_native_bridge.h"
#include <cstdio>
#include <cstdlib>

namespace pe=dawn::server::runtime::activity::public_event;
namespace use=pe::rally_use;
namespace bridge=pe::native_bridge;
unsigned checks{};
void check(bool ok,const char* text){++checks;if(!ok){std::printf("FAIL %s\n",text);std::exit(1);}}
using Bytes=std::array<std::byte,use::kBytes>;
template<class T>void put(Bytes& b,std::size_t offset,T value){std::memcpy(b.data()+offset,&value,sizeof value);}
int main(){
    pe::placement_feedback::Ticket ticket{{{99,{2}},7,1,3},{0x85C38F77,0x80F5BF33,4,0},{3,1,0,0},15,15,{0x4C8,1,true,0x815ABCED,0x388}};
    bridge::Mailbox box;check(box.bind(ticket),"placement lease binds");
    check(!box.lookup_use(0x815ABCED,0x10203040).epoch,"placement request is not rally completion");
    auto placed=box.lookup_definition(ticket.asset.definition);
    check(box.submit({placed,{{ticket.lease,pe::Stage::rally,ticket.asset,{ticket.token,pe::coo::Milestone::nativeReady}},
        {0x11223344,0x600},0x10203040,0,15,1}}),"qualified placement receipt");
    auto binding=box.lookup_use(0x815ABCED,0x10203040);
    check(binding.epoch && !box.used(ticket.lease),"placed flag waits for actual use");
    check(!box.lookup_use(0x815ABCDE,0x10203040).epoch,"wrong authored interaction rejected");
    check(!box.lookup_use(0x815ABCED,0x20203040).epoch,"different salted entity rejected");
    Bytes before{};put(before,0,0x815ABCEDU);put(before,4,0x80804FB2U);put(before,8,std::int64_t{0x388});
    put(before,0x24,0x30123456U);put(before,0x2C,binding.entity);put(before,0x2DC,std::int32_t{1});
    put(before,0x2E0,0x1020ULL);Bytes after=before;after[0x2D0]=std::byte{1};put(after,0x2D8,std::int32_t{1});
    use::Receipt receipt{};
    auto qualifies=[&](const Bytes& a,const Bytes& b){return use::qualify(binding,a,b,0x40012345,0x50012345,use::kProducerRva,receipt);};
    check(qualifies(before,after),"native consumed request and active state qualify");
    check(!qualifies(before,before),"unchanged prompt cannot start event");
    check(!qualifies(after,after),"already used state cannot replay completion");
    for(const auto offset:std::array<std::size_t,9>{0,4,8,0x24,0x2C,0x2D0,0x2D8,0x2DC,0x2E0}){
        auto changed=after;changed[offset]^=std::byte{4};check(!qualifies(before,changed),"mismatched post-call identity/counters rejected");
    }
    auto blocked=before;blocked[0x2C0]=std::byte{1};check(!qualifies(blocked,after),"blocked interaction rejected");
    check(!use::qualify(binding,before,after,UINT32_MAX,0x50012345,use::kProducerRva,receipt),"unresolved requester rejected");
    check(!use::qualify(binding,before,after,0x40012345,UINT32_MAX,use::kProducerRva,receipt),"unresolved player rejected");
    check(!use::qualify(binding,before,after,1,2,0xF32CD0,receipt),"prompt callback not completion producer");
    check(qualifies(before,after),"valid receipt restored");
    auto wrong=receipt;wrong.binding.ticket.lease.boot++;check(!box.submit_use(wrong),"wrong boot rejected");
    wrong=receipt;wrong.binding.ticket.lease.owner.incarnation.value++;check(!box.submit_use(wrong),"wrong incarnation rejected");
    wrong=receipt;wrong.binding.epoch++;check(!box.submit_use(wrong),"wrong binding epoch rejected");
    wrong=receipt;wrong.binding.entity++;check(!box.submit_use(wrong),"wrong placed entity rejected");
    check(box.submit_use(receipt) && box.used(ticket.lease),"qualified native use latches once");
    check(!box.submit_use(receipt) && !box.lookup_use(0x815ABCED,binding.entity).epoch,"duplicate use cannot restart");
    box.release(ticket.lease.owner);check(!box.used(ticket.lease) && !box.submit_use(receipt),"owner retirement cancels in-flight old receipt");
    check(box.bind(ticket) && !box.used(ticket.lease),"new binding inherits no used state");
    check(!box.submit_use(receipt),"same asset rebind rejects old epoch");
    std::printf("%u rally use checks passed\n",checks);
}
