/**
 * @file    logix_instruction_set.h
 * @brief   L5: ControlLogix Specialized Instruction Set
 *
 * Knowledge Mapping:
 *   L5 Algorithms:    TOT (Totalizer), LDLG (Low-Pass Lead-Lag Filter),
 *                     D2SD (Discrete 2-State Device), D3SD (Discrete 3-State Device),
 *                     SRTP (Split Range Time Proportioning), FAL (File Arithmetic
 *                     and Logic), FSC (File Search and Compare), SCL (Scale),
 *                     SQO (Sequencer Output), TON/TOF/RTO (Timers),
 *                     CTU/CTD (Counters)
 *   L7 Applications:  Rockwell ControlLogix process instruction library
 *
 * Reference: Rockwell Automation Publication 1756-RM006
 *   "Logix5000 Controllers Process Control and Drives Instructions"
 *
 * Course Alignment:
 *   RWTH Aachen - PLC/SCADA Engineering: PLC instruction sets
 *   Purdue ME 575 - Industrial Control: process instrumentation
 *   ISA/IEC 61131-3: standard function blocks
 */

#ifndef LOGIX_INSTRUCTION_SET_H
#define LOGIX_INSTRUCTION_SET_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L5: Timer Instructions (TON, TOF, RTO)
 *
 * These are the three timer types defined in IEC 61131-3 and implemented
 * in all Logix 5000 controllers:
 *
 * TON (Timer On-Delay): Accumulates time while Enable is true.
 *   DN (Done) set when ACC >= PRE.
 *   ACC resets to 0 when Enable goes false.
 *
 * TOF (Timer Off-Delay): DN set when Enable is true; starts timing
 *   when Enable goes false. DN cleared when ACC >= PRE (after Enable false).
 *
 * RTO (Retentive Timer): Like TON but ACC retained when Enable goes false.
 *   Requires RES (Reset) instruction to clear ACC.
 *
 * Time base: milliseconds (1ms resolution)
 * PRE range: 0 to 2,147,483,647 ms (approximately 24.8 days)
 *
 * Reference: 1756-RM003 "Logix5000 Controllers General Instructions"
 * ======================================================================== */

typedef enum {
    TIMER_TYPE_TON = 0,
    TIMER_TYPE_TOF = 1,
    TIMER_TYPE_RTO = 2
} logix_timer_type_t;

typedef struct {
    logix_timer_type_t type;
    uint32_t           pre;      /* Preset value (ms) */
    uint32_t           acc;      /* Accumulated value (ms) */
    bool               enable;   /* Enable input */
    bool               dn;       /* Done bit (ACC >= PRE) */
    bool               tt;       /* Timer Timing bit (Enable && !DN) */
    bool               en;       /* Enable output (mirrors Enable) */
    uint64_t           last_time_ms;  /* Last execution timestamp */
} logix_timer_t;

/* ========================================================================
 * L5: Counter Instructions (CTU, CTD, CTUD)
 *
 * CTU (Count Up): Increments ACC on false-to-true transition of CU.
 *   DN set when ACC >= PRE.
 *
 * CTD (Count Down): Decrements ACC on false-to-true transition of CD.
 *   DN set when ACC <= 0.
 *
 * CTUD (Count Up/Down): Combines both. CU increments, CD decrements.
 *   OV (Overflow) set on overflow (> 2,147,483,647).
 *   UN (Underflow) set on underflow (< -2,147,483,648).
 *
 * Counter range: -2,147,483,648 to 2,147,483,647 (signed 32-bit)
 *
 * Reference: 1756-RM003
 * ======================================================================== */

typedef struct {
    int32_t   pre;       /* Preset value */
    int32_t   acc;       /* Accumulated count */
    bool      cu;        /* Count Up trigger */
    bool      cd;        /* Count Down trigger */
    bool      res;       /* Reset (clears ACC to 0, clears OV/UN/DN) */
    bool      dn;        /* Done bit */
    bool      cu_prev;   /* Previous scan value for edge detection */
    bool      cd_prev;   /* Previous scan value for edge detection */
    bool      ov;        /* Overflow flag */
    bool      un;        /* Underflow flag */
} logix_counter_t;

