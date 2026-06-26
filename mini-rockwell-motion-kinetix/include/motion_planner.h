/**
 * @file motion_planner.h
 * @brief Motion profile generation and trajectory planning for Kinetix motion.
 *
 * Level: L2 Core Concepts + L3 Engineering Structures + L5 Algorithms
 * Reference:
 *   - Rockwell Automation MOTION-RM002, "Logix 5000 Motion Instructions"
 *   - Biagiotti & Melchiorri, "Trajectory Planning for Automatic Machines" (2008)
 *   - Ellis, "Control System Design Guide" (2012), Ch.12
 *   - Khalil & Dombre, "Modeling, Identification and Control of Robots" (2002), Ch.13
 *
 * Implements:
 *   L2: Trapezoidal velocity profile, S-curve (jerk-limited) profile
 *   L3: Profile generator finite state machine, segment sequencing
 *   L5: Electronic camming with cubic spline interpolation, electronic gearing,
 *       registration-based position capture, blending/merge between moves
 *
 * Alignment: MIT 2.171, Stanford ENGR205, Berkeley ME233, Tsinghua Motion Control
 */

#ifndef MOTION_PLANNER_H
#define MOTION_PLANNER_H

#include "axis_types.h"
#include <stddef.h>

/*===========================================================================
 * L2: Core Concepts – Motion Profile Types
 *===========================================================================*/

/**
 * @brief Motion profile type enumeration.
 *
 * TRAPEZOIDAL: 3-segment profile (accel – cruise – decel), no jerk limiting.
 *              Acceleration changes instantaneously → infinite jerk.
 *              Computationally simplest, used for rough positioning.
 *
 * S_CURVE:      7-segment profile with jerk limiting. Segments:
 *               accel_jerk_up → accel_const → accel_jerk_down →
 *               cruise → decel_jerk_up → decel_const → decel_jerk_down.
 *               Reduces mechanical shock and settling time by 10-30%.
 *
 * CUSTOM:       User-defined profile table (arbitrary position-velocity-time triples).
 *
 * BLENDED:      Continuous blending between consecutive moves (no stop between).
 *               Requires next-move lookahead.
 */
typedef enum {
    PROFILE_TYPE_TRAPEZOIDAL = 0,
    PROFILE_TYPE_S_CURVE     = 1,
    PROFILE_TYPE_CUSTOM      = 2,
    PROFILE_TYPE_BLENDED     = 3
} profile_type_t;

/**
 * @brief Profile generator segment phase.
 */
typedef enum {
    PROFILE_PHASE_IDLE         = 0,   /**< No active profile */
    PROFILE_PHASE_ACCELERATING = 1,   /**< Positive acceleration phase */
    PROFILE_PHASE_CONST_VEL    = 2,   /**< Constant velocity cruise */
    PROFILE_PHASE_DECELERATING = 3,   /**< Negative acceleration phase */
    PROFILE_PHASE_DWELLING     = 4,   /**< Dwell (zero velocity hold) */
    PROFILE_PHASE_COMPLETE     = 5,   /**< Profile completed */
    PROFILE_PHASE_ABORTED      = 6    /**< Profile aborted mid-sequence */
} profile_phase_t;

/*===========================================================================
 * L3: Engineering Structures – Motion Profile Parameters
 *===========================================================================*/

/**
 * @brief Parameters for a trapezoidal velocity profile.
 *
 * Specifies a point-to-point move with constant acceleration/deceleration.
 *
 * The profile has three phases:
 *   1. Acceleration from 0 to cruise_velocity at specified accel
 *   2. Cruise at constant velocity
 *   3. Deceleration to 0 at specified decel
 *
 * If total distance is too short to reach cruise velocity, the profile
 * degenerates to a triangular (accel-decel) profile.
 */
typedef struct {
    double   start_position;          /**< Initial position (eng units) */
    double   target_position;         /**< Target position (eng units) */
    double   cruise_velocity;         /**< Desired constant velocity (eng units/sec) */
    double   acceleration;            /**< Acceleration rate (eng units/sec²) */
    double   deceleration;            /**< Deceleration rate (eng units/sec²) */
    move_type_t move_type;            /**< Absolute, relative, or additive */
} trapezoidal_params_t;

/**
 * @brief Parameters for an S-curve (jerk-limited) velocity profile.
 *
 * Extends trapezoidal profile with jerk limiting (finite jerk = d^3x/dt^3).
 * The S-curve has up to 7 segments using bounded jerk.
 *
 * Reference: Biagiotti & Melchiorri (2008), Section 3.3 – Jerk-Limited Trajectories
 */
