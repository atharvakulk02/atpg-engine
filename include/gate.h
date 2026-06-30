#pragma once
#include <string>
#include <vector>
using namespace std;

enum GateType{
    AND,
    OR,
    NAND,
    NOR,
    XOR,
    XNOR,
    NOT,
    INPUT,
    OUTPUT,
    BUF
};

enum LogicValue{
    ZERO,
    ONE,
    D,
    D_BAR,
    X
};

struct Gate{
    string name;
    GateType type;
    vector<string> inputs;
    string output;
    LogicValue value;
};