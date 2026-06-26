/**
 * @file axis_types.h
 * @brief Core motion axis type definitions for Rockwell Kinetix servo systems.
 *
 * Level: L1 Definitions + L3 Engineering Structures
 * Reference:
 *   - CIP Motion Specification (ODVA, Volume 1, Common Industrial Protocol)
 *   - Rockwell Automation Publication MOTION-RM002 (Logix 5000 Motion Instructions)
 *   - Kinetix 5100/5300/5500/5700 User Manuals (2198-*)
 *   - Hughes & Drury, "Electric Motors and Drives" (2013), Ch.4-7
 *   - Ellis, "Control System Design Guide" (4th Ed, 2012), Ch.8-10
 *
 * This header defines all data types used across the Kinetix motion control
 * implementation: axis states per CIP Motion, instruction types, motor models,
 * feedback types, and safety states.
 *
 * Alignment: MIT 2.171 (Digital Control), Berkeley EE C128 (Mechatronics),
 *            RWTH Industrial Control Systems, Tsinghua Motion Control Engineering
 */

#ifndef AXIS_TYPES_H
#define AXIS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * L1: Core Definitions – CIP Motion Axis States
 * Reference: ODVA CIP Motion, Rockwell MOTION-RM002
 *===========================================================================*/

/**
 * @brief CIP Motion axis operational states.
 *
 * These mirror the states defined in the CIP Motion specification
 * and correspond to the AXIS_CIP_DRIVE status word bits in Logix 5000.
 *
 * State diagram (simplified):
 *   DISABLED → STARTING → RUNNING ⇄ STOPPING
 *                    ↓            ↓
 *               ERROR_STOP ←────────
 *   Any state → AXIS_OFFLINE (communication loss)
 */
typedef enum {
    AXIS_STATE_DISABLED     = 0,  /**< Power structure disabled, no bus voltage output */
    AXIS_STATE_STARTING     = 1,  /**< Pre-charge, encoder initialization, commutation find */
    AXIS_STATE_RUNNING      = 2,  /**< Servo loop active, following command reference */
    AXIS_STATE_STOPPING     = 3,  /**< Decelerating to zero speed before disabling */
    AXIS_STATE_ERROR_STOP   = 4,  /**< Faulted stop – fault reaction active */
    AXIS_STATE_ABORTING     = 5,  /**< Fastest possible stop (MAS with immediate) */
    AXIS_STATE_HOMING       = 6,  /**< Executing homing sequence */
    AXIS_STATE_TESTING      = 7,  /**< Motor test / hookup diagnostics active */
    AXIS_STATE_AXIS_OFFLINE = 8   /**< Communication lost, drive unreachable */
} axis_state_t;

/**
 * @brief CIP Motion axis fault codes (subset of CIP Motion fault codes).
 */
typedef enum {
    AXIS_FAULT_NONE                 = 0x0000,
    AXIS_FAULT_MOTOR_OVERLOAD       = 0x0001,
    AXIS_FAULT_DRIVE_OVERTEMP       = 0x0002,
    AXIS_FAULT_ENCODER_LOSS         = 0x0004,
    AXIS_FAULT_FOLLOWING_ERROR      = 0x0008,
    AXIS_FAULT_OVERVOLTAGE          = 0x0010,
    AXIS_FAULT_UNDERVOLTAGE         = 0x0020,
    AXIS_FAULT_CURRENT_FOLDBACK     = 0x0040,
    AXIS_FAULT_SAFE_TORQUE_OFF      = 0x0080,
    AXIS_FAULT_COMMUTATION_LOSS     = 0x0100,
    AXIS_FAULT_SOFTWARE_OVERTRAVEL  = 0x0200,
    AXIS_FAULT_HARDWARE_OVERTRAVEL  = 0x0400,
    AXIS_FAULT_BUS_UNDERVOLTAGE     = 0x0800,
    AXIS_FAULT_GROUND_SHORT         = 0x1000,
    AXIS_FAULT_PHASE_LOSS           = 0x2000,
    AXIS_FAULT_DRIVE_OVERLOAD       = 0x4000
} axis_fault_code_t;

