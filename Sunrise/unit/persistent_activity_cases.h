#pragma once
#include "server/runtime/activity/persistent_activity.h"
#include "server/runtime/activity/mercury_definition.h"
void persistent_activity_cases() {
    namespace a=sunrise::server::runtime::activity;
    namespace mercury=a::mercury;
    std::ifstream input("Sunrise/scripts/mercury_freeroam.json",std::ios::binary);CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)),{});
    std::string error;
    std::shared_ptr<const s::MissionDocument> document=s::MissionDocument::parse(text,mercury::kProfile,error);
    CHECK(document);CHECK(a::PersistentActivity::valid(mercury::kActivity,*document));
    a::PersistentActivity activity;
    CHECK(activity.begin({42,{7}},mercury::kActivity,document,123));
    CHECK(!activity.begin({42,{7}},mercury::kActivity,document,123));
    CHECK(activity.update(15,false).placements.count==0);
    CHECK(activity.update(14,true).populations.count==0);
    a::NativeActivityFrame frame;
    for(int i=0;i<4;++i) frame=activity.update(15,true);
    CHECK(frame.placements.count==10 && frame.populations.count==2);
    namespace placement_wire=sunrise::middleware::bap::activity_message::native::placement;
    for(std::uint16_t slot=0;slot<3;++slot) CHECK(placement_wire::find(frame.placements,0x2749BAAE,4,slot));
    for(std::uint16_t slot=3;slot<15;++slot) CHECK(!placement_wire::find(frame.placements,0x2749BAAE,4,slot));
    CHECK(frame.populations.entries[0].slot==1 && frame.populations.entries[0].source.looseRequested==1);
    CHECK(frame.populations.entries[1].source.registry==0x564C6ECE);
    const auto revision=activity.population().revision();
    for(int i=0;i<100;++i) {frame=activity.update(15,true);CHECK(frame.placements.count==10);}
    CHECK(activity.population().revision()==revision);
    CHECK(activity.diagnostics().phase==c::Phase::complete);
    // Region departure must preserve the exact accepted source generations and
    // flags without advancing the script or issuing a second request.
    const auto beforeDeparture=frame;
    const auto scriptPhase=activity.diagnostics().phase;
    const auto lastRequest=activity.population().last_request();
    for(const auto bubble:{16U,14U,15U})for(const bool arrived:{false,true}) {
        const auto retained=activity.update(bubble,arrived);
        CHECK(retained.placements.count==beforeDeparture.placements.count);
        CHECK(retained.populations.count==beforeDeparture.populations.count);
        for(std::size_t i=0;i<retained.populations.count;++i) {
            const auto& actual=retained.populations.entries[i];
            const auto& expected=beforeDeparture.populations.entries[i];
            CHECK(actual.slot==expected.slot && actual.bubble==expected.bubble);
            CHECK(actual.source.registry==expected.source.registry);
            CHECK(actual.source.generation==expected.source.generation);
            CHECK(actual.source.looseRequested==expected.source.looseRequested);
        }
        for(std::size_t i=0;i<retained.placements.count;++i) {
            const auto& actual=retained.placements.entries[i];
            const auto& expected=beforeDeparture.placements.entries[i];
            CHECK(actual.registry==expected.registry && actual.slot==expected.slot);
            CHECK(actual.bubble==expected.bubble && actual.generation==expected.generation);
        }
        CHECK(activity.population().revision()==revision);
        CHECK(activity.population().last_request()==lastRequest);
        CHECK(activity.diagnostics().phase==scriptPhase);
    }
    // Cold owners do not acquire another incarnation's retained requests.
    a::PersistentActivity cold;CHECK(cold.begin({42,{8}},mercury::kActivity,document,124));
    CHECK(cold.update(16,true).populations.count==0);
    CHECK(cold.update(16,true).placements.count==0);
    CHECK(activity.population().request({{42,{7}},revision,100,0x74337EDD,1,2,123},16)==a::population::Result::stale);
    CHECK(activity.update(15,true).placements.count==10);
    CHECK(activity.population().revision()==revision);
    CHECK(activity.population().request({{42,{7}},revision,99,0x74337EDD,1,2,122},15)==a::population::Result::stale);

    // JSON alone changes selection, scheduling and population policy.
    auto edited=s::json::Reader(text).parse();
    field(field(edited,"parameters"),"pond_initial_count").number=2;
    // This fixture renames Mercury's root graph. Disable the exact native rally probe whose
    // retained dependency intentionally remains bound to the authored `lighthouse` graph.
    field(field(edited,"parameters"),"public_event_rally_probe").number=0;
    auto& graphList=field(edited,"graphs");
    for(auto& pair:graphList.members) if(pair.first=="lighthouse") pair.first="renamed_zone";
    field(field(edited,"roles"),"persistent").text="renamed_zone";
    std::shared_ptr<const s::MissionDocument> changed=s::MissionDocument::parse(encode(edited),mercury::kProfile,error);
    CHECK(changed);CHECK(a::PersistentActivity::valid(mercury::kActivity,*changed));
    a::PersistentActivity second;CHECK(second.begin({99,{1}},mercury::kActivity,changed,124));
    for(int i=0;i<4;++i) frame=second.update(15,true);
    CHECK(frame.placements.count==9);
    std::size_t editedPondIndex=frame.populations.count;
    for(std::size_t i=0;i<frame.populations.count;++i)
        if(frame.populations.entries[i].source.registry==0x74337EDD
           && frame.populations.entries[i].slot==1) editedPondIndex=i;
    CHECK(editedPondIndex<frame.populations.count
          && frame.populations.entries[editedPondIndex].source.looseRequested==2);
    CHECK(activity.update(15,true).placements.count==10); // Old document stays pinned.

    // A destination file can remove an Adventure beacon without changing the
    // native service or enabling its neighboring heroic/patrol/forge objects.
    auto flagsEdited=s::json::Reader(text).parse();
    field(field(flagsEdited,"parameters"),"public_event_rally_probe").number=0;
    auto& flagSteps=field(graph(flagsEdited,"lighthouse"),"steps").items;
    for(auto& step:flagSteps) if(field(step,"id").text=="adventures") field(step,"commands").items.pop_back();
    std::shared_ptr<const s::MissionDocument> flagsDocument=s::MissionDocument::parse(encode(flagsEdited),mercury::kProfile,error);
    CHECK(flagsDocument);a::PersistentActivity flagsActivity;
    CHECK(flagsActivity.begin({102,{1}},mercury::kActivity,flagsDocument,126));
    for(int i=0;i<4;++i) frame=flagsActivity.update(15,true);
    CHECK(frame.placements.count==8);
    CHECK(placement_wire::find(frame.placements,0x2749BAAE,4,0));
    CHECK(placement_wire::find(frame.placements,0x2749BAAE,4,1));
    CHECK(!placement_wire::find(frame.placements,0x2749BAAE,4,2));
    CHECK(activity.update(15,true).placements.count==10);

    auto bad=s::json::Reader(text).parse();
    field(field(field(bad,"assets"),"landing_portal"),"slot").number=9;
    CHECK(!s::MissionDocument::parse(encode(bad),mercury::kProfile,error));
    bad=s::json::Reader(text).parse();field(field(bad,"parameters"),"pond_initial_count").number=64;
    CHECK(!s::MissionDocument::parse(encode(bad),mercury::kProfile,error));
    bad=s::json::Reader(text).parse();field(bad,"mission").text="wrong_activity";
    auto other=s::MissionDocument::parse(encode(bad),mercury::kProfile,error);CHECK(other);
    CHECK(!a::PersistentActivity::valid(mercury::kActivity,*other));

    // Independent identity/profile uses exactly the same adapter. This is an
    // offline contract fixture, not a claim of another playable destination.
    auto alternateProfile=mercury::kProfile;alternateProfile.id="test.destination.v1";
    auto alternateDefinition=mercury::kActivity;alternateDefinition.activity="another_destination";
    alternateDefinition.profile=&alternateProfile;
    alternateDefinition.adventureOpenings={}; // Mercury selections do not bind a renamed root package.
    alternateDefinition.ambientInitial={};
    alternateDefinition.optionalRegistries={}; // Mercury's optional dependencies belong to its exact activity.
    alternateDefinition.publicEventInitials={}; // Their gates require those exact optional dependencies.
    bad=s::json::Reader(text).parse();field(bad,"mission").text="another_destination";
    field(bad,"profile").text="test.destination.v1";
    field(field(bad,"parameters"),"public_event_rally_probe").number=0;
    std::shared_ptr<const s::MissionDocument> alternate=s::MissionDocument::parse(encode(bad),alternateProfile,error);
    CHECK(alternate);a::PersistentActivity reused;
    CHECK(reused.begin({100,{1}},alternateDefinition,alternate,125));
    for(int i=0;i<4;++i) frame=reused.update(15,true);
    CHECK(frame.placements.count==9 && frame.populations.count==2);
}
