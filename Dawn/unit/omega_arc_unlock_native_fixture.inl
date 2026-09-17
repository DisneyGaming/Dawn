// Original dynamic-tag decode and interaction lock application. Reflection
// scalar reads are modeled from the pinned descriptor; no live input/use is
// simulated. F32820, 9FA4B0, 9F9B30 and F33930 execute original instructions.
struct ArcOverrideBits {
    std::vector<bool> bits; std::size_t cursor{};
    bool write(std::uint64_t value,unsigned width) {
        for(unsigned i=width;i>0;--i) bits.push_back(((value>>(i-1))&1)!=0);
        return true;
    }
    std::uint32_t take(unsigned width) {
        check(cursor+width<=bits.size(),"original override decoder stays inside production wire");
        std::uint32_t value{};while(width--) value=(value<<1)|bits[cursor++];return value;
    }
};
bool __fastcall arc_override_bool(ArcOverrideBits* stream) {return stream->take(1)!=0;}
std::uint32_t __fastcall arc_override_word(ArcOverrideBits* stream,unsigned width) {return stream->take(width);}
void __fastcall arc_override_schema(std::uint32_t tag,ArcOverrideBits* stream,std::byte* out,std::byte*,int) {
    if(tag==0x80800046U) {
        using ReadTag=void(__fastcall*)(std::byte*,unsigned,void*,ArcOverrideBits*);
        reinterpret_cast<ReadTag>(image+0x9F9B30)(out,0,nullptr,stream);
        return;
    }
    check(tag==0x80804FB8U,"original dynamic dispatch selects native interaction schema");
    // Pinned 80804FB8 fields: i8(+1,2), reference55, i32(+2^31), bool.
    put(out,0,static_cast<std::uint8_t>(stream->take(2)-1));
    put(out,4,stream->take(32));put(out,8,static_cast<std::uint8_t>(stream->take(7)-1));
    put(out,10,static_cast<std::uint16_t>(stream->take(16)-0x8000));
    put(out,12,stream->take(32)-0x80000000U);put(out,16,static_cast<std::uint8_t>(stream->take(1)));
}
void arc_unlock_native_test(const char* nativeImage) {
    DWORD old{};
    const std::array pages{0xF32000U,0xF33000U,0x9FA000U,0x9F9000U,0x4C7000U,0x350000U,0x351000U};
    for(auto page:pages) check(VirtualProtect(image+page,0x1000,PAGE_READWRITE,&old)!=0,"prepare original interaction override instructions");
    for(const auto span:std::array{std::array{0xF32820U,0xA7U},std::array{0xF33930U,0x88U},
                                  std::array{0x9FA4B0U,0x63U},std::array{0x9F9B30U,0x47U}})
        copy_file(nativeImage,image+span[0],span[1],span[0]);
    fixture_thunk(0x4C74B0,reinterpret_cast<void*>(&arc_override_schema));
    fixture_thunk(0x350EF0,reinterpret_cast<void*>(&arc_override_bool));
    fixture_thunk(0x3513B0,reinterpret_cast<void*>(&arc_override_word));
    for(const auto entry:std::array{std::array{0x2076F18U,0x80804FB8U},std::array{0x204A348U,0x80800046U}}) {
        auto* tag=allocate(4);put(tag,0,entry[1]);put(image,entry[0],tag);
    }
    for(auto page:pages) check(VirtualProtect(image+page,0x1000,PAGE_EXECUTE_READ,&old)!=0,"execute original interaction override instructions");
    FlushInstructionCache(GetCurrentProcess(),image,0x6270000);
    using Setup=bool(__fastcall*)(std::byte*,const void*);
    using Decode=void(__fastcall*)(std::byte*,unsigned,void*,ArcOverrideBits*);
    using Apply=void(__fastcall*)(std::byte*,const std::byte*);
    const auto setup=reinterpret_cast<Setup>(image+0xF32820);
    const auto decode=reinterpret_cast<Decode>(image+0x9FA4B0);
    const auto apply=reinterpret_cast<Apply>(image+0xF33930);
    for(const auto& row:omega::transit::sources) if(row.role==omega::transit::Role::sink) {
        std::array<std::byte,0x400> sink{};std::array<std::byte,16> missingSetup{};
        check(setup(sink.data(),missingSetup.data()) && read<std::uint8_t>(sink.data(),0x2C0)==1,
            "original setup reproduces missing prompt: absent setup locks sink");
        for(unsigned phase=0;phase<4;++phase) {
            omega::mission::Snapshot s{};s.generation=2;s.command.cycle=row.cycle;
            s.chargeEnabled=phase==1 || phase==2;s.chargePickedUp=phase>=2;s.chargeDunked=phase==3;
            ArcOverrideBits stream;
            check(omega::transit::write(stream,s,row.registry,4,row.slot) && stream.bits.size()==375,"production sink record width");
            stream.cursor=250;std::array<std::byte,0x130> overrides{};
            put(overrides.data(),0,stream.take(2));
            decode(overrides.data()+0x10,0,nullptr,&stream);
            check(stream.cursor==stream.bits.size(),"original dynamic decoder consumes exact production record");
            const auto prior=sink;
            apply(sink.data(),overrides.data());
            check(read<std::uint8_t>(sink.data(),0x2C0)==(s.chargeEnabled?0:1),"native handler unlocks route and carrying, locks dormant and retired sink");
            check(read<std::uint8_t>(sink.data(),0x280)==1,"native handler invalidates prompt eligibility cache");
            check(std::memcmp(sink.data()+0x2D0,prior.data()+0x2D0,0x24)==0,"unlock never submits or consumes use or changes player association");
            const auto applied=sink;apply(sink.data(),overrides.data());
            check(sink==applied,"repeated same native override is idempotent");
        }
    }
}
