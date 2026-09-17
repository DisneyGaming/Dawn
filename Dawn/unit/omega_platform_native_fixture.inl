// Execute the original DF1F70 channel routine in this isolated test process.
// Captured runtime links identify the actual phase-in and phase-out graphs.
// Only effect execution and frame duration are modeled; no game is opened.
std::vector<std::uint32_t> platformEffects;
float platform_epsilon() noexcept {return 0.00001F;}
void platform_effect(std::byte* component,float) noexcept {
    platformEffects.push_back(read<std::uint32_t>(component,0));
}
void platform_native_test(const char* nativeImage) {
    for(const auto& d:omega_cannon_delivery::kDevices) if(d.kind==2 && d.transitIndex>=0
        && omega::transit::sources[static_cast<std::size_t>(d.transitIndex)].role==omega::transit::Role::bridge) {
        omega::mission::Snapshot s{};s.generation=2;s.command.cycle=d.cannon;s.chargeEnabled=true;
        std::array<std::byte,0x70> object{};std::array<std::byte,0x18> body{};
        put(object.data(),0,d.registry);put(object.data(),4,std::uint8_t{23});put(object.data(),6,d.slot);
        put(object.data(),12,d.schema);put(object.data(),0x68,14U);put(object.data(),0x6E,std::uint8_t{1});
        put(body.data(),0,1.F);put(body.data(),4,std::int16_t{1});
        put(body.data(),8,1.F);put(body.data(),12,std::int16_t{-1});put(body.data(),20,std::int16_t{-1});
        check(omega_cannon_delivery::authority(d,s,object,body),"platform delivery accepts materializing authority for all seven gates");
        put(body.data(),0,0.F);
        check(!omega_cannon_delivery::authority(d,s,object,body),"old zero-to-zero activation rejected by current platform delivery");
        s.command.cycle++;put(body.data(),4,std::int16_t{2});
        check(omega_cannon_delivery::authority(d,s,object,body),"platform delivery accepts decreasing retirement");
    }
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_platform";
    auto* device=allocate(0x1000);
    copy_file((folder/"device-38108.bin").string().c_str(),device,0x1000);
    check(read<std::uint32_t>(device,0)==0x80C22861U,"captured platform native device");
    for(const auto offset:{0x3B8U,0x408U}) {
        const auto handle=read<std::uint32_t>(device,offset);
        auto* effect=bind(handle,0x30);
        copy_file((folder/(offset==0x3B8?"phase-in-38108.bin":"phase-out-38108.bin")).string().c_str(),effect,0x30);
        check(read<std::uint32_t>(effect,0x24)==handle,"captured direction callback retains full effect identity");
        check(read<std::int64_t>(device,offset+8)==0,"captured effect reference has zero displacement");
    }
    DWORD old{};
    for(auto page:{0xDF1000U,0xDF2000U,0x3BC000U,0x58E000U})
        check(VirtualProtect(image+page,0x1000,PAGE_READWRITE,&old)!=0,"prepare private channel instructions");
    copy_file(nativeImage,image+0xDF1F70,0x4CB,0xDF1F70);
    for(auto rva:{0x1BA2B80U,0x1BA2C00U,0x1BA64E4U,0x1BAA778U}) {
        check(VirtualProtect(image+(rva&~0xFFFU),0x1000,PAGE_READWRITE,&old)!=0,"prepare actual native float constants");
        copy_file(nativeImage,image+rva,16,rva);
    }
    fixture_thunk(0x3BC940,reinterpret_cast<void*>(&platform_epsilon));
    fixture_thunk(0x58E420,reinterpret_cast<void*>(&platform_effect));
    for(auto page:{0xDF1000U,0xDF2000U,0x3BC000U,0x58E000U})
        check(VirtualProtect(image+page,0x1000,PAGE_EXECUTE_READ,&old)!=0,"execute private original channel routine");
    FlushInstructionCache(GetCurrentProcess(),image+0xDF1F70,0x4CB);
    std::array<std::byte,0x30> definition{},frame{};
    put(definition.data(),0x14,1.F);put(frame.data(),0x10,0.25F);
    using PlatformChannelTick=void(__fastcall*)(std::byte*,const std::byte*,std::uint8_t,const std::byte*);
    auto tick=[&] {reinterpret_cast<PlatformChannelTick>(image+0xDF1F70)(device+0x350,definition.data(),0,frame.data());};
    check(read<float>(device,0x370)==0.F && read<float>(device,0x37C)==0.F,"live missing platform retained zero current and target");
    platformEffects.clear();put(device,0x37C,1.F);tick();
    check(platformEffects==std::vector<std::uint32_t>{0x80BFD0FDU},"original increasing channel starts actual phase_in graph");
    check(read<float>(device,0x370)>0.F,"native channel progresses toward materialized position");
    for(unsigned i=0;i<4;++i) tick();
    check(read<float>(device,0x370)==1.F && platformEffects.size()==1,"activation reaches one without repeating phase_in");
    put(device,0x37C,0.F);tick();
    check(platformEffects==std::vector<std::uint32_t>{0x80BFD0FDU,0x80BFD0FEU},"original decreasing channel starts actual phase_out graph");
    for(unsigned i=0;i<4;++i) tick();
    check(read<float>(device,0x370)==0.F && platformEffects.size()==2,"retirement reaches zero without repeating phase_out");
}

