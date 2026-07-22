#pragma once
#include "gate.h"

int goodcircuit(LogicValue gv);
int badcircuit(LogicValue bv);
LogicValue combval(int g, int b);

// Controllability-aware reductions over {0, 1, -1(=X)}. Unlike raw
// bitwise &/|, these correctly treat X as "unknown" rather than as an
// identity/absorbing element: e.g. andReduce(0, X) = 0 (controlling
// value wins), andReduce(1, X) = X (genuinely undetermined).
int andReduce(int a, int b);
int orReduce(int a, int b);
int xorReduce(int a, int b);
int notReduce(int a);