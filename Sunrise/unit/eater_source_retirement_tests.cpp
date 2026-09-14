#include <cstdio>
#include <cstdlib>

unsigned checks{};
void check(bool condition,const char* message) {
    ++checks;if(!condition) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
#include "eater_source_retirement_tests.inl"
int main() {
    eater_source_retirement_policy_tests();
    std::printf("PASS: %u Eater source retirement checks\n",checks);
}
