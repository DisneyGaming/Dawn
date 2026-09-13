#include "state/activity/coo/mission_script.h"
#include "state/activity/coo/omega_script.h"
#include "state/activity/deadly_trial/profile.h"
#include "fixtures/mission_semantics.h"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
namespace c=sunrise::state::activity::coo;
namespace s=c::script;
namespace trial=sunrise::state::activity::deadly_trial;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
std::string read(const char* path) { std::ifstream file(path,std::ios::binary);CHECK(file.good());return {std::istreambuf_iterator<char>(file),{}}; }
void rejects(std::string_view source,std::string_view expected={}) {
    std::string error;auto doc=s::MissionDocument::parse_lua(source,trial::kProfile,error,"bad-mission.lua");
    CHECK(!doc);CHECK(error.find("bad-mission.lua")!=std::string::npos);
    if(!expected.empty() && error.find(expected)==std::string::npos) { std::fprintf(stderr,"Expected %.*s; got %s\n",static_cast<int>(expected.size()),expected.data(),error.c_str());CHECK(false); }
    CHECK(!error.empty());
}
void omega_lua_documents() {
    std::string error;
    auto baseline=s::MissionDocument::parse_lua(read("Sunrise/scripts/omega.lua"),s::omega_profile(),error);
    if(!baseline) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(baseline);
    auto authored=s::MissionDocument::read("Sunrise/scripts/omega.lua",s::omega_profile(),error);
    if(!authored) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(authored);CHECK(error.empty());
    CHECK(mission_test::semantics(baseline->views())==mission_test::semantics(authored->views()));
    CHECK(authored->views().graphs.size()==14);CHECK(authored->views().roles.size()==14);
    CHECK(authored->views().mission.modules.size()==3);CHECK(authored->views().mission.observations.size()==8);
    CHECK(authored->views().phases.empty());CHECK(authored->views().conditions.empty());CHECK(!authored->views().observationStart);
    auto adapter=s::Document::read("Sunrise/scripts/omega.lua",error);CHECK(adapter);
    CHECK(mission_test::semantics(adapter->views())==mission_test::semantics(authored->views()));
    CHECK(authored->fingerprint()==baseline->fingerprint());
}
int main() {
    omega_lua_documents();
    const auto source=read("Sunrise/scripts/deadly_trial.lua");std::string error;
    auto lua=s::MissionDocument::read("Sunrise/scripts/deadly_trial.lua",trial::kProfile,error);
    if(!lua) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(lua);CHECK(error.empty());
    const auto expected=mission_test::semantics(lua->views());
    CHECK(lua->views().phases.size()==2);CHECK(!lua->views().observationStart);CHECK(!lua->views().conditions.empty());
    CHECK(s::authorized(lua->views(),trial::kProfile));
    // File loading is Lua-only and checks extensions before any source fallback.
    for(const auto* path:{"legacy.json","misleading.lua.json","mission.txt","extensionless"}) {
        CHECK(!s::MissionDocument::read(path,trial::kProfile,error));CHECK(error.find(".lua extension")!=std::string::npos);
    }
    const auto mixedCase=std::filesystem::path("build/coo")/("extension-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".LuA");
    std::filesystem::create_directories(mixedCase.parent_path());
    { std::ofstream file(mixedCase,std::ios::binary);CHECK(file.good());file<<source; }
    auto mixed=s::MissionDocument::read(mixedCase,trial::kProfile,error);std::filesystem::remove(mixedCase);CHECK(mixed);
    CHECK(mission_test::semantics(mixed->views())==expected);
    rejects(R"({"format_version":2,"mission":"not_lua"})");
    // User chunks are isolated, text-only and have no filesystem/native escape.
    const std::string restricted="assert(os==nil and io==nil and package==nil and debug==nil and require==nil and load==nil and loadfile==nil and dofile==nil and collectgarbage==nil and coroutine==nil and pcall==nil and xpcall==nil and getmetatable==nil and setmetatable==nil)\n";
    auto restrictedDoc=s::MissionDocument::parse_lua(restricted+source,trial::kProfile,error);CHECK(restrictedDoc);
    CHECK(mission_test::semantics(restrictedDoc->views())==expected);
    auto privateDoc=s::MissionDocument::parse_lua("private_test=31\n"+source,trial::kProfile,error);CHECK(privateDoc);
    auto nextDoc=s::MissionDocument::parse_lua("assert(private_test==nil)\n"+source,trial::kProfile,error);CHECK(nextDoc);
    CHECK(mission_test::semantics(lua->views())==expected); // VM destruction cannot invalidate views.
    rejects("local broken =\n return");
    rejects("while true do end","instruction limit");
    rejects("local text='xxxxxxxx'; for i=1,30 do text=text..text end","memory");
    rejects("local function recurse() return 1+recurse() end; return recurse()");
    rejects("local shared={};for i=1,2000 do shared[i]=1 end;local t={};for i=1,20 do t[i]=shared end;return t","aggregate table limit");
    rejects("local t={};for i=1,20 do t={t} end;return t","nesting or node limit");
    rejects(std::string(1048577,' '));
    rejects(std::string("\x1bLua",4));
    rejects("return io.open('nope')");
    rejects("return require('os')");
    rejects("return function() end");
    rejects("return {a=0/0}");
    rejects("return {a=1.5}");
    rejects("return {a=-1}");
    rejects("return {a=4294967296}");
    rejects("return {a=function() end}");
    rejects("local t={};t.self=t;return t");
    rejects("return {[1]='a',[3]='b'}");
    rejects("return {[true]='a'}");
    rejects("return {[1]='a',field='b'}");
    rejects("return {a='\\255'}");
    rejects("return {a='\\0'}");
    rejects("return command('unknown.capability')","unknown.capability");
    rejects("return command('walker.dead',{argument=999})");
    rejects("return command('walker.dead',{wait='requested'})");
    // DSL errors name the reference and retain the author line.
    const std::string prefix="local composition=graph('composition','test',sequence(step('mission',parallel('opening.module','opening.checked'))))\n";
    const std::string suffix="\nreturn mission{id='deadly_trial',graphs={composition,g},entry='composition',modules={'opening'},observations={'opening.checked'}}";
    rejects(prefix+"local g=graph('opening','test',{step('a','landing.entered',{after={'missing_step'}})})"+suffix,"missing_step");
    rejects(prefix+"local g=graph('opening','test',{step('a','landing.entered')},{receipts={ready='missing_command'}})"+suffix,"missing_command");
    rejects(prefix+"local g=graph('opening','test',{step('a','landing.entered',{after={'b'}}),step('b','square.entered',{after={'a'}})})"+suffix,"cycle");
    rejects(prefix+"local g=graph('opening','test',{step('a',command('walker.dead',{id='bad_argument',argument=999}))})"+suffix,"registered native capability");
    rejects(prefix+"local g=graph('opening','test',{step('a','landing.entered')},{receipts={}})"+suffix,"named receipt");
    rejects(prefix+"local g=graph('opening','test',{step('a','opening.checked')})"+suffix,"different service domain");
    const auto unknownSource="\n\nlocal value=command('not_registered'); return value";
    CHECK(!s::MissionDocument::parse_lua(unknownSource,trial::kProfile,error,"builder-line.lua"));
    CHECK(error.find("not_registered")!=std::string::npos);CHECK(error.find("builder-line.lua:3")!=std::string::npos);
    // Diagnostics retain the authored Lua source name and line.
    CHECK(!s::MissionDocument::parse_lua("\n\nerror('author line')",trial::kProfile,error,"line-check.lua"));
    CHECK(error.find("line-check.lua")!=std::string::npos);CHECK(error.find(":3:")!=std::string::npos);
    auto invalidProfile=trial::kProfile;invalidProfile.schema=c::Schema::unspecified;
    CHECK(!s::MissionDocument::parse_lua(source,invalidProfile,error));
    CHECK(!s::MissionDocument::read("Sunrise/scripts/absent.lua",trial::kProfile,error));CHECK(error.find("absent.lua")!=std::string::npos);
    // A failed parse cannot poison a later load or previously retained document.
    for(unsigned i=0;i<4;++i) {
        auto again=s::MissionDocument::parse_lua(source,trial::kProfile,error);CHECK(again);CHECK(error.empty());CHECK(mission_test::semantics(again->views())==expected);
    }
    std::printf("Lua front end: %u checks passed; full semantics, sandbox, budgets, invalid values, diagnostics, isolation\n",checks);
}
