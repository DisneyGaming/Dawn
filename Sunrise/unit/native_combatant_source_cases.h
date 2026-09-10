#pragma once
#include "middleware/bap/activity_message/native/combatant_source.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"

void native_combatant_source_cases() {
    namespace source=sunrise::middleware::bap::activity_message::native::combatant_source;
    namespace bits=sunrise::middleware::encoding::bits;
    std::array<std::byte,96> body{};
    source::Source vendor{0x564C6ECEU,1,0,1,{}};
    vendor.hasSpawnRule=false;
    bits::Writer writer(body);CHECK(source::write_source(writer,vendor));CHECK(writer.bit_count()==641);
    bits::Reader reader(body);std::uint64_t value{};
    // Reflected 80807EC9 spawn-rule reference: presence, registry, biased type,
    // biased slot. Absence uses the native sentinel, not a dummy device 0.
    CHECK(reader.skip(382));CHECK(reader.read(1,value) && value==1);
    CHECK(reader.read(32,value) && value==0x811C9DC5U);
    CHECK(reader.read(7,value) && value==0);CHECK(reader.read(16,value) && value==32767);
    auto malformed=vendor;malformed.ruleSlot=1;
    bits::Writer invalid(body);CHECK(!source::write_source(invalid,malformed));CHECK(invalid.bit_count()==0);
    source::Source squad{0x74337EDDU,1,9,2,{0x74337EDDU,2,0}};
    bits::Writer native(body);CHECK(source::write_source(native,squad));CHECK(native.bit_count()==641);
    bits::Reader assigned(body);CHECK(assigned.skip(382));CHECK(assigned.read(1,value) && value==1);
    CHECK(assigned.read(32,value) && value==squad.registry);CHECK(assigned.read(7,value) && value==67);
    CHECK(assigned.read(16,value) && value==32768+9);
    squad.hasSecondCategory=true;squad.secondRequested=1;
    bits::Writer two(body);CHECK(source::write_source(two,squad));CHECK(two.bit_count()==673);
    squad.looseRequested=63;bits::Writer over(body);CHECK(!source::write_source(over,squad));CHECK(over.bit_count()==0);
    std::array<std::byte,8> small{};bits::Writer shortBuffer(small);
    CHECK(!source::write_source(shortBuffer,vendor));
}
