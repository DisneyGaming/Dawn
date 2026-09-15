#include "persistence.h"
#include "../runtime/state_account_transaction_helpers.h"
#include "../../core/filesystem/path.h"
#include <Windows.h>
#include <vector>
#include <memory>
#include <bit>
#include <cwchar>

namespace sunrise::state::vendors::persistence {
namespace {
core::path::Buffer path;
std::uint64_t owner{};
bool enabled{};
std::uint64_t checksum(std::span<const std::byte> bytes) noexcept {
    auto hash=UINT64_C(14695981039346656037);
    for(const auto byte:bytes) {hash=(hash^std::to_integer<unsigned char>(byte))*UINT64_C(1099511628211);}return hash;
}
struct Codec {
    std::vector<std::byte>& bytes;bool reading{};std::size_t pos{};std::uint32_t version{3};
    template<class T> bool field(T& v) {
        using U=std::make_unsigned_t<T>;U value=static_cast<U>(v);
        if(reading) {value=0;if(pos>bytes.size() || sizeof(T)>bytes.size()-pos) {return false;}}
        for(std::size_t i=0;i<sizeof(T);++i) {
            if(reading) {value|=static_cast<U>(std::to_integer<unsigned char>(bytes[pos++]))<<(i*8);}
            else {bytes.push_back(static_cast<std::byte>((value>>(i*8))&255));++pos;}
        }
        if(reading) {v=static_cast<T>(value);}return true;
    }
    bool item(account::inventory::Item& item) {
        if(!field(item.instanceSoid) || !field(item.definitionHash) || !field(item.quantity)
            || !field(item.level) || !field(item.mutationSerial) || !field(item.flags)) {return false;}
        auto policy=static_cast<std::uint8_t>(item.sockets.policy);auto count=static_cast<std::uint8_t>(item.sockets.plugCount);
        if(!field(policy) || policy>1 || !field(count) || count>item.sockets.plugs.size()) {return false;}
        item.sockets.policy=static_cast<account::inventory::SocketPolicy>(policy);item.sockets.plugCount=count;
        for(auto& plug:item.sockets.plugs) {
            std::uint8_t present=plug.has_value();std::uint32_t hash=plug.value_or(0);
            if(!field(present) || present>1 || (present && !field(hash))) {return false;}
            if(present) {plug=hash;}else {plug.reset();}
        }
        if(version>=3) {
            std::uint8_t recovery=item.postmaster;
            if(!field(recovery) || recovery>1) {return false;}item.postmaster=recovery!=0;
        } else if(reading) {item.postmaster=false;}
        return account::inventory::valid(item);
    }
    bool ranks(ProgressBank& bank,std::uint8_t scope) {
        for(std::size_t i=0;i<bank.size();++i) {
            auto& row=bank[i];
            if(!field(row.vendor) || !field(row.points) || !field(row.rewards)) {return false;}
            if(row.vendor==0xffff) {if(row.points || row.rewards) {return false;}continue;}
            const auto* f=faction(row.vendor);
            if(!f || f->scope!=scope || row.points<0 || row.rewards<0 || row.rewards>row.points/f->rankStep) {return false;}
            for(std::size_t j=0;j<i;++j) if(bank[j].vendor==row.vendor) {return false;}
        }
        return true;
    }
    bool unlocks(Unlocks& state) {
        for(int numeric=0;numeric<2;++numeric) {
            auto& rows=numeric?state.values:state.flags;
            auto count=static_cast<std::uint16_t>(rows.size());
            if(rows.size()>kUnlockLimit || !field(count) || count>kUnlockLimit) {return false;}
            if(reading) {rows.resize(count);}
            for(std::size_t i=0;i<count;++i) {
                auto& row=rows[i];
                if(!field(row.slot) || !field(row.value) || row.slot>=(numeric?15500:23500)
                    || (!numeric && row.value!=1 && row.value!=2)) {return false;}
                for(std::size_t j=0;j<i;++j) if(rows[j].slot==row.slot) {return false;}
            }
        }
        return true;
    }
    bool inventory(AccountState& a) {
        std::uint32_t magic=0x56454E44U;auto soid=a.primarySoid;
        if(!field(magic) || magic!=0x56454E44U || !field(version) || version<1 || version>3 || !field(soid) || soid!=a.primarySoid) {return false;}
        auto count=static_cast<std::uint16_t>(a.profileItemCount);
        if(!field(count) || count>a.profileItems.size()) {return false;}
        if(reading) {a.profileItems={};a.profileItemCount=count;}
        for(std::size_t i=0;i<count;++i) {
            auto& item=a.profileItems[i];
            if(!field(item.instanceSoid) || !field(item.definitionHash) || !field(item.quantity) || !field(item.mutationSerial)) {return false;}
        }
        if(!ranks(a.vendorProgress,0)) {return false;}
        auto characters=static_cast<std::uint8_t>(a.characterCount);
        if(!field(characters) || characters!=a.characterCount) {return false;}
        for(std::size_t i=0;i<characters;++i) {
            auto& c=a.characters[i];auto id=c.soid;
            if(!field(id) || id!=c.soid || !field(c.nextInventorySerial)) {return false;}
            for(auto& slot:c.equipment.slots) {
                std::uint8_t present=slot.has_value();if(!field(present) || present>1) {return false;}
                if(!present) {slot.reset();continue;}if(!slot) {slot.emplace();}
                if(!item(*slot)) {return false;}
            }
            auto count=static_cast<std::uint16_t>(c.inventory.count);
            if(!field(count) || count>c.inventory.values.size()) {return false;}
            if(reading) {c.inventory={};c.inventory.count=count;}
            for(std::size_t n=0;n<count;++n) if(!item(c.inventory.values[n])) {return false;}
            if(!field(c.vendorCampaigns) || c.vendorCampaigns>7 || !ranks(c.vendorProgress,1)) {return false;}
        }
        if(version>=2) {
            if(!unlocks(a.vendorUnlocks)) {return false;}
            for(std::size_t i=0;i<characters;++i) if(!unlocks(a.characters[i].vendorUnlocks)) {return false;}
        } else {
            a.vendorUnlocks={};
            for(std::size_t i=0;i<characters;++i) {a.characters[i].vendorUnlocks={};}
        }
        return account::valid(a) && runtime::detail::valid_profile_inventory(a);
    }
};
}
bool active() noexcept {return enabled;}
bool restore(void* module,AccountState& account) noexcept {
    enabled=false;path={};owner=account.primarySoid;
    if(!module) {return true;}
    wchar_t name[80]{};std::swprintf(name,std::size(name),L"\\Sunrise\\vendor-inventory-%016llX.bin",static_cast<unsigned long long>(owner));
    if(!core::path::module_directory(module,path) || !core::path::append(path,name)) {return false;}
    const auto file=CreateFileW(path.chars.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {return GetLastError()==ERROR_FILE_NOT_FOUND;}
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size) && size.QuadPart>=24 && size.QuadPart<=262144;
    std::vector<std::byte> bytes(ok?static_cast<std::size_t>(size.QuadPart):0);DWORD read{};
    if(ok) {ok=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size();}
    CloseHandle(file);if(!ok) {return false;}
    std::uint64_t stored{};Codec tail{bytes,true,bytes.size()-8};
    if(!tail.field(stored) || stored!=checksum(std::span(bytes).first(bytes.size()-8))) {return false;}
    bytes.resize(bytes.size()-8);auto candidate=std::make_unique<AccountState>(account);Codec codec{bytes,true};
    if(!codec.inventory(*candidate) || codec.pos!=bytes.size()) {return false;}
    middleware::datagen::family4::loadout::ResolvedLoadout resolved{};
    // The native loadout resolver requires exactly one selected character. Check
    // every character's buckets without changing the selection saved by boot.
    for(std::size_t i=0;i<candidate->characterCount;++i) {
        for(std::size_t j=0;j<candidate->characterCount;++j) {candidate->characters[j].selected=j==i;}
        if(!middleware::datagen::family4::loadout::resolve(*candidate,i,resolved)) {return false;}
    }
    for(std::size_t i=0;i<candidate->characterCount;++i) {candidate->characters[i].selected=account.characters[i].selected;}
    account=*candidate;enabled=true;return true;
}
bool save(const AccountState& account,bool activate) noexcept {
    if(!enabled && !activate) {return true;}
    if(!path.length) {return true;} // Offline fixtures can operate without a module directory.
    if(account.primarySoid!=owner) {return false;}
    auto candidate=std::make_unique<AccountState>(account);std::vector<std::byte> bytes;Codec codec{bytes,false};
    if(!codec.inventory(*candidate)) {return false;}auto hash=checksum(bytes);if(!codec.field(hash)) {return false;}
    auto temporary=path;static unsigned sequence{};wchar_t suffix[64]{};
    std::swprintf(suffix,std::size(suffix),L".%08X.%08X.%08X.tmp",GetCurrentProcessId(),GetCurrentThreadId(),++sequence);
    if(!core::path::append(temporary,suffix)) {return false;}
    const auto file=CreateFileW(temporary.chars.data(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {return false;}
    DWORD written{};bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)
        && written==bytes.size() && FlushFileBuffers(file);CloseHandle(file);
    if(ok) {ok=MoveFileExW(temporary.chars.data(),path.chars.data(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
    if(!ok) {DeleteFileW(temporary.chars.data());return false;}enabled=true;return true;
}
}
