#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace dawn::client::hooks::bootflow::native_generated_population_identity {

inline constexpr std::uint32_t kSourceRuntimeClass = 0x8080501DU;
inline constexpr std::uint32_t kSourceDefinitionClass = 0x8080501CU;
inline constexpr std::uint32_t kWorkerReferenceClass = 0x80805017U;
inline constexpr std::uint32_t kWorkerRuntimeClass = 0x80804FECU;
// Native tick stores (authority seed & +0x948) | +0x94C at +0x940.
// +0x94C alone is the override operand and is normally zero.
inline constexpr std::int64_t kWorkerEffectiveSeedOffset = 0x940LL;
inline constexpr std::uint32_t kPaletteRowReferenceClass = 0x8080502DU;
inline constexpr std::uint32_t kPaletteArrayElementClass = 0x8080502EU;
inline constexpr std::uint32_t kMemberReferenceClass = 0x808099D8U;
inline constexpr std::uint32_t kArrayHeaderMarker = 0x80809FBDU;
inline constexpr std::uint32_t kInvalidHandle = UINT32_MAX;
inline constexpr std::size_t kEntryStride = 0x38U;
inline constexpr std::size_t kActorRowStride = 0xC0U;
inline constexpr std::size_t kPoolRowIdentityBytes = 0x18U;
inline constexpr std::size_t kPoolRowIdentityOffset = 0x18U;
inline constexpr std::size_t kPoolRowReadBytes = kPoolRowIdentityOffset + kPoolRowIdentityBytes;
inline constexpr std::size_t kMaximumBitmapBytes = 1024U;
inline constexpr std::size_t kMaximumPoolBytes = 1024U * 1024U;
inline constexpr std::int64_t kMaximumResourceOffset = 0x100000LL;
inline constexpr std::int64_t kActorRowReferenceBaseOffset = 0xD8LL;
inline constexpr std::int64_t kActorRowReferenceStride = 0x30LL;

/** The native identity needed by the generated-population mailbox lookup. */
struct Identity final {
    std::uint32_t resourceTag{};
    std::uint32_t seed{};
    std::uint32_t workerDefinitionTag{};
    std::uint32_t workerDefinitionOffset{};
    std::uint32_t paletteDefinitionTag{};
    std::uint32_t paletteDefinitionOffset{};
    std::uint32_t actorRowsOffset{};
    std::uint32_t actorRowCount{};
    std::uint32_t memberPrefabTag{};
    std::uint32_t completionGroup{UINT32_MAX};
};

template<class T>
[[nodiscard]] inline T field(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};
    if(offset<=bytes.size() && sizeof(T)<=bytes.size()-offset)
        std::memcpy(&value,bytes.data()+offset,sizeof(T));
    return value;
}

template<class Reference>
[[nodiscard]] inline bool same_reference(const Reference& left,const Reference& right) noexcept {
    return left.handle==right.handle && left.kind==right.kind && left.offset==right.offset;
}

template<class Reader>
[[nodiscard]] inline bool add(std::uintptr_t base,std::int64_t offset,std::uintptr_t& output) noexcept {
    if(base<0x10000) return false;
    if(offset>=0) {
        const auto amount=static_cast<std::uint64_t>(offset);
        if(amount>UINTPTR_MAX-base) return false;
        output=base+static_cast<std::uintptr_t>(amount);
    } else {
        const auto amount=static_cast<std::uint64_t>(-(offset+1))+1U;
        if(amount>base) return false;
        output=base-static_cast<std::uintptr_t>(amount);
    }
    return output>=0x10000;
}

template<class Reader,class Reference>
[[nodiscard]] inline bool resolve_handle(Reader& read,std::uint32_t handle,
    std::uintptr_t& output) noexcept {
    if(handle==kInvalidHandle) return false;
    return read.resolve(Reference{handle,0,0},output);
}

/**
 * Qualifies one actor's generated Forest identity using only native reads.
 *
 * `actorSource` is the source backlink copied from the actor record and
 * `member` is its member backlink. The returned identity contains no lease;
 * the caller performs the mailbox lookup after this exact native identity has
 * been established. The Reader must expose the existing `copy`, `value` and
 * `resolve(Reference, uintptr_t&)` read-only operations.
 */
