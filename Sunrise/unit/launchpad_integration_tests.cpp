#include <array>
#include <algorithm>
#include "../src/server/bap/encrypted/push/activity/launchpad_roster.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "../src/state/activity/Newlight/launchpad/controller.h"
#include "../src/state/activity/Newlight/launchpad/authority.h"
#include "../src/state/activity/Newlight/launchpad/quest.h"
#include "../src/middleware/datagen/family4/character/layout.h"
#include "../src/middleware/encoding/bit_writer.h"

namespace lp=sunrise::state::activity::newlight::launchpad;
namespace coo=sunrise::state::activity::coo;
namespace character=sunrise::middleware::datagen::family4::character;
static unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}} while(false)

void verify_packet(const lp::Frame& frame) {
    namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;
    namespace roster=sunrise::server::bap::encrypted::push::activity::launchpad_roster;
    namespace layout=sunrise::state::build_data::scenarios;
    struct Storage {
        std::array<layout::RosterGroup,wire::kGroupCapacity> rosterGroups{};
        std::array<wire::BubbleSubBlock,4> rosterSubBlocks{};
        std::array<std::array<std::uint32_t,wire::kBubbleKeyCapacity>,4> rosterSubBlockKeys{};
    };
    auto storage=std::make_unique<Storage>();
    auto snapshot=std::make_unique<wire::Snapshot>();
    layout::Definition definition{};definition.tag=lp::kScenario;definition.bubbleCount=4;
    std::copy(lp::kPackage.begin(),lp::kPackage.end(),definition.name.begin());
    definition.nameLength=static_cast<std::uint8_t>(lp::kPackage.size());
    snapshot->lifetime=3;snapshot->launchpad=frame;snapshot->phaseOneOnly=false;
    snapshot->hasRegion=true;snapshot->region=frame.bubble;snapshot->gameplayClockTicks=frame.gameplayClockTicks;
    snapshot->roster.playerKeyGroup=0x4786C0E0U;
    std::uint32_t failed{};
    CHECK(roster::admit(definition,*storage,snapshot->roster,[](std::size_t i,layout::RosterGroup& group) {
        for(const auto& expected:lp::kGroups) if(expected.hint==i) {roster::recovered(group,expected);return true;}
        return false;
    },failed));
    roster::movies(*storage,snapshot->roster,frame.cinematic);
    std::array<std::byte,65536> packet{};std::size_t written{};
    CHECK(wire::encode_sensor_auth_update(*snapshot,packet,written));
    CHECK(written>0 && written<packet.size());
}

void verify_bodies(const lp::Frame& frame) {
    verify_packet(frame);
    for(const auto& binding:lp::kAssets) {
        const auto a=binding.asset;
        CHECK(a.type<=UINT8_MAX);
        const auto type=static_cast<std::uint8_t>(a.type);
        const auto bits=lp::body_bits(frame,a.registry,type,a.slot);
        if(!bits) continue;
        std::vector<std::byte> bytes((bits+7)/8);
        sunrise::middleware::encoding::bits::Writer writer(bytes);
        CHECK(lp::write_body(writer,frame,a.registry,type,a.slot));
        if(writer.bit_count()!=bits) std::fprintf(stderr,"body %08X/%u/%u expected=%zu got=%zu\n",a.registry,a.type,a.slot,bits,writer.bit_count());
        CHECK(writer.bit_count()==bits);
    }
}