/*===========================================================================
 * L1: Core Definitions – Motion Instruction Types
 * Reference: Rockwell Automation MOTION-RM002, Chapter 1
 *===========================================================================*/

/**
 * @brief Logix 5000 motion instruction types.
 *
 * These correspond to the instructions available in RSLogix/Studio 5000
 * ladder logic for motion control.
 */
typedef enum {
    MOTION_INSTR_MAJ  = 0,  /**< Motion Axis Jog – jog axis at constant speed */
    MOTION_INSTR_MAM  = 1,  /**< Motion Axis Move – point-to-point absolute/relative move */
    MOTION_INSTR_MAS  = 2,  /**< Motion Axis Stop – controlled deceleration to stop */
    MOTION_INSTR_MAFR = 3,  /**< Motion Axis Fault Reset – clear axis faults */
    MOTION_INSTR_MAG  = 4,  /**< Motion Axis Gearing – electronic gearing (slave follows master) */
    MOTION_INSTR_MAPC = 5,  /**< Motion Axis Position Cam – electronic camming */
    MOTION_INSTR_MATC = 6,  /**< Motion Axis Time Cam – time-based cam profile */
    MOTION_INSTR_MAH  = 7,  /**< Motion Axis Home – execute homing sequence */
    MOTION_INSTR_MAW  = 8,  /**< Motion Axis Watch – registration position capture */
    MOTION_INSTR_MAHD = 9,  /**< Motion Axis Hookup Diagnostic – motor/feedback test */
    MOTION_INSTR_MRAT = 10, /**< Motion Redefine Axis Type – change axis configuration */
    MOTION_INSTR_MRHD = 11, /**< Motion Redefine Hookup Diagnostic */
    MOTION_INSTR_MSF  = 12, /**< Motion Servo Off – disable drive power structure */
    MOTION_INSTR_MSO  = 13, /**< Motion Servo On  – enable drive power structure */
    MOTION_INSTR_MAAT = 14, /**< Motion Apply Axis Tuning – download tuning parameters */
    MOTION_INSTR_MRAT_LOAD = 15, /**< Motion Redefine Axis Load – load observer config */
    MOTION_INSTR_MDR  = 16  /**< Motion Drive Reset – power cycle simulation */
} motion_instr_type_t;

/*===========================================================================
 * L1: Core Definitions – Motor Types */
/**
 * @brief Motor type classification for Kinetix drives.
 *
 * Kinetix 5100: Rotary induction and PM servo
 * Kinetix 5300: Rotary PM servo, catalog TLY/VPL/Bxxx
 * Kinetix 5500: Compact rotary servo
 * Kinetix 5700: High-power servo + linear motors (LDC/LDL series)
 * Kinetix 6000: Multi-axis servo (legacy)
 */
typedef enum {
    MOTOR_TYPE_ROTARY_PM        = 0,  /**< Rotary permanent-magnet synchronous motor (VPL, MPL, TLY) */
    MOTOR_TYPE_ROTARY_INDUCTION = 1,  /**< Rotary induction motor (open-loop V/Hz or vector) */
    MOTOR_TYPE_LINEAR_PM        = 2,  /**< Linear permanent-magnet motor (LDC, LDL series) */
    MOTOR_TYPE_DIRECT_DRIVE     = 3,  /**< Direct-drive torque motor (Cartridge DDR) */
    MOTOR_TYPE_VIRTUAL          = 4   /**< Virtual axis – no physical motor, timing master only */
} motor_type_t;

/**
 * @brief Feedback device types supported by Kinetix.
 */
typedef enum {
    FEEDBACK_TYPE_NONE           = 0,
    FEEDBACK_TYPE_INCREMENTAL    = 1,  /**< A/B/Z incremental encoder (TTL or RS-422) */
    FEEDBACK_TYPE_ABSOLUTE       = 2,  /**< Absolute encoder (EnDat, Hiperface, BiSS, SSI) */
    FEEDBACK_TYPE_RESOLVER       = 3,  /**< Resolver feedback */
    FEEDBACK_TYPE_HALL_ONLY      = 4,  /**< Hall-effect commutation only */
    FEEDBACK_TYPE_SINCOS         = 5   /**< Sin/Cos 1Vpp analog encoder */
} feedback_type_t;

