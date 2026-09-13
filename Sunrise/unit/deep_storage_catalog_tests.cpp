#include "../src/state/activity/deep_storage/authority.h"
#include "../src/state/activity/deep_storage/hologram_owner.h"
#include "../src/state/activity/deep_storage/mechanism_catalog.h"
#include "../src/middleware/encoding/bit_writer.h"
#include <cmath>
#include <cstdio>

namespace native = sunrise::state::activity::deep_storage;
namespace coo = sunrise::state::activity::coo;
using Writer = sunrise::middleware::encoding::bits::Writer;
#define CHECK(expression) do { if(!(expression)) {std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expression);return false;} } while(false)

// Independent package identities, rather than expectations derived from the encoder.
static_assert(native::kActivity==295 && native::kInvestment==0x550500EEU);
static_assert(native::kScenario==0x80B5606DU && native::kLaunchTag==0x80F9F35EU);
static_assert(native::kRoot==0xE6E910D2U && native::kBank==0x80F1EEB6U);
static_assert(native::kBubbleCount==22 && std::size(native::kGroups)==11);
static_assert(std::size(native::kAssets)==269 && std::size(native::kVolumes)==57);
static_assert(std::size(native::kSources)==72 && std::size(native::kSpawns)==72);
static_assert(std::size(native::kDialogue)==15 && std::size(native::kObjectives)==10);
static_assert(native::find(0xA13D8A45U,4,4)->asset.definition==0x80B560D5U);
static_assert(native::find(0x59700FA7U,1,52)->asset.definition==0x80B56421U);
static_assert(native::find(0x59700FA7U,1,52)->authority==0x80807EC9U);
static_assert(native::find(0xA13D8A45U,4,2)->authority==0x8080992FU);
static_assert(native::find(0x59700FA7U,23,38)->authority==0x80804F48U);
static_assert(native::find(0x4324A238U,65,0)->asset.definition==0x80B56174U);
static_assert(native::find(0xEA42F517U,65,0)->asset.definition==0x80B5654EU);
static_assert(native::find(0x59700FA7U,65,0)==nullptr);
static_assert(native::objective(0xF8F223A7U)!=native::objective(0x149B4756U));
static_assert(native::objective(0xF8F223A7U)->title==native::objective(0x149B4756U)->title);
static_assert(native::objective(0x772F4471U)->description=="Descend deeper into the Pyramidion.");
static_assert(native::objective(0xFFFFFFFFU)==nullptr);
static_assert(native::kPlateEntity==0x80C6BE61U && native::kPlateTimer==0x815B8B3BU);
static_assert(native::kPlateTimerOffset==0x248U && native::kPlateDeviceOffset==0xA78U);
static_assert(native::kGhostScans[0].component==0x8156EFA4U);
static_assert(native::kGhostScans[1].component==0x8157E6B1U);
static_assert(native::kScanOverrideClass==0x80804D39U && native::kScanOverrideClassOffset==0x614U && native::kScanOverrideDurationOffset==0x638U);
static_assert(native::kScans[0].guid==0xCDFC784B0C4A9CF5ULL);
static_assert(native::kScans[1].guid==0x844920F35A95B55BULL);

