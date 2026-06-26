# Knowledge Graph — mini-studio5000-controllogix

## Dependency Graph

```
                     ┌─────────────────────┐
                     │  ControlLogix        │
                     │  Platform (1756)     │
                     └──────────┬──────────┘
                                │
          ┌─────────────────────┼─────────────────────┐
          │                     │                     │
          ▼                     ▼                     ▼
   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
   │ Studio 5000  │    │  Tag Model   │    │   IEC 61131  │
   │   Project    │    │ (UDT/AOI)    │    │  Compliance  │
   └──────┬───────┘    └──────┬───────┘    └──────────────┘
          │                   │
          ▼                   ▼
   ┌──────────────┐    ┌──────────────────────────────┐
   │  Execution   │◄───│        Instruction Set        │
   │   Model      │    ├──────────┬─────────┬─────────┤
   │ (Scan Cycle) │    │  PIDE    │ Timers  │ Counters │
   └──────┬───────┘    │          │TON/TOF  │CTU/CTD   │
          │            │          │  RTO    │          │
          │            └────┬─────┴────┬────┴────┬─────┘
          │                 │          │         │
          ▼                 ▼          ▼         ▼
   ┌──────────┐    ┌──────────┐ ┌──────────┐ ┌──────────┐
   │  Alarm   │    │  Motion  │ │ Redundancy│ │ Process  │
   │  Model   │    │ Control  │ │  Model    │ │ Scaling  │
   │ALMA/ALMD │    │ Axis/Servo│ │ Dual Pair │ │SCL/LDLG  │
   └──────────┘    └──────────┘ └──────────┘ └──────────┘
```

## Key Theorems Implemented

| # | Theorem | Location | Reference |
|---|---------|----------|-----------|
| 1 | Rate-Monotonic Feasibility: U ≤ n(2^(1/n)-1) | execution_model.c | Liu & Layland 1973 |
| 2 | PID Velocity Form: ΔCV = Kp·ΔE + Ki·Ts·E + Kd/Ts·Δ²PV | pide_instruction.c | 1756-RM006 |
| 3 | Bumpless Transfer: I_sum_init = CV - P_term | pide_instruction.c | Astrom & Hagglund 1995 |
| 4 | Backward Euler Filter: y(n)=α·x(n)+(1-α)·y(n-1), α=Ts/(Ts+τ) | pide_instruction.c | Franklin et al. 2015 |
| 5 | Dual Redundancy: A_pair = 1-(1-A)^2 | redundancy.c | 1756-UM535 |
| 6 | S-Curve 7-Phase: jerk-limited trajectory | motion_control.c | MOTION-RM003 |
| 7 | Alarm Availability: λ = 1/MTBF, A = MTBF/(MTBF+MTTR) | redundancy.c | ISA-18.2 |
| 8 | EEMUA 191 Priority Matrix | alarm_model.c | EEMUA 191:2013 |
| 9 | Tank Level: dh/dt = (Qin - Cv·√(2gh)) / A | examples/example03 | Seborg et al. 2016 |
| 10 | CRC-32: x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1 | instruction_set.c | IEEE 802.3 |

## Cross-Module Dependencies

```
mini-studio5000-controllogix
├── depends on: (none — zero external deps, only libc + libm)
├── used by: mini-rockwell-plc-engineering (other modules)
│   ├── mini-add-on-instruction-aoi → logix_udt_aoi
│   ├── mini-factorytalk-view-hmi → logix_alarm_model
│   ├── mini-rockwell-ethernet-ip-cip → logix_tag_model (produced/consumed)
│   └── mini-guardlogix-safety-plc → logix_execution_model (safety memory)
└── related standards:
    ├── IEC 61131-3:2013
    ├── ISA-18.2-2016
    ├── ISA-88
    ├── EEMUA 191
    ├── IEEE 802.3 (CRC-32)
    └── ODVA CIP Specification
```