/**
 * @brief Homing method types per CIP Motion / DS402.
 *
 * Subset of the 35 standard homing methods defined in IEC 61800-7-201 (CiA 402).
 */
typedef enum {
    HOME_METHOD_TORQUE_LIMIT_POS   = 1,  /**< Home to torque limit in positive direction */
    HOME_METHOD_TORQUE_LIMIT_NEG   = 2,  /**< Home to torque limit in negative direction */
    HOME_METHOD_SWITCH_POS         = 7,  /**< Home to positive limit switch + index pulse */
    HOME_METHOD_SWITCH_NEG         = 8,  /**< Home to negative limit switch + index pulse */
    HOME_METHOD_INDEX_POS          = 17, /**< Home to index pulse in positive direction */
    HOME_METHOD_INDEX_NEG          = 18, /**< Home to index pulse in negative direction */
    HOME_METHOD_ABSOLUTE           = 35, /**< Absolute encoder – no homing motion needed */
    HOME_METHOD_DIRECT             = 37  /**< Current position becomes home */
} home_method_t;

/**
 * @brief Position unit selection.
 */
typedef enum {
    POSITION_UNIT_COUNTS     = 0,  /**< Raw encoder counts */
    POSITION_UNIT_REVOLUTION = 1,  /**< Motor revolutions */
    POSITION_UNIT_DEGREE     = 2,  /**< Degrees */
    POSITION_UNIT_RADIAN     = 3,  /**< Radians */
    POSITION_UNIT_MM         = 4,  /**< Millimeters (linear) */
    POSITION_UNIT_INCH       = 5,  /**< Inches (linear) */
    POSITION_UNIT_MICRON     = 6   /**< Micrometers (linear, high-precision) */
} position_unit_t;

/**
 * @brief Move type for MAM instruction.
 */
typedef enum {
    MOVE_TYPE_ABSOLUTE = 0,  /**< Move to absolute position */
    MOVE_TYPE_RELATIVE = 1,  /**< Move relative to current command position */
    MOVE_TYPE_ADDITIVE = 2   /**< Add distance to pending target */
} move_type_t;

/*===========================================================================
 * L3: Engineering Structures – Axis Configuration
 *===========================================================================*/

/**
 * @brief Motor nameplate / catalog data structure.
 *
 * Used to store the essential motor parameters needed for field-oriented
 * control (FOC) and auto-tuning. Matches Rockwell Motor Database entries.
 */
typedef struct {
    char     catalog_number[32];    /**< Rockwell catalog number (e.g., "VPL-B1003F") */
    motor_type_t type;              /**< Motor type */
    double   rated_power_kw;        /**< Continuous rated power (kW) */
    double   rated_torque_nm;       /**< Continuous rated torque (N·m) */
    double   peak_torque_nm;        /**< Peak torque (3× overload, N·m) */
    double   rated_speed_rpm;       /**< Rated speed (RPM) */
    double   max_speed_rpm;         /**< Maximum mechanical speed (RPM) */
    double   rated_current_amps;    /**< Continuous rated current (A RMS) */
    double   peak_current_amps;     /**< Peak current (A RMS) */
    double   torque_constant_kt;    /**< Torque constant Kt (N·m/A RMS) */
    double   voltage_constant_kv;   /**< Back-EMF constant Kv (Vpeak/kRPM) */
    double   resistance_ohms;       /**< Phase-to-phase resistance (Ω) */
    double   inductance_mh;         /**< Phase-to-phase inductance (mH) */
    uint32_t pole_pairs;            /**< Pole pairs (P/2) */
    double   rotor_inertia_kgcm2;   /**< Rotor moment of inertia (kg·cm²) */
    double   thermal_time_const_s;  /**< Thermal time constant (seconds) */
    uint32_t encoder_counts_per_rev; /**< Encoder counts per revolution */
    feedback_type_t feedback;       /**< Feedback device type */
    double   max_winding_temp_c;    /**< Maximum winding temperature (°C) */
    bool     brake_fitted;          /**< True if holding brake is present */
} motor_database_t;

/**
 * @brief Load mechanical parameters identified during auto-tuning.
 */
