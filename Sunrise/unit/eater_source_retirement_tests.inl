#include "../src/client/hooks/bootflow/eater_source_retirement.h"

static void eater_source_retirement_policy_tests() {
    namespace retirement=sunrise::client::hooks::bootflow::eater_source_retirement;
    retirement::ResetState state{};state.incoming=state.applied=state.request[0]=19;
    state.request[2]=state.request[3]=UINT32_MAX;
    check(retirement::reset(19,state),"exact applied generation and cleared scheduler state qualifies");
    auto changed=state;++changed.incoming;check(!retirement::reset(19,changed),"incoming generation must be exact");
    changed=state;--changed.applied;check(!retirement::reset(19,changed),"native applied generation must cross boundary");
    changed=state;changed.owned=1;check(!retirement::reset(19,changed),"owned actor prevents receipt");
    changed=state;changed.scheduling=1;check(!retirement::reset(19,changed),"active source scheduler prevents receipt");
    changed=state;changed.request[0]=0;check(!retirement::reset(19,changed),"an uninitialized request generation cannot acknowledge cleanup");
    changed=state;changed.request[0]=18;check(!retirement::reset(19,changed),"a previous request generation cannot acknowledge cleanup");
    for(const auto generation:{130U,132U}) {
        auto captured=state;captured.incoming=captured.applied=captured.request[0]=generation;
        check(retirement::reset(generation,captured),"captured empty ship queues permit death/retry with retained request generation");
    }
    for(std::size_t i=0;i<state.request.size();++i) {
        changed=state;changed.request[i]=i==2 || i==3?0U:1U;
        check(!retirement::reset(19,changed),"every native request reset field is required");
    }
    for(std::size_t i=0;i<state.queues.size();++i) {
        changed=state;changed.queues[i]=1;
        check(!retirement::reset(19,changed),"every native queued-request field is required");
    }
    check(retirement::entity(7,7,0,false)==retirement::EntityState::unreadable,"unreadable entity fails closed");
    check(retirement::entity(7,8,0,true)==retirement::EntityState::replaced,"salted entity replacement is settled");
    check(retirement::entity(7,7,4,true)==retirement::EntityState::marked,"native removed bit is settled");
    check(retirement::entity(7,7,0,true)==retirement::EntityState::live,"same unmarked entity remains live");
    retirement::Gate gate{};
    check(!gate.sample(19,true,true),"first exact world observation arms receipt");
    check(gate.sample(19,true,true),"second consecutive exact observation acknowledges");
    check(!gate.sample(19,false,true),"reset regression clears consecutive evidence");
    check(!gate.sample(19,true,true) && gate.sample(19,true,true),"regression requires two new exact observations");
    check(!gate.sample(20,true,true),"new generation cannot reuse prior evidence");
    check(gate.sample(20,true,true),"new generation qualifies on its own second observation");
}
