#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
extern "C" {
std::uintptr_t cleanup_owner_cookie{},cleanup_owner_resolver{},cleanup_owner_resume{},cleanup_owner_tail{};
volatile LONG64 cleanup_owner_misses{};
void native_cleanup_owner_body(void*) noexcept;
}
static unsigned checks{};
static void check(bool value) {++checks;if(!value){std::fprintf(stderr,"FAIL %u\n",checks);std::exit(1);}}
int main() {
    DWORD64 image{};
    const auto code=reinterpret_cast<DWORD64>(&native_cleanup_owner_body);
    const auto function=RtlLookupFunctionEntry(code,&image,nullptr);
    check(function!=nullptr);
    // Every body instruction uses the complete borrowed native stack frame.
    // Use the OS unwinder, not a synthetic interpretation of MASM directives.
    constexpr std::array<DWORD,22> boundaries{0x19,0x20,0x23,0x26,0x2a,0x2d,0x33,0x36,
        0x38,0x3a,0x3e,0x40,0x42,0x49,0x50,0x53,0x5b,0x5d,0x61,0x63,0x65,0x6c};
    for(const DWORD offset:boundaries) {
        check(offset<function->EndAddress-function->BeginAddress);
        alignas(16) std::array<DWORD64,64> stack{};
        const auto sp=reinterpret_cast<DWORD64>(stack.data());
        stack[0x70/8]=15;stack[0x78/8]=14;stack[0x80/8]=12;
        stack[0x88/8]=7;stack[0x90/8]=5;stack[0x98/8]=0x123456789;
        stack[0xA8/8]=3;stack[0xB0/8]=6;
        CONTEXT c{};c.Rsp=sp;c.Rbp=sp+0x70;c.Rip=code+offset;
        PVOID data{};DWORD64 frame{};
        RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,c.Rip,function,&c,&data,&frame,nullptr);
        check(c.Rsp==sp+0xA0 && c.Rip==0x123456789);
        check(c.Rbx==3 && c.Rsi==6 && c.Rdi==7 && c.Rbp==5);
        check(c.R12==12 && c.R14==14 && c.R15==15);
    }
    std::printf("PASS %u native cleanup guard unwind checks\n",checks);
}
