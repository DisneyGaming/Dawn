#pragma once
#include "client/hooks/bootflow/beyond_infinity_forest_recipe.h"
#include "state/activity/beyond_infinity/forest_selection.h"
#include "middleware/encoding/bit_writer.h"
#include <array>
#include <span>
namespace beyond_forest_fixture {
template<class Check> void run(Check check) {
    namespace recipe=dawn::client::hooks::bootflow::beyond_forest;
    namespace race=dawn::state::activity::beyond_infinity::forest;
    // Captured from the admitted native sensor, PID13124, before live repair.
    // The old adapter compared this runtime header against lookup class4EF6.
    constexpr std::array<unsigned char,16> capturedSensor{
        0xFA,0x60,0xF4,0x80,0xF7,0x4E,0x80,0x80,0x68,0x0D,0,0,0,0,0,0};
    const auto header=std::as_bytes(std::span{capturedSensor});
    check(recipe::sensor_identity(recipe::read<std::uint32_t>(header,0),recipe::read<std::uint32_t>(header,4),
        recipe::read<std::int64_t>(header,8)),"captured live Forest sensor runtime header is admitted");
    check(!recipe::sensor_identity(0x80F460FAU,0x80804EF6U,0xD68),"definition-class reference is not a runtime sensor header");
    check(!recipe::sensor_identity(0x80F460FAU,0x80804EF7U,0xD98),"scope field cannot impersonate sensor definition offset");
    check(!recipe::sensor_identity(0x80F460F9U,0x80804EF7U,0xD68),"foreign generator is rejected");
    std::array<std::byte,0x9D0> worker{};worker.fill(std::byte{0xCD});
    const std::uint32_t kind=0x80804FEC,config=0x80F4D0F1;
    std::memcpy(worker.data()+4,&kind,4);std::memcpy(worker.data()+0x96C,&config,4);
    auto original=worker;
    check(recipe::matches(worker),"exact Forest A worker admitted");
    const auto firstSeed=recipe::seed(17,5,1),secondSeed=recipe::seed(17,5,2);
    check(firstSeed && firstSeed<=0x7FFFFFFF && secondSeed && firstSeed!=secondSeed,"fresh layout per Forest pass");
    check(firstSeed==recipe::seed(17,5,1) && firstSeed!=recipe::seed(17,6,1),"stable layout within owner and fresh retry generation");
    check(recipe::prepare(worker,firstSeed,1),"first pass recipe");
    constexpr std::array<std::size_t,4> column{0x970,0x979,0x981,0x989},weight{0x974,0x97C,0x984,0x98C},start{0x978,0x980,0x988,0x990};
    constexpr std::array<int,4> first{-1,0,1,-1},second{0,0,-1,-1},height{0,0,1,0};
    for(std::size_t i=0;i<4;++i) {
        check(recipe::read<std::int8_t>(worker,column[i])==first[i],"first route uses A north and past exits");
        check(recipe::read<std::int8_t>(worker,column[i]+1)==height[i],"mission corridor heights");
        check(recipe::read<float>(worker,weight[i])==(i==1?1.F:0.F),"first route goal is the past");
        check(recipe::read<std::uint8_t>(worker,start[i])==(i==2?1:0),"native entry activation only");
    }
    const auto once=worker;
    check(recipe::prepare(worker,firstSeed,1) && worker==once,"same pass does not change native generation inputs");
    check(recipe::prepare(worker,secondSeed,2),"second pass regenerates");
    for(std::size_t i=0;i<4;++i) {
        check(recipe::read<std::int8_t>(worker,column[i])==second[i],"second route links past return and future");
        check(recipe::read<float>(worker,weight[i])==(i==0?1.F:0.F),"second route goal is the future");
        check(recipe::read<std::uint8_t>(worker,start[i])==(i==1?1:0),"past return is the second entry");
    }
    for(std::size_t i=0;i<worker.size();++i) {
        const bool owned=(i>=0x948 && i<0x950) || (i>=0x970 && i<0x994);
        if(!owned) { check(worker[i]==original[i],"effective native state, encounters and gates untouched"); }
    }
    // Independent native geometry: owner80F4D0F2 at y1004.318298; each
    // coarse anchor row contains nine 15m cells. The fixed corridor Y bands
    // are captured from80F460EE, not inferred from this recipe's arrays.
    const auto corridor_row=[&](std::size_t edge) {
        return 1004.318298F+(static_cast<float>(recipe::read<std::int8_t>(worker,column[edge]))+0.5F)*135.F;
    };
    check(corridor_row(1)>=1045.97827F && corridor_row(1)<=1097.47827F,
        "past return endpoint aligns with native tv_begin_past");
    check(recipe::read<std::int8_t>(worker,column[0])>=0 &&
        corridor_row(0)>=1053.53967F && corridor_row(0)<=1090.24695F,
        "future endpoint meets the authored east corridor row");
    check(recipe::read<std::int8_t>(worker,column[0]+1)==recipe::read<std::int8_t>(worker,column[1]+1),
        "both fixed corridors retain their matching native elevation");
    const auto secondOnce=worker;
    check(!recipe::prepare(worker,0,1) && !recipe::prepare(worker,1,0) && !recipe::prepare(worker,1,3),"invalid generation requests rejected");
    check(!recipe::prepare(std::span(worker).first(0x990),1,1) && worker==secondOnce,"short workers never written");
    const std::uint32_t foreign=0x80F4E6E7;std::memcpy(worker.data()+0x96C,&foreign,4);
    check(!recipe::matches(worker) && !recipe::prepare(worker,1,1),"Omega Forest D excluded");
    std::array<std::byte,0x60> record{};record.fill(std::byte{0xCD});const auto before=record;
    check(recipe::enable(record,false) && record[0x2D]==std::byte{0},"hold forest absent until Lua release");
    check(recipe::enable(record,true) && record[0x2D]==std::byte{1},"native enable command");
    for(std::size_t i=0;i<record.size();++i) { if(i!=0x2C && i!=0x2D) { check(record[i]==before[i],"authority result and progress fields untouched"); } }
    for(std::uint8_t pass=0;pass<=2;++pass) {
        auto writer=dawn::middleware::encoding::bits::Writer::measuring();
        check(race::write(writer,pass) && writer.bit_count()==194,"two typed race switches have exact wire width");
        check(race::value(pass,race::kVex)==(pass==2?race::kInactive:race::kActive),"Vex first generation");
        check(race::value(pass,race::kFallen)==(pass==2?race::kActive:race::kInactive),"Fallen second generation");
    }
}
}