static bool catalog() {
    for(std::size_t i=0;i<std::size(native::kAssets);++i) {
        const auto& a=native::kAssets[i].asset;
        CHECK(native::find(a.registry,static_cast<std::uint8_t>(a.type),a.slot)==&native::kAssets[i]);
        for(std::size_t j=i+1;j<std::size(native::kAssets);++j) {
            const auto& b=native::kAssets[j].asset;
            CHECK(a.registry!=b.registry || a.type!=b.type || a.slot!=b.slot);
        }
    }
    unsigned pyramidion{};
    for(const auto& group:native::kGroups) {
        if(group.key==0x59700FA7U) {CHECK(group.bubble==19);++pyramidion;}
        for(const auto& slot:group.slots) {
            const auto* a=native::find(group.key,slot.type,slot.index);
            CHECK(a && a->asset.definition==slot.tag && a->offset==slot.offset);
            CHECK(a->component==slot.component && a->sense==slot.sense && a->authority==slot.auth);
        }
    }
    CHECK(pyramidion==1);
    unsigned finalRoom{};
    for(const auto& v:native::kVolumes) {
        CHECK(v.vertices.size()>=3 && v.min.x<v.max.x && v.min.y<v.max.y && v.min.z<v.max.z);
        double twiceArea{};
        for(std::size_t i=0;i<v.vertices.size();++i) {
            const auto& p=v.vertices[i];const auto& q=v.vertices[(i+1)%v.vertices.size()];
            CHECK(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
            CHECK(p.x>=v.min.x-.001F && p.x<=v.max.x+.001F && p.y>=v.min.y-.001F && p.y<=v.max.y+.001F);
            twiceArea+=double(p.x)*q.y-double(q.x)*p.y;
        }
        CHECK(std::abs(twiceArea)>.01);
        if(v.asset.registry==0xEA42F517U && v.asset.slot==5) {
            CHECK(v.name=="tv_final_room" && std::abs(v.min.z+1043.90002F)<.001F);++finalRoom;
        }
    }
    CHECK(finalRoom==1);
    CHECK(native::kVolumes[0].asset==native::kPlates[0].volume);
    CHECK(native::kVolumes[0].vertices.size()==10 && std::abs(native::kVolumes[0].min.x-1279.85376F)<.001F);
    for(const auto& plate:native::kPlates) {
        CHECK(native::find(plate.source.registry,4,plate.source.slot));
        bool found{};for(const auto& volume:native::kVolumes) {found|=volume.asset==plate.volume;}CHECK(found);
    }
    for(std::size_t i=0;i<2;++i) {
        const auto& a=native::kGhostScans[i];const auto& b=native::kScans[i];
        CHECK(a.source==b.source && a.sensor==b.link && a.component==b.controllerDefinition);
        CHECK(a.sourceDuration==(i==0?5.F:9.F) && b.sourceSeconds==a.sourceDuration);
        CHECK(a.componentOffset==0x358U && a.sensorOffset==0x258U && a.defaultDuration==3.F && b.defaultSeconds==3.F);
    }
    // A value of 1 has no supported warp-frame branch. Numeric ranges do not prove visual direction.
    CHECK(native::kDeviceRanges0[1].graph==0x80F27377U && native::kDeviceRanges0[1].offset==0x14F0U);
    CHECK(native::kDeviceRanges0[1].min<.1F && native::kDeviceRanges0[1].max>.1F);
    for(const auto& r:native::kDeviceRanges0) {CHECK(!(r.min<=1.F && r.max>=1.F));}
    for(const auto& d:native::kDeviceModes) {for(const auto& r:d.ranges) {CHECK(std::isfinite(r.min) && std::isfinite(r.max) && r.min<=r.max);}}
    const auto hologram=native::find(0x59700FA7U,23,92)->asset;
    CHECK(native::device_position(hologram,false)==0.F);
    const auto display=native::device_position(hologram,true);
    CHECK(display==.5F && native::kDeviceRanges16[2].min<display && native::kDeviceRanges16[2].max>display);
    CHECK(display<native::kDeviceRanges16[3].min);
    const coo::Asset wall{0xE6402111U,0x80B56A48U,23,0};
    CHECK(native::find(wall.registry,23,0)->asset==wall);
    CHECK(native::device_position(wall,false)==.2F && native::device_position(wall,true)==.2F);
    CHECK(native::kDeviceRanges5[2].graph==0x80F4BA2DU && native::kDeviceRanges5[2].min<.2F);
    for(const coo::Asset other:{coo::Asset{wall.registry+1,wall.definition,23,0},coo::Asset{wall.registry,wall.definition+1,23,0},
        coo::Asset{wall.registry,wall.definition,4,0},coo::Asset{wall.registry,wall.definition,23,1}}) {
        CHECK(native::device_position(other,false)==0.F && native::device_position(other,true)==1.F);
    }
    const coo::Asset pit{0x59700FA7U,0x80B566FDU,23,50};
    CHECK(native::find(pit.registry,23,50)->asset==pit);
    CHECK(native::find(pit.registry,4,49)->asset.definition==0x80B566FAU);
    CHECK(native::device_position(pit,true)==0.F && native::device_position(pit,false)==1.F);
    CHECK(native::kDeviceRanges10[0].graph==0x80F567A6U && native::kDeviceRanges10[0].offset==0x11F8U);
    CHECK(native::kDeviceRanges10[0].min<=0.F && native::kDeviceRanges10[0].max>=0.F);
    CHECK(native::kDeviceRanges10[2].offset==0x1470U && native::kDeviceRanges10[2].min<1.F && native::kDeviceRanges10[2].max>=1.F);
    // The inverse mapping belongs only to the complete authored asset identity.
    for(const coo::Asset other:{coo::Asset{pit.registry+1,pit.definition,23,50},coo::Asset{pit.registry,pit.definition+1,23,50},
        coo::Asset{pit.registry,pit.definition,4,50},coo::Asset{pit.registry,pit.definition,23,51}}) {
        CHECK(native::device_position(other,true)==1.F && native::device_position(other,false)==0.F);
    }
    CHECK(native::find(0x59700FA7U,23,88)->asset.definition==0x80B568F9U);
    CHECK(native::find(0x59700FA7U,4,76)->asset.definition==0x80B5689CU);
    CHECK(native::kDeviceModes[11].entity==0x80F4B0CFU);
    CHECK(native::kDeviceRanges11[0].graph==0x80BFC20EU && native::kDeviceRanges11[0].offset==0x13D0U);
    CHECK(native::kDeviceRanges11[0].min<=0.F && native::kDeviceRanges11[0].max>=0.F);
    CHECK(native::kDeviceRanges11[2].offset==0x14F8U && native::kDeviceRanges11[2].min<1.F);
    unsigned requestCapabilities{},encounterCompletion{};
    std::bitset<17> requestSlots;
    for(const auto& cap:native::kCapabilities) {
        if(cap.spec.operation==coo::Operation::population && cap.id.ends_with(".request")) {
            CHECK(cap.spec.asset.registry==0x59700FA7U && cap.spec.asset.type==1 && cap.spec.asset.slot>=1 && cap.spec.asset.slot<=17);
            CHECK(cap.spec.wait==coo::Wait::requested && cap.spec.argument==1);
            CHECK(!requestSlots[cap.spec.asset.slot-1]);requestSlots.set(cap.spec.asset.slot-1);++requestCapabilities;
        }
        if(cap.spec.operation==coo::Operation::population && cap.id.ends_with(".spawn")) {CHECK(cap.spec.wait==coo::Wait::nativeReady);}
        if(cap.id=="map.encounter.finish") {
            CHECK(cap.domain=="map_room" && cap.spec.asset==native::kModule && cap.spec.operation==coo::Operation::mechanic);
            CHECK(cap.spec.argument==12 && cap.spec.wait==coo::Wait::requested);++encounterCompletion;
        }
    }
    CHECK(requestCapabilities==17 && requestSlots.all() && encounterCompletion==1);
    // Source indices and per-category counts come from the recovered final-room rows.
    const std::uint16_t first[2][5]{{99,100,101,105,106},{102,103,104,107,108}};
    const std::uint16_t second[2][5]{{109,111,113,114,117},{110,112,115,116,118}};
    for(unsigned side=0;side<2;++side) {
        unsigned count1{},count2{};
        for(auto slot:first[side]) {const auto* p=native::spawn(0x59700FA7U,slot);CHECK(p && p->tactical.slot==97 && p->tactical.row==0);count1+=p->count;}
        for(auto slot:second[side]) {const auto* p=native::spawn(0x59700FA7U,slot);CHECK(p && p->tactical.slot==97 && p->tactical.row==0);count2+=p->count;}
        CHECK(count1==5 && count2==7);
    }
    unsigned twoCategories{},fallbacks{};
    for(std::size_t i=0;i<native::kSpawns.size();++i) {
        const auto& p=native::kSpawns[i];const auto& s=native::kSources[i];
        CHECK(p.registry==s.asset.registry && p.source==s.asset.slot && p.definition==s.asset.definition);
        CHECK(p.categories==s.categories && (p.categories==1 || p.categories==2));
        CHECK(p.count==p.categories); // Wave counts remain authored.
        CHECK(native::find(p.registry,1,p.source)->offset==p.offset && p.offset==0x728U);
        CHECK(native::find(p.registry,66,p.rule));
        CHECK(p.tactical.registry==p.registry && p.tactical.row>=0 && p.tactical.row<24);
        CHECK(native::find(p.tactical.registry,3,p.tactical.slot));
        if(s.hasRule) {CHECK(p.rule==s.rule);} else {++fallbacks;}
        if(p.categories==2) {++twoCategories;}
    }
    CHECK(twoCategories==4 && fallbacks==10);
    const auto* cyclops=native::spawn(0x59700FA7U,52);
    CHECK(cyclops && cyclops->rule==239 && cyclops->tactical.slot==51 && cyclops->tactical.row==0);
    return true;
}

static bool parity(const native::Frame& f,const coo::Asset a,std::size_t expected) {
    CHECK(native::body_bits(f,a.registry,static_cast<std::uint8_t>(a.type),a.slot)==expected);
    auto counter=Writer::measuring();
    CHECK(native::write_body(counter,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot)==(expected!=0));
    CHECK(counter.bit_count()==expected);
    if(!expected) {return true;}
    std::array<std::byte,4096> bytes{};
    Writer writer(std::span<std::byte>{bytes}.first((expected+7)/8));
    CHECK(native::write_body(writer,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot));
    std::size_t written{};CHECK(writer.finish(written));
    CHECK(writer.bit_count()==expected && written==(expected+7)/8);
    Writer shortWriter(std::span<std::byte>{bytes}.first((expected+7)/8-1));
    CHECK(!native::write_body(shortWriter,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot));
    return true;
}

static bool authority() {
    native::Frame f{};
    for(const auto& a:native::kAssets) {CHECK(parity(f,a.asset,0));}
    f.enabled=true;
    for(const auto& a:native::kAssets) {CHECK(parity(f,a.asset,0));}
    f.spawnGeneration=9;f.generations.fill(2);
    f.presentation={0x025CC54FU,0x772F4471U,3,native::marker(0xCB573BE1U),true,true};
    unsigned sources{},objects{},devices{},scans{},directives{},dialogues{};
    for(bool active:{false,true}) {
        for(auto& state:f.native) {state={7,true,true,true,active,false};}
        for(const auto& a:native::kAssets) {
            std::size_t expected{};
            switch(a.asset.type) {
            case 1: expected=native::kSpawns[native::spawn_index(a.asset)].categories==2?673U:641U;++sources;break;
            case 4: expected=252;for(const auto& plate:native::kPlates) {if(active && a.asset==plate.source) {expected+=709;}}++objects;break;
            case 23: expected=147;++devices;break;
            case 65: expected=65;++scans;break;
            case 68: expected=4802;++directives;break;
            case 53: expected=19767+64*f.generations.size();++dialogues;break;
            default:break;
            }
            CHECK(parity(f,a.asset,expected));
        }
    }
    CHECK(sources==144 && objects>0 && devices==36 && scans==4 && directives==2 && dialogues==2);
    for(std::uint8_t row=0;row<15;++row) {f.activeRow=row;CHECK(parity(f,native::kDialogueAsset,19767+64*f.generations.size()));}
    for(const auto& event:native::kObjectives) {
        f.presentation.event=event.event;f.presentation.marker=native::marker(event.event);
        CHECK(parity(f,{0xE6E910D2U,0x80B565DFU,68,0},4802));
    }
    // The native scan link uses scoped slot 0 independently in two registries.
    for(std::size_t i=0;i<2;++i) {
        for(unsigned mode=0;mode<4;++mode) {
            f.scanArmed[i]=mode!=0;f.scanComplete[i]=mode==2;f.finished=mode==3;
            std::array<std::byte,9> bytes{};Writer w(bytes);const auto a=native::kScans[i].link;
            CHECK(native::write_body(w,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot) && w.bit_count()==65);
            CHECK((std::to_integer<unsigned>(bytes[4])>>7)==(mode==1?1U:0U));
        }
    }
    // Independently inspect the position IEEE754 word and revision at native wire bits0..47.
    const coo::Asset wall{0xE6402111U,0x80B56A48U,23,0};
    CHECK(native::find(wall.registry,23,0)->asset==wall);
    CHECK(native::device_position(wall,false)==.2F && native::device_position(wall,true)==.2F);
    CHECK(native::kDeviceRanges5[2].graph==0x80F4BA2DU && native::kDeviceRanges5[2].min<.2F);
    for(const coo::Asset other:{coo::Asset{wall.registry+1,wall.definition,23,0},coo::Asset{wall.registry,wall.definition+1,23,0},
        coo::Asset{wall.registry,wall.definition,4,0},coo::Asset{wall.registry,wall.definition,23,1}}) {
        CHECK(native::device_position(other,false)==0.F && native::device_position(other,true)==1.F);
    }
    const coo::Asset pit{0x59700FA7U,0x80B566FDU,23,50};
    for(bool closed:{true,false}) {
        auto& state=f.native[native::asset_index(pit)];state={19,true,closed,true,closed,false};
        std::array<std::byte,19> bytes{};Writer w(bytes);
        CHECK(native::write_body(w,f,pit.registry,23,pit.slot) && w.bit_count()==147);
        CHECK(std::to_integer<unsigned>(bytes[0])==(closed?0U:0x3FU));
        CHECK(std::to_integer<unsigned>(bytes[1])==(closed?0U:0x80U));
        CHECK(bytes[2]==std::byte{} && bytes[3]==std::byte{});
        CHECK(bytes[4]==std::byte{0x80} && bytes[5]==std::byte{19});
        CHECK((std::to_integer<unsigned>(bytes[6])&0x80U)==0); // Native interpolation remains enabled.
        CHECK(parity(f,pit,147));
    }
    // Catch mode.5 raises native outputs; mode0 fades them. Side and center lasers use1/0.
    for(const coo::Asset a:{coo::Asset{0x59700FA7U,0x80B5690EU,23,93},coo::Asset{0x59700FA7U,0x80B56914U,23,94},
                           coo::Asset{0x59700FA7U,0x80B56900U,23,89},coo::Asset{0x59700FA7U,0x80B568F5U,23,87},
                           coo::Asset{0x59700FA7U,0x80B568F9U,23,88}}) {
        CHECK(native::find(a.registry,23,a.slot)->asset==a);
        for(bool active:{false,true}) {
            f.native[native::asset_index(a)]={20,true,active,true,active,false};
            CHECK(native::device_position(a,active)==(active?(a.slot>=93?.5F:1.F):0.F));
            std::array<std::byte,19> bytes{};Writer w(bytes);
            CHECK(native::write_body(w,f,a.registry,23,a.slot) && w.bit_count()==147);
            CHECK(bytes[0]==std::byte(active?0x3F:0));
            CHECK(bytes[1]==std::byte(active && a.slot<93?0x80:0));
            CHECK(bytes[2]==std::byte{} && bytes[3]==std::byte{});
            CHECK(bytes[4]==std::byte{0x80} && bytes[5]==std::byte{20});CHECK(parity(f,a,147));
        }
    }
    CHECK(native::device_position({0x59700FA7U,0x80B5690FU,23,93},true)==1.F);
    CHECK(native::device_position({0x59700FA6U,0x80B5690EU,23,93},true)==1.F);
    CHECK(native::kLensEntity==0x80F4803CU && native::kLensHealth==0x80F48026U);
    CHECK(native::kLensHealthKind==0x80804B8AU && native::kLensHealthOffset==0xB08U && native::kLensGraph==0x80F48031U);
    // Independent graph modes and big-endian native position words for the box.
    CHECK(native::kLens==native::find(0x59700FA7U,4,79)->asset);
    CHECK(native::kLensDevice==native::find(0x59700FA7U,23,90)->asset);
    CHECK(native::find(0x59700FA7U,4,79)->offset==0x4C8);
    CHECK(native::find(0x59700FA7U,26,96)->asset.definition==0x80B56920U);
    CHECK(native::find(0x59700FA7U,26,96)->offset==0xAC8);
    const auto lens=native::kLensDevice;
    for(unsigned mode=0;mode<3;++mode) {
        auto& state=f.native[native::asset_index(lens)];state={static_cast<std::uint32_t>(21+mode),true,true,true,true,false};
        f.lensExposed=mode>0;f.lensDestroyed=mode==2;
        const float expected=mode==0?1.F:mode==1?.75F:0.F;
        CHECK(native::device_position(f,lens)==expected);
        std::array<std::byte,19> bytes{};Writer w(bytes);
        CHECK(native::write_body(w,f,lens.registry,23,lens.slot) && w.bit_count()==147);
        CHECK(bytes[0]==std::byte(mode==2?0:0x3F));
        CHECK(bytes[1]==std::byte(mode==0?0x80:mode==1?0x40:0));
        CHECK(bytes[2]==std::byte{} && bytes[3]==std::byte{});
        CHECK(bytes[4]==std::byte{0x80} && bytes[5]==std::byte(21+mode));
        CHECK(parity(f,lens,147));
    }
    f.lensExposed=true;f.lensDestroyed=false;
    f.native[native::asset_index(lens)].active=false;
    CHECK(native::device_position(f,lens)==0.F);
    // The frame override cannot affect the separately inverted Cyclops pit.
    f.native[native::asset_index(pit)].active=true;CHECK(native::device_position(f,pit)==0.F);
    f.native[native::asset_index(pit)].active=false;CHECK(native::device_position(f,pit)==1.F);
    for(bool active:{false,true}) {
        f.native[native::asset_index(wall)]={25,true,active,true,active,false};
        std::array<std::byte,19> bytes{};Writer writer(bytes);
        CHECK(native::write_body(writer,f,wall.registry,23,0) && writer.bit_count()==147);
        CHECK(bytes[0]==std::byte{0x3E} && bytes[1]==std::byte{0x4C} && bytes[2]==std::byte{0xCC} && bytes[3]==std::byte{0xCD});
        CHECK(bytes[4]==std::byte{0x80} && bytes[5]==std::byte{25});CHECK(parity(f,wall,147));
    }
    f.finished=false;f.activeRow=coo::kNoDialogue;
    for(auto& state:f.native) {state.managed=false;}
    for(const auto& a:native::kAssets) {if(a.asset.type==1 || a.asset.type==4 || a.asset.type==23) {CHECK(parity(f,a.asset,0));}}
    CHECK(parity(f,{0xDEADBEEFU,0U,4,0},0));
    return true;
}

static bool retained_hologram() {
    namespace h=native::hologram_owner;
    const coo::Generation owner{7,129};
    const native::NativeState desired{130,true,true,true,true,false};
    const h::Generic generic{0x80FD20CDU,0x80803910U,0x67F9F64DU,0x60FAA46EU,0xA78U};
    CHECK(h::retained(owner,h::kSource,desired,130,129,generic.entity,generic));
    CHECK(!h::retained({},h::kSource,desired,130,129,generic.entity,generic));
    CHECK(!h::retained({7,1},h::kSource,desired,130,129,generic.entity,generic));
    CHECK(!h::retained(owner,h::kSource,desired,129,129,generic.entity,generic));
    CHECK(!h::retained(owner,h::kSource,desired,130,1,generic.entity,generic));
    CHECK(!h::retained(owner,h::kSource,desired,130,128,generic.entity,generic));
    CHECK(!h::retained(owner,h::kSource,desired,130,130,generic.entity,generic));
    CHECK(!h::retained(owner,h::kSource,desired,130,129,UINT32_MAX,generic));
    CHECK(!h::retained(owner,h::kSource,desired,130,129,generic.entity+1U,generic));
    for(unsigned i=0;i<4;++i) {
        auto state=desired;
        switch(i) {case 0:state.managed=false;break;case 1:state.desired=false;break;case 2:state.prepared=false;break;default:state.active=false;break;}
        CHECK(!h::retained(owner,h::kSource,state,130,129,generic.entity,generic));
    }
    for(unsigned i=0;i<4;++i) {
        auto source=h::kSource;
        switch(i) {case 0:++source.registry;break;case 1:++source.definition;break;case 2:++source.type;break;default:++source.slot;break;}
        CHECK(!h::retained(owner,source,desired,130,129,generic.entity,generic));
    }
    for(unsigned i=0;i<4;++i) {
        auto part=generic;
        switch(i) {case 0:++part.definition;break;case 1:++part.kind;break;case 2:++part.offset;break;default:part.self=UINT32_MAX;break;}
        CHECK(!h::retained(owner,h::kSource,desired,130,129,generic.entity,part));
    }
    // Captured first run and a later lease both work; a prior run's commit does not.
    CHECK(h::retained({1,1},h::kSource,{2,true,true,true,true,false},2,1,generic.entity,generic));
    CHECK(!h::retained({7,32766},h::kSource,{32767,true,true,true,true,false},32767,32766,generic.entity,generic));
    return true;
}

int main() {
    if(!catalog() || !authority() || !retained_hologram()) {return 1;}
    std::puts("PASS: Deep Storage package catalog and native wire encoding; no live gameplay acceptance");
}
