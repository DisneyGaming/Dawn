#include "../src/server/bap/encrypted/push/activity/native_roster_lifetime.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace life=sunrise::server::bap::encrypted::push::activity::roster_lifetime;
namespace {
unsigned checks{};
void check(bool value,int line) {++checks;if(!value){std::printf("FAIL line %d\n",line);std::exit(1);}}
#define CHECK(value) check((value),__LINE__)
constexpr life::Identity id{0x123456789U,1,0x1122,0x3344,0x80f4696a};
constexpr std::array<std::uint32_t,2> topKeys{0x564c6ece,0x2571c34d};
constexpr std::array<std::uint8_t,2> present{1,1},states{0x87,0x89};
constexpr std::array<std::uint32_t,1> bubble15{0x4a3e4900},bubble16{0x11223344};
constexpr std::array<std::uint8_t,1> one{1},zero{0},state{0x85};

life::State initial() {
    life::State out{};
    const std::array blocks{life::WireBlock{15,{bubble15,one,state}}};
    CHECK(life::seed(id,{topKeys,present,states},blocks,out)==life::Result::ready);
    return out;
}
void unchanged_on_failure(const life::State& prior,life::Request request,life::Result expected) {
    auto out=prior;
    std::array<std::byte,sizeof(life::State)> before{};
    std::memcpy(before.data(),&out,sizeof out);
    CHECK(life::plan(prior,request,out)==expected);
    CHECK(std::memcmp(before.data(),&out,sizeof out)==0);
}
void normal() {
    auto prior=initial();life::State out{};
    auto allOnes=id;allOnes.epochLo=UINT64_MAX;allOnes.epochHi=UINT64_MAX;
    CHECK(life::seed(allOnes,{topKeys,present,states},{},out)==life::Result::ready);
    life::Request request{id,15,0xfe,{}, {}};
    CHECK(life::plan(prior,request,out)==life::Result::ready);
    CHECK(out.top.count==2 && out.top.states[0]==0x87 && out.top.states[1]==0x89);
    CHECK(out.blockCount==1 && out.blocks[0].entries.states[0]==0x85);
    // Reversed desired order never shifts prior wire ordinals or replaces states.
    const std::array<std::uint32_t,3> reverse{topKeys[1],0x44556677,topKeys[0]};
    request.top={reverse,{}};
    CHECK(life::plan(prior,request,out)==life::Result::ready);
    CHECK(out.top.count==3 && out.top.keys[0]==topKeys[0] && out.top.keys[1]==topKeys[1]);
    CHECK(out.top.keys[2]==reverse[1] && out.top.states[2]==0xfe);
    CHECK(out.top.states[0]==0x87 && out.top.states[1]==0x89);
    // Tombstone retains ordinal and state; a later explicit presence edge re-adds.
    const std::array first{topKeys[0]};request.top={first,zero};
    CHECK(life::plan(out,request,out)==life::Result::ready);
    CHECK(out.top.count==3 && out.top.keys[0]==first[0] && out.top.presence[0]==0);
    request.top={first,one};CHECK(life::plan(out,request,out)==life::Result::ready);
    CHECK(out.top.presence[0]==1 && out.top.states[0]==0x87);
    const std::array desiredBlocks{life::DesiredBlock{16,{bubble16,{}}},life::DesiredBlock{15,{bubble15,{}}}};
    request.top={};request.blocks=desiredBlocks;request.currentBubble=16;
    CHECK(life::plan(prior,request,out)==life::Result::ready);
    CHECK(out.blockCount==2 && out.blocks[0].bubble==15 && out.blocks[1].bubble==16);
    CHECK(out.blocks[0].entries.keys[0]==bubble15[0] && out.blocks[0].entries.states[0]==0x85);
    CHECK(out.blocks[1].entries.keys[0]==bubble16[0] && out.blocks[1].entries.states[0]==0xfe);
    CHECK(life::view(out.top).states.size()==out.top.count);
}
void rejected() {
    const auto prior=initial();life::Request request{id,15,0x80,{}, {}};
    request.identity.owner++;unchanged_on_failure(prior,request,life::Result::identityMismatch);
    request.identity=id;request.identity.epochLo++;unchanged_on_failure(prior,request,life::Result::identityMismatch);
    request.identity=id;request.currentBubble=64;unchanged_on_failure(prior,request,life::Result::invalidBubble);
    request.currentBubble=15;request.initialStateByte=0x7f;unchanged_on_failure(prior,request,life::Result::invalidState);
    request.initialStateByte=0x80;
    const std::array duplicate{topKeys[0],topKeys[0]};request.top={duplicate,{}};
    unchanged_on_failure(prior,request,life::Result::duplicateKey);
    request.top={topKeys,one};unchanged_on_failure(prior,request,life::Result::invalidView);
    request.top={bubble15,{}};unchanged_on_failure(prior,request,life::Result::keyScopeChanged);
    request.top={bubble16,zero};unchanged_on_failure(prior,request,life::Result::unknownRemoval);
    request.top={};const std::array dormantRemoveUnknown{life::DesiredBlock{16,{bubble16,zero}}};
    request.blocks=dormantRemoveUnknown;unchanged_on_failure(prior,request,life::Result::unknownRemoval);
    const std::array dormantRemove{life::DesiredBlock{15,{bubble15,zero}}};
    request.currentBubble=16;request.blocks=dormantRemove;
    unchanged_on_failure(prior,request,life::Result::dormantChange);
    const std::array moved{life::DesiredBlock{16,{topKeys,{}}}};request.blocks=moved;
    unchanged_on_failure(prior,request,life::Result::keyScopeChanged);
    const std::array duplicateBlocks{life::DesiredBlock{15,{}},life::DesiredBlock{15,{}}};
    request.blocks=duplicateBlocks;unchanged_on_failure(prior,request,life::Result::duplicateBubble);
    auto corrupt=prior;corrupt.blockCount=65;request.blocks={};
    unchanged_on_failure(corrupt,request,life::Result::invalidPrior);
    corrupt=prior;corrupt.blocks[0].entries.count=97;
    unchanged_on_failure(corrupt,request,life::Result::invalidPrior);
}
void limits() {
    std::array<std::uint32_t,life::kTopCapacity> keys{};
    std::array<std::uint8_t,life::kTopCapacity> presence{},fullStates{};
    for(std::size_t i=0;i<keys.size();++i){keys[i]=static_cast<std::uint32_t>(i+1);presence[i]=1;fullStates[i]=0x80;}
    life::State full{},out{};
    CHECK(life::seed(id,{keys,presence,fullStates},{},full)==life::Result::ready);
    const std::array extra{0xfedcba98U};life::Request request{id,15,0xff,{extra,{}},{}};
    unchanged_on_failure(full,request,life::Result::capacity);
    std::array<std::uint32_t,life::kBlockKeyCapacity> blockKeys{};
    std::array<std::uint8_t,life::kBlockKeyCapacity> bp{},bs{};
    for(std::size_t i=0;i<blockKeys.size();++i){blockKeys[i]=static_cast<std::uint32_t>(0x1000+i);bp[i]=1;bs[i]=0x81;}
    const std::array blocks{life::WireBlock{15,{blockKeys,bp,bs}}};
    CHECK(life::seed(id,{keys,presence,fullStates},blocks,full)==life::Result::ready);
    const std::array append{life::DesiredBlock{15,{extra,{}}}};request.top={};request.blocks=append;
    unchanged_on_failure(full,request,life::Result::capacity);
    const auto committed=full;
    // A rejected staged publication can retain the old immutable backing value.
    request.blocks={};CHECK(life::plan(full,request,out)==life::Result::ready);
    CHECK(committed.blocks[0].entries.count==96 && committed.top.count==256);
    const std::array<std::uint8_t,1> badState{0x40};
    CHECK(life::seed(id,{bubble15,one,badState},{},out)==life::Result::invalidState);
    for(std::size_t i=0;i<full.top.count;++i)CHECK(full.top.keys[i]==keys[i] && full.top.states[i]==0x80);
    for(std::size_t i=0;i<full.blocks[0].entries.count;++i)CHECK(full.blocks[0].entries.keys[i]==blockKeys[i]);
}
void mercury_acknowledgement() {
    // Exact t79000 r13 native sense ACK: one global and eight bubble15 rows,
    // all at0x83. The new bubble16 row below is an isolated test key.
    constexpr std::array<std::uint32_t,1> global{0x4786c0e0};
    constexpr std::array<std::uint32_t,8> keys{0x74337edd,0x564c6ece,0xf25b938b,0x2749baae,
        0xeb1e8934,0x85c38f77,0x4a3e4900,0x2571c34d};
    std::array<std::uint8_t,8> p{},s{};p.fill(1);s.fill(0x83);
    constexpr std::array<std::uint8_t,1> globalState{0x83};
    const std::array blocks{life::WireBlock{15,{keys,p,s}}};
    life::State committed{},candidate{};
    CHECK(life::seed(id,{global,one,globalState},blocks,committed)==life::Result::ready);
    const std::array next{life::DesiredBlock{16,{bubble16,{}}}};
    const life::Request request{id,16,0x84,{global,{}},next};
    CHECK(life::plan(committed,request,candidate)==life::Result::ready);
    CHECK(candidate.top.states[0]==0x83 && candidate.blockCount==2);
    CHECK(candidate.blocks[0].bubble==15 && candidate.blocks[0].entries.count==8);
    for(std::size_t i=0;i<keys.size();++i) {
        CHECK(candidate.blocks[0].entries.keys[i]==keys[i]);
        CHECK(candidate.blocks[0].entries.presence[i]==1);
        CHECK(candidate.blocks[0].entries.states[i]==0x83);
    }
    CHECK(candidate.blocks[1].bubble==16 && candidate.blocks[1].entries.states[0]==0x84);
    CHECK(committed.blockCount==1); // Discarding candidate leaves committed mirror intact.
}
void dormant_registration() {
    auto committed=initial();life::State staged{};
    const std::array forest{life::DesiredBlock{11,{bubble16,{}}}};
    life::Request request{id,15,0xfd,{},forest};
    CHECK(life::plan(committed,request,staged)==life::Result::ready);
    CHECK(committed.blockCount==1 && staged.blockCount==2);
    CHECK(staged.blocks[0].bubble==15 && staged.blocks[0].entries.keys[0]==bubble15[0]);
    CHECK(staged.blocks[0].entries.states[0]==0x85);
    CHECK(staged.blocks[1].bubble==11 && staged.blocks[1].entries.count==1);
    CHECK(staged.blocks[1].entries.keys[0]==bubble16[0]);
    CHECK(staged.blocks[1].entries.presence[0]==1 && staged.blocks[1].entries.states[0]==0xfd);
    // Advancing to the preregistered bubble preserves the exact wire byte. The
    // native entry callback, separately exercised against original code, creates.
    committed=staged;request.currentBubble=11;request.initialStateByte=0x80;
    CHECK(life::plan(committed,request,staged)==life::Result::ready);
    CHECK(staged.blockCount==2 && staged.blocks[1].entries.states[0]==0xfd);
    constexpr std::array<std::uint32_t,2> forestAppend{0x55667788,bubble16[0]};
    const std::array append{life::DesiredBlock{11,{forestAppend,{}}}};
    request.currentBubble=15;request.blocks=append;
    CHECK(life::plan(committed,request,staged)==life::Result::ready);
    CHECK(staged.blocks[1].entries.count==2 && staged.blocks[1].entries.keys[0]==bubble16[0]);
    CHECK(staged.blocks[1].entries.keys[1]==forestAppend[0]);
    CHECK(staged.blocks[1].entries.states[0]==0xfd && staged.blocks[1].entries.states[1]==0x80);
    const std::array remove{life::DesiredBlock{11,{bubble16,zero}}};
    request.blocks=remove;unchanged_on_failure(staged,request,life::Result::dormantChange);
    // Native unload does not consume arbitrary dormant tombstones. Reactivating
    // one could mask an unperformed removal, so that edge remains rejected too.
    staged.blocks[1].entries.presence[0]=0;request.blocks=forest;
    unchanged_on_failure(staged,request,life::Result::dormantChange);
}
} // namespace

int main(){normal();rejected();limits();mercury_acknowledgement();dormant_registration();std::printf("PASS %u checks\n",checks);}
