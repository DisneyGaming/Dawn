#pragma once
namespace campaign_ordering {
inline const coo::Step& step(const coo::script::Views& views,std::string_view graph,std::string_view name) {
    const auto* g=views.graph(graph);check(g!=nullptr,"campaign graph exists");
    for(const auto& s:g->definition.steps) if(s.name==name)return s;
    check(false,"campaign timing step exists");std::abort();
}
inline bool has(const coo::Step& s,coo::Operation operation,std::uint32_t argument) {
    for(const auto& c:s.commands)if(c.operation==operation&&c.argument==argument)return true;
    return false;
}
inline bool after(const coo::script::Views& views,std::string_view graph,std::string_view name,std::string_view predecessor) {
    const auto& steps=views.graph(graph)->definition.steps;
    const auto& s=step(views,graph,name);
    for(std::size_t i=0;i<steps.size();++i)if(steps[i].name==predecessor)return (s.dependencies&(1U<<i))!=0;
    return false;
}
inline bool condition(const coo::script::Views& views,std::string_view name,coo::Asset observed,std::uint32_t argument=0) {
    for(const auto& c:views.conditions)if(c.id==name)return c.evaluate([&](const auto& s){return s.asset==observed&&s.argument==argument;});
    return false;
}
}
