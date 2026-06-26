# Course Alignment — mini-rockwell-motion-kinetix

## Nine-School Curriculum Mapping

### MIT
- **2.171 Digital Control**: Discrete-time motion profiles, sampling at 2ms, ZOH discretization of PID
- **6.302 Feedback Systems**: Cascaded loop design, bandwidth hierarchy, stability margins
- Mapping: S-curve profiles, notch filter design, Lyapunov stability

### Stanford
- **ENGR205 Process Control**: Feedforward (velocity/acceleration FF), disturbance rejection
- **AA272 GNSS**: Registration position capture (similar to GNSS snapshot positioning)
- Mapping: Feedforward for servo, registration latching, Monte Carlo robustness

### Berkeley
- **ME233 Advanced Control**: Lyapunov-based adaptation, observer design
- **EE C128 Mechatronics**: Sensor feedback, encoder signal processing, motor drives
- Mapping: Luenberger observer, adaptive gain, commutation, friction compensation

### CMU
- **24-677 Advanced Control Systems**: Multi-variable control, cascaded architectures
- **18-771 Linear Systems**: Transfer function analysis, frequency response
- Mapping: Cascaded servo loops, de-coupling, anti-resonance identification

### Georgia Tech
- **ECE 6550 Nonlinear Control**: Lyapunov methods, adaptive control
- **AE 6530 Optimal Estimation**: RLS parameter estimation, Kalman filtering
- Mapping: Lyapunov adaptive laws, RLS inertia estimation

### Purdue
- **ME 575 Industrial Control**: Practical PID tuning, industrial communication
- **ECE 602 Lumped Systems**: Transfer function modeling of mechanical systems
- Mapping: Relay auto-tuning, CIP Motion, two-mass system modeling

### RWTH Aachen
- **Industrial Control Systems**: PLC motion control, fieldbus integration
- **PLC/SCADA Engineering**: IEC 61131-3 motion blocks, CIP Safety
- Mapping: CIP Motion protocol, STO safety, Kinetix drive integration

### Tsinghua
- **Motion Control Engineering**: Servo drive theory, PM motor control, trajectory planning
- **Industrial Internet**: EtherNet/IP, CIP Sync, edge computing for motion
- Mapping: FOC commutation, VPL motor database, CIP Sync time coordination

### ISA/IEC
- **IEC 61131-3**: PLCopen motion control function blocks
- **IEC 61508/61511**: Functional safety for servo drives
- **IEC 61800-5-2**: Safe torque off, safe stop functions
- **IEC 61800-7-201 (CiA 402)**: Servo drive profile, homing methods
- **ODVA CIP Motion**: Motion device profile, connection management
- **ISO 13849-1**: PL e / Category 4 safety architecture

## Chapter Mapping Detail

| School | Chapter / Module | Submodule File |
|--------|-----------------|----------------|
| MIT 2.171 | Ch.4 Discrete-time systems | src/servo_tuning.c (discretized filters) |
| MIT 6.302 | Ch.7 Loop shaping | src/servo_tuning.c (notch filters) |
| Stanford ENGR205 | Ch.8 Cascade control | src/servo_tuning.c (bandwidth hierarchy) |
| Berkeley ME233 | Ch.8 Adaptive control | src/motion_advanced.c (Lyapunov) |
| CMU 24-677 | Multi-loop architectures | src/servo_tuning.c (three-loop cascade) |
| RWTH | Motion control over EtherNet/IP | src/cip_motion.c |
| Tsinghua | Servo system commissioning | examples/example_servo_tuning.c |
| ISA/IEC 61131-3 | Motion function blocks | src/cip_motion.c (instruction encoding) |
