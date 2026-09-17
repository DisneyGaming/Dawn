#pragma once
#include "state/activity/coo/script_views.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

// Serialize public, validated semantics, never addresses or source formatting.
// Graph/receipt/role map order is irrelevant; publication and action order is not.
namespace mission_test {
namespace c=dawn::state::activity::coo;
struct Semantics {
    std::ostringstream out;
    template<class... T> void row(const T&... v) { ((out<<v<<' '),...);out<<'\n'; }
    void asset(c::Asset a) { row(a.registry,a.definition,a.type,a.slot); }
    void actions(std::span<const c::PresentationAction> list) {
        row(list.size());for(const auto& a:list) { row(static_cast<unsigned>(a.operation),a.value,a.delayMs); }
    }
    void definition(const c::Definition& d) {
        row(std::quoted(std::string(d.name)),static_cast<unsigned>(d.schema),d.steps.size());
        for(const auto& s:d.steps) {
            row(std::quoted(std::string(s.name)),s.dependencies,s.commands.size());
            for(const auto& cmd:s.commands) { row(static_cast<unsigned>(cmd.operation),cmd.argument,static_cast<unsigned>(cmd.wait));asset(cmd.asset); }
        }
        std::vector<c::ReceiptBinding> receipts(d.receipts.begin(),d.receipts.end());
        std::sort(receipts.begin(),receipts.end(),[](const auto& a,const auto& b){return a.name<b.name;});
        row(receipts.size());for(const auto& r:receipts) { row(std::quoted(std::string(r.name)),unsigned(r.step),unsigned(r.command)); }
    }
};
inline std::string semantics(const c::script::Views& v) {
    Semantics s;s.row(v.valid,std::quoted(std::string(v.missionId)),std::quoted(std::string(v.profileId)));
    std::vector<const c::script::GraphView*> graphs;for(const auto& g:v.graphs) { graphs.push_back(&g); }
    std::sort(graphs.begin(),graphs.end(),[](auto a,auto b){return a->id<b->id;});s.row(graphs.size());
    for(const auto* g:graphs) {
        s.row(std::quoted(std::string(g->id)),std::quoted(std::string(g->domain)));s.definition(g->definition);s.row(g->commands.size());
        for(const auto& cmd:g->commands) { s.row(std::quoted(std::string(cmd.id)),std::quoted(std::string(cmd.capability)),unsigned(cmd.step),unsigned(cmd.command)); }
    }
    std::vector<c::script::RoleView> roles(v.roles.begin(),v.roles.end());
    std::sort(roles.begin(),roles.end(),[](const auto& a,const auto& b){return a.id<b.id;});s.row(roles.size());
    for(const auto& r:roles) { s.row(std::quoted(std::string(r.id)),std::quoted(std::string(r.graph->id))); }
    s.definition(v.mission.sequence);s.row(v.mission.modules.size());
    for(const auto& m:v.mission.modules) { s.row(m.id);s.asset(m.asset); }
    s.row(v.mission.observations.size());for(const auto& o:v.mission.observations) { s.row(unsigned(o.fact),unsigned(o.step),unsigned(o.command)); }
    s.row(v.dialogue.bank,v.dialogue.dispatchTimeoutMs,v.dialogue.spacingMs,v.dialogue.rows.size());
    for(const auto& r:v.dialogue.rows) { s.row(r.selector,r.durationMs,r.delayMs,r.sceneOwned); }
    s.row(v.dialogue.objectiveCues.size());for(const auto& cue:v.dialogue.objectiveCues) { s.row(unsigned(cue.row),cue.objective); }
    s.row(v.cueSets.size());for(const auto& set:v.cueSets) {
        s.row(std::quoted(std::string(set.id)),set.cues.size());for(const auto& cue:set.cues) { s.row(cue.event,unsigned(cue.cycles));s.actions(cue.actions); }
    }
    s.row(v.actionSets.size());for(const auto& set:v.actionSets) { s.row(std::quoted(std::string(set.id)));s.actions(set.actions); }
    s.row(v.tables.size());for(const auto& t:v.tables) {
        s.row(std::quoted(std::string(t.id)),static_cast<unsigned>(t.bindings.schema),t.bindings.traversal.size());
        for(const auto& b:t.bindings.traversal) { s.asset(b.asset);s.row(unsigned(b.stage)); }
        s.row(t.bindings.objectives.size());for(const auto& b:t.bindings.objectives) { s.asset(b.asset);s.row(b.event); }
        s.row(t.bindings.dialogue.size());for(const auto& b:t.bindings.dialogue) { s.asset(b.asset);s.row(unsigned(b.row),b.delayMs); }
    }
    s.row(v.markers.size());for(const auto& m:v.markers) { s.row(m.event);s.asset(m.target.asset);for(auto l:m.target.locator) { s.row(l); } }
    return s.out.str();
}
}
