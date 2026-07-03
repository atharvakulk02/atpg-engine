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
    redundant
};

struct Fault{
    string location;
    saval val;
    detection status;
};

vector<Fault> generateFaults(Circuit circuit);