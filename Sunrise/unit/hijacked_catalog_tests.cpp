#include "../src/state/activity/hijacked/placement_ownership.h"
#include "../src/state/activity/hijacked/retirement_identity.h"
#include "../src/state/activity/hijacked/retirement_unload.h"
#include "../src/state/activity/hijacked/authority.h"
#include "../src/state/activity/forced/prelaunch_profile.h"
#include "../src/middleware/encoding/bit_writer.h"
#include "hijacked_roster_lookup_tests.h"
#include <array>
#include <cmath>
#include <cstdio>

namespace native=sunrise::state::activity::hijacked;
namespace coo=sunrise::state::activity::coo;
using Writer=sunrise::middleware::encoding::bits::Writer;
#define CHECK(expression) do {if(!(expression)) {std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expression);return false;}} while(false)

// Independent constants from installed package/cache extraction, not encoder outputs.
static_assert(native::kActivity==297 && native::kInvestment==0x83211FEDU);
namespace prelaunch=sunrise::state::activity::forced::prelaunch;
static_assert(prelaunch::kHijacked.activity==native::kActivity);
static_assert(prelaunch::kHijacked.investmentHash==native::kInvestment);
static_assert(prelaunch::kHijacked.packageHash==native::kRoot);
static_assert(prelaunch::kHijacked.package==native::kPackage);
static_assert(prelaunch::matches(prelaunch::kHijacked,297,297,"adventure_rumba"));
static_assert(!prelaunch::matches(prelaunch::kHijacked,296,296,"adventure_rumba"));

static_assert(native::kActivityTag==0x80B4200FU && native::kLaunchTag==0x80FB5018U);
static_assert(native::kScenario==0x80B4206AU && native::kRoot==0x77852DB9U);
static_assert(native::kBank==0x80F5C3A2U && native::kPackage=="adventure_rumba");
static_assert(native::kBubbleCount==45 && std::size(native::kGroups)==14);
static_assert(std::size(native::kAssets)==230 && std::size(native::kVolumes)==69);
static_assert(std::size(native::kSources)==56 && std::size(native::kSpawns)==56);
static_assert(std::size(native::kObjectives)==8 && std::size(native::kDialogue)==14);
static_assert(native::find(0x153E22CDU,1,21)->asset.definition==0x80B421FAU);
static_assert(native::find(0x153E22CDU,1,21)->authority==0x80807EC9U);
static_assert(native::find(0x153E22CDU,2,22)->asset.definition==0x80B42323U);
static_assert(native::find(0x153E22CDU,23,29)->asset.definition==0x80B4235CU);
static_assert(native::find(0x153E22CDU,23,29)->authority==0x80804F48U);
static_assert(native::find(0xD997395EU,4,19)->asset.definition==0x80B429A9U);
static_assert(native::find(0xD997395EU,4,23)->asset.definition==0x80B423B5U);
static_assert(native::find(0xD997395EU,4,23)->authority==0x8080992FU);
static_assert(native::find(0x40A009B5U,65,0)->asset.definition==0x80B42410U);
static_assert(native::find(0x40A009B5U,65,0)->offset==0x258U);
static_assert(native::find(0xD997395EU,65,0)==nullptr);
static_assert(native::kDialogue[0].selector==0x79EB0CD2U && native::kDialogue[0].durationMs==10789);
static_assert(native::kDialogue[9].selector==0xC42F1AF5U && native::kDialogue[9].durationMs==10963);
static_assert(native::kDialogue[13].selector==0x54690F03U && native::kDialogue[13].durationMs==28080);
static_assert(native::kObjectives[3].event==0x9F0648D1U);
static_assert(native::kObjectives[7].event==0xEDEDCBB4U);
static_assert(std::size(native::kPlates)==1 && std::size(native::kScans)==1);
static_assert(native::kPlates[0].source==coo::Asset{0xD997395EU,0x80B429A9U,4,19});
static_assert(native::kPlates[0].volume==coo::Asset{0xD997395EU,0x80B4238DU,60,114});
static_assert(native::kScans[0].source==coo::Asset{0xD997395EU,0x80B423B5U,4,23});
static_assert(native::kScans[0].link==coo::Asset{0x40A009B5U,0x80B42410U,65,0});
static_assert(native::kScans[0].controllerDefinition==0x8157E6B1U);

