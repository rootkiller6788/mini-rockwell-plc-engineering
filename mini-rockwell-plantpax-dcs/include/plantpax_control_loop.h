/**
 * @file plantpax_control_loop.h
 * @brief PlantPAx Control Loop ? PID, Cascade, Ratio, Feedforward, Override Selectors
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L2 Control Concepts, L3 PID Structure, L5 Algorithms, L6 Problems
 *
 * PlantPAx Process Objects Library provides pre-built control strategies
 * implemented as Add-On Instructions (AOIs) in Studio 5000:
 *
 *   - P_PIDE: Enhanced PID with auto/manual, tracking, anti-windup
 *   - P_ValveSO: Solenoid-operated valve control
 *   - P_ValveMO: Motor-operated valve control
 *   - P_Motor: Motor control with start/stop and feedback
 *   - P_Motor2Spd: Two-speed motor
 *   - P_AIn: Analog input with alarming and scaling
 *   - P_AOut: Analog output with fault handling
 *   - P_Intlk: Interlock logic
 *   - P_Perm: Permissive logic
 *
 * PID Equation (ISA Standard Form, Dependent Gains):
 *   CO = Kc * [ E(t) + (1/Ti)*integral(E dt) + Td*dE/dt ]
 *
 * PID Equation (Independent Gains, ISA Alternative):
 *   CO = Kp * E(t) + Ki * integral(E dt) + Kd * dE/dt
 *
 * Velocity Form (used for bumpless transfer):
 *   delta_CO = Kc * [ delta_E + (Ts/Ti)*E + (Td/Ts)*(delta_E - delta_E_prev) ]
 *
 * Reference:
 *   Rockwell PlantPAx Process Objects Reference Manual (PROCES-RM013)
 *   Astrom & Hagglund, PID Controllers: Theory, Design, and Tuning
 *   ISA-5.1 Instrumentation Symbols and Identification
 * Curriculum: MIT 6.302, Stanford ENGR205, Berkeley ME233, Purdue ME 575
 */

#ifndef PLANTPAX_CONTROL_LOOP_H
#define PLANTPAX_CONTROL_LOOP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: Control Loop Core Definitions
 * ------------------------------------------------------------------------- */

/** PID equation form */
typedef enum {
    PAX_PID_FORM_DEPENDENT = 0,      /**< ISA standard: Kc, Ti, Td */
    PAX_PID_FORM_INDEPENDENT = 1,    /**< Independent: Kp, Ki, Kd */
    PAX_PID_FORM_VELOCITY = 2        /**< Velocity/incremental form */
} pax_pid_form_t;

/** Controller operating modes */
typedef enum {
    PAX_LOOP_MODE_MANUAL = 0,        /**< Operator-set output */
    PAX_LOOP_MODE_AUTO = 1,          /**< Automatic PID control */
    PAX_LOOP_MODE_CASCADE = 2,       /**< Cascade (remote SP from primary) */
    PAX_LOOP_MODE_REMOTE_CASCADE = 3,/**< Remote cascade via network */
    PAX_LOOP_MODE_RATIO = 4,         /**< Ratio control */
    PAX_LOOP_MODE_OVERRIDE = 5,      /**< Override/selector control */
    PAX_LOOP_MODE_TRACK = 6,         /**< Output tracking (initialization) */
    PAX_LOOP_MODE_HAND = 7,          /**< Hand (field) control */
    PAX_LOOP_MODE_INHIBIT = 8        /**< Control inhibited */
} pax_loop_mode_t;

/** Control action direction */
typedef enum {
    PAX_ACTION_DIRECT = 0,           /**< PV increase -> CO increase */
    PAX_ACTION_REVERSE = 1           /**< PV increase -> CO decrease */
} pax_control_action_t;

/** PID anti-windup strategy */
typedef enum {
    PAX_AW_NONE = 0,                 /**< No anti-windup */
    PAX_AW_CLAMPING = 1,             /**< Integrator clamping at limits */
    PAX_AW_BACK_CALCULATION = 2,     /**< Back-calculation (ISA standard) */
    PAX_AW_CONDITIONAL_INTEGRATION = 3 /**< Freeze integral when saturated */
} pax_anti_windup_t;

/** Control loop types */
typedef enum {
    PAX_LOOP_TYPE_SIMPLE_PID = 0,    /**< Single PID loop */
    PAX_LOOP_TYPE_CASCADE = 1,       /**< Primary-secondary cascade */
    PAX_LOOP_TYPE_RATIO = 2,         /**< Ratio control */
    PAX_LOOP_TYPE_FEEDFORWARD = 3,   /**< PID + feedforward */
    PAX_LOOP_TYPE_SPLIT_RANGE = 4,   /**< Single output to two valves */
    PAX_LOOP_TYPE_OVERRIDE = 5,      /**< Override/selector */
    PAX_LOOP_TYPE_DUAL_PID = 6       /**< Two independent PIDs sharing PV */
} pax_loop_type_t;

