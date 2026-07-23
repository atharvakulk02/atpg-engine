# atpg-engine

A combinational Automatic Test Pattern Generation (ATPG) engine for stuck-at faults, built around the D-calculus and a PODEM-style backtracking search.

Given a gate-level netlist and a stuck-at fault list, it generates, for each fault, a primary-input pattern that sensitizes the fault and propagates its effect to an observable output, or proves that no such pattern exists.

## How it works

For each fault (a line stuck permanently at 0 or 1):

1. **Fault activation**: the fault site's own driving logic is backward-traced (not assumed) to find primary-input values that make the good-circuit value at that line differ from the stuck-at value.
2. **Propagation**: once active, the algorithm repeatedly picks a D-frontier gate (a gate whose output is still unresolved but has a `D`/`D̄` on one input) and backward-traces a primary-input decision that drives it toward its non-controlling value, so the fault effect can pass through.
3. **Backtracking**: every decision is verified by full (incremental) forward simulation. If a decision leads to a dead end, the opposite polarity is tried; if both fail, the search backtracks further up the decision tree.
4. **Verdict**: each fault ends up in one of three states:
   - **Detected**: a valid test pattern was found.
   - **Redundant**: the search space was fully explored with no possible test (a genuine proof, not a guess).
   - **Aborted**: the per-fault search budget (backtrack count plus a wall-clock cap) was exceeded before a verdict was reached. This is kept strictly separate from "redundant" so an inconclusive result never gets reported as a proof.

Only primary inputs are ever decided directly; internal wires are always derived through their real driving logic. This matters: it's what makes the generated patterns physically realizable rather than internally inconsistent assumptions.

The 5-valued algebra (`0`, `1`, `D`, `D̄`, `X`) is evaluated with controllability-aware reductions (`andReduce`/`orReduce`/`xorReduce`/`notReduce` in `dhelper.cpp`), so `X` behaves correctly under partial assignment (e.g. `AND(0, X) = 0`, not `X`).

## Building

No build system, compile directly with g++ (C++17):

```sh
g++ -std=c++17 -O2 -o atpg src/main.cpp src/parser.cpp src/fault.cpp src/datpg.cpp src/dhelper.cpp -Iinclude
```

## Usage

```sh
./atpg <bench_file>
```

Results are written to `results.txt`: for each detected fault, the fault location/type and the input pattern that detects it, followed by a summary with detected / redundant / aborted counts (and the list of aborted faults, if any).

## Circuit format

Structural `.bench` format (the ISCAS-85 style):

```
INPUT(1)
INPUT(2)
OUTPUT(7)
6 = NAND(1, 2)
7 = NAND(6, 2)
```

Supported gate types: `AND`, `OR`, `NAND`, `NOR`, `XOR`, `XNOR`, `NOT`, `BUF`/`BUFF`.

The engine is purely combinational: it has no concept of clocking or state. To test a sequential design, convert it to a full-scan-style combinational netlist first (cut every flip-flop: its `Q` becomes a pseudo primary input, its `D` a pseudo primary output). `benchmarks/picorv32_scan.bench` was produced this way from a synthesized PicoRV32 RISC-V core, using Yosys to resolve wiring and a small converter to decompose the standard-cell library down to the gate types above.

## Project layout

```
include/    gate.h, circuit.h, fault.h (core data types)
            parser.h, dhelper.h, datpg.h (module interfaces)
src/
  parser.cpp   .bench file -> Circuit
  fault.cpp    fault list generation (SA0/SA1 per line, uncollapsed)
  dhelper.cpp  5-valued algebra (good/bad-circuit projection, combval, reductions)
  datpg.cpp    the ATPG search itself
  main.cpp     CLI driver
benchmarks/    c17, c880 (ISCAS-85), picorv32_scan (full-scan RISC-V core)
```

## Benchmarks

| Circuit | Gates | Faults | Detected | Redundant | Aborted |
|---|---|---|---|---|---|
| c17 | 11 | 22 | 22 (100%) | 0 | 0 |
| c880 | 443 | 886 | 886 (100%) | 0 | 0 |
| picorv32 (full-scan) | 17,089 | 34,178 | 33,986 (99.4%) | 136 | 56 |

All detected patterns across these runs have been independently re-verified with a from-scratch simulator (no shared code with the engine): each pattern is checked to actually drive the fault site's good-circuit value away from the stuck-at value, and to actually propagate a difference to a primary output.

## Notes

- Fault list is uncollapsed (every line gets both SA0 and SA1 checked independently); no fault collapsing/equivalence-class reduction is applied.
- The per-fault search budget defaults to a 2,000,000-step cap and a 5-second wall-clock deadline (`kStepLimit` / `kFaultTimeLimit` in `datpg.cpp`).
