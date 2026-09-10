#pragma once
#include "server/runtime/activity/live_definitions.h"
#include <atomic>
#include <thread>

// Independent native test profile; no game engine or gameplay receipts are mocked
// into production. The surrounding generic compiler test supplies the lab fixture.
void live_definition_cases(const s::json::Value& original) {
    namespace live=sunrise::server::runtime::activity;
    const s::ParameterCapability parameters[]{
        {"population.replacement_delay_ms",1000,120000,45000,true},
        {"population.initial_requests",1,6,2,false}
    };
    auto native=profile;native.parameters=parameters;
    std::string error;
    auto parse=[&](const s::json::Value& value) -> live::DocumentPtr {
        auto doc=s::MissionDocument::parse(encode(value),native,error);CHECK(doc);CHECK(error.empty());return doc;
    };
    const auto initial=parse(original);
    CHECK(initial->views().parameter("population.replacement_delay_ms")->value==45000);
    CHECK(initial->views().parameter("population.initial_requests")->value==2);
    CHECK(!initial->views().parameter("unknown"));
    auto edited=original;
    edited.members.emplace_back("parameters",s::json::Reader(R"({"population.replacement_delay_ms":1000})").parse());
    const auto candidate=parse(edited);
    CHECK(candidate->views().parameter("population.replacement_delay_ms")->value==1000);
    CHECK(candidate->views().parameter("population.initial_requests")->value==2);
    CHECK(initial->same_structure(*candidate));
    CHECK(live::LiveDefinitions::classify(*initial,*candidate)==live::Change::futureRequests);
    CHECK(!s::MissionDocument::parse(encode(edited),profile,error)); // unregistered policy
    for(const auto invalid:{"0","120001","true","-1","1.5","null","\"1000\""}) {
        auto bad=encode(edited);const auto offset=bad.find(":1000");CHECK(offset!=std::string::npos);
        bad.replace(offset+1,4,invalid);CHECK(!s::MissionDocument::parse(bad,native,error));CHECK(!error.empty());
    }
    auto bad=edited;field(bad,"parameters").members.emplace_back("unknown",s::json::Reader("1").parse());
    CHECK(!s::MissionDocument::parse(encode(bad),native,error));
    bad=edited;field(bad,"parameters").members.emplace_back("population.replacement_delay_ms",s::json::Reader("1000").parse());
    CHECK(!s::MissionDocument::parse(encode(bad),native,error));
    auto invalidProfile=native;auto invalidParameters=std::to_array(parameters);
    invalidParameters[0].defaultValue=0;invalidProfile.parameters=invalidParameters;
    CHECK(!s::MissionDocument::parse(encode(original),invalidProfile,error));
    invalidParameters[0]=parameters[1];CHECK(!s::MissionDocument::parse(encode(original),invalidProfile,error));
    // The returned document owns default parameter names, even if the caller's
    // profile and name storage disappear immediately after compilation.
    live::DocumentPtr ownedNames;
    {
        std::string temporaryName="population.temporary_policy_name_exceeding_small_string_storage";
        const s::ParameterCapability temporary[]{ {temporaryName,0,9,4,true} };
        auto temporaryProfile=profile;temporaryProfile.parameters=temporary;
        ownedNames=s::MissionDocument::parse(encode(original),temporaryProfile,error);CHECK(ownedNames);
    }
    CHECK(ownedNames->views().parameter("population.temporary_policy_name_exceeding_small_string_storage")->value==4);

    live::LiveDefinitions store;
    CHECK(!store.begin({0,1,15},initial));CHECK(!store.begin({7,1,64},initial));CHECK(!store.begin({7,1,15},{}));
    CHECK(store.begin({7,1,15},initial));CHECK(!store.begin({7,1,15},initial));
    const auto pinned=store.acquire();CHECK(pinned);CHECK(pinned.document==initial);
    auto wrong=pinned.revision;++wrong.owner.instance;
    CHECK(store.stage(wrong,candidate).result==live::EditResult::stale);
    wrong=pinned.revision;++wrong.owner.bubble;
    CHECK(store.stage(wrong,candidate).result==live::EditResult::stale);
    CHECK(store.stage(pinned.revision,{}).result==live::EditResult::invalid);
    const auto first=store.stage(pinned.revision,candidate);
    CHECK(first.result==live::EditResult::accepted);CHECK(first.change==live::Change::futureRequests);
    const auto staged=store.stage(pinned.revision,candidate); // explicit replacement invalidates the prior preview
    CHECK(store.request_apply(first.token,1)==live::EditResult::stale);
    CHECK(store.request_apply(staged.token,0)==live::EditResult::invalid);
    std::atomic_uint accepted{};
    std::array<std::thread,8> clients;
    for(auto& client:clients) { client=std::thread([&] { if(store.request_apply(staged.token,1)==live::EditResult::accepted) { ++accepted; } }); }
    for(auto& client:clients) { client.join(); }
    CHECK(accepted==1);CHECK(store.status().queued);
    CHECK(store.acquire().document==initial); // enqueueing does not mutate server policy
    CHECK(store.stage(pinned.revision,initial).result==live::EditResult::busy);
    CHECK(store.apply_pending(wrong)==live::EditResult::stale);CHECK(store.status().queued);
    CHECK(store.apply_pending(pinned.revision)==live::EditResult::accepted);
    const auto applied=store.acquire();CHECK(applied.document==candidate);CHECK(applied.revision.number>pinned.revision.number);
    CHECK(pinned.document->views().parameter("population.replacement_delay_ms")->value==45000);
    CHECK(applied.document->views().parameter("population.replacement_delay_ms")->value==1000);
    CHECK(store.previous().document==initial);
    CHECK(store.request_apply(staged.token,1)==live::EditResult::duplicate);
    CHECK(store.request_apply(staged.token,2)==live::EditResult::stale);
    CHECK(store.apply_pending(applied.revision)==live::EditResult::empty);
    // An executor keeps its old graph and authentic receipts after policy adoption.
    c::Executor executor;Service services;
    CHECK(executor.start(pinned.document->views().graph("room")->definition,1));executor.update(services);
    CHECK(executor.enqueue({executor.token("scene.ready"),c::Milestone::nativeReady}));executor.update(services);
    CHECK(executor.receipt_state("scene.ready").phase==c::StepPhase::complete);executor.cancel(services);
    // A settings rollback is another forward revision, not a rewind of native state.
    const auto rollback=store.stage(applied.revision,store.previous().document);
    CHECK(store.request_apply(rollback.token,2)==live::EditResult::accepted);
    CHECK(store.apply_pending(applied.revision)==live::EditResult::accepted);
    const auto restored=store.acquire();CHECK(restored.document==initial);CHECK(restored.revision.number>applied.revision.number);
    auto structural=original;field(graph(structural,"room"),"name").text="Changed native section";
    const auto needsRestart=store.stage(restored.revision,parse(structural));
    CHECK(needsRestart.change==live::Change::restartRequired);
    CHECK(store.request_apply(needsRestart.token,3)==live::EditResult::restartRequired);
    auto fixedPolicy=original;fixedPolicy.members.emplace_back("parameters",s::json::Reader(R"({"population.initial_requests":3})").parse());
    CHECK(store.stage(restored.revision,parse(fixedPolicy)).change==live::Change::restartRequired);
    auto reordered=original;std::reverse(reordered.members.begin(),reordered.members.end());
    const auto identical=store.stage(restored.revision,parse(reordered));CHECK(identical.change==live::Change::unchanged);
    CHECK(store.request_apply(identical.token,3)==live::EditResult::unchanged);
    auto differentWorld=original;field(differentWorld,"mission").text="other_destination";
    CHECK(store.stage(restored.revision,parse(differentWorld)).result==live::EditResult::invalid);
    const auto leaving=store.stage(restored.revision,candidate);CHECK(store.request_apply(leaving.token,3)==live::EditResult::accepted);
    CHECK(!store.end(pinned.revision));CHECK(store.end(restored.revision));CHECK(!store.acquire());
    CHECK(store.begin({7,1,15},initial)); // even a reused owner gets a fresh revision
    CHECK(store.acquire().revision.number>restored.revision.number);
    CHECK(!store.status().queued);CHECK(store.request_apply(leaving.token,4)==live::EditResult::stale);
    CHECK(store.apply_pending(restored.revision)==live::EditResult::stale);
    CHECK(pinned.document->views().graph("room")!=nullptr); // native consumer lease remains alive
    CHECK(store.end(store.acquire().revision));
}
