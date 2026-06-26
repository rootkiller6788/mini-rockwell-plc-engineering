# mini-factorytalk-view-hmi
## FactoryTalk View HMI — Rockwell Automation HMI Engineering

**Module Status: COMPLETE ✅**

| Level | Rating | Score |
|-------|--------|-------|
| L1 — Definitions | Complete | 2/2 |
| L2 — Core Concepts | Complete | 2/2 |
| L3 — Engineering Structures | Complete | 2/2 |
| L4 — Engineering Laws/Standards | Complete | 2/2 |
| L5 — Algorithms/Methods | Complete | 2/2 |
| L6 — Canonical Problems | Complete | 2/2 |
| L7 — Industrial Applications | Complete | 2/2 |
| L8 — Advanced Topics | Partial | 1/2 |
| L9 — Research Frontiers | Partial | 1/2 |
| **Total** | **COMPLETE** | **16/18** |

---

## Overview

This module implements a complete FactoryTalk View HMI engineering library covering tag management, alarm handling per ISA-18.2, security & RBAC per ISA-62443, graphics & animation, data logging & trending, and OPC UA / RSLinx communication.

## Core Definitions (L1)

| Definition | C Type | Header |
|-----------|--------|--------|
| HMI tag (analog/digital/integer/string) | `ftview_value_t`, `ftview_tag_t` | ftview_common.h, ftview_tag.h |
| ISA-101 hierarchy levels (1-4) | `isa101_level_t` | ftview_common.h |
| OPC UA quality flags | `ftview_quality_t` | ftview_common.h |
| Alarm definition & severity | `ftview_alarm_def_t`, `ftview_alarm_severity_t` | ftview_alarm.h |
| Security privilege mask | `ftview_privilege_t` | ftview_security.h |
| Display object types (12) | `ftview_object_type_t` | ftview_graphics.h |
| Data log model & trend config | `ftview_datalog_model_t`, `ftview_trend_config_t` | ftview_datalog.h |
| RSLinx topic & connection | `ftview_topic_t`, `ftview_connection_t` | ftview_communication.h |

## Core Theorems (L4, Lean 4)

| Theorem | Statement | Location |
|---------|----------|----------|
| ISA level strict order | `priority l1 < priority l2 → l1 ≠ l2` | ftview_formal.lean |
| No direct normal→ack | `normal state cannot transition to ackAlm directly` | ftview_formal.lean |
| Shelved can return | `normal ∈ allowedTransitions shelved` | ftview_formal.lean |
| Hash bounds | `fnv1a_hash input N hN < N` | ftview_formal.lean |
| Scale monotonicity | `raw1 ≤ raw2 → scaled(raw1) ≤ scaled(raw2)` | ftview_formal.lean |
| Zero alarm rate | `alarms_per_hour 0 w h = 0` | ftview_formal.lean |
| Combined privilege mask | `mask_has_priv m1 p → mask_has_priv (m1 \|\|\| m2) p` | ftview_formal.lean |
| E-signature equality | 3-field structural equality decision | ftview_formal.lean |

## Core Algorithms (L5)

| Algorithm | Complexity | Function |
|-----------|-----------|----------|
| FNV-1a 32-bit hash | O(n) | `ftview_tag_name_hash()` |
| Linear/sqrt engineering-unit scaling | O(1) | `ftview_tag_scale_raw_to_eu()` |
| Alarm deadband (Schmitt trigger) | O(1) | `ftview_alarm_deadband_check()` |
| On-delay / off-delay timers | O(1) | `ftview_alarm_on/off_delay_timer()` |
| AABB hit-test | O(n) | `ftview_display_hit_test()` |
| Bilinear colour interpolation | O(1) | `ftview_color_bilinear()` |
| Swinging-door compression | O(1) | `ftview_datalog_swinging_door()` |
| Piecewise linear interpolation | O(1) | `ftview_datalog_interpolate()` |

## Canonical Problems (L6)

