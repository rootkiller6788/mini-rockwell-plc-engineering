# Knowledge Graph — GuardLogix Safety PLC

## L1: Definitions (Complete)

| # | Definition | Type | Implemented In |
|---|-----------|------|----------------|
| 1 | SIL (Safety Integrity Level) | enum | `guardlogix_safety.h` |
| 2 | PL (Performance Level) | enum | `guardlogix_safety.h` |
| 3 | Controller Family (5560S, 5570S, 5580S, Compact) | enum | `guardlogix_safety.h` |
| 4 | Safety Partner Role (Primary/Secondary) | enum | `guardlogix_safety.h` |
| 5 | Safety Partner Status (OK/Faulted/Mismatch/Offline/Degraded) | enum | `guardlogix_safety.h` |
| 6 | Safety Lock State (Locked/Unlocking/Unlocked) | enum | `guardlogix_safety.h` |
| 7 | Safety Task Type (Periodic/Event/Signature) | enum | `guardlogix_safety.h` |
| 8 | Safety Tag Class (Safety/Standard/Mapped) | enum | `guardlogix_safety.h` |
| 9 | Safety I/O Type (Discrete In/Out, Analog In, Relay Out, Speed) | enum | `guardlogix_safety.h` |
| 10 | Controller Operational State (PowerUp/Locked/Fault/Overrun/HardFault) | enum | `guardlogix_safety.h` |
| 11 | Safety Signature (64-bit CRC-based) | struct | `guardlogix_safety.h` |
| 12 | Safety Network Number (SNN) | struct | `guardlogix_safety.h` |
| 13 | 1oo2D Diagnostics Status | struct | `guardlogix_safety.h` |
| 14 | Safety Watchdog | struct | `guardlogix_safety.h` |
| 15 | SIF Architecture Types (1oo1/1oo2/2oo2/2oo3/1oo2D/2oo4D) | enum | `guardlogix_sil.h` |
| 16 | HFT Levels (0/1/2) | enum | `guardlogix_sil.h` |
| 17 | Component Type (A/B) | enum | `guardlogix_sil.h` |
| 18 | ISO 13849 Category (B/1/2/3/4) | enum | `guardlogix_sil.h` |
| 19 | Failure Rates (lambda_s, lambda_dd, lambda_du, MTBF, MTTR) | struct | `guardlogix_sil.h` |
| 20 | Safety Input Mode (Equivalent/Complementary/Single) | enum | `guardlogix_safety_io.h` |
| 21 | Safety Output Pulse Mode (None/Light/Dark/Both) | enum | `guardlogix_safety_io.h` |
| 22 | CIP Safety Connection State (Closed/Opening/Established/TimedOut/Faulted) | enum | `guardlogix_safety_net.h` |
| 23 | Diagnostic Test Types (RAM/ROM/CPU/Watchdog/...15 types) | enum | `guardlogix_diagnostics.c` |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | 1oo2D Dual-Channel Processing | `glx_cross_check_update()` in `guardlogix_safety.c` |
| 2 | Safety State Machine | `glx_safety_state_transition()` with guard conditions |
| 3 | Watchdog Monitoring | `glx_safety_watchdog_t` + overrun detection |
| 4 | Safety Reaction Time | `glx_compute_safety_reaction_time()` |
| 5 | Proof Test Coverage | `glx_proof_test_effective_coverage()` |
| 6 | Muting (temporary safety bypass) | `example_light_curtain.c` -- muting state machine |
| 7 | Manual Reset | `example_estop.c` and `example_safety_interlock.c` |
| 8 | Safe State (de-energize to trip) | `glx_safety_output_emergency_shutdown()` |
| 9 | Black Channel Principle | `glx_safety_data_packet_create/validate()` |
| 10 | SNN Cross-Network Prevention | `glx_safety_connection_open()` -- SNN validation |

## L3: Engineering Structures (Complete)

| # | Structure | Type | File |
|---|-----------|------|------|
| 1 | Safety Controller Object | `glx_safety_controller_t` | `guardlogix_safety.h` |
| 2 | SIF Reliability Model | `glx_sif_model_t` | `guardlogix_sil.h` |
| 3 | SIL Verification Result | `glx_sil_verification_t` | `guardlogix_sil.h` |
| 4 | Safety Input Point (dual channel) | `glx_safety_input_point_t` | `guardlogix_safety_io.h` |
| 5 | Safety Output Point (pulse test) | `glx_safety_output_point_t` | `guardlogix_safety_io.h` |
| 6 | Safety Analog Input | `glx_safety_analog_input_t` | `guardlogix_safety_io.h` |
| 7 | Safety Task Control Block | `glx_safety_task_cb_t` | `guardlogix_safety_task.h` |
| 8 | CRC Signature Block | `glx_signature_block_t` | `guardlogix_signature.h` |
| 9 | CIP Safety Connection | `glx_safety_connection_t` | `guardlogix_safety_net.h` |
| 10 | Diagnostic Suite | `glx_diagnostic_suite_t` | `guardlogix_diagnostics.c` |