// Decode the MSB-first wire independently of the native authority helpers.
static std::uint64_t bits(std::span<const std::byte> bytes,std::size_t offset,unsigned width) {
    std::uint64_t value{};
    for(unsigned i=0;i<width;++i) {
        const auto bit=offset+i;
        value=(value<<1)|((std::to_integer<unsigned>(bytes[bit/8])>>(7U-static_cast<unsigned>(bit%8)))&1U);
    }
    return value;
}
static bool catalog() {
    for(std::size_t i=0;i<std::size(native::kAssets);++i) {
        const auto& a=native::kAssets[i];CHECK(native::find(a.asset.registry,a.asset.type,a.asset.slot)==&a);
        for(std::size_t j=i+1;j<std::size(native::kAssets);++j) {
            const auto& b=native::kAssets[j].asset;
            CHECK(a.asset.registry!=b.registry || a.asset.type!=b.type || a.asset.slot!=b.slot);
        }
    }
    unsigned cave{},surface{},well{};
    for(const auto& g:native::kGroups) {
        if(g.key==0x153E22CDU) {CHECK(g.bubble==33 && !g.topLevel);++cave;}
        if(g.key==0x3E9B74F3U) {CHECK(g.bubble==37 && !g.topLevel);++surface;}
        if(g.key==0xD997395EU) {CHECK(g.bubble==40 && !g.topLevel);++well;}
        for(const auto& s:g.slots) {
            const auto* a=native::find(g.key,s.type,s.index);
            CHECK(a && a->asset.definition==s.tag && a->offset==s.offset);
            CHECK(a->component==s.component && a->sense==s.sense && a->authority==s.auth);
        }
    }
    CHECK(cave==1 && surface==1 && well==1);
    constexpr std::array<coo::Asset,8> expectedTargets{{
        {0xF5737F85U,0x80B42335U,31,2},{0xD8FA09CAU,0x80B4224DU,31,0},
        {0x2D20FD66U,0x80B42246U,31,1},{0x153E22CDU,0x80B421FAU,1,21},
        {0x3C7C8AE9U,0x80B4223DU,31,0},{0x701F9CE5U,0x80B42356U,31,2},
        {0xA12CA9FAU,0x80B42408U,31,1},{0xD997395EU,0x80B423B5U,4,23},
    }};
    CHECK(std::size(native::kObjectiveTargets)==expectedTargets.size());
    for(std::size_t i=0;i<expectedTargets.size();++i) {
        const auto target=native::marker(native::kObjectives[i].event);
        CHECK(target.valid() && target.asset==expectedTargets[i]);
        const auto* a=native::find(target.asset.registry,target.asset.type,target.asset.slot);
        CHECK(a && a->asset==target.asset);
        bool rostered{};
        for(const auto& g:native::kGroups) {if(g.key==target.asset.registry) {rostered=true;}}
        CHECK(rostered);
    }
    CHECK(!native::marker(UINT32_MAX).valid());
    unsigned plateVolume{},twoCategories{},fallbacks{};
    for(const auto& v:native::kVolumes) {
        CHECK(v.vertices.size()>=3 && v.min.x<v.max.x && v.min.y<v.max.y && v.min.z<v.max.z);
        double area{};
        for(std::size_t i=0;i<v.vertices.size();++i) {
            const auto& p=v.vertices[i];const auto& q=v.vertices[(i+1)%v.vertices.size()];
            CHECK(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
            CHECK(p.x>=v.min.x-.001F && p.x<=v.max.x+.001F && p.y>=v.min.y-.001F && p.y<=v.max.y+.001F);
            area+=double(p.x)*q.y-double(q.x)*p.y;
        }
        CHECK(std::abs(area)>.01);
        if(v.asset==native::kPlates[0].volume) {
            CHECK(v.name=="pf_sync_plate_central._volume");
            CHECK(std::abs(v.min.z+242.5434265F)<.001F && std::abs(v.max.z+230.5434265F)<.001F);++plateVolume;
        }
    }
    CHECK(plateVolume==1);
    for(std::size_t i=0;i<std::size(native::kSpawns);++i) {
        const auto& p=native::kSpawns[i];const auto& s=native::kSources[i];
        CHECK(p.registry==s.asset.registry && p.source==s.asset.slot && p.definition==s.asset.definition);
        CHECK(p.offset==0x728U && native::find(p.registry,1,p.source)->offset==0x728U);
        CHECK(p.categories==s.categories && (p.categories==1 || p.categories==2));
        CHECK(p.count==p.requested[0]+p.requested[1] && p.count<=16);
        CHECK(p.requested[0]>0 && (p.categories==2?p.requested[1]>0:p.requested[1]==0));
        CHECK(native::find(p.registry,66,p.rule) && native::find(p.tactical.registry,3,p.tactical.slot));
        CHECK(p.tactical.registry==p.registry && p.tactical.row>=0 && p.tactical.row<24);
        if(p.categories==2) {++twoCategories;}
        if(s.hasRule) {CHECK(s.rule==p.rule);} else {++fallbacks;}
    }
    CHECK(twoCategories==4 && fallbacks==10);
    const auto* boss=native::spawn(0x153E22CDU,21);
    CHECK(boss && boss->rule==117 && boss->count==1 && boss->tactical.slot==20);
    CHECK(native::objective(0xAC66AAD0U)->description=="Track down and destroy an Entangled Mind.");
    CHECK(native::objective(0xEDEDCBB4U)->description=="Inspect the conflux.");
    CHECK(native::objective(0xAC66AAD0U)!=native::objective(0xEBBB3BECU));
    CHECK(native::objective(0xAC66AAD0U)->title==native::objective(0xEBBB3BECU)->title);
    CHECK(native::objective(UINT32_MAX)==nullptr);
    return true;
}
static bool parity(const native::Frame& frame,coo::Asset asset,std::size_t expected) {
    CHECK(native::body_bits(frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot)==expected);
    auto measure=Writer::measuring();
    CHECK(native::write_body(measure,frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot)==(expected!=0));
    CHECK(measure.bit_count()==expected);
    if(!expected) {return true;}
    std::array<std::byte,4096> bytes{};const auto size=(expected+7)/8;
    Writer writer(std::span<std::byte>{bytes}.first(size));
    CHECK(native::write_body(writer,frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot));
    std::size_t written{};CHECK(writer.finish(written) && writer.bit_count()==expected && written==size);
    Writer shortWriter(std::span<std::byte>{bytes}.first(size-1));
    CHECK(!native::write_body(shortWriter,frame,asset.registry,static_cast<std::uint8_t>(asset.type),asset.slot));
    return true;
}
static bool all_bodies() {
    native::Frame frame{};
    for(const auto& a:native::kAssets) {CHECK(parity(frame,a.asset,0));}
    frame.enabled=true;
    for(const auto& a:native::kAssets) {CHECK(parity(frame,a.asset,0));}
    frame.spawnGeneration=9;frame.generations.fill(2);
    frame.presentation={0x025CC54FU,0xAC66AAD0U,3,native::marker(0x9F0648D1U),true,true};
    std::array<unsigned,6> kinds{};
    for(bool active:{false,true}) {
        for(auto& s:frame.native) {s={19,true,true,true,active,false};}
        for(const auto& a:native::kAssets) {
            std::size_t expected{};
            switch(a.asset.type) {
            case 1:expected=native::kSpawns[native::spawn_index(a.asset)].categories==2?673U:641U;++kinds[0];break;
            case 4:expected=252;for(const auto& plate:native::kPlates) {if(active && a.asset==plate.source) {expected+=709;}}++kinds[1];break;
            case 23:expected=147;++kinds[2];break;
            case 65:expected=65;++kinds[3];break;
            case 68:expected=4802;++kinds[4];break;
            case 53:expected=19767+64*frame.generations.size();++kinds[5];break;
            default:break;
            }
            CHECK(parity(frame,a.asset,expected));
        }
    }
    CHECK((kinds==std::array<unsigned,6>{112,28,12,2,2,2}));
    for(std::uint8_t row=0;row<14;++row) {frame.activeRow=row;CHECK(parity(frame,native::kDialogueAsset,19767+64*frame.generations.size()));}
    for(const auto& objective:native::kObjectives) {
        frame.presentation.event=objective.event;frame.presentation.marker=native::marker(objective.event);
        CHECK(parity(frame,{0x77852DB9U,0x80B4241CU,68,0},4802));
    }
    for(auto& s:frame.native) {s.managed=false;}
    for(const auto& a:native::kAssets) {
        if(a.asset.type==1 || a.asset.type==4 || a.asset.type==23) {CHECK(parity(frame,a.asset,0));}
    }
    frame.presentation.published=false;CHECK(parity(frame,{0x77852DB9U,0x80B4241CU,68,0},0));
    CHECK(parity(frame,{0xDEADBEEFU,0U,4,0},0));
    CHECK(parity(frame,{0x40A009B5U,0U,65,1},0));
    return true;
}
static bool device_and_scan_fields() {
    native::Frame frame{};frame.enabled=true;frame.spawnGeneration=9;
    const coo::Asset barrier{0x153E22CDU,0x80B4235CU,23,29};
    const coo::Asset hologram{0xD997395EU,0x80B429CCU,23,25};
    for(auto a:{barrier,hologram}) {
        for(bool active:{false,true}) {
            frame.native[native::asset_index(a)]={19,true,active,true,active,false};
            const auto word=a==barrier?(active?0U:0x3F800000U):(active?0x3F000000U:0U);
            std::array<std::byte,19> bytes{};Writer writer(bytes);
            CHECK(native::write_body(writer,frame,a.registry,23,a.slot));
            CHECK(bits(bytes,0,32)==word && bits(bytes,32,16)==0x8013U && bits(bytes,48,1)==0);
            CHECK(bits(bytes,49,32)==0x3F800000U && bits(bytes,81,16)==0x7FFFU);
            CHECK(bits(bytes,97,1)==0 && bits(bytes,98,32)==0 && bits(bytes,130,16)==0x7FFFU && bits(bytes,146,1)==0);
        }
        for(const coo::Asset other:{coo::Asset{a.registry+1,a.definition,a.type,a.slot},coo::Asset{a.registry,a.definition+1,a.type,a.slot},
            coo::Asset{a.registry,a.definition,4,a.slot},coo::Asset{a.registry,a.definition,a.type,static_cast<std::uint16_t>(a.slot+1)}}) {
            CHECK(native::device_position(other,false)==0.F && native::device_position(other,true)==1.F);
        }
    }
    const auto source=native::kScans[0].source;const auto link=native::kScans[0].link;
    for(unsigned mode=0;mode<4;++mode) {
        frame.scanArmed[0]=mode!=0;frame.scanComplete[0]=mode==2;frame.finished=mode==3;
        frame.native[native::asset_index(source)]={27,true,true,true,mode!=0,false};
        std::array<std::byte,9> bytes{};Writer writer(bytes);
        CHECK(native::write_body(writer,frame,link.registry,65,link.slot));
        CHECK(bits(bytes,0,32)==0x8000000AU); // Link belongs to spawnGeneration+1, not source revision27.
        CHECK(bits(bytes,32,1)==(mode==1?1U:0U) && bits(bytes,33,32)==0x811C9DC5U);
        std::array<std::byte,32> object{};Writer objectWriter(object);
        CHECK(native::write_body(objectWriter,frame,source.registry,4,source.slot));
        CHECK(bits(object,0,32)==0x8000001BU && bits(object,32,32)==0x80000000U);
        CHECK(bits(object,64,1)==(mode!=0?1U:0U) && bits(object,65,1)==0);
        CHECK(bits(object,66,32)==0x7FFFFFFFU && bits(object,98,32)==0x811C9DC5U);
        CHECK(bits(object,130,7)==0 && bits(object,137,16)==0x7FFFU);
        CHECK(parity(frame,source,252) && parity(frame,link,65));
    }
    return true;
}
static bool boss_source_fields() {
    native::Frame frame{};frame.enabled=true;frame.spawnGeneration=9;
    constexpr coo::Asset boss{0x153E22CDU,0x80B421FAU,1,21};
    const std::array<unsigned,3> rows{6,3,2}; // Native tactical rows5/2/1, biased by1.
    for(std::uint8_t stage=0;stage<3;++stage) {
        frame.bossStage=stage;
        for(bool active:{false,true}) {
            frame.native[native::asset_index(boss)]={19,true,active,true,active,false};
            std::array<std::byte,81> bytes{};Writer writer(bytes);
            CHECK(native::write_body(writer,frame,boss.registry,1,21) && writer.bit_count()==641);
            CHECK(bits(bytes,0,1)==1 && bits(bytes,1,32)==0x153E22CDU && bits(bytes,33,7)==4 && bits(bytes,40,16)==0x8014U);
            CHECK(bits(bytes,121,32)==(active?0x80000001U:0x80000000U));
            CHECK(bits(bytes,172,1)==1 && bits(bytes,173,31)==19);
            CHECK(bits(bytes,382,1)==1 && bits(bytes,383,32)==0x153E22CDU);
            CHECK(bits(bytes,415,7)==67 && bits(bytes,422,16)==0x8075U); // Native spawn rule117.
            CHECK(bits(bytes,565,1)==1 && bits(bytes,566,5)==rows[stage]);
            CHECK(bits(bytes,571,1)==1 && bits(bytes,572,31)==19);
            CHECK(parity(frame,boss,641));
        }
    }
    return true;
}
static bool squad_counts() {
    // User-directed reconstruction: these are explicit density expectations,
    // not claims that native variant or placement-array lengths encode counts.
    struct Expected {std::uint32_t registry;std::uint16_t source;std::uint8_t first,second;};
    constexpr std::array expected{
        Expected{0x153E22CDU,1,1,0},Expected{0x153E22CDU,2,3,0},
        Expected{0x153E22CDU,7,2,0},Expected{0x153E22CDU,8,2,0},Expected{0x153E22CDU,9,2,0},Expected{0x153E22CDU,10,2,0},Expected{0x153E22CDU,12,2,0},Expected{0x153E22CDU,19,2,0},Expected{0x153E22CDU,23,2,0},Expected{0x153E22CDU,24,2,0},Expected{0x153E22CDU,25,2,0},
        Expected{0x153E22CDU,14,3,0},Expected{0x153E22CDU,15,3,0},
        Expected{0x153E22CDU,4,1,0},Expected{0x153E22CDU,5,1,0},Expected{0x153E22CDU,6,1,0},
        Expected{0x153E22CDU,16,1,0},Expected{0x153E22CDU,17,1,0},Expected{0x153E22CDU,21,1,0},
        Expected{0x3E9B74F3U,1,1,0},Expected{0x3E9B74F3U,2,1,0},
        Expected{0x3E9B74F3U,3,3,0},Expected{0x3E9B74F3U,4,3,0},
        Expected{0x3E9B74F3U,5,2,0},Expected{0x3E9B74F3U,6,3,0},Expected{0x3E9B74F3U,7,3,0},
        Expected{0x3E9B74F3U,13,1,2},Expected{0x3E9B74F3U,16,1,2},
        Expected{0x3E9B74F3U,18,1,0},Expected{0x3E9B74F3U,20,1,2},Expected{0x3E9B74F3U,21,1,2},
        Expected{0xD997395EU,27,3,0},Expected{0xD997395EU,28,3,0},
        Expected{0xD997395EU,29,3,0},Expected{0xD997395EU,30,3,0},
        Expected{0xD997395EU,31,3,0},Expected{0xD997395EU,32,3,0},
        Expected{0xD997395EU,33,3,0},Expected{0xD997395EU,34,3,0},
        Expected{0xD997395EU,35,1,0},Expected{0xD997395EU,37,3,0}};
    for(const auto e:expected) {
        const auto* source=native::spawn(e.registry,e.source);CHECK(source);
        CHECK(source->requested[0]==e.first && source->requested[1]==e.second);
        CHECK(source->count==e.first+e.second);
        native::Frame frame{};frame.enabled=true;frame.spawnGeneration=9;
        const auto asset=native::find(e.registry,1,e.source)->asset;
        auto& state=frame.native[native::asset_index(asset)];
        for(bool active:{false,true}) for(bool prepared:{false,true}) {
            state={9,true,true,prepared,active,false};
            std::array<std::byte,85> bytes{};Writer writer(bytes);
            CHECK(native::write_body(writer,frame,e.registry,1,e.source));
            CHECK(bits(bytes,117,4)==(e.second?2U:1U));
            CHECK(bits(bytes,121,32)==0x80000000U+(active && prepared?e.first:0U));
            if(e.second) {CHECK(bits(bytes,153,32)==0x80000000U+(active && prepared?e.second:0U));}
        }
    }
    // Same per-source capacity as Controller. Full count, native readiness and
    // every genuine death are required; one receipt cannot clear a requested squad.
    coo::PopulationService<native::EnemyReceipt,std::size(native::kSpawns),16> population;
    unsigned total{},single{},pairs{},squads{};
    for(std::size_t i=0;i<std::size(native::kSpawns);++i) {
        const auto& source=native::kSpawns[i];const auto& cohort=native::kCohorts[i];
        CHECK(cohort.registry==source.registry && cohort.source==source.source && cohort.count==source.count);
        CHECK(source.count>=1 && source.count<=3);
        if(source.count==2) {CHECK(source.registry==0x153E22CDU
            || (source.registry==0x3E9B74F3U && source.source==5));}total+=source.count;
        single+=source.count==1;pairs+=source.count==2;squads+=source.count==3;
        population.enable(i);const auto tactical=source.tactical;
        population.policy(i,{true,coo::EnemyIntent::combat,tactical.registry,tactical.slot,tactical.row});
        std::array<native::EnemyReceipt,16> actors{};
        for(unsigned n=0;n<source.count;++n) {
            CHECK(!population.admitted(i,source.count) && !population.cleared(i,source.count));
            auto& actor=actors[n];actor={710,static_cast<std::uint32_t>(1000+i*16+n),static_cast<std::uint32_t>(3000+i),9,source.source,source.registry};
            CHECK(population.admit(native::kCohorts,actor,710,9)==coo::Admission::accepted);
            CHECK(!population.ready(i,source.count));
            CHECK(population.observe(actor,{true,true,true,true,actor.actor+9000,tactical.registry,tactical.slot,tactical.row}));
        }
        CHECK(population.admitted(i,source.count) && population.ready(i,source.count));
        for(unsigned n=0;n<source.count;++n) {
            CHECK(!population.cleared(i,source.count));
            CHECK(population.died(actors[n],710,9));
        }
        CHECK(population.cleared(i,source.count));
    }
    CHECK(total==122 && single==18 && pairs==10 && squads==28);
    CHECK(native::kExteriorSources.size()==7 && native::kExteriorPopulation==16);
    return true;
}
static bool retired_sources() {
    native::Frame frame{};frame.enabled=true;frame.spawnGeneration=129;
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto asset=native::find(0x3E9B74F3U,1,slot)->asset;
        auto& state=frame.native[native::asset_index(asset)];
        state={129,true,true,true,true,false};
        std::array<std::byte,85> before{};Writer active(before);
        CHECK(native::write_body(active,frame,asset.registry,1,slot));
        CHECK(bits(before,173,31)==129 && bits(before,603,2)==2);
        state={130,true,false,true,false,false,true};
        std::array<std::byte,85> after{};Writer retired(after);
        CHECK(native::write_body(retired,frame,asset.registry,1,slot));
        CHECK(bits(after,121,32)==0x80000000U);
        CHECK(bits(after,173,31)==130 && bits(after,572,31)==130);
        CHECK(bits(after,603,2)==1 && bits(after,605,3)==1);
        CHECK(active.bit_count()==retired.bit_count() && retired.bit_count()==641);
    }
    // Serialization defaults for every other mission/source retain normal BC=1.
    coo::native_combatant::Source source{0x3E9B74F3U,130,1,1};source.retireOwned=true;
    auto invalid=Writer::measuring();CHECK(!coo::native_combatant::write_source(invalid,source));
    return true;
}
static bool placement_readiness() {
    // A native source is registered and seeded while its requested count stays zero.
    // Publishing the exact same lease after placement readiness releases that count.
    native::Frame frame{};frame.enabled=true;frame.spawnGeneration=9;
    for(const auto& source:native::kSpawns) {
        const auto asset=native::find(source.registry,1,source.source)->asset;
        auto& state=frame.native[native::asset_index(asset)];state={9,true,true,false,true,false};
        for(bool prepared:{false,true}) {
            state.prepared=prepared;
            std::array<std::byte,85> bytes{};Writer writer(bytes);
            CHECK(native::write_body(writer,frame,source.registry,1,source.source));
            CHECK(bits(bytes,121,32)==0x80000000U+(prepared?source.requested[0]:0U));
            if(source.categories==2) {CHECK(bits(bytes,153,32)==0x80000000U+(prepared?source.requested[1]:0U));}
            CHECK(state.generation==9);
        }
        unsigned count{};for(const auto& p:native::kPlacements) {if(p.registry==source.registry && p.source==source.source) {++count;}}
        CHECK(count>0 && count<=8);
    }
    const auto p=native::kPlacements[0];constexpr std::uint32_t handle=0x17F42002;
    std::array<std::byte,0xA0> bytes{};
    const auto set=[&](std::size_t offset,auto value) {std::memcpy(bytes.data()+offset,&value,sizeof value);};
    set(12,handle);set(0x4C,p.entity);set(0x88,p.table);set(0x8C,p.record);set(0x90,p.guid);
    CHECK(native::placed_identity(bytes,p,handle));
    CHECK(!native::placed_identity(bytes,p,handle+0x2000)); // Same index, different salt.
    set(0x88,p.table+1);CHECK(!native::placed_identity(bytes,p,handle));set(0x88,p.table);
    set(0x8C,p.record+1);CHECK(!native::placed_identity(bytes,p,handle));set(0x8C,p.record);
    set(0x90,p.guid+1);CHECK(!native::placed_identity(bytes,p,handle));set(0x90,p.guid);
    set(4,4U);CHECK(!native::placed_identity(bytes,p,handle));set(4,0U);
    CHECK(!native::placed_identity(std::span(bytes).first(0x90),p,handle));
    // A registry row from an old instance must not authorize placement creation.
    std::array<std::byte,0x248> sourceBytes{};const auto& source=native::kSpawns[0];
    const auto putSource=[&](std::size_t offset,auto value) {std::memcpy(sourceBytes.data()+offset,&value,sizeof value);};
    putSource(0,source.definition);putSource(4,0x8080948FU);putSource(8,static_cast<std::int64_t>(source.offset));
    for(auto offset:{0x30U,0x48U}) {putSource(offset,handle);putSource(offset+4,0x80809A3BU);}
    putSource(0x1FC,9U);putSource(0x244,9U);
    const auto owned=[&]() {return native::placement_source_identity(sourceBytes,handle,source.definition,source.offset,9);};
    CHECK(owned());
    putSource(0x30,handle+0x2000);CHECK(!owned());putSource(0x30,handle);
    putSource(0x48,handle+0x2000);CHECK(!owned());putSource(0x48,handle);
    putSource(0,source.definition+1);CHECK(!owned());putSource(0,source.definition);
    putSource(0x1FC,8U);CHECK(!owned());putSource(0x1FC,9U);
    putSource(0x244,8U);CHECK(owned()); // Actor-result sense state cannot gate initial placement.
    CHECK(!native::placement_source_identity(std::span(sourceBytes).first(0x1FC),handle,source.definition,source.offset,9));
    unsigned shared{};for(const auto& item:native::kPlacements) {if(item.table==0x81582ABDU) {CHECK(item.registry==0x153E22CDU && item.source==7 && item.guid==0x99851CF588D0F1FDULL);++shared;}}
    CHECK(shared==1);
    return true;
}
static bool exterior_entity_retirement() {
    // Reproduce the live failure: source generation detached the actor while
    // the entity stayed live without local object authority. Retain its identity
    // before that transition so the native teardown can still target it.
    constexpr coo::Generation run{5,9};
    const native::RetirementWorld world{0x120000,0x100,3,0x500000};
    constexpr std::uint32_t actor=0x15F42003U,owner=0x41F9006EU,parent=0x38F9E553U,entity=0x0BFAA07AU,bundle=0x6AF9EB7BU;
    std::array<std::byte,0x68> a{};std::array<std::byte,0x50> e{};
    const auto setA=[&](std::size_t offset,auto value) {std::memcpy(a.data()+offset,&value,sizeof value);};
    const auto setE=[&](std::size_t offset,auto value) {std::memcpy(e.data()+offset,&value,sizeof value);};
    setA(0x38,owner);setA(0x3C,0x80809A3BU);setA(0x48,actor);setA(0x4C,entity);setA(0x50,parent);
    setA(0x60,UINT32_MAX);setE(12,entity);setE(4,0x70020U);setE(0x4C,bundle);
    native::RetirementEnemy target{};
    CHECK(native::capture_retirement_enemy(run,3,owner,world,a,e,target));
    CHECK(target.run==run && target.source==3 && target.entity==entity);
    setA(0x38,UINT32_MAX);setA(0x3C,UINT32_MAX);
    CHECK(native::retirement_entity_matches(target,world,a,e));
    // A recycled actor slot cannot erase the still-matching entity lease.
    setA(0x48,actor+0x2000);setA(0x50,parent+0x2000);setA(0x4C,entity+0x2000);
    CHECK(native::retirement_entity_matches(target,world,e));
    CHECK(native::retirement_actor_allows(target,a));
    setA(0x4C,entity);setA(0x38,owner+0x2000);
    CHECK(!native::retirement_actor_allows(target,a));
    setA(0x48,actor);setA(0x50,parent);setA(0x38,UINT32_MAX);
    CHECK(!native::retirement_entity_matches(target,{world.entities,world.stride,world.space,world.manager+1},e));
    // An unload or transient read cannot discard a lease. Resolve it again
    // when the same world returns; only proof of replacement/removal expires it.
    auto readState=native::retirement_entity_state(target,{world.entities+0x1000,world.stride,world.space,world.manager},true,false,{});
    CHECK(readState==native::RetirementRead::otherWorld && !native::retirement_expired(readState));
    CHECK(!native::retirement_expired(native::retirement_entity_state(target,world,false,false,{})));
    CHECK(!native::retirement_expired(native::retirement_entity_state(target,world,true,false,{})));
    CHECK(native::retirement_entity_state(target,world,true,true,e)==native::RetirementRead::live);
    setE(12,entity+0x2000);
    CHECK(native::retirement_expired(native::retirement_entity_state(target,world,true,true,e)));setE(12,entity);
    setE(4,0x70024U);CHECK(native::retirement_expired(native::retirement_entity_state(target,world,true,true,e)));setE(4,0x70020U);
    // Native admission can precede entity construction. Preserve authenticated
    // actor/parent origin, then bind readiness after source detachment.
    setA(0x38,owner);setA(0x3C,0x80809A3BU);setA(0x4C,UINT32_MAX);
    native::RetirementOrigin origin{};
    CHECK(native::capture_retirement_origin(run,3,owner,0x230000,0xA450,a,origin));
    native::RetirementEnemy late{};
    CHECK(!native::bind_retirement_entity(origin,0x230000,0xA450,a,world,e,late));
    setA(0x38,UINT32_MAX);setA(0x3C,UINT32_MAX);setA(0x4C,entity);
    CHECK(native::bind_retirement_entity(origin,0x230000,0xA450,a,world,e,late));
    CHECK(late.entity==entity && late.owner==owner && late.source==3);
    CHECK(!native::capture_retirement_origin(run,3,owner,0x230000,0xA450,a,origin));
    setA(0x48,actor+0x2000);CHECK(!native::bind_retirement_entity(origin,0x230000,0xA450,a,world,e,late));setA(0x48,actor);
    setA(0x50,parent+0x2000);CHECK(!native::bind_retirement_entity(origin,0x230000,0xA450,a,world,e,late));setA(0x50,parent);
    setA(0x38,owner+0x2000);CHECK(!native::bind_retirement_entity(origin,0x230000,0xA450,a,world,e,late));setA(0x38,UINT32_MAX);
    CHECK(!native::bind_retirement_entity(origin,0x240000,0xA450,a,world,e,late));
    // Detached/unattributed actors cannot create a new deletion lease.
    native::RetirementEnemy unscoped{};
    CHECK(!native::capture_retirement_enemy(run,3,owner,world,a,e,unscoped));
    setA(0x38,owner+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setA(0x38,UINT32_MAX);
    setA(0x48,actor+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setA(0x48,actor);
    setA(0x50,parent+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setA(0x50,parent);
    setA(0x4C,entity+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setA(0x4C,entity);
    setE(12,entity+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setE(12,entity);
    setE(0x4C,bundle+0x2000);CHECK(!native::retirement_entity_matches(target,world,a,e));setE(0x4C,bundle);
    setE(4,0x70024U);CHECK(!native::retirement_entity_matches(target,world,a,e));setE(4,0x70020U);
    CHECK(!native::retirement_entity_matches(target,{world.entities+0x1000,world.stride,world.space},a,e));
    CHECK(!native::retirement_entity_matches(target,{world.entities,world.stride,world.space+1},a,e));
    CHECK(!native::retirement_entity_matches(target,world,std::span(a).first(0x50),e));
    setA(0x38,owner);setA(0x3C,0x80809A3BU);
    CHECK(!native::capture_retirement_enemy({},3,owner,world,a,e,unscoped));
    CHECK(!native::capture_retirement_enemy(run,8,owner,world,a,e,unscoped));
    CHECK(!native::capture_retirement_enemy(run,0,owner,world,a,e,unscoped));
    setA(0x40,std::int64_t{8});CHECK(!native::capture_retirement_enemy(run,3,owner,world,a,e,unscoped));
    return true;
}
static bool exterior_native_unload() {
    // The native source callback in PID46916 arrived after all 14 original
    // entities were replaced. Selection must use the original full identities
    // at area unload, never copy a deletion lease to a returned ownerless actor.
    constexpr coo::Generation run{5,9};
    const native::RetirementWorld world{0x120000,0x100,3,0x500000};
    native::RetirementEnemy target{run,world,3,0x41F9006EU,0x15F42003U,0x38F9E553U,0x0BFAA07AU,0x6AF9EB7BU};
    std::array<std::byte,0x50> entity{};
    const auto set=[&](std::size_t offset,auto value) {std::memcpy(entity.data()+offset,&value,sizeof value);};
    set(4,0x70020U);set(0x0C,target.entity);set(0x4C,target.bundle);
    const auto address=world.entities+(target.entity&0x1FFFU)*world.stride;
    const auto matches=[&](coo::Generation current,std::uint32_t region,std::uintptr_t caller,
        std::uint32_t component,std::uintptr_t row,std::uint32_t bundle) {
        return native::retirement_unload_matches(target,current,region,caller,component,row,bundle,world,entity);
    };
    CHECK(matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle));
    // Remaining kill counts and objective sections are deliberately absent:
    // optional combat before this real area transition cannot block cleanup.
    CHECK(!matches(run,296,0xA07CBE,0x80807E3EU,address,target.bundle)); // Returning to exterior.
    CHECK(!matches(run,265,0xA07CBE,0x80807E3EU,address,target.bundle)); // Another slice in bubble33.
    CHECK(!matches(run,264,0xA07CC0,0x80807E3EU,address,target.bundle)); // Output may be consumed elsewhere.
    CHECK(!matches(run,264,0xA07CBE,0x808082ECU,address,target.bundle));
    CHECK(!matches({5,10},264,0xA07CBE,0x80807E3EU,address,target.bundle));
    CHECK(!matches({6,9},264,0xA07CBE,0x80807E3EU,address,target.bundle));
    CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address+world.stride,target.bundle));
    CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle+0x2000));
    set(0x0C,target.entity+0x2000);CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle));set(0x0C,target.entity);
    set(4,0x70024U);CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle));set(4,0x70020U);
    target.source=8;CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle));target.source=3;
    target.world.manager+=1;CHECK(!matches(run,264,0xA07CBE,0x80807E3EU,address,target.bundle));
    return true;
}
int main() {
    if(!hijacked_roster_lookup_tests::run() || !catalog() || !all_bodies() || !device_and_scan_fields() || !boss_source_fields() || !squad_counts() || !retired_sources() || !exterior_entity_retirement() || !exterior_native_unload() || !placement_readiness()) {return 1;}
    std::puts("PASS: Hijacked package identities and native wire fields; no live gameplay acceptance");return 0;
}
