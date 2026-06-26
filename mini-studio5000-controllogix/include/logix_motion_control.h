/**
 * @file    logix_motion_control.h
 * @brief   L5-L6: ControlLogix Integrated Motion (CIP Motion)
 *
 * Knowledge Mapping:
 *   L5 Algorithms:    Motion Axis Move (MAM), Jog (MAJ), Stop (MAS),
 *                     Homing (MAH), Gear (MAG), Position Cam (MAPC),
 *                     Motion Coordinated Transform (MCT)
 *   L6 Problems:      Servo motor positioning, electronic gearing,
 *                     flying shear, pick-and-place, rotary knife
 *   L3 Structures:   Motion planner coarse update rate, servo loop
 *                     structure, axis state machine
 *   L7 Applications: Rockwell Kinetix 5700/6500 servo drives
 *
 * ControlLogix uses CIP Motion protocol over EtherNet/IP for real-time
 * motion control. The motion planner runs on the controller and sends
 * position/velocity/torque commands to drives at the coarse update rate.
 *
 * Reference: Rockwell Automation Publication MOTION-RM003
 *   "Logix5000 Controllers Motion Instructions"
 *
 * Course Alignment:
 *   MIT 2.171 - Digital Control: servo control systems
 *   Purdue ME 575 - Industrial Control: motion control
 *   Georgia Tech AE 6530 - Optimal Estimation: Kalman filtering for servo
 */

#ifndef LOGIX_MOTION_CONTROL_H
#define LOGIX_MOTION_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Motion Axis State Machine
 *
 * The ControlLogix axis state machine defines the lifecycle of a motion axis:
 *
 *   AXIS_STATE_SHUTDOWN  ->  AXIS_STATE_PRE_CHARGE (power bus pre-charge)
 *   AXIS_STATE_PRE_CHARGE -> AXIS_STATE_STOPPED (ready, drive enabled)
 *   AXIS_STATE_STOPPED -> AXIS_STATE_STARTING_HOMING / STARTING_MOTION
 *   AXIS_STATE_STARTING_HOMING -> AXIS_STATE_HOMING
 *   AXIS_STATE_HOMING -> AXIS_STATE_STOPPED (home complete)
 *   AXIS_STATE_STARTING_MOTION -> AXIS_STATE_RUNNING (move command accepted)
 *   AXIS_STATE_RUNNING -> AXIS_STATE_STOPPING / STOPPED
 *   AXIS_STATE_STOPPING -> AXIS_STATE_STOPPED
 *   Any -> AXIS_STATE_MAJOR_FAULTED (E-Stop, drive fault, following error)
 *   AXIS_STATE_MAJOR_FAULTED -> AXIS_STATE_SHUTDOWN (after fault reset)
 *
 * Reference: MOTION-RM003, Chapter 2 "Axis State Model"
 * ======================================================================== */

typedef enum {
    AXIS_STATE_SHUTDOWN          = 0,
    AXIS_STATE_PRE_CHARGE        = 1,
    AXIS_STATE_STOPPED           = 2,
    AXIS_STATE_STARTING_HOMING   = 3,
    AXIS_STATE_HOMING            = 4,
    AXIS_STATE_STARTING_MOTION   = 5,
    AXIS_STATE_RUNNING           = 6,
    AXIS_STATE_STOPPING          = 7,
    AXIS_STATE_MAJOR_FAULTED     = 8,
    AXIS_STATE_DISABLED          = 9
} motion_axis_state_t;

typedef enum {
    AXIS_TYPE_ROTARY    = 0,  /* Unlimited rotation (e.g., spindle) */
    AXIS_TYPE_LINEAR    = 1   /* Bounded travel (e.g., ballscrew) */
} motion_axis_type_t;

typedef enum {
    MOTION_MOVE_ABSOLUTE = 0,
    MOTION_MOVE_INCREMENTAL = 1,
    MOTION_MOVE_ROTARY_SHORTEST = 2,
    MOTION_MOVE_ROTARY_POSITIVE = 3,
    MOTION_MOVE_ROTARY_NEGATIVE = 4
} motion_move_type_t;

/* ========================================================================
 * L3: Axis Configuration and Dynamic Parameters
 * ======================================================================== */