typedef struct {
    double   load_inertia_ratio;    /**< J_load / J_motor (dimensionless) */
    double   load_inertia_kgcm2;    /**< Total reflected load inertia (kg·cm²) */
    double   viscous_friction;      /**< Viscous friction coefficient (N·m/kRPM) */
    double   coulomb_friction_nm;   /**< Coulomb (static) friction torque (N·m) */
    double   compliance_nm_per_rad; /**< Torsional stiffness (N·m/rad) */
    double   backlash_degrees;      /**< Mechanical backlash (degrees) */
    bool     load_coupled_directly; /**< True if direct drive (no gearbox) */
    double   gear_ratio;            /**< Gear ratio (motor:load, >1 = reduction) */
    double   gear_efficiency;       /**< Gearbox efficiency (0-1) */
} load_parameters_t;

/**
 * @brief Complete axis configuration structure.
 *
 * This struct contains all parameters needed to configure a Kinetix axis
 * in a Logix 5000 motion group. It mirrors the Axis Properties dialog
 * in Studio 5000.
 */
typedef struct {
    /* Axis identification */
    char     axis_name[64];          /**< ASCII axis tag name (e.g., "Axis_Conveyor1") */
    motor_database_t motor;          /**< Motor catalog data */
    uint16_t axis_instance;          /**< CIP Motion axis instance number */
    bool     is_virtual;             /**< True if virtual (no physical drive) */

    /* Feedback configuration */
    feedback_type_t feedback_type;   /**< Primary feedback device */
    uint32_t feedback_counts_per_rev; /**< Primary feedback resolution */
    bool     feedback_inverted;      /**< True if direction sense reversed */
    double   feedback_noise_filter;  /**< Feedback noise filter cutoff (Hz) */
    uint32_t feedback_loss_window_ms;/**< Encoder loss detection window (ms) */

    /* Unit scaling */
    position_unit_t position_unit;   /**< Position engineering unit */
    double   counts_per_unit;        /**< Encoder counts per engineering unit */
    double   position_unwind;        /**< Rotary unwind value (e.g., 360° for modulo) */
    bool     position_rotary;        /**< True for rotary (modulo) axes */
    double   velocity_units_per_sec; /**< Velocity scaling factor */
    double   accel_units_per_sec2;   /**< Acceleration scaling factor */

    /* Dynamic limits */
    double   max_velocity;           /**< Maximum velocity (eng units/sec) */
    double   max_acceleration;       /**< Maximum acceleration (eng units/sec²) */
    double   max_deceleration;       /**< Maximum deceleration (eng units/sec²) */
    double   max_jerk;               /**< Maximum jerk (eng units/sec³, 0 = jerk-limited disabled) */
    double   max_torque_pct;         /**< Torque limit (0-100% of peak) */
    double   max_current_pct;        /**< Current limit (0-100% of peak) */

    /* Soft travel limits */
    bool     soft_limits_enabled;    /**< Enable software position limits */
    double   soft_limit_positive;    /**< Maximum positive position */
    double   soft_limit_negative;    /**< Minimum negative position */

    /* Homing parameters */
    home_method_t home_method;       /**< Homing sequence method */
    double   home_speed_fast;        /**< Approach speed to home switch */
    double   home_speed_slow;        /**< Creep speed to index pulse */
    double   home_offset;            /**< Offset from home position to zero */
    home_method_t home_return_method;/**< Post-home move sequence */

    /* Stopping behavior */
    double   max_stop_deceleration;  /**< Deceleration rate for MAS (stop) */
    double   max_abort_deceleration; /**< Deceleration rate for abort (MAS immediate) */
    double   disable_delay_s;        /**< Delay before brake engage on disable (s) */
    bool     brake_engage_on_disable;/**< True to engage brake on servo off */

    /* Fault configuration */
    double   pos_error_tolerance;    /**< Following error tolerance (eng units) */
    double   pos_error_fault_action; /**< 0=Status Only, 1=Stop Planner, 2=Stop Drive, 3=Disable */
    double   velocity_error_window;  /**< Velocity error tolerance window */
    double   max_velocity_error_pct; /**< Max velocity error percentage */

    /* Load parameters (identified during tuning) */
    load_parameters_t load;

    /* Communication */
    uint32_t rpi_us;                 /**< Requested Packet Interval (μs) */
    uint16_t cip_connection_id;      /**< CIP Connection instance ID */
    bool     unicast_enabled;        /**< Unicast vs multicast cyclic data */

    /* Drive model */
    uint16_t drive_catalog;          /**< Kinetix drive catalog ID (see drive_catalog_t) */
    double   bus_voltage_dc;         /**< DC bus voltage (V) */
    double   pwm_frequency_khz;      /**< PWM switching frequency (kHz) */
    double   current_loop_bw_hz;     /**< Current loop bandwidth target (Hz) */

    /* Safety */
    bool     sto_active;             /**< Safe Torque Off input state */
    uint8_t  sil_level;              /**< SIL level (2 or 3) */
    bool     sbc_active;             /**< Safe Brake Control active */
} axis_config_t;

