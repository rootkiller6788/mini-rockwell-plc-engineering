/**
 * @file    logix_pide_instruction.h
 * @brief   L5-L6: ControlLogix Enhanced PID (PIDE) Instruction
 *
 * Knowledge Mapping:
 *   L5 Algorithms:    PIDE algorithm with velocity/positional forms,
 *                     auto/manual/cascade modes, bumpless transfer,
 *                     anti-reset windup (clamping/back-calculation),
 *                     PV tracking, ratio/bias feedforward
 *   L6 Problems:      PID loop design for process control, cascade
 *                     control inner/outer loop, override selection
 *   L4 Standards:     ISA-5.1 instrumentation symbology
 *   L7 Applications:  Rockwell ControlLogix PIDE function block
 *
 * The PIDE instruction is the enhanced PID implementation in Logix 5000.
 * It differs from the basic PID instruction in:
 *   - Velocity form PID (incremental output) as default
 *   - Built-in PV filtering (first-order lag)
 *   - Independent gain form: CV = Kp*E + Ki*integral(E) + Kd*derivative(E)
 *   - Auto/manual/cascade/override operating modes
 *   - Ratio/bias on SP and FFWD inputs
 *   - Programmable output limits and rate-of-change limits
 *   - Anti-reset windup via clamping or back-calculation
 *   - Status word for diagnostics (16-bit)
 *
 * Reference: Rockwell Automation Publication 1756-RM006
 *   "Logix5000 Controllers Process Control and Drives Instructions"
 *
 * Course Alignment:
 *   MIT 6.302 - PID implementation and discretization
 *   Stanford ENGR205 - Process control algorithms
 *   Purdue ME 575 - Industrial PID tuning
 */

#ifndef LOGIX_PIDE_INSTRUCTION_H
#define LOGIX_PIDE_INSTRUCTION_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: PIDE Operating Modes
 * ======================================================================== */

typedef enum {
    PIDE_MODE_AUTO      = 0,  /* Normal PID control, SP from operator/program */
    PIDE_MODE_MANUAL    = 1,  /* Operator sets CV directly, PID tracking SP */
    PIDE_MODE_CASCADE   = 2,  /* SP comes from cascade master (outer loop) */
    PIDE_MODE_OVERRIDE  = 3,  /* Override selector controls output */
    PIDE_MODE_HAND      = 4,  /* Hand (manual) station, output from field */
    PIDE_MODE_PROGRAM   = 5   /* Program sets CV directly (like Manual) */
} pide_mode_t;

typedef enum {
    PIDE_FORM_VELOCITY    = 0,  /* Incremental (velocity) form */
    PIDE_FORM_POSITIONAL  = 1   /* Absolute (positional) form */
} pide_form_t;

typedef enum {
    PIDE_GAIN_INDEPENDENT = 0,  /* Kp, Ki, Kd (ISA standard form) */
    PIDE_GAIN_DEPENDENT   = 1   /* Kc, Ti, Td (classical form) */
} pide_gain_form_t;

typedef enum {
    PIDE_ANTIWINDUP_NONE        = 0,
    PIDE_ANTIWINDUP_CLAMPING    = 1,
    PIDE_ANTIWINDUP_BACKCALC    = 2
} pide_antiwindup_t;

/* ========================================================================
 * L5: PIDE Parameter Structure
 * ======================================================================== */

/**
 * @brief PIDE instruction data structure
 *
 * The PIDE instruction implements the ISA standard form of the PID algorithm
 * in incremental (velocity) form as its primary mode.
 *
 * Velocity Form (discrete-time):
 *   Delta_CV(n) = Kp * [E(n) - E(n-1)]  (proportional on error change)
 *               + Ki * Ts * E(n)         (integral term)
 *               + Kd/Ts * [PV(n) - 2*PV(n-1) + PV(n-1)]  (derivative on PV)
 *
 *   CV(n) = CV(n-1) + Delta_CV(n)
 *
 * Positional Form (discrete-time):
 *   CV(n) = Kp * E(n)
 *         + Ki * Ts * SUM(E(i), i=0..n)
 *         + Kd/Ts * [PV(n) - PV(n-1)]
 *
 * E(n) = SP(n) - PV(n)   (error = setpoint - process variable)
 *
 * Derivative on PV (not error) eliminates derivative kick on SP changes.
 * Proportional on error (not PV) allows faster response to setpoint changes.
 *
 * The PIDE also supports:
 *   - SP ratio: SP_eff = SP * SP_Ratio + SP_Bias
 *   - FFWD ratio: FFWD_eff = FFWD * FFWD_Ratio + FFWD_Bias
 *   - PV tracking: when in Manual, SP tracks PV for bumpless transfer
 *   - Output rate-of-change limiting: |Delta_CV| <= CV_ROC_Limit / Ts
 *
 * Reference: 1756-RM006, Chapter 1 "PIDE Instruction"
 *            Astrom & Hagglund (1995), PID Controllers, Ch. 3
 */
