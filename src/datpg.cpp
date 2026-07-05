#include "../include/gate.h"
#include "../include/circuit.h"
#include "../include/fault.h"
#include "../include/datpg.h"

TestPattern datpg(Fault f, Circuit circuit){
    map<string,LogicValue> wireVal;
    vector<string> dfrontier;
    for (auto const& g:circuit.gates){
        wireVal[g.first]=X;       
    }
    if (f.val==SA0){
        wireVal[f.location]=D;
    }
    else{
        wireVal[f.location]=D_BAR;
    }
    for (auto const& g:circuit.gates){
        for(auto const& i:g.second.inputs){
            if (i==f.location){
                dfrontier.push_back(g.first);
            }
        }
    }
    while (!dfrontier.empty()){
        Gate g=circuit.gates[dfrontier[0]];
    }
}
vector<TestPattern> runATPG(vector<Fault> faults, Circuit circuit){
    vector<TestPattern> atpg_result;
    for (Fault f :faults){
        TestPattern t;
        t=datpg(f,circuit);
        atpg_result.push_back(t);
    }
    return(atpg_result);
}