## L4: Engineering Laws (Complete)

| # | Law/Standard | Implementation |
|---|-------------|---------------|
| 1 | PFDavg = lambda_du * T_pt / 2 (1oo1) | `glx_pfdavg_1oo1()` |
| 2 | PFDavg = (1-beta)*(lambda_du)^2*(T_pt)^2/3 + beta*lambda_du*T_pt/2 (1oo2) | `glx_pfdavg_1oo2()` |
| 3 | PFDavg = (lambda_du)^2*(T_pt)^2 + beta*lambda_du*T_pt/2 (2oo3) | `glx_pfdavg_2oo3()` |
| 4 | 1oo2D PFDavg with diagnostic coverage | `glx_pfdavg_1oo2d()` |
| 5 | SFF = (lambda_s + lambda_dd) / (lambda_s + lambda_dd + lambda_du) | `glx_compute_sff()` |
| 6 | Architectural SIL limits (IEC 61508-2 Tables 3/4) | `glx_architectural_sil_limit()` |
| 7 | SIL-to-PFDavg bands (IEC 61508-1 Table 2) | `glx_pfdavg_to_sil()` |
| 8 | RRF = 1 / PFDavg (IEC 61511-1 section 11) | `glx_compute_rrf()` |

## L5: Algorithms (Complete)

| # | Algorithm | Function |
|---|-----------|----------|
| 1 | CRC-32 with IEEE 802.3 polynomial | `glx_crc32_compute()` |
| 2 | Multi-segment safety signature | `glx_signature_finalize()` |
| 3 | Binary search for proof test interval | `glx_compute_proof_test_interval()` |
| 4 | Complete SIF verification | `glx_verify_sif_sil()` |
| 5 | Inverse proof test interval | `glx_invert_proof_test_interval()` |
| 6 | Spurious trip rate estimation | `glx_spurious_trip_rate()` |
| 7 | Diversity-based beta estimation | `glx_estimate_beta()` |
| 8 | Input debounce filter | `apply_debounce()` |
| 9 | Safety output pulse test | `glx_safety_output_pulse_test()` |
| 10 | P99 execution time computation | `glx_safety_task_p99_execution_time()` |
| 11 | Safety data packet CRC validation | `glx_safety_data_packet_validate()` |
| 12 | Proof test sensitivity analysis | `glx_proof_test_sensitivity()` |
| 13 | DC aging exponential model | `glx_dc_aging_update()` |

## L6: Canonical Problems (Complete)

| # | Problem | Example File |
|---|---------|-------------|
| 1 | Emergency Shutdown (E-Stop) | `examples/example_estop.c` |
| 2 | Light Curtain with Muting | `examples/example_light_curtain.c` |
| 3 | Safety Gate Interlock with Reset | `examples/example_safety_interlock.c` |

## L7: Industrial Applications (Complete)

| # | Application | Reference |
|---|-------------|-----------|
| 1 | GuardShield 450L Light Curtain Integration | `example_light_curtain.c` |
| 2 | SensaGuard 440N Interlock Switch | `example_safety_interlock.c` |
| 3 | 440R Safety Relay (E-Stop contactors) | `example_estop.c` |
| 4 | 1734-IB8S/OB8S Safety I/O Modules | `guardlogix_safety_io.c` |
| 5 | GuardLogix 5580/5570/5560/Compact | `guardlogix_safety.c` |

## L8: Advanced Topics (Partial)

| # | Topic | Status | Implementation |
|---|-------|--------|---------------|
| 1 | DC Aging Model (time-varying diagnostic coverage) | Complete | `glx_dc_aging_update()` |
| 2 | Markov Chain Reliability Modeling | Partial | Documented, not implemented |
| 3 | Fault Injection Testing | Partial | Fault types defined, no injection harness |
| 4 | Beta Factor Diversity Optimization | Complete | `glx_estimate_beta()` with diversity flags |
| 5 | Game of Life / Agent-Based Safety | Not Implemented | Beyond scope |

## L9: Research Frontiers (Partial)

| # | Topic | Status |
|---|-------|--------|
| 1 | IT/OT Safety Convergence | Documented |
| 2 | Wireless Safety (CIP Safety over 5G/Wi-Fi 6) | Documented |
| 3 | Autonomous Safety Functions (L4) | Documented |
| 4 | Industrial Zero Trust Safety Architecture | Documented |