typedef struct {
    /* Primary control parameters */
    double      kp;             /* Proportional gain (dimensionless) */
    double      ki;             /* Integral gain (1/sec) */
    double      kd;             /* Derivative gain (sec) */
    double      ts_sec;         /* Sample time (seconds) */

    /* Setpoint and feedforward */
    double      sp;             /* Setpoint (engineering units) */
    double      sp_ratio;       /* Setpoint ratio multiplier */
    double      sp_bias;        /* Setpoint bias offset */
    double      pv;             /* Process variable (engineering units) */
    double      pv_eu_min;      /* PV engineering units minimum */
    double      pv_eu_max;      /* PV engineering units maximum */
    double      ffwd;           /* Feedforward input */
    double      ffwd_ratio;     /* Feedforward ratio multiplier */
    double      ffwd_bias;      /* Feedforward bias offset */

    /* Output */
    double      cv;             /* Control variable output (%) */
    double      cv_min;         /* Output lower limit (%) */
    double      cv_max;         /* Output upper limit (%) */
    double      cv_roc_limit;   /* Max rate of change (%/sec, 0=disabled) */

    /* Operating mode */
    pide_mode_t mode;           /* Current operating mode */
    pide_form_t form;           /* Velocity or positional */
    pide_gain_form_t gain_form;  /* Independent or dependent */
    double      mode_cv_manual; /* Manual mode output value (%) */

    /* Anti-reset windup */
    pide_antiwindup_t antiwindup_method;
    double      backcalc_gain;  /* Back-calculation gain (typically 1.0) */

    /* PV Filtering */
    bool        pv_filter_enabled;
    double      pv_filter_tau_sec;  /* First-order lag time constant */
    double      pv_filtered;        /* Filtered PV value */

    /* Internal state (velocity form) */
    double      error_prev;     /* E(n-1) */
    double      pv_prev;        /* PV(n-1) */
    double      pv_prev2;       /* PV(n-2) for second-order filtering */
    double      integral_sum;   /* Accumulated integral term */
    double      cv_prev;        /* CV(n-1) */

    /* Cascaded input (when in Cascade mode) */
    double      cascade_sp;     /* SP from outer loop master */

    /* Diagnostics */
    bool        cv_high_limit_active;
    bool        cv_low_limit_active;
    bool        cv_roc_limit_active;
    bool        pv_bad;         /* PV quality bad (from transducer fault) */
    bool        windup_occurred;
    bool        initialized;
    double      error;          /* Current error: SP - PV */
    double      p_term;         /* Proportional contribution */
    double      i_term;         /* Integral contribution */
    double      d_term;         /* Derivative contribution */
    double      ffwd_term;      /* Feedforward contribution */

    /* Status word bits (16-bit, matching Logix architecture) */
    uint16_t    status_word;
} logix_pide_t;

/* PIDE Status Word Bit Definitions (matching Logix 5000) */
#define PIDE_STATUS_ENABLED       0x0001
#define PIDE_STATUS_CASCADE_MODE  0x0002
#define PIDE_STATUS_MANUAL_MODE   0x0004
#define PIDE_STATUS_OVERRIDE_MODE 0x0008
#define PIDE_STATUS_HAND_MODE     0x0010
#define PIDE_STATUS_PV_HI_LIMIT   0x0020
#define PIDE_STATUS_PV_LO_LIMIT   0x0040
#define PIDE_STATUS_CV_HI_LIMIT   0x0080
#define PIDE_STATUS_CV_LO_LIMIT   0x0100
#define PIDE_STATUS_ROC_LIMIT     0x0200
#define PIDE_STATUS_WINDUP        0x0400
#define PIDE_STATUS_PV_BAD        0x0800
#define PIDE_STATUS_INITIALIZED   0x1000

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief Initialize PIDE instruction with default parameters
 *
 * Defaults:
 *   - Independent gain form (Kp=1.0, Ki=0.0, Kd=0.0)
 *   - Velocity form
 *   - Manual mode (safe startup)
 *   - Anti-windup via clamping
 *   - Sample time 0.1 sec
 *   - Output limits 0-100%
 *   - All internal states zeroed
 *
 * Complexity: O(1)
 * Reference: 1756-RM006, "PIDE Instruction Parameters"
 */
void logix_pide_init(logix_pide_t *pide, double ts_sec);

