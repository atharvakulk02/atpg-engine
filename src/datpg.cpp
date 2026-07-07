#include "../include/gate.h"
#include "../include/circuit.h"
#include "../include/fault.h"
#include "../include/datpg.h"
#include "../include/dhelper.h"
#include <algorithm>

LogicValue computeOutput(Gate g, map<string,LogicValue> wirev){
    switch(g.type){
        case AND:{
        int g_out=1;
        int b_out=1;
        for (string inp:g.inputs){
            g_out=g_out&goodcircuit(wirev[inp]);
            b_out=b_out&badcircuit(wirev[inp]);
        }
        return combval(g_out,b_out);
        break;
        }
        case NAND:{
        int g_out=1;
        int b_out=1;
        for (string inp:g.inputs){
            g_out=(g_out&goodcircuit(wirev[inp]));
            b_out=(b_out&badcircuit(wirev[inp]));
        }
        g_out=!g_out;
        b_out=!b_out;
        return combval(g_out,b_out);
        break;
        }
        case OR:{
        int g_out=0;
        int b_out=0;
        for (string inp:g.inputs){
            g_out=g_out|goodcircuit(wirev[inp]);
            b_out=b_out|badcircuit(wirev[inp]);
        }
        return combval(g_out,b_out);
        break;
        }
        case NOR:{
        int g_out=0;
        int b_out=0;
        for (string inp:g.inputs){
            g_out=(g_out|goodcircuit(wirev[inp]));
            b_out=(b_out|badcircuit(wirev[inp]));
        }
        g_out=!g_out;
        b_out=!b_out;
        return combval(g_out,b_out);
        break;
        }
        case XOR:{
        int g_out=0;
        int b_out=0;
        for (string inp:g.inputs){
            g_out=g_out^goodcircuit(wirev[inp]);
            b_out=b_out^badcircuit(wirev[inp]);
        }
        return combval(g_out,b_out);
        break;
        }
        case XNOR:{
        int g_out=0;
        int b_out=0;
        for (string inp:g.inputs){
            g_out=(g_out^goodcircuit(wirev[inp]));
            b_out=(b_out^badcircuit(wirev[inp]));
        }
        g_out=!g_out;
        b_out=!b_out;
        return combval(g_out,b_out);
        break;
        }
        case NOT:{
        int g_out=!goodcircuit(wirev[g.inputs[0]]);
        int b_out=!badcircuit(wirev[g.inputs[0]]);
        return combval(g_out,b_out);
        break;
        }
        case INPUT:
        return wirev[g.output];
        case OUTPUT:
        return wirev[g.inputs[0]];
        case BUF:
        return wirev[g.inputs[0]];   
        default:
        return X;
    }
}

bool justify(map<string,LogicValue>& wireVal, Circuit& circuit) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& g : circuit.gates) {
            Gate& gate = g.second;
            if (gate.type == INPUT) continue;
            if (wireVal[gate.name] == X) continue;
            
            LogicValue target = wireVal[gate.name];
            int g_target = goodcircuit(target);
            int b_target = badcircuit(target);
            
            for (string& inp : gate.inputs) {
                if (wireVal[inp] != X) continue;
                
                LogicValue newVal = X;
                switch(gate.type) {
                    case AND:
                    case NAND:
                        if (g_target == 1 && b_target == 1)
                            newVal = ONE;
                        else if (g_target == 0 && b_target == 0)
                            newVal = ZERO;
                        else
                            newVal = ONE;
                        break;
                    case OR:
                    case NOR:
                        if (g_target == 1 && b_target == 1)
                            newVal = ONE;
                        else if (g_target == 0 && b_target == 0)
                            newVal = ZERO;
                        else
                            newVal = ZERO;
                        break;
                    case XOR:
                    case XNOR:
                        newVal = ZERO;
                        break;
                    case NOT:
                    case BUF:
                        newVal = (g_target == 1) ? ONE : ZERO;
                        break;
                    default:
                        break;
                }
                if (newVal == X) continue;
                if (wireVal[inp] != X) {
                    if (newVal == ONE && wireVal[inp] != ONE && wireVal[inp] != D) return false;
                    if (newVal == ZERO && wireVal[inp] != ZERO && wireVal[inp] != D_BAR) return false;
                }
                
                wireVal[inp] = newVal;
                changed = true;
            }
        }
    }
    for (string& inp : circuit.inputs) {
        if (wireVal[inp] == X) wireVal[inp] = ZERO;
    }
    return true;
}

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
    if (find(circuit.outputs.begin(), circuit.outputs.end(), f.location) != circuit.outputs.end()) {
    justify(wireVal, circuit);
    TestPattern tp;
    tp.f = f;
    tp.f.status = detected;
    for (string inp : circuit.inputs) {
        LogicValue v = wireVal[inp];
        if (v == D) v = ONE;
        if (v == D_BAR) v = ZERO;
        tp.patterns[inp] = v;
    }
    return tp;
    }
    while (!dfrontier.empty()){
        Gate g=circuit.gates[dfrontier[0]];
        LogicValue result =computeOutput(g,wireVal);
        wireVal[g.name]=result;
        if ((result==D||result==D_BAR)&&(find(circuit.outputs.begin(),circuit.outputs.end(),g.name)!=circuit.outputs.end())){
            bool justified = justify(wireVal, circuit);
            if (justified) {
                TestPattern tp;
                tp.f = f;
                tp.f.status = detected;
                for (string inp : circuit.inputs){
                    LogicValue v = wireVal[inp];
                    if (v == D) v = ONE;
                    if (v == D_BAR) v = ZERO;
                    tp.patterns[inp] = v;
                }
            return tp;
            }
        }
        dfrontier.erase(dfrontier.begin());
        for (auto const& g : circuit.gates) {
             if (wireVal[g.first] != X) continue;
                 for (auto const& i : g.second.inputs) {
                     if (wireVal[i] == D || wireVal[i] == D_BAR) {
                         if (find(dfrontier.begin(), dfrontier.end(), g.first) == dfrontier.end()) {
                            dfrontier.push_back(g.first);
                        }
                    break;
                    }
                }
        }
    }
    TestPattern empty;
    return empty;
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