#pragma once
#include "gate.h"
#include "fault.h"
#include "circuit.h"

struct TestPattern{
    map<string,LogicValue> patterns;
    Fault f;
};

TestPattern datpg(Fault f, Circuit circuit);
vector<TestPattern> runATPG(vector<Fault> faults, Circuit circuit);
bool justify(map<string,LogicValue>& wireVal, Circuit& circuit);