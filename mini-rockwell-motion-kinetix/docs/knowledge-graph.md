# Knowledge Graph — mini-rockwell-motion-kinetix

## L1: Definitions

| Entry | Definition | Type | File |
|-------|-----------|------|------|
| axis_state_t | CIP Motion axis states (DISABLED, STARTING, RUNNING, STOPPING, ERROR_STOP, ABORTING, HOMING, TESTING, OFFLINE) | enum | include/axis_types.h |
| axis_fault_code_t | CIP Motion fault codes (16-bit bitmap: overload, encoder, STO, etc.) | enum | include/axis_types.h |
| motion_instr_type_t | Logix 5000 motion instructions (MAJ, MAM, MAS, MAFR, MAG, MAPC, MATC, MAH, MSO, MSF, etc.) | enum | include/axis_types.h |
| motor_type_t | Motor types (rotary PM, induction, linear PM, direct drive, virtual) | enum | include/axis_types.h |
| feedback_type_t | Feedback devices (incremental, absolute, resolver, Hall, sin/cos) | enum | include/axis_types.h |
| home_method_t | Homing methods per CiA 402 (switch positive/negative, index, absolute, direct) | enum | include/axis_types.h |
| motor_database_t | Motor nameplate data for field-oriented control | struct | include/axis_types.h |
| axis_config_t | Complete axis configuration (Studio 5000 Axis Properties equivalent) | struct | include/axis_types.h |
| axis_runtime_t | Runtime axis state (live CIP Motion cyclic data) | struct | include/axis_types.h |
| kinetix_drive_model_t | Kinetix drive family catalog IDs (350, 5100, 5300, 5500, 5700, etc.) | enum | include/axis_types.h |
| current_loop_gains_t | Current loop PI controller gains (Kp, Ki, decoupling, deadtime) | struct | include/servo_tuning.h |
| velocity_loop_gains_t | Velocity loop PI + observer gains | struct | include/servo_tuning.h |
| position_loop_gains_t | Position loop P + feedforward gains | struct | include/servo_tuning.h |
| profile_type_t | Motion profile types (trapezoidal, S-curve, custom, blended) | enum | include/motion_planner.h |
| cip_connection_type_t | CIP Motion connection types (multicast, unicast, listen-only) | enum | include/cip_motion.h |

## L2: Core Concepts

| Concept | Description | Implementation |
|---------|-------------|----------------|
| Cascaded Servo Loop | Current → Velocity → Position with bandwidth hierarchy (inner ≥ 4× outer) | src/servo_tuning.c |
| Trapezoidal Velocity Profile | 3-phase motion: accel → cruise → decel | src/motion_planner.c |
| S-Curve Profile | 7-phase jerk-limited profile for smooth motion | src/motion_planner.c |
| Electronic Gearing | Master-slave velocity/position coupling without mechanical linkage | src/motion_planner.c |
| Electronic Camming | Non-linear master→slave position mapping via cam table | src/motion_planner.c |
| CIP Motion Connection | Cyclic I/O connection (O→T, T→O) for motion data exchange | src/cip_motion.c |
| CIP Sync (IEEE 1588) | Precision time protocol for multi-axis coordination | src/cip_motion.c |
| Field-Oriented Control | dq-axis current control (id=0, iq=torque) | src/axis_state.c |
| Following Error | Position tracking error detection and fault | src/axis_state.c |
| Safe Torque Off | Dual-channel STO per IEC 61800-5-2 (SIL 3 / PL e) | src/kinetix_drive.c |

## L3: Engineering Structures

| Structure | Description | File |
|-----------|-------------|------|
| Axis State FSM | CIP Motion state transitions with side effects | src/axis_state.c |
| Homing Sequence FSM | Multi-phase homing (fast approach → creep → index → offset) | src/axis_state.c |
| Profile Generator FSM | Motion profile phases (idle→accel→cruise→decel→complete) | src/motion_planner.c |
| CIP Motion Data Layout | O→T command (64B) and T→O feedback (64B) structures | include/cip_motion.h |
| CIP Axis Object Attributes | Instance attributes for explicit messaging (gain, limit, filter config) | include/cip_motion.h |
| Drive Power Rating DB | Kinetix 5100/5300/5700 catalog database | src/kinetix_drive.c |
| Cam Table Structure | Array of cam points with cubic Hermite spline interpolation | include/motion_planner.h |
| Bi-Quad Filter Structure | Digital notch filter with Tustin discretization | include/servo_tuning.h |
| Load Observer | Luenberger observer structure for velocity estimation | include/servo_tuning.h |
| Autotune State Machine | Multi-step auto-tuning (current → inertia → velocity → position) | include/servo_tuning.h |

## L4: Engineering Laws

| Law/Theorem | Formula | Implementation |
|-------------|--------|----------------|
| Bandwidth Hierarchy | f_current ≥ 4× f_velocity ≥ 4× f_position | src/servo_tuning.c |
| Pole-Zero Current Tuning | Kp = L·ωc, Ki = R·ωc | src/servo_tuning.c |
| Symmetrical Optimum (Velocity) | Kp = J·ωbw, Ki = J·ωbw²/3 | src/servo_tuning.c |
| Åström-Hägglund Relay Tuning | Ku = 4d/(πa), Kp=0.45·Ku, Ki=0.54·Ku/Pu | src/servo_tuning.c |
| Arrhenius Capacitor Life | L = L_rated × 2^((T_rated-T)/10) × (V_rated/V)^2.5 | src/kinetix_drive.c |
| I²t Motor Overload | t_trip = τ_th · ln((I²-I_rated²)/(I²-I_max²)) | src/kinetix_drive.c |
| STO PFHd (1oo2) | PFHd = λD1·λD2·T2·2 + β·λ_avg | src/kinetix_drive.c |
| Safe Failure Fraction | SFF = (λS+λDD)/(λS+λDD+λDU) | src/kinetix_drive.c |
| ZVD Input Shaper | 3-impulse sequence, A_i·e^{-ζωn·ti} cancel residual vibration | src/motion_advanced.c |
| Lyapunov Adaptive Law | dK̂p/dt = γ1·e²/J, dK̂i/dt = γ2·e·∫e/J | src/motion_advanced.c |

