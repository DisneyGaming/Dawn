// Original interaction setup, dynamic tag decoder and consumer execute in this
// test process only. Reflection scalar reads use the pinned 80804FB8 descriptor.
#include <Windows.h>
#include <bcrypt.h>
#include "state/activity/omega/omega_mission_devices.h"
#include "state/activity/omega/omega_transit_authority.h"
#include "state/activity/omega_arc_charge_authority.h"
#include "state/activity/deadly_trial/authority.h"
#include "client/hooks/bootflow/omega_arc_charge_native.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
namespace omega=dawn::state::activity::omega;
namespace charge=dawn::state::activity::omega_arc_charge;
namespace trial=dawn::state::activity::deadly_trial;
namespace device=dawn::state::activity::coo::native_device;
namespace hook=dawn::client::hooks::bootflow::omega_arc_charge_native;
namespace {
unsigned checks{};
std::byte* image{};
std::vector<std::byte*> allocations;
void check(bool ok,const char* message) {++checks;if(!ok) {std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
template<class T> T read(const std::byte* bytes,std::size_t offset) {T result{};std::memcpy(&result,bytes+offset,sizeof result);return result;}
template<class T> void put(std::byte* bytes,std::size_t offset,T value) {std::memcpy(bytes+offset,&value,sizeof value);}
std::byte* allocate(std::size_t size) {auto* p=static_cast<std::byte*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(p!=nullptr,"private fixture allocation");allocations.push_back(p);return p;}
void copy_file(const char* path,std::byte* out,std::size_t size,std::size_t offset=0) {std::ifstream f(path,std::ios::binary);check(bool(f),"fixture evidence available");f.seekg(static_cast<std::streamoff>(offset));f.read(reinterpret_cast<char*>(out),static_cast<std::streamsize>(size));check(f.gcount()==static_cast<std::streamsize>(size),"complete evidence read");}
void fixture_thunk(std::uintptr_t rva,void* function) {image[rva]=std::byte{0x48};image[rva+1]=std::byte{0xB8};put(image,rva+2,function);image[rva+10]=std::byte{0xFF};image[rva+11]=std::byte{0xE0};}
#include "omega_arc_unlock_native_fixture.inl"
struct MissionBits:ArcOverrideBits {std::size_t bit_count() const noexcept {return bits.size();}};
void verify_source(ArcOverrideBits stream,bool active,bool interaction,std::uint32_t generation) {
    check(stream.bits.size()==(active && interaction?375U:252U),"exact mission source width");
    check(stream.take(32)-0x80000000U==generation,"mission source generation preserved");
    check(stream.take(32)==0x80000000U,"candidate index zero preserved");
    check(stream.take(1)==unsigned(active),"source activity preserved");
    check(stream.take(1)==0 && stream.take(32)==0x7FFFFFFFU,"authored transform and auxiliary -1 preserved");
    check(stream.take(32)==0x811C9DC5U && stream.take(7)==0 && stream.take(16)==32767,"source absent scoped reference preserved");
    check(stream.take(32)==0 && stream.take(32)==0 && stream.take(32)==0 && stream.take(1)==0,"authored translation preserved");
    const auto count=stream.take(2);check(count==unsigned(active && interaction),"inactive and unrelated sources have zero dynamic records");
    if(!count) {check(stream.cursor==stream.bits.size(),"empty source consumed exactly");return;}
    std::array<std::byte,0x130> overrides{};put(overrides.data(),0,count);
    using Decode=void(__fastcall*)(std::byte*,unsigned,void*,ArcOverrideBits*);
    reinterpret_cast<Decode>(image+0x9FA4B0)(overrides.data()+0x10,0,nullptr,&stream);
    check(stream.cursor==stream.bits.size(),"original decoder consumes production mission record exactly");
    constexpr std::array<std::byte,8> predicate{std::byte{0xC5},std::byte{0x9D},std::byte{0x1C},std::byte{0x81},std::byte{0xFF},std::byte{0},std::byte{0xFF},std::byte{0xFF}};
    const hook::SinkEnableCommand previous{predicate};
    check(std::memcmp(overrides.data(),previous.bytes.data(),previous.bytes.size())==0,"decoded authority equals previously used native enable command byte for byte");
    std::array<std::byte,0x400> sink{};std::array<std::byte,16> missingSetup{};
    using Setup=bool(__fastcall*)(std::byte*,const void*);using Apply=void(__fastcall*)(std::byte*,const std::byte*);
    check(reinterpret_cast<Setup>(image+0xF32820)(sink.data(),missingSetup.data()),"original setup creates blocked interaction");
    check(read<std::uint8_t>(sink.data(),0x2C0)==1,"missing setup starts blocked");
    const auto before=sink;const auto apply=reinterpret_cast<Apply>(image+0xF33930);apply(sink.data(),overrides.data());
    check(read<std::uint8_t>(sink.data(),0x2C0)==0 && read<std::uint8_t>(sink.data(),0x280)==1,"native authority unlocks prompt and marks cache dirty");
    check(std::memcmp(sink.data()+0x2C4,predicate.data(),8)==0,"captured absent scoped predicate preserved");
    check(std::memcmp(sink.data()+0x2D0,before.data()+0x2D0,0x24)==0,"native authority leaves usage and player association unchanged");
    const auto enabled=sink;apply(sink.data(),overrides.data());check(sink==enabled,"duplicate enable publication is idempotent");
    // A hold can be pending or consumed between source retransmissions.
    put(sink.data(),0x2D0,std::uint8_t{1});put(sink.data(),0x2D8,1);put(sink.data(),0x2DC,2);put(sink.data(),0x2E0,std::uint64_t{0x1122334455667788ULL});
    const auto used=sink;apply(sink.data(),overrides.data());check(sink==used,"republication cannot reset used flag, consume a pending request or change requester");
}
void verify_image(const char* path) {
    std::ifstream file(path,std::ios::binary);check(bool(file),"native image available for identity check");
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"open SHA256 provider");
    check(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"create native image hash");
    std::array<unsigned char,65536> chunk{};
    while(file) {
        file.read(reinterpret_cast<char*>(chunk.data()),static_cast<std::streamsize>(chunk.size()));
        const auto count=file.gcount();
        if(count>0) {check(BCryptHashData(hash,chunk.data(),static_cast<ULONG>(count),0)==0,"hash native image chunk");}
    }
    check(file.eof(),"native image read completed");
    std::array<unsigned char,32> digest{};
    check(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)==0,"finish native image hash");
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr std::array<unsigned char,32> expected{0x63,0xD1,0x28,0xF1,0xC7,0x59,0xB9,0x2D,
        0x32,0xB0,0xF2,0x26,0xBC,0xBE,0xC8,0x28,0xBC,0x58,0xCE,0xF1,0x93,0xEE,0x0D,0xF6,
        0xFD,0xD5,0x82,0xBD,0x02,0x90,0xED,0x1E};
    check(digest==expected,"exact build86657 native image SHA256 required before execution");
}
void mission_test() {
    for(const auto& cycle:charge::kCycles) for(const auto object:{charge::Object::carry,charge::Object::sink,charge::Object::effect}) {
        const charge::Source source{&cycle,object};
        for(unsigned phase=0;phase<6;++phase) {
            charge::Authority authority{10,static_cast<std::uint8_t>(cycle.index+1U),phase>0 && phase<4,phase==4,phase>=2};
            if(phase==5) {authority.cycle=static_cast<std::uint8_t>(cycle.index+2U);}
            ArcOverrideBits stream;check(charge::write_authority(stream,source,authority),"archive Omega production source encodes");
            check(stream.bits.size()==charge::authority_bits(source,authority),"archive Omega reported width equals actual wire");
            const auto lifecycle=charge::lifecycle(source,authority);
            verify_source(stream,lifecycle==charge::Lifecycle::active,object==charge::Object::sink,charge::revision(authority.generation,lifecycle));
        }
    }
    trial::Frame frame{};frame.enabled=true;frame.spawnGeneration=9;frame.pikes=2;
    for(bool enabled:{false,true}) {
        frame.reviveEnabled=enabled;
        for(const auto slot:{std::uint16_t{10},std::uint16_t{21},std::uint16_t{59}}) {
            MissionBits stream;check(trial::write_body(stream,frame,trial::kAlleysB,4,slot),"Deadly Trial production source encodes");
            check(stream.bits.size()==trial::body_bits(frame,trial::kAlleysB,4,slot),"Deadly Trial reported width equals actual wire");
            verify_source(stream,slot!=59 || enabled,slot==59,slot!=59 || enabled?10U:9U);
        }
    }
    for(const auto generation:{0U,charge::kMaximumGeneration}) {ArcOverrideBits stream;const charge::Authority invalid{generation,1,true,false,true};check(!charge::write_authority(stream,{&charge::kCycles[0],charge::Object::sink},invalid) && stream.bits.empty(),"invalid generation refuses publication");}
    ArcOverrideBits invalid;check(!device::object(invalid,1,true,nullptr,static_cast<device::interaction::Mode>(3)) && invalid.bits.empty(),"invalid interaction mode rejected without partial output");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {std::fprintf(stderr,"usage: native_mission_interaction_tests native-image\n");return 2;}
    verify_image(argv[1]);image=allocate(0x6270000);arc_unlock_native_test(argv[1]);mission_test();
    std::printf("Native mission interaction: %u checks, 0 failures; original F32820/9FA4B0/9F9B30/F33930 instructions\n",checks);
    for(auto* p:allocations) {VirtualFree(p,0,MEM_RELEASE);}return 0;
}
