#include "../include/gate.h"
#include "../include/circuit.h"
#include "../include/fault.h"
#include "../include/datpg.h"
#include "../include/dhelper.h"
#include <algorithm>
#include <unordered_map>
#include <chrono>

namespace {

using Clock = std::chrono::steady_clock;

// Per-fault search budget. Real ATPG tools bound each fault's search by a
// backtrack count (deterministic, machine-independent) rather than wall
// time; we keep both here -- the step count as the primary, portable
// bound, and a wall-clock deadline as a hard backstop so a fault with an
// unusually expensive search (e.g. a very wide circuit where each step
// itself is costly) still can't stall the whole run. Either one hitting
// its limit aborts just that fault's search; the rest of the fault list
// keeps going.
constexpr long kStepLimit = 2000000;
constexpr auto kFaultTimeLimit = std::chrono::seconds(5);

// Distinguishes "no test exists" (the search space was fully explored)
// from "gave up" (budget exceeded) -- collapsing these into one bucket
// would let an aborted fault masquerade as a proven-redundant one.
enum class Verdict { FOUND, EXHAUSTED, ABORTED };

// Flat, integer-indexed view of the circuit. Built once per runATPG call
// and shared read-only across every fault's search, instead of paying
// string-map lookups/copies at every recursive decision.
struct CircuitModel {
    int n = 0;
    vector<string> name;              // idx -> wire/gate name
    unordered_map<string,int> idx;    // name -> idx
    vector<GateType> type;            // idx -> gate type
    vector<vector<int>> inputs;       // idx -> input wire indices
    vector<vector<int>> fanout;       // idx -> gate indices consuming this wire
    vector<int> level;                // idx -> logic depth from nearest primary input
    vector<int> primaryInputs;
    vector<int> primaryOutputs;
};

CircuitModel buildModel(Circuit& circuit) {
    CircuitModel cm;
    cm.n = (int)circuit.gates.size();
    cm.name.resize(cm.n);
    cm.type.resize(cm.n);
    cm.inputs.resize(cm.n);
    cm.fanout.resize(cm.n);
    cm.level.assign(cm.n, -1);

    int i = 0;
    for (auto& g : circuit.gates) {
        cm.idx[g.first] = i;
        cm.name[i] = g.first;
        i++;
    }
    for (auto& g : circuit.gates) {
        int gi = cm.idx[g.first];
        cm.type[gi] = g.second.type;
        for (auto& inp : g.second.inputs) {
            cm.inputs[gi].push_back(cm.idx[inp]);
        }
    }
    for (int gi = 0; gi < cm.n; gi++) {
        for (int wi : cm.inputs[gi]) {
            cm.fanout[wi].push_back(gi);
        }
    }
    for (auto& piName : circuit.inputs) cm.primaryInputs.push_back(cm.idx[piName]);
    for (auto& poName : circuit.outputs) cm.primaryOutputs.push_back(cm.idx[poName]);

    // Levelization (distance from a primary input), used as a cheap
    // controllability proxy: relax repeatedly until every gate whose
    // fanin is fully levelized gets a level of its own.
    for (int gi = 0; gi < cm.n; gi++) {
        if (cm.type[gi] == INPUT) cm.level[gi] = 0;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int gi = 0; gi < cm.n; gi++) {
            if (cm.level[gi] != -1) continue;
            int maxIn = -1;
            bool ready = true;
            for (int wi : cm.inputs[gi]) {
                if (cm.level[wi] == -1) { ready = false; break; }
                maxIn = max(maxIn, cm.level[wi]);
            }
            if (ready) {
                cm.level[gi] = maxIn + 1;
                changed = true;
            }
        }
    }
    for (int gi = 0; gi < cm.n; gi++) if (cm.level[gi] == -1) cm.level[gi] = cm.n;

    return cm;
}

// A stuck-at fault forces the BAD-circuit rail of exactly one wire to a
// fixed value; the GOOD-circuit rail of that same wire is still whatever
// its real driving logic computes. idx == -1 means "no active fault"
// (unused here, but keeps the override check trivially false).
struct FaultContext { int idx; int forcedBad; };

