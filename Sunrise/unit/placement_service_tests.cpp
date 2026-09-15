#include "../src/server/runtime/activity/placement_service.h"
#include "../src/middleware/encoding/bit_writer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

namespace placement=sunrise::server::runtime::activity::placement;
namespace interaction=sunrise::middleware::bap::activity_message::native::interaction;
namespace registry=sunrise::state::activity::coo::registry;
namespace wire=sunrise::middleware::bap::activity_message::native::placement;
namespace bits=sunrise::middleware::encoding::bits;

unsigned checks{},failures{};
void check(bool value,const char* message) {
    ++checks;
    if(!value) { ++failures;std::cerr<<"FAIL "<<message<<'\n'; }
}

struct Encoded final {
    std::array<std::byte,64> bytes{};
    std::size_t size{},bits{};
    bool valid{};
};

Encoded encode(const wire::Request& request) {
    Encoded result{};
    bits::Writer writer(std::span(result.bytes));
    result.valid=wire::write(writer,request) && writer.finish(result.size);
    result.bits=writer.bit_count();
    return result;
}

bool same_bytes(const Encoded& left,const Encoded& right) {
    if(!left.valid || !right.valid || left.size!=right.size || left.bits!=right.bits) return false;
    for(std::size_t i=0;i<left.size;++i) if(left.bytes[i]!=right.bytes[i]) return false;
    return true;
}

bool bit_at(const Encoded& encoded,std::size_t bit) {
    const auto byte=std::to_integer<unsigned>(encoded.bytes[bit/8]);
    return (byte & (1U<<(7U-(bit%8U))))!=0;
}

int main() {
    const std::array<registry::Slot,1> chestSlots{{
        {3,4,0x80809927,0x8080992E,0x8080992F,0xA1000001}}};
    const std::array<registry::Slot,1> otherSlots{{
        {7,4,0x80809927,0x8080992E,0x8080992F,0xA1000002}}};
    const registry::Definition chest{
        "astra",0xA0000001,0xA0000002,0xA0000003,0xA0000004,5,chestSlots};
    const registry::Definition other{
        "astra",0xA0000011,0xA0000012,0xA0000013,0xA0000014,5,otherSlots};
    check(sunrise::server::runtime::activity::registry::valid(chest)
        && sunrise::server::runtime::activity::registry::valid(other),"synthetic registries are valid");

    const placement::Capability legacy{&chest,3,0};
    wire::Batch projected{};
    check(placement::project(std::span(&legacy,1),5,projected) && projected.count==1,
        "default capability projects one placement");
    if(projected.count==1) {
        const wire::Request expected{chest.key,3,chest.bubble};
        const auto actualBytes=encode(projected.entries[0]);
        const auto expectedBytes=encode(expected);
        check(projected.entries[0].interactionMode==interaction::Mode::unchanged,
            "legacy aggregate initializer keeps unchanged interaction mode");
        check(wire::body_bits(projected.entries[0])==wire::kActiveBits
            && actualBytes.bits==wire::kActiveBits,"unchanged body keeps existing bit width");
        check(same_bytes(actualBytes,expectedBytes),"unchanged body bytes match existing codec output");
    }

    for(const auto mode:{interaction::Mode::enabled,interaction::Mode::disabled}) {
        const placement::Capability capability{&chest,3,17,mode};
        projected={};
        check(placement::project(std::span(&capability,1),5,projected) && projected.count==1,
            "explicit interaction mode projects one placement");
        if(projected.count!=1) continue;
        const auto& request=projected.entries[0];
        const wire::Request expected{chest.key,3,chest.bubble,mode,17};
        const auto actualBytes=encode(request);
        const auto expectedBytes=encode(expected);
        check(request.generation==17 && request.interactionMode==mode,
            "explicit mode preserves native source generation");
        check(wire::body_bits(request)==wire::kActiveBits+interaction::kPayloadBits
            && actualBytes.bits==wire::kActiveBits+interaction::kPayloadBits,
            "explicit mode uses the native positive-generation record width");
        check(!bit_at(actualBytes,65),"explicit mode retains native authored transform");
        check(same_bytes(actualBytes,expectedBytes),
            "explicit mode bytes match the native authored-transform codec");
    }

    const placement::Capability otherLegacy{&other,7,23};
    const placement::Capability enabled{&chest,3,17,interaction::Mode::enabled};
    const std::array<placement::Capability,2> bindings{{enabled,otherLegacy}};
    projected={};
    check(placement::project(bindings,5,projected) && projected.count==2,
        "enabled binding and unrelated binding project together");
    if(projected.count==2) {
        const wire::Request expected{other.key,7,other.bubble,interaction::Mode::unchanged,23};
        check(projected.entries[1].registry==other.key && projected.entries[1].slot==7
            && projected.entries[1].generation==23
            && projected.entries[1].interactionMode==interaction::Mode::unchanged,
            "other binding fields remain unchanged");
        check(same_bytes(encode(projected.entries[1]),encode(expected)),
            "other binding bytes remain unchanged");
    }

    const auto preserved=projected;
    auto invalid=legacy;
    invalid.interactionMode=static_cast<interaction::Mode>(0xFF);
    projected.entries[0].registry=0xDEADBEEF;projected.count=1;
    const std::array<placement::Capability,2> invalidBatch{{legacy,invalid}};
    check(!placement::project(invalidBatch,5,projected) && projected.count==0,
        "invalid mode fails without partially projecting the batch");
    check(preserved.count==2,"invalid mode does not mutate prior caller-owned batch copy");

    auto wrongSlots=chestSlots;
    wrongSlots[0].authSchema=0xDEADBEEF;
    auto wrongSchema=chest;wrongSchema.slots=wrongSlots;
    const placement::Capability invalidSchema{&wrongSchema,3,0};
    projected.entries[0].registry=0xDEADBEEF;projected.count=1;
    check(!placement::project(std::span(&invalidSchema,1),5,projected) && projected.count==0,
        "capability with invalid native schema fails atomically");

    std::cout<<"PASS "<<checks<<" placement service and native codec checks\n";
    return failures?1:0;
}
