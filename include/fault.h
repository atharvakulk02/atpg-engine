#pragma once
#include "gate.h"
#include "circuit.h"

enum saval{
    SA0,
    SA1
};

enum detection{
    detected,
    undetected,
    redundant,   // search completed and proved no test pattern exists
    aborted      // per-fault search budget exceeded before a verdict was reached
};

struct Fault{
    string location;
    saval val;
    detection status;
};

vector<Fault> generateFaults(Circuit circuit);