LogicValue computeOutputIdx(const CircuitModel& cm, int gi, vector<LogicValue>& wv,
                             const FaultContext& fc) {
    GateType t = cm.type[gi];
    int g_out, b_out;
    switch (t) {
        case AND:
        case NAND: {
            g_out=1; b_out=1;
            for (int wi : cm.inputs[gi]) {
                g_out = andReduce(g_out, goodcircuit(wv[wi]));
                b_out = andReduce(b_out, badcircuit(wv[wi]));
            }
            if (t == NAND) { g_out = notReduce(g_out); b_out = notReduce(b_out); }
            break;
        }
        case OR:
        case NOR: {
            g_out=0; b_out=0;
            for (int wi : cm.inputs[gi]) {
                g_out = orReduce(g_out, goodcircuit(wv[wi]));
                b_out = orReduce(b_out, badcircuit(wv[wi]));
            }
            if (t == NOR) { g_out = notReduce(g_out); b_out = notReduce(b_out); }
            break;
        }
        case XOR:
        case XNOR: {
            g_out=0; b_out=0;
            for (int wi : cm.inputs[gi]) {
                g_out = xorReduce(g_out, goodcircuit(wv[wi]));
                b_out = xorReduce(b_out, badcircuit(wv[wi]));
            }
            if (t == XNOR) { g_out = notReduce(g_out); b_out = notReduce(b_out); }
            break;
        }
        case NOT: {
            g_out = notReduce(goodcircuit(wv[cm.inputs[gi][0]]));
            b_out = notReduce(badcircuit(wv[cm.inputs[gi][0]]));
            break;
        }
        case OUTPUT:
        case BUF:
            g_out = goodcircuit(wv[cm.inputs[gi][0]]);
            b_out = badcircuit(wv[cm.inputs[gi][0]]);
            break;
        default:
            return X;
    }
    if (gi == fc.idx) b_out = fc.forcedBad;
    return combval(g_out,b_out);
}

// Event-driven forward simulation: starting from a worklist of gates
// whose inputs just changed, only re-evaluates gates actually reachable
// from that change instead of repeatedly sweeping the whole circuit.
void simulate(const CircuitModel& cm, vector<LogicValue>& wv, const FaultContext& fc,
              vector<int> worklist) {
    while (!worklist.empty()) {
        int gi = worklist.back();
        worklist.pop_back();
        if (wv[gi] != X) continue;
        LogicValue result = computeOutputIdx(cm, gi, wv, fc);
        if (result != X) {
            wv[gi] = result;
            for (int consumer : cm.fanout[gi]) worklist.push_back(consumer);
        }
    }
}

// Applies a primary-input decision, routing it through the fault model
// when the decided input happens to be the fault site itself (only
// relevant for PI-located faults -- the decided value becomes the
// good-circuit rail, combined with the fault's forced bad-circuit rail).
void assignPI(vector<LogicValue>& wv, const FaultContext& fc, int piIdx, LogicValue piVal) {
    if (piIdx == fc.idx) wv[piIdx] = combval(goodcircuit(piVal), fc.forcedBad);
    else wv[piIdx] = piVal;
}

LogicValue noncontrollingValue(GateType t) {
    switch (t) {
        case AND: case NAND: return ONE;
        case OR:  case NOR:  return ZERO;
        case XOR: case XNOR: return ZERO;
        default: return X;
    }
}

bool isInverting(GateType t) {
    return t == NAND || t == NOR || t == NOT || t == XNOR;
}

// Backward walk from an internal wire toward a primary input. At each
// gate, follow the still-unassigned fanin closest to a primary input
// (smallest level) rather than an arbitrary one -- this keeps the
// backtrace path, and therefore the decision it produces, as cheap and
// as easy to satisfy as possible.
bool backtrace(int wire, LogicValue target, const CircuitModel& cm,
               vector<LogicValue>& wv, int& piIdx, LogicValue& piVal) {
    while (true) {
        if (cm.type[wire] == INPUT) {
            piIdx = wire;
            piVal = target;
            return true;
        }
        int next = -1, bestLevel = -1;
        for (int inp : cm.inputs[wire]) {
            if (wv[inp] == X && (next == -1 || cm.level[inp] < bestLevel)) {
                next = inp;
                bestLevel = cm.level[inp];
            }
        }
        if (next == -1) return false;
        if (isInverting(cm.type[wire])) {
            target = (target == ONE) ? ZERO : (target == ZERO) ? ONE : target;
        }
        wire = next;
    }
}

