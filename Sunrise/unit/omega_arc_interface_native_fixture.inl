// Original entity-interface lookup against captured live descriptor tables.
// No live code runs; groups and resolver tables belong to this test process.
void arc_interface_native_test(const char* nativeImage) {
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_arc/interfaces";
    constexpr std::array<std::pair<std::uint32_t,std::size_t>,11> routines{{
        {0x557470,0x103},{0x591290,8},{0x5980A0,0xBB},{0x598DB0,0x124},
        {0x59A350,0xD7},{0x5911D0,0xBB},{0x1258970,0x42},{0x9EBB80,0x75},
        {0x9EBF80,0x127},{0x597870,0xD},{0x9EBC00,0x24}}};
    DWORD old{};
    for(const auto& [rva,size]:routines) {
        const auto start=rva&~0xFFFU;
        check(VirtualProtect(image+start,0x2000,PAGE_READWRITE,&old)!=0,"prepare original interface lookup");
        copy_file(nativeImage,image+rva,size,rva);
    }
    check(VirtualProtect(image+0x187C000,0x1000,PAGE_READWRITE,&old)!=0,"prepare private cookie boundary");
    // The real cookie checker preserves the lookup result in AL.
    image[0x187C480]=std::byte{0xC3};
    check(VirtualProtect(image+0x187C000,0x1000,PAGE_EXECUTE_READ,&old)!=0,"execute private cookie boundary");
    for(const auto& [rva,size]:routines) {
        (void)size;check(VirtualProtect(image+(rva&~0xFFFU),0x2000,PAGE_EXECUTE_READ,&old)!=0,"execute original interface lookup");
    }
    for(auto type:{0x80803E70U,0x80803F6AU,0x80804FB0U,0x80809658U}) {
        auto* metadata=bind(type,0x30);char name[32]{};std::snprintf(name,sizeof name,"%08X.bin",type);
        copy_file((folder/name).string().c_str(),metadata,0x30);
    }
    // Native group links are 32-byte rows; +18 is the next group handle.
    constexpr std::uint32_t groupHandle=0x07F9E8A5;
    const auto bucket=975U;auto* rows=allocate(8192*32);auto* group=allocate(0x5000);
    put(tables+bucket*0x40,8,rows);put(tables+bucket*0x40,0x30,std::int32_t{32});
    put(tables+bucket*0x40,0x34,std::int32_t{-1});auto* row=rows+(groupHandle&8191)*32;
    put(row,8,reinterpret_cast<std::uintptr_t>(row)-reinterpret_cast<std::uintptr_t>(group));
    put(row,0x18,UINT32_MAX);
    std::array<std::byte,0xE0> world{};put(world.data(),0x4C,groupHandle);
    using Lookup=bool(__fastcall*)(const std::byte*,std::uint32_t,void*,std::uint32_t*);
    auto lookup=reinterpret_cast<Lookup>(image+0x557470);
    for(auto definition:{0x80F44F83U,0x80F44F89U,0x80F44F8DU,0x80F44F91U}) {
        const bool item=definition==0x80F44F83U;char name[32]{};std::snprintf(name,sizeof name,"%08X.bin",definition);
        auto* descriptor=bind(definition,0x10000);copy_file((folder/name).string().c_str(),descriptor,0x10000);
        copy_file((folder/(item?"item-group.bin":"sink-group.bin")).string().c_str(),group,0x5000);
        put(group,4,definition);
        std::array<std::byte,0x30> reference{};
        const bool oldFound=lookup(world.data(),item?0x80803E70U:0x80804FB0U,reference.data(),nullptr);
        check(!oldFound,"old declaration IDs fail actual native interface lookup");
        check(lookup(world.data(),item?0x80803F6AU:0x80809658U,reference.data(),nullptr),"registered interfaces resolve in shared item and all three native sink tables");
        check(read<std::uint32_t>(reference.data(),0x18)==groupHandle,"native lookup returns exact group owner");
        check(read<std::int64_t>(reference.data(),0x20)==(item?0x1230:0x20),"native lookup returns actual nested component offset");
        check(read<std::uint32_t>(reference.data(),0x1C)==(item?0x80803E70U:0x80804FB0U),"declaration is returned metadata, not the query interface");
    }
}
