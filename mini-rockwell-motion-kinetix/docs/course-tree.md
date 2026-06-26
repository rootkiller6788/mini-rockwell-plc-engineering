# Course Dependency Tree — mini-rockwell-motion-kinetix

## Prerequisites

This module depends on knowledge from these upstream modules:

```
mini-industrial-measurement-actuator/
├── mini-analytical-ph-cond-sensor    → Feedback devices (encoder types)
├── mini-control-valve-positioner     → Actuator saturation, position control
├── mini-vfd-variable-frequency-drive → Motor drive fundamentals, PWM
└── mini-smart-instrument-hart-ff     → Digital communication protocols

mini-pid-control-engineering/
├── mini-anti-windup-bumpless-transfer → PID discretization, AW strategies
├── mini-feedforward-cascade-ratio    → Cascade control structure
└── mini-cohen-coon-imc-tuning        → PID tuning methods

mini-plc-iec61131-fundamentals/
└── (PLC scan cycle, IEC 61131-3 basics)

mini-rockwell-plc-engineering/
└── mini-rockwell-ethernet-ip-cip     → CIP protocol fundamentals
```

## Internal Dependency Graph

```
axis_types.h ───────────────────────────────┐
  ├── axis_state.c (L1-L3, L6)              │
  │     └── uses: axis_types.h              │
  │                                         │
  ├── motion_planner.h ─────────────────────┤
  │     ├── motion_planner.c (L2-L6, L7)    │
  │     │     └── uses: axis_types.h        │
  │     │                                    │
  ├── servo_tuning.h ───────────────────────┤
  │     ├── servo_tuning.c (L4-L6, L8)      │
  │     │     └── uses: axis_types.h        │
  │     │                                    │
  ├── cip_motion.h ─────────────────────────┤
  │     ├── cip_motion.c (L3, L5, L7)       │
  │     │     └── uses: axis_types.h        │
  │     │                                    │
  ├── kinetix_drive.c (L2-L4, L6, L7)       │
  │     └── uses: axis_types.h              │
  │                                         │
  └── motion_advanced.c (L8)                │
        └── uses: axis_types.h, servo_tuning.h
```

## Knowledge Progression

```
L1 Definitions ──→ L2 Concepts ──→ L3 Structures ──→ L4 Laws ──→ L5 Algorithms
     │                   │                │               │              │
     │                   │                │               │              │
     └───────→ L6 Canonical Problems ◄────────────────────┘              │
                    │                                                     │
                    └──────→ L7 Applications ──→ L8 Advanced ──→ L9 Frontiers
```

## File-to-Level Mapping

| File | L1 | L2 | L3 | L4 | L5 | L6 | L7 | L8 | L9 |
|------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| include/axis_types.h | ● | ○ | ● | | | | ● | | |
| include/motion_planner.h | | ● | ● | | ● | | | | |
| include/servo_tuning.h | ● | ● | ● | ● | ● | | | ○ | |
| include/cip_motion.h | ○ | ● | ● | | | | ● | | |
| src/axis_state.c | ● | ● | ● | | | ● | ○ | | |
| src/motion_planner.c | | ● | ● | | ● | ● | ● | | |
| src/servo_tuning.c | | | ○ | ● | ● | ● | | ● | |
| src/cip_motion.c | | | ● | | ● | | ● | | |
| src/kinetix_drive.c | | ● | ● | ● | | ● | ● | | |
| src/motion_advanced.c | | | | ● | ● | | | ● | |
| docs/knowledge-graph.md | ● | ● | ● | ● | ● | ● | ● | ● | ● |

● = Primary coverage, ○ = Partial/secondary coverage
