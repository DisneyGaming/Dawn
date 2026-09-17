#include "server/runtime/activity/mercury_population_drain.h"
#include <cstdio>
#include <cstdlib>

namespace d=dawn::server::runtime::activity::mercury_population_drain;
namespace {
unsigned checks{};
void check(bool value,int line) {++checks;if(!value){std::printf("FAIL %d\n",line);std::exit(1);}}
#define CHECK(...) check((__VA_ARGS__),__LINE__)

d::Fence fence(std::uint32_t target=3) {
    d::Fence value;value.consumedKnown=true;value.sourcePendingKnown=true;
    value.mailboxQuiescent=true;value.target[0]=target;value.consumed[0]=target;return value;
}
void full_clear() {
    d::Controller value;CHECK(value.begin(7,{3,0},1));
    const d::Counts deadRetained{3,0,3,3,false};
    CHECK(value.update(1000,30000,deadRetained,fence())==d::Action::none);
    CHECK(value.phase()==d::Phase::cooling);
    CHECK(value.update(30999,30000,deadRetained,fence())==d::Action::none);
    CHECK(value.update(31000,30000,deadRetained,fence())==d::Action::projectDrain);
    CHECK(value.authoritative_generation()==7);
    const auto projection=value.projection();
    CHECK(projection.generation==8 && projection.requested[0]==0
        && projection.requested[1]==0 && projection.retireOwned);
    CHECK(value.sense(7)==d::Sense::current);
    CHECK(value.sense(8)==d::Sense::expectedDrain);
    CHECK(value.sense(9)==d::Sense::staleOrForeign);
    CHECK(value.update(31001,30000,{3,0,3,1,false},fence())==d::Action::none);
    CHECK(value.update(31002,30000,{3,0,3,0,false},fence())==d::Action::commit);
    CHECK(value.phase()==d::Phase::ready && value.authoritative_generation()==7);
    CHECK(value.committed(8));
    CHECK(value.phase()==d::Phase::idle && value.authoritative_generation()==8);
    CHECK(value.target()[0]==3);
}
void fences() {
    for(unsigned blocked=0;blocked<3;++blocked) {
        d::Controller value;CHECK(value.begin(1,{2,0},1));
        const d::Counts dead{2,0,2,2,false};auto guarded=fence(2);
        CHECK(value.update(1,30,dead,guarded)==d::Action::none);
        if(blocked==0)guarded.consumedKnown=false;
        if(blocked==1)guarded.sourcePending[0]=1;
        if(blocked==2)guarded.mailboxQuiescent=false;
        CHECK(value.update(31,30,dead,guarded)==d::Action::none);
        CHECK(value.phase()==d::Phase::cooling);
    }
    d::Controller partial;CHECK(partial.begin(1,{3,0},1));
    CHECK(partial.update(1,30,{3,2,1,3,false},fence())==d::Action::none);
    CHECK(partial.update(100,30,{3,2,1,3,false},fence())==d::Action::none);
    CHECK(partial.phase()==d::Phase::idle);
}
void fail_closed() {
    d::Controller birth;CHECK(birth.begin(4,{2,0},1));
    CHECK(birth.update(1,30,{2,0,2,2,false},fence(2))==d::Action::none);
    CHECK(birth.update(31,30,{2,0,2,2,false},fence(2))==d::Action::projectDrain);
    CHECK(birth.update(32,30,{3,1,2,3,false},fence(2))==d::Action::fail);
    CHECK(birth.phase()==d::Phase::failed);
    CHECK(birth.update(33,30,{3,0,3,0,false},fence(2))==d::Action::fail);

    d::Controller two;CHECK(two.begin(9,{2,2},2));auto both=fence(2);
    both.categories=2;both.target[1]=2;both.consumed[1]=2;
    CHECK(two.update(10,30,{4,0,4,4,false},both)==d::Action::none);
    CHECK(two.update(40,30,{4,0,4,4,false},both)==d::Action::projectDrain);
    both.sourcePending[1]=1;
    CHECK(two.update(41,30,{4,0,4,0,false},both)==d::Action::none);
    both.sourcePending[1]=0;
    CHECK(two.update(42,30,{4,0,4,0,false},both)==d::Action::commit);
    CHECK(!two.committed(9));CHECK(two.committed(10));
}
}
int main() {full_clear();fences();fail_closed();std::printf("PASS %u Mercury native-drain policy checks\n",checks);}
