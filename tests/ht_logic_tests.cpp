#include "../examples/showcase/here_there.hpp"
#include <cstdio>
int main(){
    using namespace rmtn::ht;
    int checks=0;
    auto check=[&](bool ok,const char* label){ ++checks; if(!ok){ std::fprintf(stderr,"FAIL %s\n",label); return false; } return true; };
    // Exhaust every valid two-atom HT interpretation against the definition.
    for(unsigned t=0;t<4;++t) for(unsigned h=0;h<4;++h){
        if(h&~t) continue;
        bool expected=(!(h&FIRE)||(h&SMOKE)) && (!(t&FIRE)||(t&SMOKE));
        if(!check(implication(h,t,FIRE,SMOKE)==expected,"implication at both worlds")) return 1;
        if(!check(negation(t,FIRE)==!(t&FIRE),"negation depends on There")) return 1;
    }
    for(unsigned t=0;t<4;++t){
        if(!check(theory(t,t)==(t!=FIRE),"three classical models")) return 1;
        if(!check(equilibrium(t)==(t==0),"rule alone: only empty stable model")) return 1;
        if(!check(equilibrium(t,true)==(t==(FIRE|SMOKE)),"fact plus rule: fire and smoke stable")) return 1;
    }
    if(!check(theory(0,SMOKE),"smoke-only countermodel witness")) return 1;
    if(!check(!negation(FIRE,FIRE),"T-only is not negated")) return 1;
    std::printf("%d HT semantic checks passed\n",checks);
}
