// Executes the unchanged pinned scalar setter against the original asset layout.
// Only downstream dirty notification is replaced; this does not test rendering.
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <cmath>
#include <mutex>
#include "client/hooking/call_gate.h"
#include "client/hooks/bootflow/omega_boss_vfx_start.h"
namespace graph=dawn::client::hooks::bootflow::omega_boss_graph;
namespace hooking=dawn::client::hooking;
namespace omega_boss_eye_diagnostics=dawn::client::hooks::bootflow::omega_boss_eye_diagnostics;
namespace omega_boss_vfx_start=dawn::client::hooks::bootflow::omega_boss_vfx_start;
std::uint64_t fixtureRun{1};
namespace state::activity {std::uint64_t mission_run_generation() noexcept {return fixtureRun;}}

namespace {
unsigned checks{},notifications{};
void check(bool value,const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr,"FAIL: %s (%u)\n",message,checks); std::exit(1); }
}
template<class T> T get(const std::byte* p,std::size_t at) {
    T value{}; std::memcpy(&value,p+at,sizeof value); return value;
}
template<class T> void put(std::byte* p,std::size_t at,T value) {std::memcpy(p+at,&value,sizeof value);}
std::vector<std::byte*> allocations;
std::byte* alloc(std::size_t size) {
    auto* p=static_cast<std::byte*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(p!=nullptr,"private test allocation"); allocations.push_back(p); return p;
}
void read(const char* path,std::byte* out,std::size_t size,std::size_t offset=0) {
    std::ifstream in(path,std::ios::binary);check(static_cast<bool>(in),"evidence available");
    in.seekg(static_cast<std::streamoff>(offset));
    in.read(reinterpret_cast<char*>(out),static_cast<std::streamsize>(size));
    check(in.gcount()==static_cast<std::streamsize>(size),"exact evidence read");
}
std::byte *image{},*asset{},*root{},*lastRoot{},*lastDefinition{};
bool lastDirty{};
graph::Owner testOwner{1,2,0x20464004,4,5,0x12346001,7,8,9,0,10};
bool ownerAvailable{true},memberEnabled{true},queueActive{},changeOwner{},changeRun{};
std::array<std::byte,0x100000> directory{};
std::byte* tables=directory.data();
void bind(std::uint32_t handle,std::byte* value) {
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(handle)>>13);
    const auto bucket=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    check(bucket<16384,"native datum bucket bounded");
    auto* descriptor=tables+bucket*0x40;
    auto* rows=get<std::byte*>(descriptor,8);
    if (!rows) {rows=alloc(0x20000);put(descriptor,8,rows);put(descriptor,0x30,std::int32_t{16});put(descriptor,0x34,std::int32_t{-1});}
    auto* row=rows+(handle&0x1FFF)*16;
    put(row,8,reinterpret_cast<std::uintptr_t>(row)-reinterpret_cast<std::uintptr_t>(value));
}
void __fastcall dirty(std::byte* owner,std::byte* definition,bool changed) {
    ++notifications;lastRoot=owner;lastDefinition=definition;lastDirty=changed;
    if (changeOwner) ++testOwner.character;
    if (changeRun) ++fixtureRun;
}
void execute_page(std::size_t at) {
    DWORD old{};check(VirtualProtect(image+at,0x1000,PAGE_EXECUTE_READ,&old)!=0,"make only bounded original code executable");
}
bool readable(const void* p,std::size_t size) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (!p || !VirtualQuery(p,&info,sizeof info) || info.State!=MEM_COMMIT
        || info.Protect&(PAGE_NOACCESS|PAGE_GUARD)) return false;
    const auto offset=reinterpret_cast<std::uintptr_t>(p)-reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    return offset<=info.RegionSize && size<=info.RegionSize-offset;
}
bool copy_native(const void* p,void* out,std::size_t size) noexcept {
    if (!readable(p,size)) return false;std::memcpy(out,p,size);return true;
}
std::byte* resolve_handle(std::uint32_t handle) noexcept {
    if (handle==0x80F6690A) return asset;
    return handle==0x12346001?root:nullptr;
}
struct MemberView {bool enabled{};int count{},head{};};
bool current_owner(const graph::Owner&,MemberView& member,graph::Owner& current) noexcept {
    current=testOwner;member.enabled=memberEnabled;member.count=queueActive?1:0;member.head=0;return ownerAvailable;
}
template<class Fn> Fn native(std::uintptr_t rva) noexcept {
    check(rva==0xA0FE60,"production helper invokes only proven native setter");return reinterpret_cast<Fn>(image+rva);
}
void log(const char*,...) noexcept {}
#include "client/hooks/bootflow/omega_boss_animation_glow.inl"
#include "client/hooks/bootflow/omega_boss_vfx_start.inl"
}