| Problem | Example | Lines |
|---------|---------|-------|
| Motor start/stop control HMI | examples/ex01_motor_control_hmi.c | ~215 |
| PID controller faceplate | examples/ex02_pid_faceplate.c | ~200 |
| Alarm management + trending | examples/ex03_alarm_management.c | ~230 |

## Industrial Applications (L7)

- **Rockwell FactoryTalk View SE/ME**: Tag database, HMI displays, alarms
- **PlantPAx DCS**: Object template pattern (`template_name` field)
- **RSLinx Classic / FactoryTalk Linx**: Topic-based routing, DRT resolution
- **OPC UA / OPC-HDA**: Client-server lifecycle, historical data access, browse
- **CIP Protocol**: Implicit/explicit messaging via connection state machine

## Standards Compliance (L4)

| Standard | Coverage |
|----------|---------|
| ISA-101.01-2015 | Full HMI hierarchy, colour palette, display archetypes |
| ISA-18.2-2016 | Full alarm state machine, shelving, rate metrics |
| ISA-62443-3-3 | RBAC, authentication, audit trails |
| 21 CFR Part 11 | Electronic signatures with audit trail |
| OPC UA Part 2/4/6/8 | Security model, services, mappings, data access |
| EEMUA 191 | Alarm performance metrics |
| IEC 61131-3 §2.4 | Directly represented variables (DRT format) |

## Nine-School Curriculum Mapping

| School | Course | Mapping |
|--------|--------|---------|
| MIT | 6.302, 2.171 | PID faceplate, digital sampling |
| Stanford | ENGR205, EE392 | Process HMI, industrial AI foundation |
| Berkeley | ME233, EE C128 | PID faceplate, mechatronics HMI |
| CMU | 24-677, 18-771 | Alarm management, scaling theory |
| Georgia Tech | ECE 6550, AE 6530 | Deadband hysteresis, state estimation |
| Purdue | ECE 602, ME 575 | RBAC security, FT View integration |
| RWTH Aachen | ICS, PLC/SCADA | OPC UA, RSLinx topology |
| Tsinghua | 过程控制工程, 工业互联网 | PID faceplate, OPC UA model |
| ISA/IEC | Multiple standards | 8 standards implemented |

## Building

```bash
make all         # Build all tests and examples
make test        # Run all tests
make examples    # Build example programs
make check       # Safety scan (filler detection)
make lines       # Count lines of code
make clean       # Remove build artifacts
```

## File Structure

```
mini-factorytalk-view-hmi/
├── Makefile
├── README.md
├── include/
│   ├── ftview_common.h        # Core types, ISA-101, colour palette
│   ├── ftview_tag.h           # Tag database API (hash table)
│   ├── ftview_alarm.h         # ISA-18.2 alarm management
│   ├── ftview_security.h      # ISA-62443 RBAC + 21 CFR Part 11
│   ├── ftview_graphics.h      # Display objects, animation, navigation
│   ├── ftview_datalog.h       # Data logging, trending, compression
│   └── ftview_communication.h # RSLinx/OPC UA communication
├── src/
│   ├── ftview_common.c
│   ├── ftview_tag.c           # Hash table tag DB, scaling
│   ├── ftview_alarm.c         # ISA-18.2 state machine
│   ├── ftview_security.c      # PBKDF2, RBAC, audit trail
│   ├── ftview_graphics.c      # AABB, bilinear, Z-order
│   ├── ftview_datalog.c       # Ring buffer, swinging door
│   ├── ftview_communication.c # Connection pool, DRT resolution
│   └── ftview_formal.lean     # Lean 4 formal proofs (12 theorems)
├── tests/
│   ├── test_tagdb.c
│   ├── test_alarm.c
│   ├── test_security.c
│   └── test_graphics_comm.c
├── examples/
│   ├── ex01_motor_control_hmi.c
│   ├── ex02_pid_faceplate.c
│   └── ex03_alarm_management.c
├── benches/
│   └── bench_tagdb.c
├── demos/
└── docs/
    ├── knowledge-graph.md
    ├── coverage-report.md
    ├── gap-report.md
    ├── course-alignment.md
    └── course-tree.md
```
