#include <iostream>
#include <fstream>
#include "../include/parser.h"
#include "../include/fault.h"
#include "../include/datpg.h"
using namespace std;

string logicToString(LogicValue v) {
    switch(v) {
        case ZERO:  return "0";
        case ONE:   return "1";
        case D:     return "D";
        case D_BAR: return "D_BAR";
        case X:     return "X";
        default:    return "?";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: atpg <bench_file>" << endl;
        return 1;
    }

    string fileName = argv[1];
    cout << "Parsing: " << fileName << endl;

    Circuit circuit = parse(fileName);
    cout << "Gates: " << circuit.gates.size() << endl;
    cout << "Inputs: " << circuit.inputs.size() << endl;
    cout << "Outputs: " << circuit.outputs.size() << endl;

    vector<Fault> faults = generateFaults(circuit);
    cout << "Total faults: " << faults.size() << endl;

    vector<TestPattern> patterns = runATPG(faults, circuit);

    int det_count = 0;
    ofstream out("results.txt");
    out << "ATPG Results for: " << fileName << endl;
    out << "================================" << endl;

    for (TestPattern& tp : patterns) {
        if (tp.patterns.empty()) continue;
        det_count++;
        out << "Fault: " << tp.f.location << " SA" << (tp.f.val == SA0 ? "0" : "1") << endl;
        out << "Pattern: ";
        for (auto& p : tp.patterns) {
            out << p.first << "=" << logicToString(p.second) << " ";
        }
        out << endl;
    }

    out << "================================" << endl;
    out << "Detected: " << det_count << "/" << faults.size() << endl;
    cout << "Done. Results written to results.txt" << endl;

    return 0;
}