typedef struct {
    double   start_position;           /**< Initial position (eng units) */
    double   target_position;          /**< Target position (eng units) */
    double   cruise_velocity;          /**< Desired constant velocity */
    double   max_jerk;                 /**< Maximum jerk magnitude (eng units/sec³) */
    double   max_acceleration;         /**< Maximum acceleration (eng units/sec²) */
    double   max_deceleration;         /**< Maximum deceleration (eng units/sec²) */
    move_type_t move_type;             /**< Absolute, relative, or additive */
} scurve_params_t;

/**
 * @brief Single entry in an electronic cam table.
 *
 * Defines a master-slave position relationship. The cam profile
 * maps master position to slave position. Points between table entries
 * are interpolated (cubic spline recommended).
 */
typedef struct {
    double   master_position;          /**< Master axis position (eng units) */
    double   slave_position;           /**< Slave axis position (eng units) */
    double   slave_velocity;           /**< Slave velocity at this point (for cubic fit) */
} cam_point_t;

/**
 * @brief Electronic cam table with interpolation metadata.
 *
 * A cam table is an array of cam_point_t entries that defines a
 * master→slave position mapping function f(master_pos) = slave_pos.
 */
typedef struct {
    cam_point_t *points;               /**< Array of cam points */
    size_t       num_points;           /**< Number of cam table entries */
    size_t       capacity;             /**< Allocated capacity */
    double       master_start;         /**< Cam start master position */
    double       master_end;           /**< Cam end master position (1 cycle) */
    bool         cyclic;               /**< True if cam is cyclic (repeating) */
    double       speed_scaling;        /**< Global speed scaling factor (1.0 = nominal) */
    bool         direction_lock;       /**< Disallow reverse master direction */
} cam_table_t;

/**
 * @brief Electronic gearing parameters.
 *
 * Electronic gearing establishes a velocity/position relationship
 * between a master and slave axis without a mechanical coupling:
 *
 *   slave_velocity = master_velocity × (numerator / denominator)
 *
 * A "clutch" mechanism provides gradual engagement/disengagement.
 */
typedef struct {
    double   gear_ratio_numerator;     /**< Electronic gear ratio numerator */
    double   gear_ratio_denominator;   /**< Electronic gear ratio denominator */
    double   clutch_acceleration;      /**< Clutch engagement acceleration rate */
    double   clutch_deceleration;      /**< Clutch disengagement deceleration rate */
    bool     clutch_active;            /**< Current clutch state */
    double   clutch_slip;              /**< Current slip (position error due to clutch ramp) */
    double   max_slave_accel;          /**< Maximum slave acceleration limit */
    double   max_slave_velocity;       /**< Maximum slave velocity limit */
} gearing_params_t;

/*===========================================================================
 * L3: Engineering Structures – Profile Generator State
 *===========================================================================*/

/**
 * @brief Complete motion planner / profile generator state.
 *
 * This is the core state machine that computes position, velocity,
 * and acceleration setpoints at each servo cycle for a given axis.
 */
typedef struct {
    profile_type_t  profile_type;      /**< Active profile type */
    profile_phase_t phase;             /**< Current phase within profile */
    double          cycle_time_s;      /**< Profile generator update period (seconds) */

    /* Output state – current command values */
    double          cmd_position;      /**< Current command position (eng units) */
    double          cmd_velocity;      /**< Current command velocity (eng units/sec) */
    double          cmd_acceleration;  /**< Current command acceleration (eng units/sec²) */
    double          cmd_jerk;          /**< Current command jerk (eng units/sec³) */

    /* Profile internal state – trapezoidal */
    double          t_acc;             /**< Duration of acceleration phase (s) */
    double          t_cruise;          /**< Duration of cruise phase (s) */
    double          t_decel;           /**< Duration of deceleration phase (s) */
    double          t_total;           /**< Total profile duration (s) */
    double          dist_acc;          /**< Distance covered during acceleration (eng units) */
    double          dist_decel;        /**< Distance covered during deceleration (eng units) */
    double          dist_total;        /**< Total move distance (eng units) */
    double          elapsed_time;      /**< Elapsed time since profile start (s) */

    /* Profile parameters snapshot */
    double          start_position;    /**< Position at profile start */
    double          target_position;   /**< Target position */
    double          cruise_velocity;   /**< Cruise velocity magnitude */
    double          acceleration;      /**< Acceleration magnitude (current profile) */
    double          deceleration;      /**< Deceleration magnitude (current profile) */
    double          jerk;              /**< Jerk magnitude (S-curve only) */
    double          direction_sign;    /**< +1 or -1 for move direction */

    /* S-curve specific */
    double          t_j1;              /**< Jerk-up time for acceleration (s) */
    double          t_j2;              /**< Jerk-down time for acceleration (s) */
    double          t_j3;              /**< Jerk-up time for deceleration (s) */
    double          t_j4;              /**< Jerk-down time for deceleration (s) */
    double          accel_peak;        /**< Peak actual acceleration achieved */

    /* Gearing/camming specific */
    bool            gearing_active;
    bool            camming_active;
    gearing_params_t gear;
    cam_table_t     cam_table;
    size_t          cam_segment_idx;   /**< Current cam table segment index */
    double          cam_phase_offset;  /**< Cam phase offset for engagement */

    /* Registration */
    bool            registration_pending;
    double          registration_position;
    bool            window_active;     /**< Registration window open */

    /* Multi-move blending / queue */
    bool            next_move_queued;
    double          blend_speed;       /**< Speed at point of blending (eng units/sec) */
    double          blend_position;    /**< Position at blend transition */
    bool            merging;           /**< Currently in merge phase of blend */

    /* Diagnostic */
    double          max_vel_achieved;  /**< Peak velocity during this move */
    double          max_accel_achieved;/**< Peak acceleration during this move */
    double          final_position_error; /**< Position error at profile completion */
} profile_generator_t;