/* ========================================================================
 * L5: Totalizer (TOT)
 *
 * The TOT instruction integrates a flow rate signal over time to compute
 * total accumulated quantity. Used extensively for:
 *   - Batch totalization (mass/volume dispensed)
 *   - Utility metering (steam, water, gas)
 *   - Production counting
 *
 * Algorithm:
 *   Total(n) = Total(n-1) + (Rate(n) + Rate(n-1))/2 * Ts * Gain * TimeFactor
 *
 * The trapezoidal integration method provides better accuracy than
 * simple rectangular integration for varying flows.
 *
 * Anti-windup: Total clamps at Target when TargetDeviation1 is configured.
 * The EnableIn bit can reset the totalizer.
 *
 * Reference: 1756-RM006, Chapter 1 "TOT Instruction"
 * ======================================================================== */

typedef struct {
    double   gain;            /* Scale factor */
    double   time_base_factor; /* Converts rate/sec to rate/time base (e.g., 3600 for /hr) */
    double   rate;            /* Current input rate (engineering units/time base) */
    double   rate_prev;       /* Previous input rate */
    double   ts_sec;          /* Sample time */
    double   total;           /* Accumulated total */
    double   target;          /* Optional batch target value */
    double   target_dev_1;    /* Target deviation alarm 1 */
    double   target_dev_2;    /* Target deviation alarm 2 */
    bool     enable;          /* Totalizing enabled */
    bool     reset;           /* Reset total to 0 */
    bool     target_reached_1;
    bool     target_reached_2;
    bool     total_negative;  /* Underflow occurred */
    bool     total_overflow;  /* Overflow occurred */
} logix_tot_t;

/* ========================================================================
 * L5: Discrete 2-State Device (D2SD)
 *
 * Models a two-state field device with feedback monitoring:
 *   - State 0 (Off/Closed) / State 1 (On/Open)
 *   - Command output and feedback input
 *   - Transition timer: time allowed to change state
 *   - Fault detection: commanded state != feedback state after timeout
 *   - Permissive interlock input
 *
 * Common applications:
 *   - Motor start/stop
 *   - Valve open/close
 *   - Pump on/off
 *   - Conveyor start/stop
 *
 * States: Ready, Starting, Running, Stopping, Faulted, Interlocked
 *
 * Reference: 1756-RM006, Chapter 1 "D2SD Instruction"
 * ======================================================================== */

typedef enum {
    D2SD_STATE_READY       = 0,
    D2SD_STATE_STARTING    = 1,
    D2SD_STATE_RUNNING     = 2,
    D2SD_STATE_STOPPING    = 3,
    D2SD_STATE_FAULTED     = 4,
    D2SD_STATE_INTERLOCKED = 5
} d2sd_state_t;

typedef struct {
    d2sd_state_t state;
    bool    command_0;       /* Command output (1 = run/open/on) */
    bool    feedback_0;      /* Feedback input (1 = running/open/on detected) */
    bool    permissive_ok;   /* Interlock permissive (1 = ok to start) */
    bool    fault_reset;     /* Rising edge clears fault */
    bool    interlock_trip;  /* Interlock has tripped */
    bool    fault_0;         /* Fault output */
    bool    alarm_0;         /* Alarm output */
    uint32_t transition_time_ms; /* Max allowed time to change state */
    uint32_t elapsed_time_ms;    /* Time in current transition state */
    uint32_t fault_code;     /* 0=no fault, 1=fail to start, 2=fail to stop, 3=interlock trip */
    uint64_t state_entry_time_ms;
} logix_d2sd_t;