/**
 * @brief Motion axis configuration
 *
 * Key dynamic parameters:
 *   - Position loop: Proportional gain + velocity feedforward
 *   - Velocity loop: PI controller (Kvp, Kvi) inside drive
 *   - Torque/current loop: inside drive (not exposed to controller)
 *
 * Units:
 *   - Rotary: position in degrees, velocity in deg/sec
 *   - Linear: position in mm, velocity in mm/sec
 *   - Drive resolution: encoder counts per revolution
 *
 * The servo loop cascade is:
 *   Position Loop (controller, CUR) -> Velocity Loop (drive) -> Current Loop (drive)
 *
 * Position loop bandwidth: typically 10-50 Hz for industrial servos
 * Velocity loop bandwidth: typically 200-1000 Hz
 * Current loop bandwidth: typically 2-5 kHz
 *
 * Reference: Motion System Application Technique MOTION-AT005
 */
typedef struct {
    char                axis_name[40];
    motion_axis_type_t  type;
    uint32_t            axis_number;       /* Logical axis number (0-255) */
    uint32_t            coarse_update_rate_us;

    /* Mechanical parameters */
    double              drive_resolution;     /* Counts per motor rev */
    double              gear_ratio_num;       /* Motor revs per load rev numerator */
    double              gear_ratio_den;       /* Denominator */
    double              lead_screw_pitch_mm;  /* mm per rev (linear only) */
    double              travel_limit_pos;     /* Positive software limit */
    double              travel_limit_neg;     /* Negative software limit */

    /* Dynamic limits */
    double              max_velocity;         /* Maximum velocity in position units/sec */
    double              max_acceleration;     /* Max accel in position units/sec^2 */
    double              max_deceleration;     /* Max decel in position units/sec^2 */
    double              max_jerk;             /* Max jerk in position units/sec^3 */

    /* Velocity profile */
    double              command_velocity;     /* Current commanded velocity */
    double              command_accel;        /* Current commanded acceleration */
    double              command_decel;        /* Current commanded deceleration */
    double              command_jerk;         /* Current commanded jerk (S-curve) */

    /* Position loop gains */
    double              pos_kp;               /* Position proportional gain (1/sec) */
    double              pos_kvff;             /* Velocity feedforward gain (0-1) */
    double              pos_kaff;             /* Acceleration feedforward (sec) */

    /* Following error */
    double              following_error;      /* Command - Actual position */
    double              following_error_limit;/* Fault if exceeded */
    double              pos_feedback_filter_tau_sec; /* Low-pass filter on feedback */

    /* Home configuration */
    double              home_position;        /* Position after homing */
    double              home_offset;          /* Offset from home sensor to 0 position */
    uint32_t            home_sequence;        /* 0=immediate, 1=switch+marker, 2=switch only */

    /* Status */
    motion_axis_state_t state;
    bool                servo_active;         /* Drive power enabled */
    bool                homed;                /* Axis has been homed */
    bool                move_in_progress;
    bool                move_complete;
    bool                faulted;
    uint32_t            fault_code;
    double              actual_position;      /* Feedback position */
    double              actual_velocity;      /* Feedback velocity */
    double              command_position;     /* Commanded position (from planner) */
    double              target_position;      /* Target position for current move */
} motion_axis_t;

/* ========================================================================
 * L5: Motion Move Profile Generation
 * ======================================================================== */

/**
 * @brief S-curve velocity profile generation
 *
 * The S-curve profile limits jerk to produce smooth acceleration transitions,
 * reducing mechanical stress and improving tracking accuracy.
 *
 * Profile phases:
 *   1. Jerk ramp-up (jerk = +J_max, accel increases linearly)
 *   2. Constant acceleration (jerk = 0, accel = A_max)
 *   3. Jerk ramp-down (jerk = -J_max, accel decreases to 0)
 *   4. Constant velocity (accel = 0, velocity = V_max)
 *   5. Jerk ramp-down (jerk = -J_max, decel begins)
 *   6. Constant deceleration (jerk = 0, decel = A_max)
 *   7. Jerk ramp-up to zero (jerk = +J_max, decel decreases to 0)
 *
 * Total move time: T_total = T1+T2+T3 + T4 + T5+T6+T7
 * Distance: s = integral of velocity profile
 *
 * For short moves (where max velocity not reached), the profile may
 * consist of only phases 1-3-5-7 (triangular acceleration profile).
 *
 * Reference: MOTION-RM003, "Motion Axis Move (MAM)"
 *            Biagiotti & Melchiorri (2008) "Trajectory Planning for
 *            Automatic Machines and Robots"
 */
