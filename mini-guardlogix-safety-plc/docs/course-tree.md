# Course Tree — GuardLogix Safety PLC

## Prerequisite Knowledge

```
                    +-----------------------+
                    | guardlogix-safety-plc |
                    +-----------------------+
                              |
          +-------------------+-------------------+
          |                   |                   |
    +-----------+      +-----------+      +------------+
    | IEC 61508 |      | IEC 61511 |      | ISO 13849  |
    | (L4 Laws) |      | (SIS)     |      | (Machinery)|
    +-----------+      +-----------+      +------------+
          |                   |                   |
    +-----------+      +-----------+      +------------+
    | Reliability|      |Process Ctrl|      |Safety Ctrl |
    | Theory     |      |(L2 Basics) |      |Architecture|
    +-----------+      +-----------+      +------------+
          |                   |                   |
    +-----------+      +-----------+      +------------+
    | Probability|      |  Feedback |      |  Redundancy|
    | & Stats    |      |  Control  |      |  & Voting  |
    +-----------+      +-----------+      +------------+
```

## Dependency Graph

1. **Pre-requisite modules** (from mini-control-engineering-practice):
   - `1. mini-pid-control-engineering` — Feedback control fundamentals
   - `4. mini-plc-iec61131-fundamentals` — PLC programming model
   - `10. mini-safety-instrumented-system` — SIS fundamentals (sibling)
   - `11. mini-industrial-real-time-database` — Real-time data for safety logs

2. **This module provides**:
   - GuardLogix-specific safety PLC engineering
   - SIL verification computation engine
   - Safety I/O dual-channel implementation
   - CIP Safety protocol modeling
   - Safety task execution and timing analysis
   - Diagnostic coverage management

3. **Modules that depend on this**:
   - `12. mini-advanced-process-control-apc` — APC with safety constraints
   - `18. mini-industrial-ai-control-fusion` — AI-based safety monitoring
   - `19. mini-control-system-cybersecurity` — Safety + security integration

## Knowledge Progression

```
L1: Definitions
  -> L2: Core Concepts (state machine, 1oo2D, proof test)
    -> L3: Engineering Structures (controller object, I/O models)
      -> L4: Engineering Laws (PFDavg, SFF, architectural constraints)
        -> L5: Algorithms (CRC, binary search, pulse test)
          -> L6: Canonical Problems (E-Stop, light curtain, interlock)
            -> L7: Applications (GuardShield, SensaGuard, 440R)
              -> L8: Advanced (DC aging, Markov model)
                -> L9: Frontiers (wireless safety, autonomous safety)
```

## Study Path

1. Start with `guardlogix_safety.h` — understand SIL/PL, controller families
2. Read `guardlogix_sil.h` — learn PFDavg formulas and architectural constraints
3. Study `guardlogix_safety_io.c` — understand dual-channel safety I/O
4. Examine `guardlogix_signature.c` — CRC-32 and safety signatures
5. Run examples: `make examples` then execute each example
6. Verify theorems: examine `src/guardlogix_safety.lean`
7. Run tests: `make test`