template<class Reader,class Reference>
[[nodiscard]] inline bool qualify(Reader& read,std::uintptr_t image,
    const Reference& actorSource,const Reference& member,Identity& output) noexcept {
    output={};
    output.completionGroup=kInvalidHandle;
    if(image<0x10000 || actorSource.handle==kInvalidHandle
        || actorSource.kind!=kSourceRuntimeClass || actorSource.offset<0
        || actorSource.offset>=kMaximumResourceOffset || (actorSource.offset&3)!=0) return false;

    std::uintptr_t sourceAddress{};
    if(!read.resolve(actorSource,sourceAddress) || sourceAddress>UINTPTR_MAX-0x150U)
        return false;
    std::array<std::byte,0x150> source{};
    if(!read.copy(sourceAddress,source)) return false;

    // The source object begins with a runtime header reference. Its kind is
    // also the header's +4 field; the definition kind is only valid after the
    // reference has been resolved and its definition header copied.
    const auto paletteDefinition=field<Reference>(source,0);
    if(paletteDefinition.handle==kInvalidHandle
        || paletteDefinition.kind!=kSourceRuntimeClass
        || field<std::uint32_t>(source,4)!=kSourceRuntimeClass
        || paletteDefinition.offset<=0 || paletteDefinition.offset>=kMaximumResourceOffset
        || (paletteDefinition.offset&3)!=0) return false;
    std::uintptr_t definitionAddress{};
    if(!read.resolve(paletteDefinition,definitionAddress)
        || definitionAddress>UINTPTR_MAX-0x120U) return false;

    const auto workerHandle=field<std::uint32_t>(source,0x148);
    const auto entryIndex=field<std::int32_t>(source,0x14C);
    std::uintptr_t workerAddress{};
    if(!resolve_handle<Reader,Reference>(read,workerHandle,workerAddress)
        || workerAddress>UINTPTR_MAX-0x28U) return false;
    std::array<std::byte,0x28> workerHeader{};
    if(!read.copy(workerAddress,workerHeader)
        || field<std::uint32_t>(workerHeader,4)!=kWorkerRuntimeClass
        || field<std::uint32_t>(workerHeader,0x24)!=workerHandle) return false;
    const auto workerDefinition=field<Reference>(workerHeader,0);
    if(workerDefinition.handle==kInvalidHandle || workerDefinition.kind!=kWorkerRuntimeClass
        || workerDefinition.offset<=0 || workerDefinition.offset>=kMaximumResourceOffset
        || (workerDefinition.offset&3)!=0) return false;
    std::uintptr_t workerDefinitionAddress{};
    if(!read.resolve(workerDefinition,workerDefinitionAddress)
        || workerDefinitionAddress>UINTPTR_MAX-0x28U) return false;
    std::array<std::byte,0x28> workerDefinitionHeader{};
    if(!read.copy(workerDefinitionAddress,workerDefinitionHeader)
        || field<std::uint32_t>(workerDefinitionHeader,4)!=kWorkerReferenceClass
        || field<std::uint32_t>(workerDefinitionHeader,0)!=workerDefinition.handle) return false;

    std::uint32_t seed{},resourceTag{};
    std::int32_t entryCount{};
    std::int64_t entriesRelative{};
    if(workerAddress>UINTPTR_MAX-0x96CU
        || !read.value(workerAddress+kWorkerEffectiveSeedOffset,seed) || !read.value(workerAddress+0x96C,resourceTag)
        || !read.value(workerAddress+0x850,entryCount)
        || !read.value(workerAddress+0x858,entriesRelative)
        || !seed || seed==kInvalidHandle || !resourceTag || resourceTag==kInvalidHandle
        || entryCount<1 || entryCount>4096 || entryIndex<0
        || entryIndex>=entryCount) return false;

    std::uintptr_t entries{};
    if(!add<Reader>(workerAddress,entriesRelative,entries)
        || !add<Reader>(entries,0x868,entries)) return false;
    std::uintptr_t entryAddress{};
    if(!add<Reader>(entries,static_cast<std::int64_t>(entryIndex)*kEntryStride,entryAddress)
        || entryAddress>UINTPTR_MAX-kEntryStride) return false;
    std::array<std::byte,kEntryStride> entry{};
    if(!read.copy(entryAddress,entry) || field<std::uint8_t>(entry,0x18)!=0) return false;
    const auto encounterHandle=field<std::uint32_t>(entry,0x34);
    if(encounterHandle==kInvalidHandle) return false;

    std::uintptr_t encounterAddress{};
    if(!resolve_handle<Reader,Reference>(read,encounterHandle,encounterAddress)
        || encounterAddress>UINTPTR_MAX-9U) return false;
    std::int8_t selector{};
    if(!read.value(encounterAddress+8,selector) || selector<0 || selector>=64) return false;

    std::uintptr_t selectorAddress{};
    if(!add<Reader>(image,0x1F921B0LL+static_cast<std::int64_t>(selector)*8,selectorAddress)) return false;
    std::uintptr_t pool{};
    if(!read.value(selectorAddress,pool) || pool<0x10000 || pool>UINTPTR_MAX-0x18U) return false;
    std::array<std::byte,0x18> poolHeader{};
    if(!read.copy(pool,poolHeader)) return false;
    const auto poolBase=field<std::uintptr_t>(poolHeader,8);
    const auto stride=field<std::int32_t>(poolHeader,0x10);
    const auto count=field<std::int32_t>(poolHeader,0x14);
    const auto poolByteCount=static_cast<std::uint64_t>(count)*static_cast<std::uint64_t>(stride);
    if(poolBase<0x10000 || stride<0x30 || stride>0x1000 || count<1 || count>8192
        || poolByteCount>kMaximumPoolBytes
        || poolByteCount>static_cast<std::uint64_t>(UINTPTR_MAX)-poolBase)
        return false;

    std::uintptr_t bitmapLink{},bitmapOwner{},bitmap{};
    if(!read.value(pool,bitmapLink) || bitmapLink<0x10000
        || !add<Reader>(bitmapLink,8,bitmapOwner) || !read.value(bitmapOwner,bitmap)
        || bitmap<0x10000 || bitmap>UINTPTR_MAX-0x10U
        || !read.value(bitmap+0x10,bitmap)) return false;
    const auto bitmapBytes=(static_cast<std::size_t>(count)+7U)/8U;
    if(bitmapBytes==0 || bitmapBytes>kMaximumBitmapBytes
        || bitmap>UINTPTR_MAX-static_cast<std::uintptr_t>(bitmapBytes)) return false;
    std::array<std::byte,kMaximumBitmapBytes> occupied{};
    if(!read.copy(bitmap,std::span{occupied}.first(bitmapBytes))) return false;

    Reference matched{};bool found{};
    for(std::int32_t rowIndex=0;rowIndex<count;++rowIndex) {
        const auto byteIndex=static_cast<std::size_t>(rowIndex)/8U;
        const auto bitIndex=static_cast<unsigned>(rowIndex)&7U;
        if((std::to_integer<std::uint8_t>(occupied[byteIndex])&(1U<<bitIndex))==0) continue;
        std::uintptr_t row{};
        if(!add<Reader>(poolBase,static_cast<std::int64_t>(rowIndex)*stride,row)
            || row>UINTPTR_MAX-kPoolRowReadBytes) return false;
        std::array<std::byte,kPoolRowIdentityBytes> rowIdentity{};
        if(!read.copy(row+kPoolRowIdentityOffset,rowIdentity)) return false;
        if(field<std::uint32_t>(rowIdentity,0)!=encounterHandle) continue;
        const auto candidate=field<Reference>(rowIdentity,8);
        if(found || !same_reference(candidate,actorSource)) return false;
        matched=candidate;found=true;
    }
    if(!found || !same_reference(matched,actorSource)) return false;

    std::array<std::byte,0x120> definition{};
    if(!read.copy(definitionAddress,definition)) return false;
    if(field<std::uint32_t>(definition,0)!=paletteDefinition.handle
        || field<std::uint32_t>(definition,4)!=kSourceDefinitionClass
        || field<std::int64_t>(definition,0x110)<1
        || field<std::int64_t>(definition,0x110)>512) return false;
    const auto actorRowCount=field<std::int64_t>(definition,0x110);
    const auto actorRowsRelative=field<std::int64_t>(definition,0x118);
    const auto sourceActorRowCount=field<std::int64_t>(source,0xC0);
    const auto sourceActorRowsRelative=field<std::int64_t>(source,0xC8);
    const auto definitionRuntimeOffset=field<std::int64_t>(definition,8);
    if(sourceActorRowCount!=actorRowCount || sourceActorRowsRelative<0
        || sourceActorRowsRelative>=kMaximumResourceOffset || definitionRuntimeOffset<0
        || definitionRuntimeOffset>=kMaximumResourceOffset) return false;
    if(actorRowsRelative<0 || actorRowsRelative>=kMaximumResourceOffset
        || paletteDefinition.offset>kMaximumResourceOffset-0x128LL-actorRowsRelative) return false;
    const auto firstRowsOffset=paletteDefinition.offset+actorRowsRelative+0x128LL;
    if(firstRowsOffset<=0 || firstRowsOffset>=kMaximumResourceOffset
        || firstRowsOffset>static_cast<std::int64_t>(UINT32_MAX)) return false;
    std::uintptr_t arrayHeaderAddress{};
    if(!add<Reader>(definitionAddress,actorRowsRelative,arrayHeaderAddress)
        || !add<Reader>(arrayHeaderAddress,0x114,arrayHeaderAddress)
        || arrayHeaderAddress>UINTPTR_MAX-20U) return false;
    std::array<std::byte,20> arrayHeader{};
    if(!read.copy(arrayHeaderAddress,arrayHeader)
        || field<std::uint32_t>(arrayHeader,0)!=kArrayHeaderMarker
        || field<std::uint64_t>(arrayHeader,4)!=static_cast<std::uint64_t>(actorRowCount)
        || field<std::uint32_t>(arrayHeader,12)!=kPaletteArrayElementClass) return false;
    std::uintptr_t firstRows{};
    if(!add<Reader>(arrayHeaderAddress,0x14,firstRows)) return false;

    if(member.handle!=paletteDefinition.handle || member.kind!=kMemberReferenceClass
        || member.offset<=0 || member.offset>=kMaximumResourceOffset
        || firstRowsOffset>kMaximumResourceOffset-0x10LL
        || member.offset<firstRowsOffset+0x10LL) return false;
    const auto memberRowOffset=member.offset-0x10;
    const auto rowDelta=memberRowOffset-firstRowsOffset;
    if(rowDelta<0 || rowDelta%static_cast<std::int64_t>(kActorRowStride)!=0
        || rowDelta/static_cast<std::int64_t>(kActorRowStride)>=actorRowCount) return false;
    std::uintptr_t selectedRow{};
    if(!add<Reader>(firstRows,rowDelta,selectedRow)
        || selectedRow>UINTPTR_MAX-kActorRowStride) return false;
    std::array<std::byte,kActorRowStride> actorRow{};
    if(!read.copy(selectedRow,actorRow)) return false;
    const auto rowReference=field<Reference>(actorRow,0);
    const auto rowIndex=rowDelta/static_cast<std::int64_t>(kActorRowStride);
    const auto definitionRuntimeOffsetUnsigned=static_cast<std::uint64_t>(definitionRuntimeOffset);
    const auto sourceActorRowsRelativeUnsigned=static_cast<std::uint64_t>(sourceActorRowsRelative);
    const auto rowIndexUnsigned=static_cast<std::uint64_t>(rowIndex);
    constexpr auto maximumOffset=static_cast<std::uint64_t>(kMaximumResourceOffset);
    constexpr auto rowReferenceBase=static_cast<std::uint64_t>(kActorRowReferenceBaseOffset);
    constexpr auto rowReferenceStride=static_cast<std::uint64_t>(kActorRowReferenceStride);
    if(definitionRuntimeOffsetUnsigned>maximumOffset
        || sourceActorRowsRelativeUnsigned>maximumOffset-definitionRuntimeOffsetUnsigned
        || definitionRuntimeOffsetUnsigned+sourceActorRowsRelativeUnsigned
            >maximumOffset-rowReferenceBase) return false;
    const auto rowReferencePrefix=definitionRuntimeOffsetUnsigned+sourceActorRowsRelativeUnsigned
        +rowReferenceBase;
    if(rowIndexUnsigned>(maximumOffset-rowReferencePrefix)/rowReferenceStride) return false;
    const auto rowReferenceOffset=rowReferencePrefix+rowIndexUnsigned*rowReferenceStride;
    if(rowReferenceOffset==0 || rowReferenceOffset>=maximumOffset) return false;
    if(rowReference.kind!=kPaletteRowReferenceClass
        || rowReference.handle!=paletteDefinition.handle
        || rowReference.offset!=static_cast<std::int64_t>(rowReferenceOffset)) return false;

    output.resourceTag=resourceTag;
    output.seed=seed;
    output.workerDefinitionTag=workerDefinition.handle;
    output.workerDefinitionOffset=static_cast<std::uint32_t>(workerDefinition.offset);
    output.paletteDefinitionTag=paletteDefinition.handle;
    output.paletteDefinitionOffset=static_cast<std::uint32_t>(paletteDefinition.offset);
    output.actorRowsOffset=static_cast<std::uint32_t>(firstRowsOffset);
    output.actorRowCount=static_cast<std::uint32_t>(actorRowCount);
    output.memberPrefabTag=field<std::uint32_t>(actorRow,0x10);
    output.completionGroup=field<std::uint32_t>(actorRow,0xA8);
    return true;
}

} // namespace dawn::client::hooks::bootflow::native_generated_population_identity