/** Feedforward model types */
typedef enum {
    PAX_FF_NONE = 0,
    PAX_FF_STATIC_GAIN = 1,          /**< Simple gain compensation */
    PAX_FF_LEAD_LAG = 2,             /**< Lead-lag dynamic compensation */
    PAX_FF_DEADTIME = 3,             /**< Deadtime compensation */
    PAX_FF_FIRST_ORDER = 4           /**< First-order dynamic model */
} pax_feedforward_type_t;

/* ---------------------------------------------------------------------------
 * L1: PID Control Data Structures
 * ------------------------------------------------------------------------- */

/** PID tuning parameters */
typedef struct {
    pax_pid_form_t form;
    double kc;                       /**< Proportional gain (dependent form) */
    double ti_sec;                   /**< Integral time in seconds */
    double td_sec;                   /**< Derivative time in seconds */
    double kp;                       /**< Proportional gain (independent) */
    double ki;                       /**< Integral gain (independent) */
    double kd;                       /**< Derivative gain (independent) */
    double sample_time_sec;
    pax_anti_windup_t anti_windup;
    double derivative_filter_sec;    /**< Derivative low-pass filter TC */
    bool derivative_on_error;        /**< TRUE = dE/dt, FALSE = -dPV/dt */
} pax_pid_params_t;

/** PID loop runtime state */
typedef struct {
    uint32_t loop_id;
    char loop_tag[64];
    pax_loop_type_t loop_type;
    pax_loop_mode_t mode;
    pax_control_action_t action;
    pax_pid_params_t params;
    double setpoint;
    double process_variable;
    double output;                   /**< Controller output (CO) */
    double error;                    /**< SP - PV (or PV - SP for direct) */
    double integral_term;
    double derivative_term;
    double proportional_term;
    double feedback_value;           /**< For back-calculation AW */
    double prev_error;
    double prev_pv;
    double prev_output;
    double output_min;               /**< Output low clamp */
    double output_max;               /**< Output high clamp */
    double setpoint_min;
    double setpoint_max;
    double setpoint_rate_limit;      /**< SP ramp rate in EU/sec */
    double pv_scale_min;
    double pv_scale_max;
    bool output_clamped_high;
    bool output_clamped_low;
    bool windup_active;
    double ff_value;                 /**< Feedforward contribution */
    double ff_gain;                  /**< Feedforward scaling */
    double tracking_value;           /**< Tracking output value */
    pax_loop_mode_t prev_mode;
    uint64_t cycle_count;
} pax_pid_loop_t;

/** Cascade control configuration */
typedef struct {
    pax_pid_loop_t *primary;         /**< Outer/primary loop */
    pax_pid_loop_t *secondary;       /**< Inner/secondary loop */
    bool cascade_enabled;
    double primary_sp_min;
    double primary_sp_max;
    double secondary_sp_ratio;       /**< Scaling: primary CO -> secondary SP */
    double secondary_sp_bias;        /**< Offset: primary CO -> secondary SP */
} pax_cascade_t;

/** Ratio control configuration */
typedef struct {
    pax_pid_loop_t *controlled;      /**< Controlled flow loop */
    double wild_variable;            /**< The uncontrolled (wild) flow */
    double ratio;                    /**< Desired ratio: controlled/wild */
    double ratio_min;
    double ratio_max;
    double bias;                     /**< Offset bias */
} pax_ratio_control_t;

/** Override selector configuration */
typedef struct {
    pax_pid_loop_t *loops[4];        /**< Up to 4 competing loops */
    uint32_t num_loops;
    uint32_t selected_loop;          /**< Currently selected (0-indexed) */
    double output_min;
    double output_max;
} pax_override_t;

/* ---------------------------------------------------------------------------
 * L1: Control Loop Constants
 * ------------------------------------------------------------------------- */

#define PAX_PID_MIN_SAMPLE_TIME_SEC    0.001
#define PAX_PID_MAX_KC                  1000.0
#define PAX_PID_MIN_KC                  0.0
#define PAX_PID_MAX_TI_SEC             10000.0
#define PAX_PID_MIN_TI_SEC             0.001
#define PAX_PID_MAX_TD_SEC             1000.0
#define PAX_PID_MIN_TD_SEC             0.0
#define PAX_PID_DEFAULT_DERIV_FILTER   0.1
#define PAX_PID_DEFAULT_SAMPLE_TIME    0.1
#define PAX_LOOP_MAX_OUTPUT            100.0
#define PAX_LOOP_MIN_OUTPUT            0.0

/* ---------------------------------------------------------------------------
 * L2/L5/L6: Control Loop API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize PID parameters with ISA standard (dependent) form.
 *
 * Sets up a PID controller with Kc, Ti, Td using the
 * ISA standard form. Default anti-windup: back-calculation.
 */
void pax_pid_params_init(pax_pid_params_t *params, pax_pid_form_t form,
                          double kc, double ti_sec, double td_sec,
                          double sample_time_sec);

/**
 * @brief Initialize a PID loop for execution.
 *
 * Sets up the loop with a tag, type, action, and limits.
 */
