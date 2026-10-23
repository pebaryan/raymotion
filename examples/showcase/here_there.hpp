#pragma once
// Small executable semantics for the explainer's two-atom examples, not an ASP solver.
namespace rmtn { namespace ht {
constexpr unsigned FIRE=1, SMOKE=2;
inline bool implication(unsigned h,unsigned t,unsigned antecedent,unsigned consequent){
    return (!(h&antecedent)||(h&consequent)) && (!(t&antecedent)||(t&consequent));
}
inline bool negation(unsigned t,unsigned atom){ return !(t&atom); }
inline bool theory(unsigned h,unsigned t,bool fireFact=false){
    return !(h&~t) && implication(h,t,FIRE,SMOKE) && (!fireFact||(h&FIRE));
}
inline bool equilibrium(unsigned t,bool fireFact=false){
    if(!theory(t,t,fireFact)) return false;
    for(unsigned h=0;h<4;++h)
        if(h!=t && !(h&~t) && theory(h,t,fireFact)) return false;
    return true;
}
}}