/**
 * @brief Runtime axis state structure – used for real-time motion control.
 *
 * This is the "live" data exchanged between the motion planner
 * and the servo loop at each CIP Sync cycle.
 */
typedef struct {
    /* Core state */
    axis_state_t state;              /**< Current CIP axis state */
    uint16_t    fault_code;          /**< Active fault bitmap (0 = no fault) */
    bool        servo_active;        /**< Servo loop on (MSO active) */
    bool        at_home;             /**< Homing complete flag */
    bool        home_calibrated;     /**< Home position calibrated to absolute reference */

    /* Command references (from motion planner) */
    double      cmd_position;        /**< Command position (eng units) */
    double      cmd_velocity;        /**< Command velocity (eng units/sec) */
    double      cmd_acceleration;    /**< Command acceleration (eng units/sec²) */
    double      cmd_torque_ff;       /**< Torque feedforward (N·m) */
    double      cmd_velocity_ff;     /**< Velocity feedforward (eng units/sec) */

    /* Actual feedback */
    double      actual_position;     /**< Actual position from feedback (eng units) */
    double      actual_velocity;     /**< Actual velocity (eng units/sec, filtered) */
    double      actual_acceleration; /**< Actual acceleration (eng units/sec², filtered) */
    double      actual_current;      /**< Actual motor current (A RMS) */
    double      actual_torque;       /**< Actual motor torque (N·m) */
    double      actual_bus_voltage;  /**< DC bus voltage (V) */
    double      actual_drive_temp_c; /**< Drive heatsink temperature (°C) */
    double      actual_motor_temp_c; /**< Motor winding temperature (°C, estimated) */

    /* Tracking errors */
    double      position_error;      /**< Following error = cmd - actual (eng units) */
    double      velocity_error;      /**< Velocity error (eng units/sec) */
    double      position_error_integral; /**< Integrated position error for stationary detect */

    /* Motion status */
    double      dist_to_go;          /**< Distance remaining in active move */
    bool        move_complete;       /**< Active move complete (PC bit) */
    bool        at_velocity;         /**< At commanded velocity (in cruise phase) */
    bool        accelerating;        /**< In acceleration phase */
    bool        decelerating;        /**< In deceleration phase */
    bool        constant_velocity;   /**< In constant velocity phase */
    bool        move_pending;        /**< Another move in buffer (IP bit) */
    int32_t     active_motion_instr; /**< Currently executing motion instruction type */

    /* Registration */
    bool        registration_armed;  /**< Registration armed */
    bool        registration_latched;/**< Registration position captured */
    double      registration_position;/**< Latched registration position */
    uint8_t     registration_input;  /**< Registration input number (1 or 2) */

    /* Gearing / Camming */
    double      master_position;     /**< Master axis position (for geared/cammed axes) */
    double      gear_ratio_active;   /**< Current electronic gear ratio */
    bool        clutch_active;       /**< Clutch engaged (MAG with clutch) */
    bool        cam_active;          /**< Cam profile active (MAPC) */
    double      cam_progress_pct;    /**< Cam progress (0-100%) */

    /* Timing */
    uint64_t    cycle_count;         /**< Servo cycle counter */
    double      cycle_time_us;       /**< Actual servo cycle time (μs) */
    uint64_t    timestamp_last_updated; /**< Last update timestamp (CIP Sync time, ns) */

    /* Active instruction details */
    motion_instr_type_t instr_type;  /**< Active motion instruction type */
    double      instr_target_pos;    /**< Target position (for MAM) */
    double      instr_speed;         /**< Commanded speed (for MAM/MAJ) */
    double      instr_accel;         /**< Commanded accel (for MAM) */
    double      instr_decel;         /**< Commanded decel (for MAM) */
    double      instr_jerk;          /**< Commanded jerk (for MAM with S-curve) */

    /* Drive internal */
    double      iq_ref;              /**< q-axis current reference (A peak) */
    double      id_ref;              /**< d-axis current reference (A peak) */
    double      iq_actual;           /**< q-axis actual current (A peak) */
    double      id_actual;           /**< d-axis actual current (A peak) */
    double      vq_ref;              /**< q-axis voltage reference (V peak) */
    double      vd_ref;              /**< d-axis voltage reference (V peak) */
    double      electrical_angle_rad;/**< Rotor electrical angle (rad) */
    double      electrical_vel_rps;  /**< Rotor electrical velocity (rad/s) */
} axis_runtime_t;

