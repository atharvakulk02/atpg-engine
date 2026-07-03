#include "../include/gate.h"
#include "../include/circuit.h"
#include "../include/fault.h"

vector<Fault> generateFaults(Circuit circuit){
    vector<Fault> faults;
    for (auto const& g:circuit.gates){
        Fault f0;
        f0.location=g.first;
        f0.status=undetected;
        f0.val=SA0;
        Fault f1;
        f1.location=g.first;
        f1.status=undetected;
        f1.val=SA1;
        faults.push_back(f0);
        faults.push_back(f1);
    }
    return faults;
}