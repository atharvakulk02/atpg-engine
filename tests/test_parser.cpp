#include <iostream>
#include "../include/circuit.h"
#include "../include/gate.h"
#include "../include/parser.h"
#include "../include/fault.h"
#include <fstream>
using namespace std;

string Backtostring(GateType gt){
    switch(gt){
        case (AND): return "AND";
        case (OR): return "OR";
        case (XOR): return "XOR";
        case (NOT): return "NOT";
        case (NAND) : return "NAND";
        case (NOR) : return "NOR";
        case (XNOR) : return "XNOR";
        case (BUF) : return "BUF";
        case (INPUT) : return "Input";
        case (OUTPUT) : return "Output";
        default : return "UNKNOWN";
    }
}


int main(){
    Circuit test_p;
    test_p=parse("benchmarks/c17.bench");
    ofstream out("output.txt");
    out<<"Inputs"<<endl;    
    for (string name:test_p.inputs){
        out<<name<<endl;    
    }
    out<<"Outputs"<<endl;
    for (string opname:test_p.outputs){
        out<<opname<<endl;
    }
    out<<"Gates"<<endl;
    for (auto const& [name,gate] : test_p.gates){
        out<<name<<"="<<Backtostring(gate.type)<<"(";
        for (string in:gate.inputs){
            out<<in<<" ";
        }
        out<<")";
        out<<endl;
    }
    vector<Fault> faultlist=generateFaults(test_p);
    out<<"Total faults = "<<faultlist.size();
    return 0;
}