int main() {
    auto controller=std::make_unique<lp::Controller>();
    CHECK(!controller->select(0));
    CHECK(controller->select(71,100));
    const auto owner=controller->owner();
    CHECK(owner.valid());
    CHECK(controller->frame().enabled);
    CHECK(controller->frame().cinematic.phase==lp::cinematics::Phase::preparing);
    CHECK(controller->frame().cinematic.route()==0);
    CHECK(controller->advance(71,100000,true)); // Loading alone cannot time out the unarmed movie.
    CHECK(!controller->frame().fault);
    verify_bodies(controller->frame());
    CHECK(!controller->fly_in_complete({},100010));
    CHECK(controller->fly_in_complete(owner,100010));
    CHECK(controller->frame().cinematic.route()==1);
    CHECK(controller->frame().cinematic.masking_opening(100011));
    CHECK(!controller->arrival(owner,4,100012));
    CHECK(controller->arrival(owner,1,100020));
    CHECK(controller->frame().cinematic.phase==lp::cinematics::Phase::offered);
    CHECK(controller->frame().cinematic.play);
    verify_bodies(controller->frame());
    const auto movie=lp::cinematics::kMovies[0];
    CHECK(!controller->cinematic(owner,{5239,0,1,6,0},100030));
    CHECK(controller->cinematic(owner,{5239,movie.registry,1,6,0},100030));
    CHECK(controller->frame().cinematic.phase==lp::cinematics::Phase::playing);
    CHECK(!controller->frame().cinematic.masking_opening(100031));
    CHECK(controller->cinematic(owner,{3338,movie.registry,2,6,0},101000));
    CHECK(controller->cinematic(owner,{1685,movie.registry,3,6,0},101010));
    CHECK(controller->frame().cinematic.phase==lp::cinematics::Phase::landing);
    CHECK(controller->frame().cinematic.route()==4);
    CHECK(!controller->arrival({},4,101020));
    CHECK(controller->arrival(owner,4,101020));
    CHECK(controller->advance(71,101030,true));
    CHECK(controller->frame().cinematic.phase==lp::cinematics::Phase::gameplay);
    CHECK(!controller->frame().fault);
    verify_bodies(controller->frame());

    // A consumed conversation retains its generation and clears its optional
    // timestamp. Reproduce the first post-cinematic publication and later rows.
    auto dialogueFrame=controller->frame();
    for(std::size_t row=0;row<dialogueFrame.generations.size();++row) {
        dialogueFrame.generations[row]=1;
        dialogueFrame.activeRow=static_cast<std::uint8_t>(row);
        verify_bodies(dialogueFrame);
        dialogueFrame.activeRow=coo::kNoDialogue;
        verify_bodies(dialogueFrame);
    }
    for(std::uint64_t now=101155;now<131030;now+=125) {
        CHECK(controller->advance(71,now,true));
        CHECK(!controller->frame().fault);
        verify_bodies(controller->frame());
    }

    // Native quest flags must start New Light and later release the forced start.
    // Existing vendor overrides and another Guardian's state must survive projection.
    sunrise::state::CharacterState guardian{};
    guardian.inventory.count=1;
    guardian.inventory.values[0].quantity=1;
    for(std::size_t step=0;step<lp::quest::kSteps.size();++step) {
        guardian.inventory.values[0].definitionHash=lp::quest::kSteps[step];
        auto object=std::make_unique<character::layout::Object>();
        object->unlockFlagCount=1;object->unlockFlags[0]={144,1,0};
        object->unlockValueCount=1;object->unlockValues[0]={900,0,456};
        lp::quest::project(guardian,*object);
        CHECK(object->acquiredFlags[20]==(step<5?std::byte{2}:std::byte{}));
        CHECK(object->acquiredFlags[59]==(step?std::byte{2}:std::byte{}));
        CHECK(object->unlockFlagCount==5 && object->unlockValueCount==7);
        CHECK(object->unlockFlags[0].slot==144 && object->unlockFlags[0].value==1);
        CHECK(object->unlockValues[0].slot==900 && object->unlockValues[0].value==456);
        lp::quest::project(guardian,*object);
        CHECK(object->unlockFlagCount==5 && object->unlockValueCount==7);
    }
    guardian.inventory.values[0].postmaster=true;
    std::array<std::byte,60> untouched{};untouched[20]=std::byte{1};
    lp::quest::project_start(guardian,untouched);CHECK(untouched[20]==std::byte{1});
    std::printf("Launchpad integration: %u checks passed\n",checks);
}