## L5: Algorithms/Methods

| Algorithm | Complexity | Reference | File |
|-----------|-----------|-----------|------|
| Trapezoidal Profile Planning | O(1) per plan | Biagiotti & Melchiorri (2008) §2.2 | src/motion_planner.c |
| S-Curve Profile Planning | O(1) per plan | Biagiotti & Melchiorri (2008) §3.3 | src/motion_planner.c |
| Cubic Hermite Cam Interpolation | O(log n) binary search | Catmull-Rom splines | src/motion_planner.c |
| Clutch Ramp for Gearing | O(1) per step | Linear ramp with acceleration limit | src/motion_planner.c |
| Bi-Quad Notch Filter (Tustin) | O(1) per sample | Ellis (2012) §13.3 | src/servo_tuning.c |
| Relay Auto-Tuning | O(n) cycles | Åström-Hägglund (1984) | src/servo_tuning.c |
| Least-Squares Inertia ID | O(n) samples | Ellis (2012) §13.1 | src/servo_tuning.c |
| Friction Compensation | O(1) per step | tanh smooth sign model | src/servo_tuning.c |
| Luenberger Velocity Observer | O(1) per step | Ellis (2012) §14.2 | src/servo_tuning.c |
| RLS with Forgetting Factor | O(1) per step | Ljung (1999) §11 | src/motion_advanced.c |
| Monte Carlo Robustness | O(N·M) trials | Ray & Stengel (1993) | src/motion_advanced.c |
| Cogging Torque Compensation | O(1) per step | Jahns & Soong (1996) | src/motion_advanced.c |
| Input Shaping (ZVD) | O(1) per step | Singer & Seering (1990) | src/motion_advanced.c |

## L6: Canonical Problems

| Problem | Description | Example |
|---------|-------------|---------|
| Point-to-Point Positioning | Pick-and-place conveyor with trapezoidal profile | examples/example_point_to_point.c |
| Rotary Knife (Camming) | Master-slave position cam for cutting on-the-fly | examples/example_camming.c |
| Servo Drive Commissioning | Motor selection → drive sizing → auto-tune → filter design → robustness check | examples/example_servo_tuning.c |
| Servo Start-Up Sequence | DC bus check → STO verify → commutation align → loop enable | src/axis_state.c |
| Drive Sizing | Motor-drive matching based on continuous/peak current | src/kinetix_drive.c |
| Regenerative Energy | DC bus voltage rise during deceleration, shunt sizing | src/kinetix_drive.c |

## L7: Industrial Applications

| Application | Product/Standard | Implementation |
|-------------|-----------------|----------------|
| Kinetix 5100/5300/5700 drives | Rockwell Automation 2198 series | src/kinetix_drive.c |
| CIP Motion on EtherNet/IP | ODVA CIP Motion Vol.1-2 | src/cip_motion.c |
| Studio 5000 Motion Group config | Rockwell Logix 5000 | include/axis_types.h (axis_config_t) |
| MAM/MAJ/MAS/MSO/MSF/MAFR/MAG instructions | Rockwell MOTION-RM002 | src/cip_motion.c |
| CiA 402 Homing Methods | IEC 61800-7-201 | src/axis_state.c |
| IEEE 1588 CIP Sync | Precision Time Protocol | src/cip_motion.c |
| VPL/MPL/TLY/LDC/LDL motors | Rockwell motor portfolio | src/axis_state.c |
| Safe Torque Off SIL 3 / PL e | ISO 13849-1, IEC 61800-5-2 | src/kinetix_drive.c |

## L8: Advanced Topics

| Topic | Method | File |
|-------|--------|------|
| Lyapunov-Based Adaptive Control | Gradient descent on Lyapunov function for gain adaptation | src/motion_advanced.c |
| Monte Carlo Robustness | Statistical evaluation with parameter uncertainty | src/motion_advanced.c |
| Cogging Torque Compensation | Learned table with electrical angle interpolation | src/motion_advanced.c |
| Input Shaping (ZVD) | Convolution with 3-impulse sequence for vibration suppression | src/motion_advanced.c |
| Anti-Resonance Identification | Magnitude minimum detection in frequency response | src/motion_advanced.c |
| RLS Parameter Estimation | Recursive Least Squares with forgetting for inertia tracking | src/motion_advanced.c |
| Adaptive Gain Scheduling | Inertia-dependent and position-dependent gain scaling | src/servo_tuning.c |

## L9: Research Frontiers

| Topic | Status |
|-------|--------|
| IT/OT Convergence for Motion | Documented — CIP Motion + MQTT/OPC UA gateway concept |
| Autonomous Motion Optimization | Documented — reinforcement learning for auto-tuning (conceptual) |
| Digital Twin for Servo Systems | Documented — real-time simulation for virtual commissioning |
| Industrial 5G for Motion Control | Documented — 5G URLLC for wireless CIP Motion (conceptual) |
| Zero-Trust Motion Security | Documented — CIP Security for motion command authentication |
