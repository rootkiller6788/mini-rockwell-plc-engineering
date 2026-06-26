# Course Alignment — mini-compactlogix-pointio

## Nine-University Curriculum Mapping

### MIT — 6.302 Feedback Systems
- **Connection**: CompactLogix PID loop execution depends on reliable I/O data
- **Mapping**: Point I/O scan timing analysis → Nyquist criterion for discrete control
- **File**: src/pointio_scanner.c — scan jitter analysis for control quality

### Stanford — ENGR205 Process Control
- **Connection**: Distributed I/O is the physical interface for process control loops
- **Mapping**: 4-20mA loop integrity → process measurement reliability
- **File**: src/pointio_analog_io.c — loop integrity, alarm management

### Berkeley — ME233 Advanced Control
- **Connection**: Mechatronic systems require precise, low-jitter I/O
- **Mapping**: COS trigger + timestamp → state estimation accuracy
- **File**: src/pointio_scanner.c — COS handling, jitter analysis

### CMU — 24-677 Advanced Control Systems
- **Connection**: System engineering perspective on distributed I/O architecture
- **Mapping**: Connection pool management → fault-tolerant system design
- **File**: src/pointio_connection.c — health check, flapping detection

### Georgia Tech — ECE 6550 Nonlinear Control
- **Connection**: Actuator saturation handling via output fault states
- **Mapping**: Fault mode (zero/hold/predefined) → safe operating envelope
- **File**: src/pointio_digital_io.c — fault state application

### Purdue — ME 575 Industrial Control
- **Connection**: Industrial I/O wiring practices and standards
- **Mapping**: Electronic keying, power budgeting → field installation
- **File**: src/pointio_module.c — power budget, module configuration

### RWTH Aachen — Industrial Control Systems
- **Connection**: German industry perspective on PLC I/O engineering
- **Mapping**: IEC 61131-2 compliance, SIL safety, PROFINET vs EtherNet/IP comparison
- **File**: src/pointio_fault.c — safety parameters, diagnostics

### Tsinghua — Process Control Engineering
- **Connection**: Manufacturing automation with Rockwell/AB controllers
- **Mapping**: CompactLogix CPU selection → capacity planning per 中国制造2025
- **File**: src/pointio_module.c — CPU properties, capacity check

### ISA/IEC Standards
- **ISA-88**: Not directly applicable (batch control level)
- **ISA-95**: Enterprise-control system integration (via EtherNet/IP)
- **ISA-101**: HMI for I/O diagnostics (LED state mapping)
- **IEC 61131-2**: PLC equipment requirements → digital I/O specs
- **IEC 61131-3**: Programming languages — this module provides the I/O data
- **IEC 61508**: Functional safety → SIL parameters, fault states
- **IEC 62443**: Industrial security → connection authentication concept
