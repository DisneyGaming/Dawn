#include "mission_script.h"
#include "script_json.h"
#include <algorithm>
#include <fstream>

namespace sunrise::state::activity::coo::script {
namespace {
using json::Value;
constexpr std::string_view operations[]{"scene","population","objective","dialogue","device","cinematic","traversal","mechanic","observation","eventAfter","complete"};
constexpr std::string_view waits[]{"requested","nativeReady","completed","observed"};
const auto& object(const Value& v,std::size_t maximum) {
    if(v.kind!=Value::Kind::object || v.members.size()>maximum) { v.fail("expected bounded object"); }return v.members;
}
void name(std::string_view text,const Value& v) {
    if(text.empty() || std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32 || c>126;})) { v.fail("name must be nonempty printable ASCII"); }
}
std::string_view name(const Value& v) { const auto& result=v.string();name(result,v);return result; }
std::uint32_t hex(const Value& v) {
    const auto& text=v.string();std::uint32_t result{};
    if(text.size()!=10 || text.substr(0,2)!="0x") { v.fail("expected 0x plus eight hexadecimal digits"); }
    const auto end=text.data()+text.size();const auto parsed=std::from_chars(text.data()+2,end,result,16);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end) { v.fail("invalid hexadecimal identity"); }return result;
}
Asset asset(const Value& v) {
    v.fields({"registry","definition","type","slot"});
    return {hex(v.at("registry")),hex(v.at("definition")),static_cast<std::uint16_t>(v.at("type").integer(UINT16_MAX)),static_cast<std::uint16_t>(v.at("slot").integer(UINT16_MAX))};
}
bool same(const CommandSpec& a,const CommandSpec& b) { return a.operation==b.operation && a.asset==b.asset && a.argument==b.argument && a.wait==b.wait; }
std::size_t enumeration(const Value& v,std::span<const std::string_view> values) {
    const auto& n=v.string();for(std::size_t i=0;i<values.size();++i) { if(n==values[i]) { return i; } }v.fail("unknown operation or wait");
}
template<class T> const T& lookup(std::span<const T> values,std::string_view id,const Value& source) {
    const T* result{};
    for(const auto& value:values) { if(value.id==id) { if(result) { source.fail("ambiguous native capability"); }result=&value; } }
    if(!result) { source.fail("unregistered native name '"+std::string(id)+"'"); }return *result;
}
bool objective(const Profile& profile,std::uint32_t value) {
    return std::find(profile.objectives.begin(),profile.objectives.end(),value)!=profile.objectives.end();
}
void validate_profile(const Profile& p,const Value& root) {
    name(p.id,root);name(p.schemaName,root);
    if(p.schema==Schema::unspecified || p.capabilities.empty() || p.capabilities.size()>4096 || p.modules.size()>8
        || p.facts.size()>32 || p.dialogue.rows.size()>64 || p.objectives.size()>256 || p.events.size()>1024 || p.tables.size()>32) { root.fail("invalid native profile limits"); }
    for(std::size_t i=0;i<p.capabilities.size();++i) {
        const auto& cap=p.capabilities[i];name(cap.id,root);name(cap.domain,root);
        if(static_cast<unsigned>(cap.spec.operation)>=std::size(operations) || static_cast<unsigned>(cap.spec.wait)>=std::size(waits)
            || (is_observation(cap.spec.operation))!=(cap.spec.wait==Wait::observed)
            || (cap.argumentMaximum && (cap.spec.operation!=Operation::eventAfter || cap.spec.argument>cap.argumentMaximum))) { root.fail("invalid native operation contract"); }
        for(std::size_t j=0;j<i;++j) { if(cap.id==p.capabilities[j].id) { root.fail("duplicate native capability"); } }
    }
    for(std::size_t i=0;i<p.modules.size();++i) {
        name(p.modules[i].id,root);
        for(std::size_t j=0;j<i;++j) { if(p.modules[i].id==p.modules[j].id || p.modules[i].binding.id==p.modules[j].binding.id || p.modules[i].binding.asset==p.modules[j].binding.asset) { root.fail("duplicate native module"); } }
    }
    for(std::size_t i=0;i<p.facts.size();++i) {
        name(p.facts[i].id,root);if(p.facts[i].fact>=32) { root.fail("native fact outside supported range"); }
        for(std::size_t j=0;j<i;++j) { if(p.facts[i].id==p.facts[j].id || p.facts[i].fact==p.facts[j].fact) { root.fail("duplicate native fact"); } }
    }
    for(std::size_t i=0;i<p.events.size();++i) {
        name(p.events[i].set,root);name(p.events[i].id,root);
        for(std::size_t j=0;j<i;++j) { if(p.events[i].set==p.events[j].set && (p.events[i].id==p.events[j].id || p.events[i].event==p.events[j].event)) { root.fail("ambiguous native presentation event"); } }
    }
    if(p.markers.size()>256) { root.fail("too many native marker targets"); }
    for(std::size_t i=0;i<p.markers.size();++i) {
        name(p.markers[i].id,root);if(!p.markers[i].target.valid()) { root.fail("invalid native marker target"); }
        for(std::size_t j=0;j<i;++j) { if(p.markers[j].id==p.markers[i].id) { root.fail("ambiguous native marker target"); } }
    }
    for(std::size_t i=0;i<p.tables.size();++i) {
        name(p.tables[i].id,root);if(p.tables[i].bindings.schema!=p.schema) { root.fail("native presentation schema mismatch"); }
        for(std::size_t j=0;j<i;++j) { if(p.tables[i].id==p.tables[j].id) { root.fail("duplicate native presentation table"); } }
    }
}
}
struct MissionDocument::Storage final {
    // Keep parsed strings alive. All views are assembled after their owning
    // vectors have their final sizes; the document is never moved or reloaded.
    Value root;
    struct AssetItem { std::string_view id;Asset value; };
    struct BindingItem { std::string_view id,capability,domain;CommandSpec spec; };
    struct Graph {
        std::vector<Step> steps;
        std::vector<std::vector<CommandSpec>> commands;
        std::vector<CommandBinding> bindings;
        std::vector<ReceiptBinding> receipts;
    };
    struct Table {
        std::vector<TraversalBinding> traversal;
        std::vector<ObjectiveBinding> objectives;
        std::vector<DialogueBinding> dialogue;
    };
    std::vector<AssetItem> assets;
    std::vector<BindingItem> bindings;
    std::vector<Graph> graphs;
    std::vector<GraphView> graphViews;
    std::vector<RoleView> roles;
    std::vector<ModuleBinding> modules;
    std::vector<ObservationBinding> observations;
    std::vector<DialogueRow> rows;
    std::vector<ObjectiveCueBinding> objectiveCues;
    std::vector<ObjectiveMarker> markers;
    std::vector<std::vector<PresentationCue>> cueLists;
    std::vector<std::vector<PresentationAction>> actionLists;
    std::vector<CueSet> cueSets;
    std::vector<ActionSet> actionSets;
    std::vector<Table> tables;
    std::vector<PresentationTable> tableViews;
    Views views;
    std::uint64_t fingerprint{};
    Asset named_asset(const Value& value) const { return lookup(std::span<const AssetItem>(assets),name(value),value).value; }
    void load_graph(std::size_t ordinal,std::string_view id,const Value& input,const Profile& profile) {
        input.fields({"name","domain","steps","receipts"});name(id,input);const auto domain=name(input.at("domain"));
        auto& graph=graphs[ordinal];const auto& steps=input.at("steps").array(Executor::kMaxSteps);
        if(steps.empty()) { input.fail("graph requires at least one step"); }
        const auto count=steps.size();graph.steps.resize(count);graph.commands.resize(count);
        std::vector<std::uint32_t> dependencies(count);std::vector<std::size_t> order;
        std::vector<std::uint8_t> destination(count,UINT8_MAX);
        for(std::size_t i=0;i<count;++i) {
            const auto& step=steps[i];step.fields({"id","after","commands"});const auto idValue=name(step.at("id"));
            for(std::size_t j=0;j<i;++j) { if(idValue==steps[j].at("id").string()) { step.fail("duplicate step id"); } }
            for(const auto& after:step.at("after").array(Executor::kMaxSteps)) {
                const auto target=name(after);std::size_t j=0;for(;j<count && steps[j].at("id").string()!=target;++j) {}
                if(j==count || j==i || (dependencies[i]&(1U<<j))) { after.fail("missing, self, or duplicate dependency"); }dependencies[i]|=1U<<j;
            }
        }
        // Stable topological order accepts forward references. Runtime still
        // receives its bounded earlier-step dependency masks.
        std::uint32_t completed{};
        while(order.size()!=count) {
            std::size_t i=0;for(;i<count;++i) { if(!(completed&(1U<<i)) && (dependencies[i]&completed)==dependencies[i]) { break; } }
            if(i==count) { input.fail("dependency cycle"); }
            destination[i]=static_cast<std::uint8_t>(order.size());order.push_back(i);completed|=1U<<i;
        }
        for(std::size_t n=0;n<count;++n) {
            const auto i=order[n];auto& output=graph.steps[n];output.name=steps[i].at("id").string();
            for(std::size_t j=0;j<count;++j) { if(dependencies[i]&(1U<<j)) { output.dependencies|=1U<<destination[j]; } }
            const auto& commands=steps[i].at("commands").array(Executor::kMaxCommands);
            if(commands.empty()) { steps[i].fail("step requires commands"); }
            for(std::size_t c=0;c<commands.size();++c) {
                const auto& cmd=commands[c];cmd.fields({"id","binding"});const auto commandId=name(cmd.at("id"));
                for(const auto& prior:graph.bindings) { if(prior.id==commandId) { cmd.fail("duplicate command id in graph"); } }
                const auto& binding=lookup(std::span<const BindingItem>(bindings),name(cmd.at("binding")),cmd);
                if(binding.domain!=domain) { cmd.fail("native operation belongs to a different service domain"); }
                graph.commands[n].push_back(binding.spec);
                graph.bindings.push_back({commandId,binding.capability,static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(c)});
            }
            output.commands=graph.commands[n];
        }
        auto& view=graphViews[ordinal];view={id,domain,{name(input.at("name")),profile.schema,graph.steps},graph.bindings};
        for(const auto& [receipt,target]:object(input.at("receipts"),256)) {
            name(receipt,target);const auto* command=view.command(name(target));
            if(!command) { target.fail("receipt refers to missing command"); }
            graph.receipts.push_back({receipt,command->step,command->command});
        }
        view.definition.receipts=graph.receipts;
        if(!Executor::valid(view.definition)) { input.fail("invalid executor graph or ambiguous receipt"); }
        for(const auto& binding:graph.bindings) {
            const auto& spec=graph.steps[binding.step].commands[binding.command];
            if(spec.wait==Wait::requested) { continue; }
            bool found{};for(const auto& receipt:graph.receipts) { found|=receipt.step==binding.step && receipt.command==binding.command; }
            if(!found) { input.fail("waiting command requires a named receipt"); }
        }
    }
    std::vector<PresentationAction> actions(const Value& list,const Profile& profile) {
        std::vector<PresentationAction> result;
        for(const auto& v:list.array(16)) {
            v.fields({"operation","value","delay_ms"});PresentationAction action{
                static_cast<Operation>(enumeration(v.at("operation"),operations)),v.at("value").integer(),v.at("delay_ms").integer(300000)};
            if(action.operation==Operation::objective) {
                if(!objective(profile,action.value) || action.delayMs) { v.fail("unknown objective or unsupported objective delay"); }
            } else if(action.operation==Operation::dialogue) {
                if(action.value>=rows.size() || rows[action.value].sceneOwned) { v.fail("unknown or scene-owned dialogue row"); }
            } else { v.fail("presentation action must be dialogue or objective"); }
            result.push_back(action);
        }
        return result;
    }
    void presentation(const Value& v,const Profile& profile) {
        bool hasMarkers{};for(const auto& item:v.members) { hasMarkers|=item.first=="markers"; }
        if(hasMarkers) {
            v.fields({"dialogue","cue_sets","action_sets","binding_tables","markers"});
            for(const auto& item:v.at("markers").array(256)) {
                item.fields({"objective","target"});const auto event=hex(item.at("objective"));
                if(!objective(profile,event)) { item.fail("marker objective is not registered"); }
                for(const auto& prior:markers) { if(prior.event==event) { item.fail("duplicate objective marker"); } }
                const auto& cap=lookup(profile.markers,name(item.at("target")),item);
                if(!cap.target.valid()) { item.fail("invalid native marker target"); }markers.push_back({event,cap.target});
            }
            views.markers=markers;
        } else { v.fields({"dialogue","cue_sets","action_sets","binding_tables"}); }
        const auto& dialogue=v.at("dialogue");dialogue.fields({"bank","rows","objective_cues","dispatch_timeout_ms","spacing_ms"});
        if(hex(dialogue.at("bank"))!=profile.dialogue.bank) { dialogue.fail("unregistered dialogue bank"); }
        const auto& rowList=dialogue.at("rows").array(64);rows.resize(profile.dialogue.rows.size());
        if(rowList.size()!=rows.size()) { dialogue.fail("dialogue rows do not match native bank"); }
        std::uint64_t seen{};
        for(const auto& row:rowList) {
            row.fields({"row","selector","duration_ms","native_delay_ms","scene_owned"});const auto i=row.at("row").integer(63);
            if(i>=rows.size() || (seen&(1ULL<<i))) { row.fail("unknown or duplicate dialogue row"); }seen|=1ULL<<i;
            rows[i]={hex(row.at("selector")),row.at("duration_ms").integer(300000),row.at("native_delay_ms").integer(),row.at("scene_owned").flag()};
            const auto& native=profile.dialogue.rows[i];
            if(rows[i].selector!=native.selector || rows[i].delayMs!=native.delayMs || rows[i].sceneOwned!=native.sceneOwned) { row.fail("native dialogue identity or ownership changed"); }
        }
        seen=0;
        for(const auto& cue:dialogue.at("objective_cues").array(64)) {
            cue.fields({"row","objective"});const auto row=cue.at("row").integer(63);const auto event=hex(cue.at("objective"));
            if(row>=rows.size() || rows[row].sceneOwned || (seen&(1ULL<<row)) || !objective(profile,event)) { cue.fail("unknown or duplicate objective cue"); }seen|=1ULL<<row;
            objectiveCues.push_back({static_cast<std::uint8_t>(row),event});
        }
        const auto timeout=dialogue.at("dispatch_timeout_ms").integer(300000);if(timeout<100) { dialogue.fail("dispatch timeout must be at least 100 ms"); }
        views.dialogue={profile.dialogue.bank,rows,objectiveCues,timeout,dialogue.at("spacing_ms").integer(60000)};
        const auto& sets=object(v.at("cue_sets"),32);cueLists.resize(sets.size());cueSets.resize(sets.size());
        // At most 32*64 cue actions plus 64 standalone lists. Reserve so spans
        // remain valid even on implementations without vector move guarantees.
        actionLists.reserve(32*64+64);
        for(std::size_t i=0;i<sets.size();++i) {
            const auto& [set,list]=sets[i];name(set,list);
            bool supported{};for(const auto& event:profile.events) { supported|=event.set==set; }
            if(!supported) { list.fail("unregistered presentation event set"); }
            for(const auto& cue:list.array(64)) {
                cue.fields({"event","cycles","actions"});const auto eventName=name(cue.at("event"));const EventCapability* event{};
                for(const auto& candidate:profile.events) { if(candidate.set==set && candidate.id==eventName) { event=&candidate; } }
                if(!event) { cue.fail("unregistered presentation event"); }
                std::uint8_t cycles{};
                for(const auto& cycle:cue.at("cycles").array(8)) {
                    const auto n=cycle.integer(8);if(!n || (cycles&(1U<<(n-1)))) { cycle.fail("invalid or duplicate cycle"); }cycles|=static_cast<std::uint8_t>(1U<<(n-1));
                }
                if((cycles & ~event->allowedCycles)!=0 || (event->allowedCycles!=0 && cycles==0)) { cue.fail("unsupported event cycle"); }
                actionLists.push_back(actions(cue.at("actions"),profile));cueLists[i].push_back({event->event,cycles,actionLists.back()});
            }
            cueSets[i]={set,cueLists[i]};
        }
        for(const auto& [set,list]:object(v.at("action_sets"),64)) { name(set,list);actionLists.push_back(actions(list,profile));actionSets.push_back({set,actionLists.back()}); }
        const auto& tableList=object(v.at("binding_tables"),32);tables.resize(tableList.size());tableViews.resize(tableList.size());
        for(std::size_t i=0;i<tableList.size();++i) {
            const auto& [id,input]=tableList[i];name(id,input);input.fields({"traversal","objectives","dialogue"});
            const auto& native=lookup(profile.tables,id,input).bindings;auto& table=tables[i];
            for(const auto& item:input.at("traversal").array(256)) {
                item.fields({"asset","stage"});TraversalBinding value{named_asset(item.at("asset")),static_cast<std::uint8_t>(item.at("stage").integer(UINT8_MAX))};
                bool found{};for(const auto& cap:native.traversal) { found|=cap.asset==value.asset && cap.stage==value.stage; }
                for(const auto& prior:table.traversal) { if(prior.asset==value.asset) { item.fail("duplicate traversal asset"); } }
                if(!found) { item.fail("unregistered traversal binding"); }table.traversal.push_back(value);
            }
            for(const auto& item:input.at("objectives").array(256)) {
                item.fields({"asset","event"});ObjectiveBinding value{named_asset(item.at("asset")),hex(item.at("event"))};
                bool found{};for(const auto& cap:native.objectives) { found|=cap.asset==value.asset && cap.event==value.event; }
                for(const auto& prior:table.objectives) { if(prior.asset==value.asset) { item.fail("duplicate objective asset"); } }
                if(!found) { item.fail("unregistered objective binding"); }table.objectives.push_back(value);
            }
            for(const auto& item:input.at("dialogue").array(64)) {
                item.fields({"asset","row","delay_ms"});DialogueBinding value{named_asset(item.at("asset")),static_cast<std::uint8_t>(item.at("row").integer(63)),item.at("delay_ms").integer(300000)};
                bool found{};for(const auto& cap:native.dialogue) { found|=cap.asset==value.asset && cap.row==value.row; }
                for(const auto& prior:table.dialogue) { if(prior.asset==value.asset) { item.fail("duplicate dialogue asset"); } }
                if(!found || value.row>=rows.size() || rows[value.row].sceneOwned) { item.fail("unregistered dialogue binding"); }table.dialogue.push_back(value);
            }
            tableViews[i]={id,{profile.schema,table.traversal,table.objectives,table.dialogue}};
        }
        views.cueSets=cueSets;views.actionSets=actionSets;views.tables=tableViews;
    }
    void load(const Profile& profile) {
        root.fields({"format_version","mission","profile","authority_schema","assets","bindings","graphs","roles","entry","modules","observations","presentation"});
        if(root.at("format_version").integer()!=2) { root.fail("unsupported mission format; expected version 2"); }
        validate_profile(profile,root);views.missionId=name(root.at("mission"));views.profileId=name(root.at("profile"));
        if(views.profileId!=profile.id || name(root.at("authority_schema"))!=profile.schemaName) { root.fail("native profile or schema mismatch"); }
        for(const auto& [id,value]:object(root.at("assets"),2048)) {
            name(id,value);const auto key=asset(value);bool allowed{};
            for(const auto& cap:profile.capabilities) { allowed|=cap.spec.asset==key; }
            if(!allowed) { value.fail("asset has no registered native capability"); }assets.push_back({id,key});
        }
        for(const auto& [id,value]:object(root.at("bindings"),4096)) {
            name(id,value);value.fields({"capability","operation","asset","argument","wait"});
            const auto key=name(value.at("capability"));const auto& cap=lookup(profile.capabilities,key,value);
            CommandSpec spec{static_cast<Operation>(enumeration(value.at("operation"),operations)),named_asset(value.at("asset")),value.at("argument").integer(),static_cast<Wait>(enumeration(value.at("wait"),waits))};
            if(!(same(spec,cap.spec) || (cap.argumentMaximum && spec.operation==Operation::eventAfter && spec.asset==cap.spec.asset && spec.wait==cap.spec.wait && spec.argument>0 && spec.argument<=cap.argumentMaximum))) { value.fail("operation does not match registered native capability"); }
            bindings.push_back({id,key,cap.domain,spec});
        }
        const auto& graphList=object(root.at("graphs"),64);if(graphList.empty()) { root.fail("mission requires graphs"); }
        graphs.resize(graphList.size());graphViews.resize(graphList.size());
        for(std::size_t i=0;i<graphList.size();++i) { load_graph(i,graphList[i].first,graphList[i].second,profile); }
        views.graphs=graphViews;
        for(const auto& [role,value]:object(root.at("roles"),64)) {
            name(role,value);const auto* target=views.graph(name(value));if(!target) { value.fail("role refers to missing graph"); }roles.push_back({role,target});
        }
        views.roles=roles;const auto* entry=views.graph(name(root.at("entry")));if(!entry) { root.fail("entry graph is missing"); }
        for(const auto& module:root.at("modules").array(8)) { modules.push_back(lookup(profile.modules,name(module),module).binding); }
        for(const auto& observation:root.at("observations").array(32)) {
            observation.fields({"fact","receipt"});const auto fact=lookup(profile.facts,name(observation.at("fact")),observation).fact;
            const auto target=name(observation.at("receipt"));const ReceiptBinding* resolved{};
            for(const auto& receipt:entry->definition.receipts) { if(receipt.name==target) { resolved=&receipt; } }
            if(!resolved || entry->definition.steps[resolved->step].commands[resolved->command].operation!=Operation::observation
                || entry->definition.steps[resolved->step].commands[resolved->command].argument!=fact) { observation.fail("fact does not match named observation receipt"); }
            observations.push_back({fact,resolved->step,resolved->command});
        }
        views.mission={entry->definition,modules,observations};if(!MissionRuntime::valid(views.mission)) { root.fail("invalid mission modules or observation mapping"); }
        presentation(root.at("presentation"),profile);views.valid=true;
    }
};
MissionDocument::MissionDocument():storage_(std::make_unique<Storage>()) {}
MissionDocument::~MissionDocument()=default;
const Views& MissionDocument::views() const noexcept { return storage_->views; }
std::uint64_t MissionDocument::fingerprint() const noexcept { return storage_->fingerprint; }
std::unique_ptr<MissionDocument> MissionDocument::parse(std::string_view text,const Profile& profile,std::string& error) noexcept {
    try {
        error.clear();auto document=std::unique_ptr<MissionDocument>(new MissionDocument);
        document->storage_->root=json::Reader(text).parse();document->storage_->load(profile);
        auto hash=UINT64_C(14695981039346656037);for(const unsigned char c:text) { hash^=c;hash*=UINT64_C(1099511628211); }document->storage_->fingerprint=hash;
        return document;
    } catch(const std::exception& exception) { error=exception.what();return {}; }
}
std::unique_ptr<MissionDocument> MissionDocument::read(const std::filesystem::path& path,const Profile& profile,std::string& error) noexcept {
    try {
        std::ifstream stream(path,std::ios::binary);if(!stream) { error="cannot open mission script";return {}; }
        std::string text(1048577,'\0');stream.read(text.data(),static_cast<std::streamsize>(text.size()));text.resize(static_cast<std::size_t>(stream.gcount()));
        if(stream.bad()) { error="cannot read mission script";return {}; }return parse(text,profile,error);
    } catch(const std::exception& exception) { error=exception.what();return {}; }
}
} // namespace sunrise::state::activity::coo::script