/*===========================================================================
 * L2: Core Concepts – Motion Planner API
 *===========================================================================*/

/**
 * @brief Initialize the motion profile generator.
 *
 * Sets all state to safe defaults and sets the update cycle time.
 *
 * @param pg        Profile generator to initialize
 * @param Ts        Servo update cycle time (seconds, must be > 0)
 */
void profile_gen_init(profile_generator_t *pg, double Ts);

/**
 * @brief Plan a trapezoidal point-to-point move.
 *
 * Computes the profile timing parameters (t_acc, t_cruise, t_decel, t_total)
 * and initializes the generator for execution.
 *
 * Handles the degenerate case where distance is too short to reach cruise
 * velocity (triangular profile).
 *
 * @param pg        Profile generator
 * @param params    Trapezoidal move parameters
 * @return          0 on success, -1 if parameters invalid
 */
int profile_gen_plan_trapezoidal(profile_generator_t *pg,
                                  const trapezoidal_params_t *params);

/**
 * @brief Plan an S-curve (jerk-limited) point-to-point move.
 *
 * Computes the extended 7-phase profile with finite jerk.
 * Handles degenerate S-curve cases (short moves where jerk phases
 * cannot complete – falls back to trapezoidal/triangular as needed).
 *
 * Reference: Biagiotti & Melchiorri (2008), Algorithm 3.1
 *
 * @param pg        Profile generator
 * @param params    S-curve move parameters
 * @return          0 on success, -1 if parameters invalid
 */
int profile_gen_plan_scurve(profile_generator_t *pg,
                             const scurve_params_t *params);

/**
 * @brief Compute the next setpoint in the profile (one time step).
 *
 * Advances the profile by one cycle time and computes position, velocity,
 * acceleration, and jerk.
 *
 * @param pg                Profile generator
 * @param[out] position     Output command position
 * @param[out] velocity     Output command velocity
 * @param[out] acceleration Output command acceleration (may be NULL)
 * @return                  Current profile phase after this step
 */
profile_phase_t profile_gen_step(profile_generator_t *pg,
                                  double *position,
                                  double *velocity,
                                  double *acceleration);

/**
 * @brief Abort the active profile with controlled deceleration.
 *
 * Computes a stop trajectory from the current state using the specified
 * deceleration rate.
 *
 * @param pg             Profile generator
 * @param decel_limit    Deceleration to use for stop (eng units/sec²)
 * @return               0 on success
 */
int profile_gen_abort(profile_generator_t *pg, double decel_limit);

/**
 * @brief Check if the active profile has completed.
 *
 * @param pg  Profile generator
 * @return    true if profile is complete (IDLE or COMPLETE phase)
 */
bool profile_gen_is_done(const profile_generator_t *pg);

/**
 * @brief Reset the profile generator to idle state.
 *
 * @param pg  Profile generator to reset
 */
void profile_gen_reset(profile_generator_t *pg);

/*===========================================================================
 * L5: Algorithms – Electronic Gearing
 *===========================================================================*/

/**
 * @brief Initialize electronic gearing parameters.
 *
 * Sets up the gear ratio and clutch ramping parameters for a MAG
 * (Motion Axis Gearing) instruction.
 *
 * @param gear          Gearing parameters struct to initialize
 * @param numerator     Gear ratio numerator
 * @param denominator   Gear ratio denominator
 * @param clutch_accel  Clutch engagement acceleration
 * @param clutch_decel  Clutch disengagement acceleration
 */
void gearing_init(gearing_params_t *gear,
                  double numerator, double denominator,
                  double clutch_accel, double clutch_decel);