/* ========================================================================
 * L5: Discrete 3-State Device (D3SD)
 *
 * Models devices with three positions (e.g., motorized valve):
 *   - State 0 (Closed/Off) / State 1 (Mid/Traveling) / State 2 (Open/On)
 *   - Two command outputs: OpenCmd and CloseCmd
 *   - Two feedback inputs: OpenLimit and CloseLimit
 *   - Transition time monitoring in both directions
 *
 * Common applications:
 *   - Motor-operated valve (MOV)
 *   - Reversing motor (forward/stop/reverse)
 *   - Damper actuator with end-of-travel limits
 *   - Three-position diverter valve
 *
 * Reference: 1756-RM006, Chapter 1 "D3SD Instruction"
 * ======================================================================== */

typedef enum {
    D3SD_STATE_CLOSED       = 0,
    D3SD_STATE_OPENING      = 1,
    D3SD_STATE_OPEN         = 2,
    D3SD_STATE_CLOSING      = 3,
    D3SD_STATE_STOPPED      = 4,
    D3SD_STATE_FAULTED      = 5,
    D3SD_STATE_INTERLOCKED  = 6
} d3sd_state_t;

typedef struct {
    d3sd_state_t state;
    bool    cmd_open;        /* Command to open */
    bool    cmd_close;       /* Command to close */
    bool    cmd_stop;        /* Command to stop (mid-travel hold) */
    bool    fbk_open;        /* Open limit switch feedback */
    bool    fbk_close;       /* Close limit switch feedback */
    bool    permissive_ok;
    bool    fault_reset;
    bool    interlock_trip;
    bool    fault_0;
    bool    alarm_0;
    uint32_t open_time_ms;    /* Max time to open */
    uint32_t close_time_ms;   /* Max time to close */
    uint32_t elapsed_time_ms;
    uint32_t fault_code;
    uint64_t state_entry_time_ms;
} logix_d3sd_t;

/* ========================================================================
 * L5: Low-Pass Lead-Lag Filter (LDLG)
 *
 * A second-order filter with lead-lag compensation:
 *
 * Transfer function:
 *   Y(s)/X(s) = K * (T_lead*s + 1) / (T_lag*s + 1)^order
 *
 * Discrete implementation uses backward difference approximation.
 *
 * Order = 1: first-order lag with optional lead
 * Order = 2: second-order lag with optional lead
 *
 * Applications:
 *   - Signal smoothing with phase advance
 *   - Feedforward dynamic compensation
 *   - Sensor noise filtering
 *   - Derivative approximation with filtering
 *
 * Reference: 1756-RM006, Chapter 1 "LDLG Instruction"
 *            Seborg, Edgar, Mellichamp (2016), Ch. 6 "Dynamic Models"
 * ======================================================================== */

typedef struct {
    double   k;             /* Overall gain */
    double   t_lead_sec;    /* Lead time constant (0 = pure lag) */
    double   t_lag_sec;     /* Lag time constant */
    uint32_t order;         /* Filter order (1 or 2) */
    double   ts_sec;        /* Sample time */
    double   input;
    double   output;
    double   x_prev;        /* X(n-1) state */
    double   x_prev2;       /* X(n-2) state for second order */
    double   y_prev;        /* Y(n-1) state */
    double   y_prev2;       /* Y(n-2) state */
    bool     initialized;
    bool     hold;          /* Hold output at current value */
} logix_ldlg_t;

/* ========================================================================
 * L5: Split Range Time Proportioning (SRTP)
 *
 * Controls two final control elements with a single PID output by
 * splitting the output range, typically using time-proportioned
 * on/off signals for modulating devices with slow cycle times.
 *
 * Example: Heating/Cooling split-range on a single PID output
 *   0-50% CV   -> Cooling valve (time proportioned)
 *   50-100% CV -> Heating valve (time proportioned)
 *   50% CV     -> Deadband (both off)
 *
 * Time proportioning period: typically 1-30 seconds for thermal systems
 * Minimum on/off time: prevents short-cycling of mechanical devices
 *
 * Reference: 1756-RM006, Chapter 1 "SRTP Instruction"
 *            Myke King (2016), "Process Control: A Practical Approach"
 * ======================================================================== */