/*===========================================================================
 * L3: Engineering Structures – Kinetix Drive Models
 *===========================================================================*/

/**
 * @brief Kinetix drive family catalog IDs.
 */
typedef enum {
    KINETIX_350     = 350,   /**< Kinetix 350 – single-axis, basic indexing */
    KINETIX_300     = 300,   /**< Kinetix 300 – indexing drive (legacy) */
    KINETIX_5100    = 5100,  /**< Kinetix 5100 – single axis servo, EtherNet/IP */
    KINETIX_5300    = 5300,  /**< Kinetix 5300 – single-axis CIP Motion servo */
    KINETIX_5500    = 5500,  /**< Kinetix 5500 – compact single-axis */
    KINETIX_5700    = 5700,  /**< Kinetix 5700 – dual-axis module, high-power */
    KINETIX_6000    = 6000,  /**< Kinetix 6000 – multi-axis (legacy SERCOS) */
    KINETIX_6200    = 6200,  /**< Kinetix 6200 – multi-axis safety (legacy) */
    KINETIX_6500    = 6500,  /**< Kinetix 6500 – multi-axis EtherNet/IP (legacy) */
    KINETIX_7000    = 7000   /**< Kinetix 7000 – high-power (legacy) */
} kinetix_drive_model_t;

/**
 * @brief Drive power rating structure.
 */
typedef struct {
    uint16_t input_voltage_class;    /**< 0=120VAC, 1=240VAC, 2=480VAC, 3=325VDC, 4=650VDC */
    double   input_current_rated;    /**< Continuous input current (A RMS) */
    double   output_current_cont;    /**< Continuous output current (A RMS) */
    double   output_current_peak;    /**< Peak output current (A RMS) */
    double   output_power_kw;        /**< Continuous output power (kW) */
    double   dc_bus_capacitance_uf;  /**< DC bus capacitance (μF) */
    double   shunt_power_w;          /**< Internal shunt resistor power (W) */
    double   shunt_resistance_ohm;   /**< Shunt resistor value (Ω) */
    double   shunt_turn_on_v;        /**< Shunt turn-on voltage (V DC) */
    double   shunt_turn_off_v;       /**< Shunt turn-off voltage (V DC) */
} drive_power_rating_t;

/**
 * @brief Homing sequence phase for internal tracking.
 */
typedef enum {
    HOME_PHASE_IDLE           = 0,
    HOME_PHASE_FAST_APPROACH  = 1,
    HOME_PHASE_SWITCH_DETECT  = 2,
    HOME_PHASE_CREEP_TO_INDEX = 3,
    HOME_PHASE_INDEX_FOUND    = 4,
    HOME_PHASE_OFFSET_MOVE    = 5,
    HOME_PHASE_COMPLETE       = 6,
    HOME_PHASE_FAILED         = 7
} home_phase_t;

/*===========================================================================
 * Motor Database Function Declarations
 *===========================================================================*/

/**
 * @brief Initialize a motor database entry with safe defaults.
 */
void motor_database_init_default(motor_database_t *motor);

/**
 * @brief Initialize VPL-B1003F catalog data (Kinetix VP Low Inertia).
 */
