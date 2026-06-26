# Course Tree — mini-studio5000-controllogix

```
Studio 5000 / ControlLogix Engineering
│
├── L1: Definitions
│   ├── Chassis types (4/7/10/13/17 slot)
│   ├── Controller types (1756-L7x, L8x, 5069 CompactLogix)
│   ├── Tag atomic types (BOOL, SINT, INT, DINT, REAL, LREAL, STRING)
│   ├── Tag scope (Controller vs Program)
│   ├── Produced/Consumed tags
│   ├── PIDE operating modes (Auto/Manual/Cascade/Override/Hand/Program)
│   ├── Timer types (TON/TOF/RTO) and Counter types (CTU/CTD)
│   ├── Alarm severity (Critical/High/Medium/Low)
│   └── Motion axis types (Rotary/Linear/Virtual)
│
├── L2: Core Concepts
│   ├── Controller Organizer hierarchy (Controller > Tasks > Programs > Routines)
│   ├── Task scheduling (Continuous/Periodic/Event)
│   ├── Scan cycle phases (Input/Program/Output/Overhead)
│   ├── Controller modes (Program/Run/Test/Fault)
│   ├── Tag name rules and validation
│   ├── Produced/Consumed connections (RPI, unicast/multicast)
│   ├── ISA-18.2 alarm state machine (NORMAL/ACTIVE_UNACK/ACTIVE_ACK/NORMAL_UNACK)
│   └── Alarm shelving vs suppression
│
├── L3: Structures
│   ├── Scan cycle timing analysis (min/max/avg)
│   ├── Task overlap detection and handling
│   ├── Watchdog configuration and timeout
│   ├── Memory layout (logic/data/IO/safety/comm)
│   ├── UDT member alignment and padding
│   ├── Array dimensioning and indexing (1D/2D/3D)
│   ├── I/O connection types (Direct/Rack-Optimized/Listen-Only)
│   └── COS (Change of State) triggering
│
├── L4: Standards
│   ├── IEC 61131-3:2013 data type compliance
│   ├── ISA-18.2-2016 Alarm Management lifecycle
│   ├── EEMUA 191 Alarm System Design guidelines
│   ├── ISA-5.1 Instrumentation symbology
│   ├── ISA-88 Batch Control program/phasing model
│   ├── Rate-Monotonic Scheduling (Liu & Layland 1973)
│   └── Dual Redundancy Availability formula
│
├── L5: Algorithms
│   ├── PIDE velocity form (incremental output)
│   ├── PIDE positional form (absolute output)
│   ├── Independent vs Dependent gain forms
│   ├── PV first-order lag filter (backward Euler)
│   ├── Anti-reset windup (clamping + back-calculation)
│   ├── Bumpless transfer on mode change
│   ├── TON/TOF/RTO timer logic
│   ├── CTU/CTD counter logic
│   ├── LDLG first-order lead-lag filter
│   ├── SCL scale with limits
│   ├── S-Curve 7-phase trajectory planning
│   ├── Servo position/velocity/torque loop
│   └── CRC-32 and FNV-1a checksums
│
├── L6: Canonical Problems
│   ├── Tank level control (PIDE + process sim)
│   ├── Motor start/stop with interlock
│   ├── Alarm rationalization example
│   ├── Redundant controller failover
│   └── Chassis power budget verification
│
├── L7: Applications
│   ├── 1756 ControlLogix chassis configuration
│   ├── 1769/5069 CompactLogix platform
│   ├── Studio 5000 v20-v36 project structure
│   ├── Kinetix servo integration
│   ├── FactoryTalk Alarms and Events
│   └── PlantPAx DCS architecture
│
├── L8: Advanced Topics
│   ├── Redundancy (synchronized pair, DLR/PRP)
│   ├── Safety PLC (GuardLogix, SIL 2/3)
│   ├── Motion: camming, gearing, registration
│   ├── Five-Nines (99.999%) availability design
│   └── System Overhead Time Slice (SOTS) tuning
│
└── L9: Frontiers (Documented)
    ├── Digital twin integration
    ├── IT/OT convergence
    ├── Edge computing on ControlLogix
    └── CIP Security protocol
```