int main(int argc,char** argv) {
    check(argc==3,"arguments: pinned native image, 80F6690A asset");
    image=alloc(0x2440000);asset=alloc(17600);
    read(argv[2],asset,17600);root=asset+0x90;
    check(get<std::uint32_t>(root,0)==0x80F6690A && get<std::uint32_t>(root,4)==0x808082EC && get<std::int64_t>(root,8)==0x3038,"exact original animation source");
    auto* definition=asset+get<std::int64_t>(root,8);
    check(get<std::uint64_t>(definition,0x320)==47 && get<std::uint64_t>(root,0x1328)==47,"native definition/runtime arrays both contain 47 rows");
    auto* definitions=definition+0x338+get<std::int64_t>(definition,0x328);
    auto* providers=root+0x1340+get<std::int64_t>(root,0x1330);
    check(definitions==asset+0x3BC0 && providers==asset+0x2760,"independently recovered original array locations");
    check(get<std::uint32_t>(definitions+40*0x30,0x28)==0xCE0BA42D,"CE is sorted definition index 40");
    check(get<std::uint32_t>(providers+40*0x30,0)==0x80F6690A && get<std::uint32_t>(providers+40*0x30,4)==0x80807EEB && get<std::int64_t>(providers+40*0x30,8)==0x4340,"CE provider retains exact original source");
    constexpr std::uint32_t runtimeHandle=0x12346001;
    put(root,0x24,runtimeHandle);bind(0x80F6690A,asset);bind(runtimeHandle,root);
    put(image,0x2439C70,&tables);
    read(argv[1],image+0xA0FE60,0x13F,0xA0FE60);
    read(argv[1],image+0xA10180,0x10B,0xA10180);
    read(argv[1],image+0xA8CB20,0x7F,0xA8CB20);
    read(argv[1],image+0xA0AD30,9,0xA0AD30);
    read(argv[1],image+0xA0C760,8,0xA0C760);
    read(argv[1],image+0x1BA2B7C,4,0x1BA2B7C);
    read(argv[1],image+0x1BCCA60,16,0x1BCCA60);
    constexpr std::array<std::uint8_t,16> setterPrefix{{0x40,0x53,0x48,0x83,0xEC,0x20,0x44,0x8B,0x09,0x48,0x8B,0xD9,0x41,0x8B,0xC1,0x4C}};
    check(std::memcmp(image+0xA0FE60,setterPrefix.data(),setterPrefix.size())==0,"pinned setter prologue");
    image[0x5906A0]=std::byte{0x48};image[0x5906A1]=std::byte{0xB8};put(image,0x5906A2,&dirty);
    image[0x5906AA]=std::byte{0xFF};image[0x5906AB]=std::byte{0xE0};
    execute_page(0xA0F000);execute_page(0xA10000);execute_page(0x590000);
    execute_page(0xA8C000);execute_page(0xA0A000);execute_page(0xA0C000);
    FlushInstructionCache(GetCurrentProcess(),image,0x2440000);
    using Setter=void(__fastcall*)(std::byte*,const std::uint32_t*,float);
    const auto setter=reinterpret_cast<Setter>(image+0xA0FE60);
    // The source's +1470 is the AI actor. The character's animation-state
    // handle belongs to a different pool and must not be substituted here.
    constexpr std::uint32_t actor=0x20464004,animation=0x30466006;
    auto* actors=alloc(0x200000);put(image,0x1F9D7F8,actors);put(image,0x1F9D800,std::uint32_t{0x100});
    put(actors+(actor&0x1FFF)*0x100,0x50,runtimeHandle);put(root,0x1470,actor);
    put(root,0x1450,std::uint64_t{0x8877665544332211});put(root,0x1458,std::uint64_t{0x1122334455667788});
    std::uint32_t nativeActor{};
    reinterpret_cast<void(__fastcall*)(std::byte*,std::uint32_t*)>(image+0xA0AD30)(root,&nativeActor);
    check(nativeActor==actor && nativeActor!=animation,"original source getter returns AI actor, not animation-state handle");
    std::array<std::byte,24> context{};
    reinterpret_cast<void(__fastcall*)(void*,std::uint32_t)>(image+0xA8CB20)(context.data(),nativeActor);
    check(get<std::uint32_t>(context.data(),0)==runtimeHandle && get<std::uint32_t>(context.data(),4)==actor,"original AI-pool lookup resolves full parent and preserves actor");
    check(std::memcmp(context.data()+8,root+0x1450,16)==0,"original parent-context getter reads the same source");
    for (std::size_t i=0;i<47;++i) {
        const auto name=get<std::uint32_t>(definitions+i*0x30,0x28);
        check(i==0 || name>get<std::uint32_t>(definitions+(i-1)*0x30,0x28),"actual definitions are hash sorted");
        for (std::size_t j=0;j<47;++j) put(providers+j*0x30,0x20,0.F);
        notifications=0;lastRoot=lastDefinition=nullptr;lastDirty=false;
        setter(root,&name,1.F);
        check(notifications==1 && lastRoot==root && lastDefinition==definitions+i*0x30+0x10 && lastDirty,"original provider write fans out exact definition to owning root");
        for (std::size_t j=0;j<47;++j) check(get<float>(providers+j*0x30,0x20)==(i==j?1.F:0.F),"native lookup changes only the selected ordinal");
        setter(root,&name,1.F);
        check(notifications==1,"unchanged native value does not emit redundant dirty event");
    }
    constexpr std::uint32_t missing=0x01020304;
    notifications=0;setter(root,&missing,1.F);check(notifications==0,"unknown name cannot modify a provider");
    constexpr std::uint32_t glow=0xCE0BA42D;
    put(providers+40*0x30,0x20,0.F);setter(root,&glow,1.F);
    check(get<float>(providers+40*0x30,0x20)==1.F && notifications==1,"authored CE setter accepts full root and float in XMM2");
    hooking::CallGate gate;gate.accept();const hooking::CallGate::Scope call(gate);
    const auto reset=[&]() {
        ++fixtureRun;testOwner.run=fixtureRun;testOwner.character=7;
        ownerAvailable=memberEnabled=true;queueActive=changeOwner=changeRun=false;
        put(root,0x2C,testOwner.entity);put(root,0x1470,testOwner.actor);
        put(root,0x1328,std::uint64_t{47});put(providers+40*0x30,0x20,0.F);
        notifications=0;gate.accept();
    };
    reset();check(omega_boss_vfx_start::source_layout({asset,17600}),"startup source guard accepts actual original asset");
    check(prepare_intro_vfx(testOwner,call) && notifications==1,"actual production startup helper calls original setter once");
    check(prepare_intro_vfx(testOwner,call) && notifications==1,"confirmed startup helper is idempotent");
    put(providers+40*0x30,0x20,0.F);
    check(!prepare_intro_vfx(testOwner,call) && notifications==1,"later native reset cannot be overwritten by repeated bridge call");
    reset();put(providers+40*0x30,0x20,1.F);
    check(prepare_intro_vfx(testOwner,call) && notifications==0,"already native-enabled source needs no mutation");
    reset();const auto issued=testOwner;changeOwner=true;
    check(!prepare_intro_vfx(issued,call) && notifications==1,"owner change during native dirty callback cannot publish confirmation");
    changeOwner=false;testOwner=issued;
    check(!prepare_intro_vfx(issued,call) && notifications==1,"uncertain native result is not retried or relabeled");
    reset();const auto oldRunOwner=testOwner;changeRun=true;
    check(!prepare_intro_vfx(oldRunOwner,call) && notifications==1,"run change during native callback cannot confirm");
    reset();queueActive=true;
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"existing native graph command blocks initialization");
    reset();memberEnabled=false;
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"disabled authority member blocks initialization");
    reset();ownerAvailable=false;
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"unresolved owner blocks initialization");
    reset();gate.quiesce();
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"quiescing hook cannot mutate native source");
    reset();put(root,0x1470,std::uint32_t{8});
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"animation-state handle cannot replace AI actor binding");
    reset();put(root,0x1328,std::uint64_t{46});
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"wrong provider table cannot enter native setter");
    reset();put(providers+40*0x30,0x20,0.5F);
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"existing intermediate native scalar is not overwritten");
    reset();put(definitions+40*0x30,0x28,std::uint32_t{0xCE0BA42C});
    check(!prepare_intro_vfx(testOwner,call) && notifications==0,"changed definition blocks native setter");
    put(definitions+40*0x30,0x28,glow);
    std::printf("PASS: %u original native VFX scalar setter checks\n",checks);
    for (auto* p:allocations) VirtualFree(p,0,MEM_RELEASE);
}
