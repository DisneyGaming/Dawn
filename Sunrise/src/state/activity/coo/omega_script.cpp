#include "omega_script.h"
#include "omega_definition.h"
#include "omega_opening.h"
#include "omega_forest.h"
#include "omega_ending_definition.h"
#include <stdexcept>
#include <vector>

namespace sunrise::state::activity::coo::script {
namespace {
namespace p=omega_presentation;
struct Contract { std::string_view role;const Definition* definition; };
const Contract contracts[]{
    {"mission",&omega::kMission.sequence},{"opening",&omega::opening::kDefinition},{"forest",&omega::forest::kDefinition},
    {"reveal",&reveal::kDefinition},{"reveal_retry",&reveal::kRetry},
    {"lair",combat::kSections[0]},{"island_a",combat::kSections[1]},{"island_b",combat::kSections[2]},
    {"island_c",combat::kSections[3]},{"crown_1",combat::kSections[4]},{"crown_2",combat::kSections[5]},
    {"crown_3",combat::kSections[6]},{"ending",&ending::kDefinition},{"ending_retry",&ending::kRetry}
};
std::string capability_name(const Contract& contract,const Step& step,std::size_t command) {
    return std::string(contract.role)+"/"+std::string(step.name)+"/"+std::to_string(command);
}
struct NativeProfile final {
    std::vector<std::string> names;
    std::vector<Capability> capabilities;
    const ModuleCapability modules[3]{{"presentation",omega::kModules[0]},{"panoptes",omega::kModules[1]},{"ending",omega::kModules[2]}};
    const FactCapability facts[8]{{"opening.complete",0},{"forest.complete",1},{"lair.complete",2},{"crown.1.complete",3},
        {"crown.2.complete",4},{"crown.3.complete",5},{"ending.complete",6},{"handoff.queued",7}};
    const EventCapability events[16]{
        {"landmarks","lighthouse",0,0},{"landmarks","tunnel",1,0},{"landmarks","forestVista",2,0},{"landmarks","forestExit",3,0},{"landmarks","lair",4,0},{"landmarks","arena",5,0},
        {"encounters","defenses",0,7},{"encounters","deletion",1,7},{"encounters","osirisArrives",2,7},{"encounters","osirisHolds",3,7},{"encounters","arcReady",4,7},
        {"encounters","arcReminder",5,7},{"encounters","eyeVulnerable",6,7},{"encounters","pursuit",7,7},{"encounters","defeated",8,7},{"encounters","cinematic",9,7}};
    const PresentationTable tables[1]{{"forest",omega::forest::kBindings}};
    Profile profile;
    NativeProfile() {
        std::size_t count{};for(const auto& contract:contracts) { for(const auto& step:contract.definition->steps) { count+=step.commands.size(); } }
        names.reserve(count);capabilities.reserve(count);
        for(const auto& contract:contracts) {
            for(const auto& step:contract.definition->steps) {
                for(std::size_t i=0;i<step.commands.size();++i) {
                    names.push_back(capability_name(contract,step,i));capabilities.push_back({names.back(),contract.role,step.commands[i]});
                }
            }
        }
        profile={"omega.archive.v1","omegaArchive",Schema::omegaArchive,capabilities,modules,facts,p::kDialogueDefinition,p::kObjectives,events,tables};
    }
};
[[noreturn]] void fail(std::string_view role,std::string_view reason) {
    throw std::runtime_error("native Omega role '"+std::string(role)+"': "+std::string(reason));
}
// These are constraints of Omega's native state machines, not JSON parser
// rules. Roles, phase capabilities and receipt names survive graph/step renames
// and independent-step reorderings. The generic compiler knows none of them.
void validate_native(const Views& views) {
    if(views.missionId!="omega") { fail("mission","wrong mission selected for this adapter"); }
    for(const auto& contract:contracts) {
        const auto* graph=views.role(contract.role);if(!graph || graph->domain!=contract.role) { fail(contract.role,"required service role missing or wrong domain"); }
        const auto& native=*contract.definition;std::vector<std::uint8_t> phases;
        for(const auto& step:native.steps) {
            std::uint8_t phase=UINT8_MAX;
            for(std::size_t c=0;c<step.commands.size();++c) {
                const auto id=capability_name(contract,step,c);const CommandBinding* found{};
                for(const auto& command:graph->commands) {
                    if(command.capability==id) { if(found) { fail(contract.role,"stateful capability used more than once"); }found=&command; }
                }
                if(!found) { fail(contract.role,"required native phase capability missing"); }
                if(phase!=UINT8_MAX && phase!=found->step) { fail(contract.role,"native phase activation and its receipt must be armed together"); }
                phase=found->step;
            }
            phases.push_back(phase);
        }
        // Qualify prerequisites by capability ownership, not authored positions.
        for(std::size_t i=0;i<native.steps.size();++i) {
            auto ancestors=graph->definition.steps[phases[i]].dependencies;
            for(std::size_t k=graph->definition.steps.size();k-->0;) {
                if(ancestors&(1U<<k)) { ancestors|=graph->definition.steps[k].dependencies; }
            }
            for(std::size_t j=0;j<i;++j) {
                if((native.steps[i].dependencies&(1U<<j)) && !(ancestors&(1U<<phases[j]))) { fail(contract.role,"required native phase prerequisite removed"); }
            }
        }
        for(const auto& required:native.receipts) {
            const auto expected=capability_name(contract,native.steps[required.step],required.command);
            const ReceiptBinding* found{};
            for(const auto& receipt:graph->definition.receipts) { if(receipt.name==required.name) { found=&receipt; } }
            if(!found) { fail(contract.role,"required named receipt missing"); }
            bool match{};for(const auto& command:graph->commands) { match|=command.step==found->step && command.command==found->command && command.capability==expected; }
            if(!match) { fail(contract.role,"named receipt bound to the wrong native capability"); }
        }
    }
    const auto* entry=views.role("mission");
    if(!entry || entry->definition.steps.data()!=views.mission.sequence.steps.data() || views.mission.modules.size()!=3) { fail("mission","wrong composition entry or modules"); }
    for(std::size_t i=0;i<3;++i) {
        if(views.mission.modules[i].id!=omega::kModules[i].id || views.mission.modules[i].asset!=omega::kModules[i].asset) { fail("mission","native producer order changed"); }
    }
    const auto* table=views.table("forest");
    if(!table || table->traversal.size()!=omega::forest::kTraversal.size() || table->objectives.size()!=omega::forest::kObjectives.size() || table->dialogue.size()!=omega::forest::kDialogue.size()) { fail("forest","required native presentation binding missing"); }
}
}
const Profile& omega_profile() { static const NativeProfile owner;return owner.profile; }
struct Document::Storage { std::unique_ptr<MissionDocument> document; };
Document::Document():storage_(std::make_unique<Storage>()) {}
Document::~Document()=default;
const Views& Document::views() const noexcept { return storage_->document->views(); }
std::uint64_t Document::fingerprint() const noexcept { return storage_->document->fingerprint(); }
std::unique_ptr<Document> Document::parse(std::string_view text,std::string& error) noexcept {
    try {
        auto result=std::unique_ptr<Document>(new Document);result->storage_->document=MissionDocument::parse(text,omega_profile(),error);
        if(!result->storage_->document) { return {}; }validate_native(result->views());return result;
    } catch(const std::exception& exception) { error=exception.what();return {}; }
}
std::unique_ptr<Document> Document::read(const std::filesystem::path& path,std::string& error) noexcept {
    try {
        auto result=std::unique_ptr<Document>(new Document);result->storage_->document=MissionDocument::read(path,omega_profile(),error);
        if(!result->storage_->document) { return {}; }validate_native(result->views());return result;
    } catch(const std::exception& exception) { error=exception.what();return {}; }
}
} // namespace sunrise::state::activity::coo::script
