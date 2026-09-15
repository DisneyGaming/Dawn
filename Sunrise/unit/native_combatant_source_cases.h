#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

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

    // Root 13 is the assignment evaluation revision. Decode it independently and
    // check the adjacent source fields stay at their legacy values.
    source::Source assignedSource=squad;
    assignedSource.tactical.revision=37;
    std::array<std::byte,96> legacyBytes{},assignedBytes{},vendorBytes{};
    bits::Writer legacyWriter(legacyBytes),assignedWriter(assignedBytes),vendorWriter(vendorBytes);
    CHECK(source::write_source(legacyWriter,squad));
    CHECK(source::write_source(assignedWriter,assignedSource));
    CHECK(source::write_source(vendorWriter,vendor));
    CHECK(legacyWriter.bit_count()==assignedWriter.bit_count() && assignedWriter.bit_count()==641);
    const auto field_at = [](std::span<const std::byte> bytes,std::size_t offset,unsigned width) {
        bits::Reader fieldReader(bytes);std::uint64_t fieldValue{};
        return fieldReader.skip(offset)
                   && fieldReader.read(static_cast<std::uint8_t>(width),fieldValue)
               ? fieldValue
               : UINT64_MAX;
    };
    CHECK(field_at(assignedBytes,494,1)==1 && field_at(assignedBytes,495,31)==37);
    CHECK(field_at(assignedBytes,121,32)==0x80000000U+squad.looseRequested); // requested count
    CHECK(field_at(assignedBytes,173,31)==squad.generation); // source generation
    CHECK(field_at(assignedBytes,205,32)==0 && field_at(assignedBytes,238,32)==0); // placement
    CHECK(field_at(assignedBytes,527,31)==0 && field_at(assignedBytes,572,31)==squad.generation); // fields 14/17
    CHECK(field_at(assignedBytes,603,2)==2 && field_at(assignedBytes,605,3)==1); // control flags
    for(std::size_t bit=0;bit<assignedWriter.bit_count();++bit) {
        if(bit>=495 && bit<526) { continue; }
        CHECK(field_at(legacyBytes,bit,1)==field_at(assignedBytes,bit,1));
    }
    auto staleAbsent=vendor;staleAbsent.tactical.revision=1;
    bits::Writer staleAbsentWriter(legacyBytes);CHECK(!source::write_source(staleAbsentWriter,staleAbsent));
    auto outOfRange=assignedSource;outOfRange.tactical.revision=0x80000000U;
    bits::Writer invalidRevisionWriter(assignedBytes);CHECK(!source::write_source(invalidRevisionWriter,outOfRange));
    auto staleRow=assignedSource;staleRow.tactical.row=-2;
    bits::Writer invalidRowWriter(assignedBytes);CHECK(!source::write_source(invalidRowWriter,staleRow));
    bits::Reader vendorRevision(vendorBytes);CHECK(vendorRevision.skip(494));
    CHECK(vendorRevision.read(1,value) && value==1 && vendorRevision.read(31,value) && value==0);
    squad.hasSecondCategory=true;squad.secondRequested=1;
    bits::Writer two(body);CHECK(source::write_source(two,squad));CHECK(two.bit_count()==673);
    squad.looseRequested=63;bits::Writer over(body);CHECK(!source::write_source(over,squad));CHECK(over.bit_count()==0);
    std::array<std::byte,8> small{};bits::Writer shortBuffer(small);
    CHECK(!source::write_source(shortBuffer,vendor));
}
