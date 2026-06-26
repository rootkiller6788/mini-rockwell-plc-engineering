# mini-guardlogix-safety-plc

GuardLogix Safety PLC Engineering Library -- Rockwell Automation safety
controller implementation with full SIL verification per IEC 61508/IEC 61511.

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (3 industrial applications)
- L8: Partial (3/5 advanced topics: DC aging model, time-varying beta, Markov diagnostic modeling)
- L9: Partial (documented IT/OT safety convergence, not fully implemented)

---

## Nine-Layer Knowledge Coverage Summary

| Level | Name | Status | Key Items |
|-------|------|--------|-----------|
| **L1** | Definitions | **Complete** | SIL/PL/HFT/SFF, 1oo2D/2oo3 voting, SNN, CIP Safety protocol, safety task types, GuardLogix controller families, safety I/O types, diagnostic test categories |
| **L2** | Core Concepts | **Complete** | 1oo2D dual-channel processing, cross-check diagnostics, safety state machine, watchdog monitoring, safety reaction time, proof test coverage, muting, safe state, black channel principle |
| **L3** | Engineering Structures | **Complete** | Safety controller object model, safety I/O dual-channel structures, CRC-32 signature blocks, CIP Safety connection parameters, safety task control blocks, diagnostic suite structures |
| **L4** | Engineering Laws | **Complete** | IEC 61508-6 PFDavg formulas (1oo1/1oo2/2oo2/2oo3/1oo2D), architectural constraints (HFT vs SFF), ISO 13849-1 Cat./PL, diagnostic coverage requirements |
| **L5** | Algorithms | **Complete** | CRC-32 (IEEE 802.3), PFDavg binary search, diversity-based beta estimation, SIF verification, spurious trip rate, P99 execution time, debounce filtering, pulse testing |
| **L6** | Canonical Problems | **Complete** | Emergency Shutdown (E-Stop), Light Curtain with Muting, Safety Gate Interlock with Reset |
| **L7** | Applications | **Complete** | GuardShield 450L light curtain, SensaGuard 440N interlock, 440R safety relay, 1734-IB8S/OB8S I/O modules, GuardLogix 5580/5570/5560/Compact |
| **L8** | Advanced Topics | **Partial** | DC aging model, time-varying diagnostic coverage, Markov chain reliability (partial), diversity optimization, fault injection testing |
| **L9** | Frontiers | **Partial** | IT/OT safety convergence documented, wireless safety (CIP Safety over wireless), autonomous safety functions |

---

## Core Definitions (L1)

- **SIL** (Safety Integrity Level): SIL 1-4 per IEC 61508, defined by PFDavg bands
- **PL** (Performance Level): PL a-e per ISO 13849-1
- **1oo2D**: One-out-of-two with Diagnostics -- GuardLogix dual-processor architecture
- **HFT** (Hardware Fault Tolerance): Number of dangerous failures tolerated
- **SFF** (Safe Failure Fraction): Ratio of safe + dangerous-detected failures
- **SNN** (Safety Network Number): 16-bit unique identifier per safety subnet
- **PFDavg**: Average Probability of Failure on Demand
- **PST** (Process Safety Time): Maximum time before hazard occurs
- **RPI** (Requested Packet Interval): CIP Safety communication rate
- **CRTL** (Connection Reaction Time Limit): Max network reaction time

## Core Algorithms (L5)

| Algorithm | File | Description |
|-----------|------|-------------|
| PFDavg 1oo1 | `guardlogix_sil.c` | Single channel PFDavg = lambda_du * T / 2 |
| PFDavg 1oo2 | `guardlogix_sil.c` | Dual channel with beta factor |
| PFDavg 1oo2D | `guardlogix_sil.c` | GuardLogix architecture with diagnostics |
| PFDavg 2oo3 | `guardlogix_sil.c` | Triple modular redundant |
| SFF Computation | `guardlogix_sil.c` | Safe Failure Fraction per IEC 61508-2 |
| Architectural Limit | `guardlogix_sil.c` | SIL limit from HFT + SFF + component type |
| CRC-32 | `guardlogix_signature.c` | IEEE 802.3 polynomial, lookup table |
| Safety Signature | `guardlogix_signature.c` | Multi-segment CRC-based project signature |
| Proof Test Interval | `guardlogix_proof_test.c` | Binary search for T_pt from target PFDavg |
| Input Debounce | `guardlogix_safety_io.c` | Majority-vote debounce filter |
| Output Pulse Test | `guardlogix_safety_io.c` | Light/Dark pulse stuck-at detection |
| CRTL Computation | `guardlogix_safety_net.c` | Connection reaction time limit |
| P99 Execution Time | `guardlogix_safety_task.c` | Percentile from ring buffer history |
| DC Aging Model | `guardlogix_diagnostics.c` | Exponential DC degradation tracking |

