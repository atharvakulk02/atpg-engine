#pragma once
#include <map>
#include <vector>
#include <string>
#include "gate.h"
using namespace std;

struct Circuit{
    map<string, Gate> gates;
    vector<string> inputs;
    vector<string> outputs;
};