/**
 * @brief Compute geared slave command from master position and velocity.
 *
 * Implements the gearing relationship:
 *   slave_position(m) = master_position(m) × ratio + clutch_offset
 *   slave_velocity = master_velocity × ratio (smoothed by clutch ramp)
 *
 * @param gear              Gearing parameters
 * @param master_position   Current master position (eng units)
 * @param master_velocity   Current master velocity (eng units/sec)
 * @param slave_velocity    Maximum allowed slave velocity (eng units/sec)
 * @param clutch_cmd        true to engage clutch, false to disengage
 * @param[out] slave_pos    Computed slave command position
 * @param[out] slave_vel    Computed slave command velocity
 * @return                  0 on success
 */
int gearing_compute(gearing_params_t *gear,
                    double master_position, double master_velocity,
                    double slave_velocity,
                    bool clutch_cmd,
                    double *slave_pos, double *slave_vel);

/*===========================================================================
 * L5: Algorithms – Electronic Camming
 *===========================================================================*/

/**
 * @brief Initialize a cam table.
 *
 * Allocates memory for the cam table with the specified capacity.
 * Cam points must be added with cam_table_add_point().
 *
 * @param cam       Cam table to initialize
 * @param capacity  Maximum number of cam points
 * @param cyclic    true if cam repeats (e.g., rotary)
 * @return          0 on success, -1 on allocation failure
 */
int cam_table_init(cam_table_t *cam, size_t capacity, bool cyclic);

/**
 * @brief Add a point to the cam table in ascending master position order.
 *
 * @param cam       Cam table
 * @param master    Master position (must be >= previous master position)
 * @param slave     Slave position
 * @return          0 on success, -1 if table full or order violation
 */
int cam_table_add_point(cam_table_t *cam, double master, double slave);

/**
 * @brief Evaluate the cam profile at a given master position.
 *
 * Uses cubic Hermite spline interpolation between cam table points
 * for smooth velocity continuity.
 *
 * @param cam           Cam table
 * @param master_pos    Master position
 * @param[out] slave_pos    Interpolated slave position
 * @param[out] slave_vel    First derivative (dslave/dmaster)
 * @return                  0 on success, -1 if master_pos out of range
 */
int cam_evaluate(const cam_table_t *cam, double master_pos,
                 double *slave_pos, double *slave_vel);

/**
 * @brief Free memory associated with a cam table.
 *
 * @param cam  Cam table to free
 */
void cam_table_free(cam_table_t *cam);

/*===========================================================================
 * L5: Algorithms – Registration Capture
 *===========================================================================*/

/**
 * @brief Arm the registration input on a profile generator.
 *
 * When armed, the next trigger on the specified registration input
 * will latch the current axis position.
 *
 * @param pg               Profile generator
 * @param input_number     Registration input (1 or 2)
 * @param window_start     Registration window start position
 * @param window_end       Registration window end position
 * @return                 0 on success
 */
int profile_gen_arm_registration(profile_generator_t *pg,
                                  uint8_t input_number,
                                  double window_start,
                                  double window_end);

/**
 * @brief Process a registration trigger event.
 *
 * Called when a physical registration sensor fires. Latches the current
 * position if the registration input is armed and within the window.
 *
 * @param pg               Profile generator
 * @param input_number     Registration input (1 or 2)
 * @param current_position Current axis position at trigger
 * @param[out] latched_pos Latched position (if triggered in window)
 * @return                 0=Not armed, 1=Latched in window, 2=Outside window
 */
int profile_gen_registration_trigger(profile_generator_t *pg,
                                      uint8_t input_number,
                                      double current_position,
                                      double *latched_pos);

/*===========================================================================
 * L5: Algorithms – Blending / Continuous Motion
 *===========================================================================*/

/**
 * @brief Set up blend parameters for the next move.
 *
 * Specifies at what speed the current move should blend into the
 * next move, avoiding a full stop between moves.
 *
 * @param pg            Profile generator
 * @param blend_speed   Speed at which to start blending (eng units/sec)
 * @param next_params   Parameters for the next move (copied internally)
 * @return              0 on success
 */
int profile_gen_setup_blend(profile_generator_t *pg,
                             double blend_speed,
                             const trapezoidal_params_t *next_params);

/**
 * @brief Get the total distance for a planned trapezoidal move.
 *
 * Useful for pre-move validation (checking against soft limits).
 *
 * @param params    Move parameters
 * @return          Total move distance (eng units), negative on error
 */
double profile_gen_compute_distance(const trapezoidal_params_t *params);

/**
 * @brief Compute the minimum time for a trapezoidal move.
 *
 * @param params    Move parameters
 * @return          Minimum move duration in seconds, negative on error
 */
double profile_gen_compute_duration(const trapezoidal_params_t *params);

#endif /* MOTION_PLANNER_H */