void motor_database_init_vpl_b1003f(motor_database_t *motor);

/**
 * @brief Initialize VPL-B1653F catalog data (mid-inertia, 165mm frame).
 */
void motor_database_init_vpl_b1653f(motor_database_t *motor);

/**
 * @brief Initialize a linear motor (LDL series) database entry.
 */
void motor_database_init_linear(motor_database_t *motor,
                                 double force_cont, double force_peak,
                                 double mass_kg, double pole_pitch_mm);

/*===========================================================================
 * Axis State Machine Function Declarations
 *===========================================================================*/

bool axis_state_transition_valid(axis_state_t current, axis_state_t requested);
axis_state_t axis_state_transition(axis_runtime_t *axis, axis_state_t requested);
axis_state_t axis_set_fault(axis_runtime_t *axis, uint16_t fault_code);
int axis_clear_faults(axis_runtime_t *axis);
bool axis_check_following_error(axis_runtime_t *axis, double tolerance);
bool axis_check_soft_overtravel(axis_runtime_t *axis, double pos_max, double pos_min);
double commutation_compute_electrical_angle(double encoder_angle_rad,
                                             uint32_t pole_pairs,
                                             double commutation_offset);
void sto_state_machine(bool sto_channel1, bool sto_channel2,
                       bool reset_needed, bool *sto_active, bool *fault);
void axis_config_init(axis_config_t *config);
void axis_runtime_init(axis_runtime_t *axis, const axis_config_t *config);
const char *axis_state_to_string(axis_state_t state);

/* Homing sequence */
int homing_init(home_method_t method, double fast_speed,
                double slow_speed, double offset);
home_phase_t homing_step(double current_position, bool switch_active,
                         bool index_pulse, double *cmd_velocity);
bool homing_is_complete(void);
void homing_abort(void);

/*===========================================================================
 * Kinetix Drive Function Declarations
 *===========================================================================*/

int kinetix_drive_lookup(uint16_t catalog_id, drive_power_rating_t *rating);
int kinetix_compute_regenerative_energy(double bus_voltage,
                                         double bus_capacitance,
                                         double inertia, double initial_velocity,
                                         double *final_voltage, double *shunt_energy);
bool kinetix_check_drive_motor_match(const drive_power_rating_t *rating,
                                      const motor_database_t *motor,
                                      double duty_cycle);
double kinetix_thermal_model_update(double current_amps, double speed_rps,
                                      const motor_database_t *motor,
                                      double ambient_temp_c,
                                      double prev_temp_c, double Ts);
double kinetix_compute_overload_time(double current_amps,
                                       const motor_database_t *motor);
double kinetix_capacitor_life_estimate(double rated_life_h,
                                         double rated_temp_c, double actual_temp_c,
                                         double rated_voltage, double actual_voltage);
bool kinetix_sto_integrity_check(bool sto1_active, bool sto2_active,
                                   bool gate_drive_disabled);
double kinetix_sto_pfhd_compute(double lambda_d1, double lambda_d2,
                                  double diag_interval);
double kinetix_sto_sff_compute(double lambda_s, double lambda_dd,
                                 double lambda_du);
int kinetix_recommend_drive(const motor_database_t *motor,
                              double load_inertia_ratio,
                              uint16_t *drive_catalog);
const char *kinetix_get_catalog_number(uint16_t catalog_id);

/*===========================================================================
 * Advanced Motion Function Declarations
 *===========================================================================*/

typedef struct {
    double   settling_time_s;
    double   overshoot_pct;
    double   iae;
    double   peak_position_error;
    double   phase_margin_deg;
    bool     unstable;
} mc_result_t;

typedef struct {
    double   st_mean, st_std;
    double   os_mean, os_std;
    double   iae_mean, iae_std;
    double   instability_pct;
    double   worst_st;
    double   best_st;
    int      total_trials;
} mc_summary_t;

void monte_carlo_robustness(int n_trials, double nominal_inertia,
                              double nominal_resistance,
                              double kp_vel, double ki_vel,
                              double inertia_std_pct, uint64_t seed,
                              mc_summary_t *summary);

#endif /* AXIS_TYPES_H */
