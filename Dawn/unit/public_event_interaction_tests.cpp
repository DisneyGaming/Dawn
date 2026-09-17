#include "middleware/bap/activity_message/native/public_event_interaction_authority.h"
#include "middleware/bap/activity_message/native/placement_authority.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <array>
namespace interaction=dawn::middleware::bap::activity_message::native::interaction;
namespace placement=dawn::middleware::bap::activity_message::native::placement;
namespace bits=dawn::middleware::encoding::bits;
namespace auth=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace {
unsigned checks{},failures{};
#define CHECK(x) do {++checks;if(!(x)){++failures;std::printf("FAIL %u: %s\n",unsigned(__LINE__),#x);}}while(false)
// This prefix is the established package-owned local placement contract.
// Exported vectors are decoded by original4BEE90, including9FA4B0 dispatch.
template<class Writer> bool encode(Writer& w,interaction::Mode mode) {
    return w.write(0,32)&&w.write(1,32)&&w.write(1,1)&&w.write(1,1)&&w.write(0,32)
        &&w.write(0x811C9DC5U,32)&&w.write(0,7)&&w.write(32767,16)
        &&w.write(0,32)&&w.write(0,32)&&w.write(0,32)&&w.write(0,1)
        &&w.write(1,2)&&interaction::write_record(w,mode);
}
}
int main(int argc,char** argv) {
    for(const auto mode:{interaction::Mode::unchanged,interaction::Mode::disabled,interaction::Mode::enabled}) {
        std::array<std::byte,48> bytes{};bits::Writer writer{bytes};
        CHECK(encode(writer,mode));
        const auto expected=placement::kActiveBits+(mode==interaction::Mode::unchanged?0:interaction::kPayloadBits);
        CHECK(writer.bit_count()==expected);
        std::size_t size{};CHECK(writer.finish(size));CHECK(size==(expected+7)/8);
        auto measure=bits::Writer::measuring();CHECK(encode(measure,mode));CHECK(measure.bit_count()==expected);
        const placement::Request request{0x85C38F77,0,15,mode};
        CHECK(placement::body_bits(request)==expected);
        std::array<std::byte,48> projected{};bits::Writer production{projected};
        CHECK(placement::write(production,request));CHECK(projected==bytes);CHECK(production.bit_count()==expected);
        auth::Snapshot snapshot{};snapshot.placements.count=1;snapshot.placements.entries[0]=request;
        CHECK(auth::legacy_auth_body_bits(snapshot,request.registry,4,request.slot,false)==expected);
        std::array<std::byte,48> routed{};bits::Writer routedWriter{routed};
        CHECK(auth::legacy_write_auth_body(routedWriter,snapshot,request.registry,4,request.slot,false));
        CHECK(routed==bytes && routedWriter.bit_count()==expected);
        if(mode==interaction::Mode::unchanged) {
            std::array<std::byte,48> baseline{};bits::Writer old{baseline};CHECK(placement::write_active(old));
            CHECK(bytes==baseline);CHECK(writer.bit_count()==old.bit_count());
        }
        for(std::size_t available=0;available<size;++available) {
            std::array<std::byte,48> shortBuffer{};bits::Writer bounded{{shortBuffer.data(),available}};
            CHECK(!encode(bounded,mode));
        }
        if(argc==2) {
            const auto path=std::filesystem::path(argv[1])/("interaction-"+std::to_string(static_cast<unsigned>(mode))+".wire.bin");
            std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(size));CHECK(file.good());
        }
    }
    for(unsigned value=3;value<256;++value) {
        const auto mode=static_cast<interaction::Mode>(value);CHECK(!interaction::valid(mode));
        auto writer=bits::Writer::measuring();CHECK(!interaction::write_record(writer,mode));CHECK(writer.bit_count()==0);
        const placement::Request request{0x85C38F77,0,15,mode};
        CHECK(placement::body_bits(request)==0);CHECK(!placement::write(writer,request));CHECK(writer.bit_count()==0);
    }
    std::printf("Public event interaction: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