---

## File Listing

### Headers (include/)
| File | Lines | Content |
|------|-------|---------|
| `guardlogix_safety.h` | ~240 | Core definitions, controller types, state machine |
| `guardlogix_sil.h` | ~125 | SIL/PL enums, PFDavg function declarations, SIF model |
| `guardlogix_safety_io.h` | ~140 | Safety I/O point/module structs, analog I/O |
| `guardlogix_safety_task.h` | ~100 | Task control block, timing, watchdog |
| `guardlogix_signature.h` | ~85 | CRC-32, signature block, segment descriptors |
| `guardlogix_safety_net.h` | ~155 | CIP Safety connection, packet, bridge |

### Source (src/)
| File | Lines | Content |
|------|-------|---------|
| `guardlogix_safety.c` | ~320 | Controller init, state transitions, cross-check |
| `guardlogix_sil.c` | ~490 | All PFDavg formulas, SFF, architectural constraints |
| `guardlogix_safety_io.c` | ~480 | Dual-channel input, pulse testing, analog eval |
| `guardlogix_safety_task.c` | ~280 | Task scheduling, statistics, timing verification |
| `guardlogix_signature.c` | ~235 | CRC-32 with table, signature generation/verification |
| `guardlogix_safety_net.c` | ~275 | CIP Safety conn, packet validation, CRTL |
| `guardlogix_proof_test.c` | ~305 | Proof test mgmt, partial stroke, sensitivity |
| `guardlogix_diagnostics.c` | ~480 | Diagnostic suite, DC aging model, SIL verification |
| `guardlogix_safety.lean` | ~400 | Lean 4 formal verification (not counted in 3000) |

---

## Nine-School Curriculum Mapping

| School | Course | Relevant Content |
|--------|--------|-----------------|
| **MIT** | 6.302 Feedback Systems | Safety system feedback, fault detection theory |
| **Stanford** | ENGR205 Process Control | SIS integration with process control |
| **Berkeley** | ME233 Advanced Control | Fault-tolerant control, reliability analysis |
| **CMU** | 24-677 Adv Ctrl Systems | Systems engineering for safety-critical systems |
| **Georgia Tech** | ECE 6550 Nonlinear Control | Safety state estimation, fault diagnosis |
| **Purdue** | ME 575 Industrial Control | GuardLogix industrial application, PLC safety |
| **RWTH Aachen** | PLC/SCADA Engineering | IEC 61508/61511 compliance in automation |
| **Tsinghua** | Process Control Engineering | SIS design for chemical/power plant safety |
| **ISA/IEC** | ISA-84 / IEC 61511 | SIS lifecycle, SIL verification, proof testing |

---

## Formal Verification

Lean 4 formalization in `src/guardlogix_safety.lean`:
- SIL ordering relation with monotonicity proofs
- PFDavg-to-SIL classification theorem (proved monotonic)
- Architectural constraint encoding (Type A/B, HFT, SFF tables)
- RRF computation from PFDavg with boundedness proof
- Safety state machine with trap state theorem (hardFault is terminal)
- 1oo2 channel redundant safety theorem
- SNN equivalence relation formalization

---

## Build

```bash
make        # Build library, tests, examples
make test   # Run unit tests (40+ test cases)
make clean  # Remove build artifacts
```

---

## References

1. IEC 61508:2010 -- Functional Safety of E/E/PE Safety-Related Systems
2. IEC 61511:2016 -- Functional Safety -- Safety Instrumented Systems
3. IEC 61784-3:2016 -- Industrial Communication Networks -- Functional Safety Fieldbuses
4. ISO 13849-1:2015 -- Safety of Machinery -- Safety-Related Parts of Control Systems
5. CIP Safety Specification Vol. 8 (ODVA)
6. GuardLogix 5580 Safety Reference Manual (1756-RM099, Rockwell Automation)
7. GuardLogix Safety Application Instruction Set (1756-RM095)
8. ISA-TR84.00.02 -- Safety Instrumented Functions (SIF) -- SIL Verification
9. Gruhn & Cheddie (2006) -- Safety Instrumented Systems: Design, Analysis, and Justification