typedef struct {
    double  jerk_up_time;      /* Time for phase 1 (sec) */
    double  const_accel_time;  /* Time for phase 2 (sec) */
    double  jerk_down_time;    /* Time for phase 3 (sec) */
    double  const_vel_time;    /* Time for phase 4 (sec) */
    double  decel_jerk_time;   /* Time for phase 5 (sec) */
    double  const_decel_time;  /* Time for phase 6 (sec) */
    double  final_jerk_time;   /* Time for phase 7 (sec) */
    double  total_time;        /* Total profile duration (sec) */
    double  total_distance;    /* Total move distance */
    bool    is_triangular;     /* True if V_max not reached (short move) */
} motion_s_curve_profile_t;

/**
 * @brief L5: Motion planner state for a single move
 *
 * Generates position, velocity, acceleration commands at each
 * coarse update cycle based on the S-curve profile.
 */
typedef struct {
    motion_move_type_t  move_type;
    double              start_position;
    double              target_position;
    double              profile_velocity;
    double              profile_accel;
    double              profile_decel;
    double              profile_jerk;
    double              elapsed_time;
    double              total_time;
    double              current_velocity;
    double              current_position;
    double              distance_remaining;
    bool                move_complete;
    bool                move_active;
} motion_planner_t;

/* ========================================================================
 * L5: Electronic Gearing and Camming
 * ======================================================================== */

/**
 * @brief Motion Axis Gear (MAG) parameters
 *
 * Electronic gearing synchronizes a slave axis to a master axis with
 * a programmable gear ratio. Common applications:
 *   - Conveyor synchronization
 *   - Multi-axis web handling
 *   - Flying shear (slave follows master with offset)
 *
 * Slave position = Master position * ratio_num / ratio_den + offset
 *
 * Ratio can be changed on-the-fly (real-time gear ratio adjustment).
 *
 * Reference: MOTION-RM003, "Motion Axis Gear (MAG)"
 */
typedef struct {
    uint32_t master_axis_number;
    uint32_t slave_axis_number;
    double   ratio_num;          /* Numerator of gear ratio */
    double   ratio_den;          /* Denominator of gear ratio */
    double   offset;             /* Position offset */
    bool     gearing_active;
    bool     clutch_active;      /* Enable/disable gearing engagement */
    double   master_reference;
    double   slave_command;
    double   actual_ratio;       /* Measured ratio = delta_slave/delta_master */
} motion_gear_t;

/**
 * @brief Motion Axis Position Cam (MAPC) parameters
 *
 * Position camming defines a non-linear relationship between master and slave
 * positions via a cam table. Used for:
 *   - Flying shear (complex cut profiles)
 *   - Rotary knife (circular to linear motion)
 *   - Pick-and-place mechanisms
 *   - Packaging machines
 *
 * The cam profile is defined by a table of master/slave position pairs
 * with optional cubic interpolation between points.
 *
 * Reference: MOTION-RM003, "Motion Axis Position Cam (MAPC)"
 */
typedef struct {
    uint32_t master_axis_number;
    uint32_t slave_axis_number;
    uint32_t cam_point_count;
    double   cam_master[256];    /* Master positions at each cam point */
    double   cam_slave[256];     /* Slave positions at each cam point */
    bool     camming_active;
    uint32_t current_segment;
    double   master_start_position;  /* Master position at cam start */
    double   slave_start_position;   /* Slave position at cam start */
} motion_cam_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief Initialize a motion axis with default parameters
 * Complexity: O(1)
 */
void logix_motion_axis_init(motion_axis_t *axis,
                             const char *name,
                             motion_axis_type_t type,
                             uint32_t axis_number);

/**
 * @brief Configure axis mechanical parameters
 * Complexity: O(1)
 */
void logix_motion_axis_set_mechanical(motion_axis_t *axis,
                                        double resolution,
                                        double gear_num, double gear_den,
                                        double pitch_mm);