typedef struct {
    double   input_cv;          /* PID output 0-100% */
    double   split_point;       /* CV split point (default 50%) */
    double   deadband_pct;      /* Deadband around split point */
    double   cycle_period_sec;  /* Time proportioning cycle period */
    double   min_on_time_sec;   /* Minimum on time to prevent short-cycling */
    double   min_off_time_sec;  /* Minimum off time */
    double   ts_sec;            /* Execution sample time */
    bool     output_0;          /* First device on/off (below split point) */
    bool     output_1;          /* Second device on/off (above split point) */
    double   accum_time_0;      /* Time accumulator for device 0 */
    double   accum_time_1;      /* Time accumulator for device 1 */
    double   elapsed_cycle;     /* Elapsed time in current cycle */
    bool     is_heating;        /* Output_1 is heating (accumulator direction) */
} logix_srtp_t;

/* ========================================================================
 * L5: Scale (SCL) Instruction
 *
 * Performs linear scaling of an input value from one range to another:
 *   Output = (Input - InMin) / (InMax - InMin) * (OutMax - OutMin) + OutMin
 *
 * Handles reverse scaling (when OutMax < OutMin) and input limiting.
 *
 * Reference: 1756-RM003
 * ======================================================================== */

typedef struct {
    double   input;
    double   output;
    double   in_min;
    double   in_max;
    double   out_min;
    double   out_max;
    bool     limit_input;    /* Clamp input to [in_min, in_max] */
    bool     input_in_range;
} logix_scl_t;

/* ========================================================================
 * API
 * ======================================================================== */

/* Timer operations */
void logix_timer_init(logix_timer_t *timer, logix_timer_type_t type,
                       uint32_t preset_ms);
bool logix_timer_execute(logix_timer_t *timer, bool enable,
                          uint64_t current_time_ms);
void logix_timer_reset(logix_timer_t *timer);

/* Counter operations */
void logix_counter_init(logix_counter_t *ctr, int32_t preset);
void logix_counter_execute(logix_counter_t *ctr, bool cu, bool cd, bool res);
void logix_counter_reset(logix_counter_t *ctr);

/* Totalizer operations */
void logix_tot_init(logix_tot_t *tot, double gain, double time_base_factor,
                     double ts_sec);
double logix_tot_execute(logix_tot_t *tot, double rate, bool enable, bool reset);
void logix_tot_set_target(logix_tot_t *tot, double target,
                           double dev1, double dev2);

/* D2SD operations */
void logix_d2sd_init(logix_d2sd_t *dev, uint32_t transition_time_ms);
d2sd_state_t logix_d2sd_execute(logix_d2sd_t *dev, bool cmd,
                                  bool fbk, bool permissive,
                                  bool fault_reset, uint64_t time_ms);

/* D3SD operations */
void logix_d3sd_init(logix_d3sd_t *dev, uint32_t open_time_ms,
                      uint32_t close_time_ms);
d3sd_state_t logix_d3sd_execute(logix_d3sd_t *dev, bool cmd_open,
                                  bool cmd_close, bool cmd_stop,
                                  bool fbk_open, bool fbk_close,
                                  bool permissive, bool fault_reset,
                                  uint64_t time_ms);

/* LDLG Filter operations */
void logix_ldlg_init(logix_ldlg_t *filter, double k, double t_lead_sec,
                      double t_lag_sec, uint32_t order, double ts_sec);
double logix_ldlg_execute(logix_ldlg_t *filter, double input);
void logix_ldlg_reset(logix_ldlg_t *filter);
void logix_ldlg_hold(logix_ldlg_t *filter, bool hold);

/* SRTP operations */
void logix_srtp_init(logix_srtp_t *srtp, double split_point,
                      double deadband_pct, double cycle_period_sec,
                      double min_on_sec, double min_off_sec,
                      double ts_sec);
void logix_srtp_execute(logix_srtp_t *srtp, double cv_input,
                         bool *output_0, bool *output_1);

/* Scale operations */
void logix_scl_init(logix_scl_t *scl, double in_min, double in_max,
                     double out_min, double out_max, bool limit_input);
double logix_scl_execute(logix_scl_t *scl, double input);

#endif /* LOGIX_INSTRUCTION_SET_H */