void pax_pid_loop_init(pax_pid_loop_t *loop, uint32_t id,
                        const char *tag, pax_loop_type_t type,
                        pax_control_action_t action,
                        double out_min, double out_max);

/**
 * @brief Execute one iteration of the PID algorithm.
 *
 * Implements the velocity form of the PID controller:
 *   delta_CO = Kc * [ delta_E + (Ts/Ti)*E + (Td/Ts)*(delta_E - delta_E_prev) ]
 *
 * This form is inherently bumpless and anti-windup friendly.
 * Handles mode transitions (manual->auto bumpless).
 *
 * @param loop The PID loop to execute
 * @param pv Current process variable
 * @param sp Current setpoint
 * @param dt_sec Time since last execution
 * @return Updated controller output
 */
double pax_pid_execute(pax_pid_loop_t *loop, double pv, double sp, double dt_sec);

/**
 * @brief Convert dependent PID gains to independent form.
 *
 * Kp = Kc
 * Ki = Kc / Ti
 * Kd = Kc * Td
 */
void pax_pid_convert_dependent_to_independent(const pax_pid_params_t *dep,
                                               pax_pid_params_t *indep);

/**
 * @brief Execute a cascade control loop pair.
 *
 * Primary loop computes setpoint for secondary loop.
 * Secondary loop executes with faster sample time.
 *
 * Handles bumpless cascade initialization:
 *   - When cascade enabled: primary initializes to secondary SP
 *   - When cascade disabled: secondary tracks primary CO
 */
int pax_cascade_execute(pax_cascade_t *cascade,
                         double primary_pv, double secondary_pv,
                         double primary_dt, double secondary_dt);

/**
 * @brief Execute ratio control.
 *
 * Controlled flow SP = wild_variable * ratio + bias
 * The ratio loop maintains a fixed proportion between two streams.
 */
double pax_ratio_execute(pax_ratio_control_t *ratio, double dt_sec);

/**
 * @brief Execute override/selector control.
 *
 * Among N competing loops, selects the output that is:
 *   - Minimum (for low-select, e.g., furnace fuel gas)
 *   - Maximum (for high-select, e.g., compressor anti-surge)
 *
 * Non-selected loops track the selected output (anti-windup).
 */
int pax_override_execute(pax_override_t *override,
                          const double *pvs, const double *sps,
                          double dt_sec, bool low_select,
                          double *output, uint32_t *selected);

/**
 * @brief Compute feedforward contribution.
 *
 * FF_output = FF_gain * FF_signal (static)
 * For lead-lag: FF(z) = (1 + T_lead/Ts * (1 - z^-1)) / (1 + T_lag/Ts * (1 - z^-1))
 */
double pax_feedforward_compute(double ff_signal, double ff_gain,
                                pax_feedforward_type_t ff_type,
                                double t_lead_sec, double t_lag_sec,
                                double sample_time_sec,
                                double *prev_ff_input, double *prev_ff_output);

/**
 * @brief Apply setpoint rate limiting.
 *
 * Limits how fast the setpoint can change to prevent
 * process upsets from sudden SP steps.
 */
double pax_setpoint_rate_limit(double target_sp, double current_sp,
                                double rate_limit_eu_s, double dt_sec);

/**
 * @brief Check if loop is in windup condition.
 *
 * Windup occurs when:
 *   - Controller output is saturated, AND
 *   - Integral term is pushing further into saturation
 */
bool pax_pid_check_windup(const pax_pid_loop_t *loop);

/**
 * @brief Perform bumpless transfer initialization.
 *
 * When switching from Manual to Auto:
 *   - Set integral term so current output is maintained (no bump)
 *   - I_term = CO_current - P_term - D_term
 */
void pax_pid_bumpless_transfer(pax_pid_loop_t *loop);

/**
 * @brief Tune PID using Ziegler-Nichols open-loop method.
 *
 * From step response (process reaction curve):
 *   Kc = 1.2 * T / (K * L)   for P-only
 *   Kc = 0.9 * T / (K * L), Ti = 3.33 * L   for PI
 *   Kc = 1.2 * T / (K * L), Ti = 2.0 * L, Td = 0.5 * L   for PID
 *
 * @param process_gain Process gain K
 * @param time_constant Process time constant T
 * @param deadtime Process deadtime L
 * @param output Tuning parameters (output)
 */
void pax_pid_ziegler_nichols_open_loop(double process_gain,
                                        double time_constant,
                                        double deadtime,
                                        pax_pid_params_t *output);

/**
 * @brief Compute loop performance metrics.
 *
 * Calculates IAE (Integral Absolute Error), ISE (Integral Squared Error),
 * and ITAE (Integral Time-weighted Absolute Error).
 */
void pax_pid_performance_metrics(const pax_pid_loop_t *loop,
                                  double *iae, double *ise, double *itae);

/**
 * @brief Dump loop status to stdout.
 */
void pax_loop_dump(const pax_pid_loop_t *loop);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_CONTROL_LOOP_H */