/**
 * @brief Configure axis dynamic limits
 * Complexity: O(1)
 */
void logix_motion_axis_set_dynamics(motion_axis_t *axis,
                                      double max_vel,
                                      double max_accel,
                                      double max_decel,
                                      double max_jerk);

/**
 * @brief Configure position loop gains
 * Complexity: O(1)
 */
void logix_motion_axis_set_gains(motion_axis_t *axis,
                                   double kp, double kvff, double kaff);

/**
 * @brief Compute an S-curve velocity profile for a position move
 *
 * Given start position, target position, and dynamic limits, computes
 * the complete S-curve profile including all phase times.
 *
 * @param start_pos   Starting position
 * @param target_pos  Desired final position
 * @param max_vel     Maximum velocity limit
 * @param max_accel   Maximum acceleration limit
 * @param max_jerk    Maximum jerk limit
 * @param profile     [out] Computed S-curve profile
 *
 * @return Total move time in seconds
 *
 * Complexity: O(1)
 * Reference: Biagiotti & Melchiorri (2008), Ch. 3 "Trajectory Planning"
 */
double logix_motion_compute_s_curve(double start_pos, double target_pos,
                                      double max_vel, double max_accel,
                                      double max_jerk,
                                      motion_s_curve_profile_t *profile);

/**
 * @brief Execute one step of the motion planner
 *
 * Advances the motion profile by dt_sec and computes the new
 * position/velocity command based on the S-curve profile.
 *
 * @param planner   Motion planner state
 * @param dt_sec    Time step (typically coarse update rate)
 * @param pos_cmd   [out] New command position
 * @param vel_cmd   [out] New command velocity
 *
 * Complexity: O(1)
 */
void logix_motion_planner_step(motion_planner_t *planner,
                                 double dt_sec,
                                 double *pos_cmd,
                                 double *vel_cmd);

/**
 * @brief Simulate axis servo loop (position loop only)
 *
 * Position loop: Vel_cmd = Kp * (Pos_cmd - Pos_fbk) + Kvff * Vel_cmd_ff
 *
 * Velocity and current loops assumed to be handled by drive.
 *
 * @param axis      Axis state
 * @param pos_cmd   Position command from planner
 * @param vel_ff    Velocity feedforward from planner
 * @param dt_sec    Time step
 *
 * Complexity: O(1)
 */
void logix_motion_servo_update(motion_axis_t *axis,
                                double pos_cmd, double vel_ff,
                                double dt_sec);

/**
 * @brief Electronic gear ratio computation
 *
 * Slave_cmd = Master_ref * ratio_num / ratio_den + offset
 *
 * Complexity: O(1)
 */
double logix_motion_gear_compute(const motion_gear_t *gear,
                                   double master_reference);

/**
 * @brief Electronic cam position interpolation
 *
 * Uses cubic Hermite interpolation between cam table points to
 * compute smooth slave position from master position.
 *
 * @param cam            Cam table
 * @param master_pos     Current master position
 * @return Slave position corresponding to master position
 *
 * Complexity: O(log N) for binary search + O(1) for interpolation
 */
double logix_motion_cam_interpolate(const motion_cam_t *cam,
                                      double master_pos);

/**
 * @brief Check if axis following error exceeds limit
 *
 * Following error = |command_position - actual_position|
 * If following_error > following_error_limit, axis should fault.
 *
 * @return true if error exceeds limit
 * Complexity: O(1)
 */
bool logix_motion_check_following_error(const motion_axis_t *axis);

/**
 * @brief Compute the minimum achievable move time for given parameters
 *
 * For a triangular profile (short move, max velocity not reached):
 *   T_min = 2 * sqrt(distance / max_jerk)  (for pure jerk-limited move)
 *
 * For a trapezoidal profile:
 *   T_min_vel_limited = distance / max_vel + max_vel / max_accel
 *
 * @return Minimum achievable move time in seconds
 * Complexity: O(1)
 * Reference: Biagiotti & Melchiorri (2008)
 */
double logix_motion_min_move_time(double distance,
                                    double max_vel,
                                    double max_accel,
                                    double max_jerk);

#endif /* LOGIX_MOTION_CONTROL_H */