# Course Alignment — GuardLogix Safety PLC

## Nine-School Curriculum Mapping

### MIT — 6.302 Feedback Systems
- **Topic**: Safety-critical feedback loops, fault detection
- **Mapping**: Safety state machine transitions, diagnostic feedback
- **Implemented**: `glx_safety_state_transition()`, `glx_cross_check_update()`

### Stanford — ENGR205 Process Control
- **Topic**: Safety Instrumented Systems in process plants
- **Mapping**: SIF verification per IEC 61511, proof test optimization
- **Implemented**: `glx_verify_sif_sil()`, `glx_proof_test_*()`

### Berkeley — ME233 Advanced Control
- **Topic**: Fault-tolerant control, reliability
- **Mapping**: 1oo2D architecture, dual-channel voting
- **Implemented**: `glx_pfdavg_1oo2d()`, `glx_cross_check_update()`

### CMU — 24-677 Advanced Control Systems
- **Topic**: Systems engineering for safety-critical applications
- **Mapping**: Complete safety lifecycle, diagnostic coverage
- **Implemented**: `glx_diagnostic_run_full_suite()`, SIL verification

### Georgia Tech — ECE 6550 Nonlinear Control
- **Topic**: Fault diagnosis, nonlinear observers
- **Mapping**: DC aging model, time-varying parameters
- **Implemented**: `glx_dc_aging_update()`

### Purdue — ME 575 Industrial Control
- **Topic**: PLC safety programming, industrial networks
- **Mapping**: GuardLogix application, CIP Safety protocol
- **Implemented**: `guardlogix_safety_net.c`, all examples

### RWTH Aachen — Industrial Control Systems
- **Topic**: IEC 61508/61511 compliance, German safety standards
- **Mapping**: Architectural constraints, SFF computation
- **Implemented**: `glx_architectural_sil_limit()`, `glx_compute_sff()`

### Tsinghua University — Process Control Engineering
- **Topic**: SIS for chemical/power plant safety
- **Mapping**: E-Stop, interlock, proof test management
- **Implemented**: All three L6 examples

### ISA/IEC — International Standards
- **Topic**: ISA-84, IEC 61508, IEC 61511
- **Mapping**: All PFDavg formulas, SIL verification
- **Implemented**: Complete `guardlogix_sil.c`

## Course Chapter Mapping

| Standard/University | Chapter | This Module |
|--------------------|---------|-------------|
| IEC 61508-1 | General Requirements | L1 Definitions |
| IEC 61508-2 | Requirements for E/E/PE | L2-L4 |
| IEC 61508-3 | Software Requirements | L5 CRC, signature |
| IEC 61508-4 | Definitions | L1 complete |
| IEC 61508-5 | SIL Determination | L4 SIL bands |
| IEC 61508-6 | Guidelines (Annex B) | L4 PFDavg formulas |
| IEC 61508-7 | Techniques (Annex A-G) | L5 algorithms |
| MIT 6.302 Ch.7 | Frequency Domain | Safety response time |
| Stanford ENGR205 Ch.5 | SIS Design | L6 canonical problems |
| Purdue ME 575 Ch.3 | PLC Architecture | L3 engineering structures |