/**
 * @brief Execute one iteration of the PIDE algorithm
 *
 * Performs:
 *   1. Apply PV filtering (first-order lag if enabled)
 *   2. Calculate error: E = SP_eff - PV_filt
 *   3. Compute PID terms (velocity or positional form)
 *   4. Apply output limits and ROC limiting
 *   5. Apply anti-reset windup
 *   6. Update internal state (E(n-1), PV(n-1), CV(n-1))
 *   7. Update status word
 *
 * @return Control variable output (%)
 *
 * Complexity: O(1)
 * Reference: 1756-RM006, "PIDE Algorithm"
 *            Astrom & Hagglund (1995), Ch. 3 "PID Control Algorithm"
 */
double logix_pide_execute(logix_pide_t *pide);

/**
 * @brief Set PIDE operating mode with bumpless transfer
 *
 * Bumpless transfer rules:
 *   - Manual -> Auto: Initialize integral sum to current CV minus P term;
 *     this prevents output bump on mode transition
 *   - Auto -> Manual: SP tracks PV (if PV tracking enabled)
 *   - Cascade -> Auto: SP held at last cascade SP
 *   - Any -> Manual: CV stays at last value
 *
 * Complexity: O(1)
 * Reference: 1756-RM006, "PIDE Mode Transitions"
 *            Astrom & Hagglund (1995), Ch. 5 "Bumpless Transfer"
 */
void logix_pide_set_mode(logix_pide_t *pide, pide_mode_t new_mode);

/**
 * @brief Convert between independent and dependent gain forms
 *
 * Conversion formulas:
 *   Kp (independent) = Kc (dependent)
 *   Ki (independent) = Kc / Ti     where Ti = integral time (sec)
 *   Kd (independent) = Kc * Td     where Td = derivative time (sec)
 *
 * Inverse:
 *   Kc = Kp
 *   Ti = Kc / Ki   (if Ki != 0)
 *   Td = Kd / Kc   (if Kc != 0)
 *
 * Complexity: O(1)
 * Reference: Astrom & Hagglund (1995), Ch. 2 "PID Control"
 */
void logix_pide_convert_gains(logix_pide_t *pide);

/**
 * @brief Configure PV first-order lag filter
 *
 * Transfer function: PV_filt(s) / PV(s) = 1 / (tau*s + 1)
 *
 * Discrete approximation (backward Euler):
 *   PV_filt(n) = alpha * PV(n) + (1-alpha) * PV_filt(n-1)
 *   where alpha = Ts / (Ts + tau)
 *
 * @param tau_sec Filter time constant (0 = disable)
 * Complexity: O(1)
 */
void logix_pide_set_pv_filter(logix_pide_t *pide, double tau_sec);

/**
 * @brief Enable feedforward with ratio and bias
 *
 * FFWD_eff = FFWD * ratio + bias
 *
 * Feedforward is added directly to the PID output, providing anticipatory
 * control action for measured disturbances.
 *
 * Complexity: O(1)
 * Reference: Seborg, Edgar, Mellichamp (2016), Ch. 15 "Feedforward Control"
 */
void logix_pide_set_feedforward(logix_pide_t *pide,
                                  double ratio, double bias);

/**
 * @brief Set output rate-of-change limiting
 *
 * Prevents abrupt CV changes that could damage actuators:
 *   |CV(n) - CV(n-1)| <= ROC_limit * Ts
 *
 * @param roc_limit_per_sec Max rate of change (%/sec, 0 = disabled)
 * Complexity: O(1)
 */
void logix_pide_set_roc_limit(logix_pide_t *pide, double roc_limit_per_sec);

/**
 * @brief Get the status word for GSV access
 * Complexity: O(1)
 */
uint16_t logix_pide_get_status(const logix_pide_t *pide);

/**
 * @brief Check if PIDE is in a mode that allows PV tracking
 * Complexity: O(1)
 */
bool logix_pide_is_tracking(const logix_pide_t *pide);

/**
 * @brief Compute theoretical closed-loop bandwidth given gains
 *
 * For a PI controller controlling a first-order process:
 *   K = Kp * K_process
 *   omega_bw ≈ K / (1 + K * T_process)  (rad/sec)
 *
 * This is the approximate closed-loop bandwidth for a well-tuned
 * PI controller on a first-order-plus-deadtime process.
 *
 * @param process_gain   Static gain of the process
 * @param process_tau    Dominant time constant of the process
 * @param pide           Configured PIDE gains
 * @return Estimated closed-loop bandwidth in rad/sec
 *
 * Complexity: O(1)
 * Reference: Skogestad (2003), "Simple analytic rules for PID tuning"
 */
double logix_pide_closed_loop_bandwidth(const logix_pide_t *pide,
                                          double process_gain,
                                          double process_tau);

#endif /* LOGIX_PIDE_INSTRUCTION_H */