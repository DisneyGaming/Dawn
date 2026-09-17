#include "../src/client/hooks/bootflow/native_property_list_guard.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
namespace p=dawn::client::hooks::bootflow::native_property_list;
unsigned checks{};
void check(bool yes,const char* why) {++checks;if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Segment {std::uintptr_t address{};std::vector<std::byte> bytes;};
struct Memory {
    std::vector<Segment> segments;
    std::uintptr_t deny{};
    static bool read(void* context,std::uintptr_t address,std::span<std::byte> out) noexcept {
        auto& m=*static_cast<Memory*>(context);
        if(m.deny && address<=m.deny && m.deny-address<out.size())return false;
        for(auto& s:m.segments)if(address>=s.address && address-s.address<=s.bytes.size()
            && out.size()<=s.bytes.size()-(address-s.address)) {
            std::memcpy(out.data(),s.bytes.data()+(address-s.address),out.size());return true;
        }
        return false;
    }
    p::Source source(){return {0,this,&read};}
    template<class T>T get(std::uintptr_t address) {T value{};check(p::read(source(),address,value),"read fixture");return value;}
    template<class T>void put(std::uintptr_t address,const T& value) {
        for(auto& s:segments)if(address>=s.address && address-s.address+sizeof(T)<=s.bytes.size()) {
            std::memcpy(s.bytes.data()+(address-s.address),&value,sizeof value);return;
        }
        check(false,"write fixture range");
    }
    bool erase(std::uintptr_t list,int i,int count) {
        for(int j=i;j<count-1;++j)put(list+4+std::uintptr_t(j)*16,get<p::Group>(list+4+std::uintptr_t(j+1)*16));
        put(list,std::int32_t(count-1));return true;
    }
};
constexpr std::uintptr_t list=0x10000,meta=0x11000,header=0x11100,bitmap=0x11200,bits=0x11300,data=0x12000;
Memory fixture(std::uint16_t capacity=128) {
    Memory m; m.segments.push_back({list,std::vector<std::byte>(0x130000)});
    m.put(list+0x808,meta);m.put(meta,header);m.put(meta+8,data);
    m.put(meta+0x1C,0x80U);m.put(meta+0x20,0x88U);m.put(meta+0x24,UINT32_MAX);m.put(meta+0x34,915U);
    m.put(header+0x1C,capacity);m.put(header+8,bitmap);
    m.put(bitmap+8,(std::uint32_t(capacity)+31U)&~31U);m.put(bitmap+0x10,bits);
    return m;
}
p::Pool owner(Memory& m) {p::Pool pool{};check(p::pool(m.source(),list,pool),"pool layout");return pool;}
std::uint32_t datum(Memory& m,unsigned index,std::uint32_t serial=5) {return p::handle(owner(m),serial,index);}
void node(Memory& m,unsigned i,std::uint32_t key,std::uint32_t prev,std::uint32_t next,std::uint32_t serial=5) {
    m.put(bits+(i/32)*4,m.get<std::uint32_t>(bits+(i/32)*4)|(1U<<(i%32)));
    m.put(data+i*0x88,key);m.put(data+i*0x88+0x74,prev);m.put(data+i*0x88+0x78,next);m.put(data+i*0x88+0x80,serial);
}
void groups(Memory& m,std::initializer_list<p::Group> values) {
    m.put(list,static_cast<std::int32_t>(values.size()));unsigned i{};
    for(auto g:values)m.put(list+4+16*i++,g);
}
struct Outcome {std::vector<std::uint32_t> updated;std::vector<std::string> reports;};
Outcome run(Memory& m,std::uintptr_t address=list,bool removal=true) {
    Outcome o;
    p::run(m.source(),address,removal,
        [&](std::uintptr_t row){o.updated.push_back(m.get<p::Group>(row).key);},
        [&](int i,int count){return m.erase(address,i,count);},
        [&](const char* result,std::uint32_t){o.reports.emplace_back(result);});return o;
}
void synthetic() {
    for(unsigned n: {1U,2U,3U,127U,128U,1500U,8192U}) {
        auto m=fixture(static_cast<std::uint16_t>(n));
        for(unsigned i=0;i<n;++i)node(m,i,42,i?datum(m,i-1):p::kAbsent,i+1<n?datum(m,i+1):p::kAbsent);
        groups(m,{{42,datum(m,0),datum(m,n-1),7}});
        const auto before=m.segments[0].bytes;auto result=run(m);
        check(result.updated==std::vector<std::uint32_t>{42},"valid native group forwarded exactly once");
        check(result.reports.empty() && m.segments[0].bytes==before,"valid native memory untouched");
    }
    {auto m=fixture();groups(m,{{1,datum(m,2),datum(m,3),0},{2,datum(m,4),datum(m,4),0},{3,datum(m,6),datum(m,8),0}});
        node(m,4,2,p::kAbsent,p::kAbsent);auto o=run(m);
        check(m.get<int>(list)==1 && m.get<p::Group>(list+4).key==2,"retired rows compact around survivor");
        check(o.updated==std::vector<std::uint32_t>{2},"survivor forwarded once");}
    for(unsigned kind=0;kind<7;++kind) {
        auto m=fixture();auto a=datum(m,1),b=datum(m,2);
        node(m,1,11,p::kAbsent,b);node(m,2,11,a,p::kAbsent);groups(m,{{11,a,b,0}});
        if(kind==0)m.put(data+2*0x88+0x78,a); // cycle
        if(kind==1)m.put(data+0x88+0x74,a); // self previous
        if(kind==2)m.put(data+0x88+0x80,6U); // reused salt
        if(kind==3)m.put(list+8,0U); // zero is not the native end sentinel
        if(kind==4)m.put(list+12,a); // wrong tail
        if(kind==5)m.put(data+2*0x88,12U); // foreign key
        if(kind==6)m.deny=data+0x88+0x78; // unreadable link
        const auto before=m.segments[0].bytes;auto o=run(m);
        check(o.updated.empty() && m.get<int>(list)==1,"damaged live group blocked, not erased");
        check(m.segments[0].bytes==before,"no live-slot mutation on damaged list");
    }
    {auto m=fixture();groups(m,{{1,datum(m,1),datum(m,1),0},{2,datum(m,2),datum(m,2),0}});
        node(m,1,1,datum(m,1),datum(m,1));node(m,2,2,p::kAbsent,p::kAbsent);
        auto o=run(m);check(o.updated==std::vector<std::uint32_t>{2},"bad group does not block independent valid group");}
    {auto m=fixture();groups(m,{{1,datum(m,1),datum(m,1),0}});m.deny=bits;
        auto o=run(m);check(m.get<int>(list)==1 && o.updated.empty(),"incomplete allocation evidence never erases");}
    {auto m=fixture();groups(m,{{1,datum(m,1),datum(m,1),0}});auto o=run(m,list,false);
        check(m.get<int>(list)==1 && o.updated.empty(),"quiesce does not mutate or traverse retired group");}
    {auto m=fixture();m.put(list,129);auto o=run(m);check(o.reports[0]=="invalid_count","oversized native count rejected");}
    {auto m=fixture();groups(m,{{1,p::kAbsent,p::kAbsent,0}});auto o=run(m);check(o.updated.size()==1,"empty sentinel valid");}
    {auto m=fixture();groups(m,{{1,datum(m,1),datum(m,1),0}});m.put(meta+0x20,0x90U);
        auto o=run(m);check(o.reports[0]=="unreadable_pool" && m.get<int>(list)==1,"wrong allocator shape unchanged");}
    {auto m=fixture();m.put(meta+0x34,0x40003F93U);auto a=datum(m,1,6);node(m,1,1,p::kAbsent,p::kAbsent,6);
        groups(m,{{1,a,a,0}});check(run(m).updated.size()==1,"extended native handle encoding");}
}
template<class T>T input(std::ifstream& f){T value{};f.read(reinterpret_cast<char*>(&value),sizeof value);check(bool(f),"fixture input");return value;}
void captured(const char* path,const char* output) {
    std::ifstream f(path,std::ios::binary);check(bool(f),"open captured fixture");
    check(input<std::uint32_t>(f)==0x504C4731,"fixture magic");const auto address=input<std::uint64_t>(f);
    const auto n=input<std::uint32_t>(f);check(n>0 && n<32,"fixture segment bounds");Memory m;
    for(unsigned i=0;i<n;++i) {Segment s;s.address=input<std::uint64_t>(f);const auto length=input<std::uint32_t>(f);
        check(length<0x200000,"fixture length");s.bytes.resize(length);f.read(reinterpret_cast<char*>(s.bytes.data()),length);check(bool(f),"fixture bytes");m.segments.push_back(std::move(s));}
    check(m.get<int>(address)==3,"dump has three stale groups");auto o=run(m,address);
    check(o.updated.empty() && o.reports.size()==3 && m.get<int>(address)==0,"captured stale groups removed");
    for(const auto& report:o.reports)check(report=="retired_group_removed","captured repair reason");
    // Export the repaired descriptor prefix for native replay, not pool contents.
    std::ofstream out(output,std::ios::binary);std::array<std::byte,0x804> bytes{};
    check(Memory::read(&m,address,bytes),"repaired prefix");out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(out),"repaired prefix written");
}
int main(int argc,char** argv) {synthetic();if(argc==3)captured(argv[1],argv[2]);check(argc==1 || argc==3,"arguments");
    std::printf("native property list: %u checks passed%s\n",checks,argc==3?" including captured hang":"");}
