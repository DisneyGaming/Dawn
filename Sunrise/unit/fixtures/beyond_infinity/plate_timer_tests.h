#pragma once
#include "../../../src/client/hooks/bootflow/beyond_infinity_plate_timer.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace beyond_plate_timer_fixture {
namespace timer=sunrise::client::hooks::bootflow::beyond_infinity_plate_timer;
template<class T> T field(const timer::Command& command,std::size_t offset) {
    T value{};std::memcpy(&value,command.bytes.data()+offset,sizeof value);return value;
}
struct Memory {
    static constexpr std::uintptr_t component=0x10000,definition=0x20000;
    std::array<std::byte,16> header{};float definitionEnd{},liveEnd{37.F};
    bool resolves{true},readable{true};std::uintptr_t resolved{definition},lastScalar{};
    Memory() {
        const std::uint32_t tag=timer::kDefinition,kind=timer::kKind;const std::uint64_t offset=timer::kDefinitionOffset;
        std::memcpy(header.data(),&tag,4);std::memcpy(header.data()+4,&kind,4);std::memcpy(header.data()+8,&offset,8);
    }
    bool copy(std::uintptr_t address,std::span<std::byte> bytes) {
        if(!readable || address!=component || bytes.size()!=header.size()) { return false; }
        std::memcpy(bytes.data(),header.data(),header.size());return true;
    }
    bool resolve(std::uint32_t tag,std::uintptr_t& out) { out=resolved;return resolves && tag==timer::kDefinition; }
    bool value(std::uintptr_t address,float& out) {
        lastScalar=address;
        if(address==definition+timer::kDefinitionOffset+timer::kEndOffset) { out=definitionEnd;return readable; }
        if(address==component+timer::kEndOffset) { out=liveEnd;return readable; }return false;
    }
};
inline void run(void (*check)(bool,const char*)) {
    constexpr auto payload=timer::kStateOffset;
    constexpr std::uint64_t duration=5U*timer::kTicksPerSecond,epoch=100U*timer::kTicksPerSecond;
    timer::Command command{};
    check(timer::start(command,duration,epoch),"native timer accepts valid duration and native epoch");
    check(command.bytes.size()==0x70 && timer::kStateBytes==0x48 && timer::kEntryStride==0x60,"native timer command dimensions in bytes");
    check(field<std::int32_t>(command,0)==1 && field<std::uint32_t>(command,0x10)==0x80804FCAU,"native timer one-entry typed envelope");
    check(field<std::uint8_t>(command,payload)==1 && field<std::uint8_t>(command,payload+8)==1,"native timer active and advancing flags");
    check(field<std::uint64_t>(command,payload+0x10)==0 && field<std::uint64_t>(command,payload+0x18)==duration,"native timer lower and upper bounds");
    check(field<std::uint64_t>(command,payload+0x20)==0 && field<std::uint64_t>(command,payload+0x28)==duration,"native timer starts with zero elapsed and full remaining duration");
    check(field<std::uint64_t>(command,payload+0x30)==epoch && field<float>(command,payload+0x38)==1.F,"native timer uses provided native clock and unit rate");
    check(field<std::uint8_t>(command,payload+0x40)==0,"native timer extra state remains inactive");
    // Independently emulate only FEE1C0's typed list copy, not the builder.
    std::array<std::byte,0x48> received{};
    for(std::int32_t i=0;i<field<std::int32_t>(command,0);++i) {
        const auto offset=static_cast<std::size_t>(i)*0x60;
        if(field<std::uint32_t>(command,offset+0x10)==0x80804FCAU) { std::memcpy(received.data(),command.bytes.data()+offset+0x20,received.size()); }
    }
    std::uint64_t receivedRemaining{};std::memcpy(&receivedRemaining,received.data()+0x28,8);
    check(receivedRemaining==duration && received[0]==std::byte{1},"native list decoder receives the intended timer state");
    for(const std::uint64_t invalid:{std::uint64_t{0},timer::kMaximumDurationTicks+1,(std::numeric_limits<std::uint64_t>::max)()}) {
        check(!timer::start(command,invalid,epoch) && command.bytes==timer::Command{}.bytes,"invalid timer duration leaves no applicable command");
    }
    check(!timer::start(command,duration,(std::numeric_limits<std::uint64_t>::max)()),"unset native epoch rejected");
    check(!timer::start(command,duration,(std::numeric_limits<std::uint64_t>::max)()-duration),"native epoch ending at unset sentinel rejected");
    check(!timer::start(command,duration,(std::numeric_limits<std::uint64_t>::max)()-duration+1),"native epoch wrap rejected");
    check(timer::start(command,duration,0),"zero native clock epoch is valid");
    check(timer::start(command,timer::kMaximumDurationTicks,0),"signed native millisecond duration boundary accepted");
    const auto stop=timer::stopped();
    check(field<std::int32_t>(stop,0)==1 && field<std::uint32_t>(stop,0x10)==0x80804FCAU,"stop retains native typed envelope");
    bool zero=true;for(std::size_t i=payload;i<stop.bytes.size();++i) { zero=zero && stop.bytes[i]==std::byte{}; }
    check(zero,"stopped timer state is fully zero including epoch rate and flags");
    check(timer::complete(1,1,1.F,0.F),"native latched endpoint accepted");
    check(!timer::complete(1,0,1.F,0.F) && !timer::complete(0,1,1.F,0.F),"native endpoint requires active state and original completion latch");
    check(!timer::complete(2,1,1.F,0.F) && !timer::complete(1,2,1.F,0.F),"invalid native boolean states rejected");
    check(!timer::complete(1,1,std::nextafter(1.F,0.F),0.F) && !timer::complete(1,1,std::nextafter(1.F,2.F),0.F),"native endpoint must equal one exactly");
    check(!timer::complete(1,1,std::numeric_limits<float>::quiet_NaN(),0.F) && !timer::complete(1,1,1.F,std::numeric_limits<float>::infinity()),"nonfinite native timer outputs rejected");
    check(!timer::complete(1,1,1.F,-1.F),"negative native remaining time rejected");
    Memory memory;float end{};
    check(timer::authored_end(memory,Memory::component,end) && end==0.F,"exact plate timer definition has zero normalization end");
    check(memory.lastScalar==Memory::definition+0x248+0x148,"normalization end read from resolved definition rather than live component");
    memory.definitionEnd=1.F;check(!timer::authored_end(memory,Memory::component,end),"different timer normalization contract rejected");
    memory.definitionEnd=std::numeric_limits<float>::quiet_NaN();check(!timer::authored_end(memory,Memory::component,end),"invalid authored normalization end rejected");memory.definitionEnd=0.F;
    for(const auto offset:{0U,4U,8U}) { memory.header[offset]^=std::byte{1};check(!timer::authored_end(memory,Memory::component,end),"changed timer definition triple rejected");memory.header[offset]^=std::byte{1}; }
    const std::uint32_t linked=0x815B3829U;std::memcpy(memory.header.data(),&linked,4);
    check(!timer::authored_end(memory,Memory::component,end),"linked timer on another entity cannot substitute for plate timer");memory=Memory{};
    memory.resolves=false;check(!timer::authored_end(memory,Memory::component,end),"unresolved authored timer definition rejected");memory.resolves=true;
    memory.resolved=UINTPTR_MAX;check(!timer::authored_end(memory,Memory::component,end),"timer definition address overflow rejected");memory=Memory{};
    memory.readable=false;check(!timer::authored_end(memory,Memory::component,end),"unreadable timer definition rejected");
}
}