// Classic PODEM: only ever decides primary-input values, verified by
// (incremental) forward simulation, backtracking over both polarities.
// Before anything can propagate, the fault itself must be *activated*:
// if the fault site hasn't resolved yet, that IS the objective (backtrace
// through its own real driving logic, same as any other objective) --
// this is what stops the search from just assuming the fault's
// good-circuit value instead of deriving it. Once active, the objective
// switches to the D-frontier gate's unassigned side-input with the
// shallowest level, to keep each decision's backtrace short.
//
// Returns FOUND with `result` populated, EXHAUSTED if every branch from
// here was fully explored with no test found (a real proof, not a
// guess), or ABORTED if the search budget ran out before a verdict was
// reached -- ABORTED short-circuits immediately since there's no budget
// left to try the sibling branch either.
Verdict solve(vector<LogicValue> wv, const CircuitModel& cm, const FaultContext& fc,
              vector<LogicValue>& result, long& steps, const Clock::time_point& deadline) {
    if (++steps > kStepLimit || Clock::now() > deadline) return Verdict::ABORTED;

    for (int po : cm.primaryOutputs) {
        if (wv[po] == D || wv[po] == D_BAR) {
            for (int pi : cm.primaryInputs) if (wv[pi] == X) wv[pi] = ZERO;
            result = wv;
            return Verdict::FOUND;
        }
    }

    int objWire;
    LogicValue objVal;

    if (wv[fc.idx] == X) {
        objWire = fc.idx;
        objVal = (fc.forcedBad == 0) ? ONE : ZERO;
    } else {
        int bestWire = -1, bestLevel = -1;
        GateType bestGateType = INPUT;
        for (int gi = 0; gi < cm.n; gi++) {
            if (cm.type[gi] == INPUT) continue;
            if (wv[gi] != X) continue;
            bool onFrontier = false;
            for (int wi : cm.inputs[gi]) {
                if (wv[wi] == D || wv[wi] == D_BAR) { onFrontier = true; break; }
            }
            if (!onFrontier) continue;
            for (int wi : cm.inputs[gi]) {
                if (wv[wi] != X) continue;
                if (bestWire == -1 || cm.level[wi] < bestLevel) {
                    bestWire = wi;
                    bestLevel = cm.level[wi];
                    bestGateType = cm.type[gi];
                }
            }
        }
        if (bestWire == -1) return Verdict::EXHAUSTED; // D-frontier empty: proven dead end

        objVal = noncontrollingValue(bestGateType);
        if (objVal == X) return Verdict::EXHAUSTED;
        objWire = bestWire;
    }

    int piIdx; LogicValue piVal;
    if (!backtrace(objWire, objVal, cm, wv, piIdx, piVal)) return Verdict::EXHAUSTED;

    vector<LogicValue> branch = wv;
    assignPI(branch, fc, piIdx, piVal);
    simulate(cm, branch, fc, cm.fanout[piIdx]);
    Verdict v1 = solve(branch, cm, fc, result, steps, deadline);
    if (v1 == Verdict::FOUND || v1 == Verdict::ABORTED) return v1;

    branch = wv;
    assignPI(branch, fc, piIdx, (piVal == ONE) ? ZERO : ONE);
    simulate(cm, branch, fc, cm.fanout[piIdx]);
    Verdict v2 = solve(branch, cm, fc, result, steps, deadline);
    if (v2 == Verdict::FOUND || v2 == Verdict::ABORTED) return v2;

    return Verdict::EXHAUSTED; // both polarities fully explored, neither works
}

TestPattern datpgWithModel(Fault f, const CircuitModel& cm) {
    vector<LogicValue> wv(cm.n, X);
    int faultIdx = cm.idx.at(f.location);
    FaultContext fc{faultIdx, (f.val == SA0) ? 0 : 1};

    vector<LogicValue> result;
    long steps = 0;
    Clock::time_point deadline = Clock::now() + kFaultTimeLimit;
    Verdict v = solve(wv, cm, fc, result, steps, deadline);

    TestPattern tp;
    tp.f = f;
    if (v == Verdict::ABORTED) {
        tp.f.status = aborted;
        return tp;
    }
    if (v == Verdict::EXHAUSTED) {
        tp.f.status = redundant;
        return tp;
    }
    tp.f.status = detected;
    for (int pi : cm.primaryInputs) {
        LogicValue v2 = result[pi];
        if (v2 == D) v2 = ONE;
        if (v2 == D_BAR) v2 = ZERO;
        tp.patterns[cm.name[pi]] = v2;
    }
    return tp;
}

} // namespace

TestPattern datpg(Fault f, Circuit circuit){
    CircuitModel cm = buildModel(circuit);
    return datpgWithModel(f, cm);
}

vector<TestPattern> runATPG(vector<Fault> faults, Circuit circuit){
    CircuitModel cm = buildModel(circuit);
    vector<TestPattern> atpg_result;
    for (Fault& f : faults) {
        atpg_result.push_back(datpgWithModel(f, cm));
    }
    return atpg_result;
}
