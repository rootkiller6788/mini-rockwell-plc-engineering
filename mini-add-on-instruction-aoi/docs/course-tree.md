# Course Tree — mini-add-on-instruction-aoi

## Prerequisite Knowledge Dependencies

```
PLC Fundamentals (IEC 61131-3)
  ├── Ladder Logic (LD) basics
  ├── Structured Text (ST) basics
  ├── Function Block Diagram (FBD) basics
  └── Program Organization Units (POUs)
       │
       ▼
Rockwell Platform Basics
  ├── ControlLogix architecture (backplane, I/O modules)
  ├── Studio 5000 IDE (project structure, .ACD format)
  ├── Tag-based addressing (vs register-based)
  └── EtherNet/IP communication basics
       │
       ▼
Add-On Instructions (AOI) — THIS MODULE
  ├── AOI Definition (parameters, local tags, routines)
  ├── AOI Instance (runtime state, memory layout)
  ├── AOI Signature (SHA-1, canonical serialization)
  ├── AOI Library (project management, version control)
  ├── Context-Sensitive Parameters (v31+)
  └── Safety AOI Certification (IEC 61508)
       │
       ▼
Advanced AOI Topics (L8)
  ├── Online editing algorithm
  ├── L5X import/export
  ├── Redundancy state sync
  └── Controller performance modeling
       │
       ▼
Research Frontiers (L9)
  ├── Digital twin integration
  ├── Edge AI (CompactLogix 5480)
  ├── IT/OT convergence (FactoryTalk)
  └── CIP Security (zero-trust)
```

## Dependency Map (What This Module Depends On)

| Dependency | Module | Status |
|-----------|--------|--------|
| IEC 61131-3 programming languages | mini-plc-iec61131-fundamentals | ✓ |
| PID control theory | mini-pid-control-engineering | ✓ |
| Industrial communication protocols | mini-industrial-communication-protocol | ✓ |
| Tags, UDTs, data types | mini-plc-iec61131-fundamentals | ✓ |
| Safety instrumented systems | mini-safety-instrumented-system | Partial |
| ISA-88 batch process | mini-batch-process-control-isa88 | Reference only |

## Dependency Map (What Depends On This Module)

| Consumer | Why |
|----------|-----|
| DCS architecture (mini-dcs) | AOIs drive DCS faceplate design |
| SCADA/HMI (mini-scada) | Tag database provides I/O model |
| Siemens comparison (mini-siemens) | Cross-vendor AOI comparison |
| Motion control (mini-motion) | MOTION_AOI integration |
| Industrial AI (mini-industrial-ai) | AOI as deployment container |
