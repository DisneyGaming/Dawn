// Native predicate and request-readiness evidence. This does not model the
// prompt selector or claim that a player input has been accepted in game.
void arc_eligibility_native_test(const char* nativeImage) {
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_arc/eligibility";
    DWORD old{};
    for(auto page:{0xC99000U,0xF30000U})
        check(VirtualProtect(image+page,0x1000,PAGE_READWRITE,&old)!=0,"prepare private Arc eligibility instructions");
    copy_file(nativeImage,image+0xC994A0,0x5E,0xC994A0);
    copy_file(nativeImage,image+0xF30540,0x91,0xF30540);
    for(auto page:{0xC99000U,0xF30000U})
        check(VirtualProtect(image+page,0x1000,PAGE_EXECUTE_READ,&old)!=0,"execute original Arc eligibility instructions");
    FlushInstructionCache(GetCurrentProcess(),image+0xC994A0,0x5E);
    FlushInstructionCache(GetCurrentProcess(),image+0xF30540,0x91);
    for(const auto& entry:std::array<std::pair<std::uint32_t,const char*>,2>{{
        {0x80F7A6E0U,"charge-property.bin"},{0x80FEF337U,"other-property.bin"}}}) {
        auto* definition=bind(entry.first,0xA40);
        copy_file((folder/entry.second).string().c_str(),definition,0xA40);
        // The getter reads only the component's authored definition reference.
        // Both providers were enumerated on the live player's group chain.
        std::array<std::byte,0x30> provider{};
        put(provider.data(),0,entry.first);put(provider.data(),8,std::int64_t{0x620});
        const std::uint32_t required=0x9C99BE55U,wrong=0x9C99BE54U;
        using Property=bool(__fastcall*)(std::byte*,const std::uint32_t*);
        auto property=reinterpret_cast<Property>(image+0xC994A0);
        check(property(provider.data(),&required)==(entry.first==0x80F7A6E0U),"original property getter identifies the actual Arc charge requirement");
        check(!property(provider.data(),&wrong),"original getter rejects a different required item property");
    }
    auto* definition=bind(0x80F6666EU,0x770);
    copy_file((folder/"sink-definition.bin").string().c_str(),definition,0x770);
    auto* sink=allocate(0x900);
    copy_file((folder/"sink-live-7320.bin").string().c_str(),sink,0x900);
    using Ready=bool(__fastcall*)(std::byte*);
    auto ready=reinterpret_cast<Ready>(image+0xF30540);
    check(ready(sink),"original F30540 accepts the captured unused sink; source activation is not the blocker");
    put(sink,0x2DC,1U);
    check(!ready(sink),"original sink rejects another use while its request is pending");
    put(sink,0x2D8,1U);put(sink,0x2D0,std::uint8_t{1});
    check(!ready(sink),"original one-shot sink rejects repeated use after completion");
}
