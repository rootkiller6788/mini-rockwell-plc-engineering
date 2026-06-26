/**
 * @file    logix_alarm_model.h
 * @brief   L6: ControlLogix ALMA/ALMD Alarm Instructions
 *
 * Knowledge Mapping:
 *   L1 Definitions:  ALMA (Analog Alarm), ALMD (Digital Alarm),
 *                    alarm severity, alarm class, alarm state machine,
 *                    alarm acknowledge/suppress/disable/shelve
 *   L2 Concepts:     ISA-18.2 alarm management lifecycle, alarm
 *                    rationalization, alarm flooding prevention,
 *                    shelving vs suppression
 *   L6 Problems:     Tank level high-high alarm, motor fault alarm,
 *                    reactor temperature deviation alarm
 *   L4 Standards:    ISA-18.2 Alarm Management, EEMUA 191
 *   L7 Applications: FactoryTalk Alarms and Events / PlantPAx
 *
 * Reference: Rockwell Automation Publication 1756-RM006
 *   "Logix5000 Controllers Process Control and Drives Instructions"
 *
 * Course Alignment:
 *   Purdue ME 575 - Industrial Control: alarm management
 *   ISA-18.2 - Alarm Management for the Process Industries
 *   ISA-101 - HMI design standards
 */

#ifndef LOGIX_ALARM_MODEL_H
#define LOGIX_ALARM_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Alarm Severity and Classification
 *
 * ISA-18.2 defines alarm severity based on consequence:
 *   - Critical: Immediate operator action required (safety)
 *   - High: Prompt operator action (environmental/equipment)
 *   - Medium: Timely operator action (quality)
 *   - Low: Operator awareness (advisory)
 *
 * Alarm class maps to PlantPAx alarm categories:
 *   - Safety: SIL-related alarms
 *   - Process: Process variable deviations
 *   - Device: Equipment fault/failure
 *   - System: Controller/network diagnostic
 *
 * EEMUA 191 guidelines:
 *   - Average alarm rate: < 1 per 10 minutes (steady state)
 *   - Peak alarm rate: < 10 per 10 minutes (upset)
 *   - Standing alarms: < 10 (no alarms suppressed/inhibited indefinitely)
 *   - Alarm floods: < 10 per hour (burst of > 10 alarms in 10 min)
 *
 * Reference: ISA-18.2-2016
 *            EEMUA Publication 191 (2013)
 * ======================================================================== */

typedef enum {
    ALARM_SEVERITY_CRITICAL = 0,
    ALARM_SEVERITY_HIGH     = 1,
    ALARM_SEVERITY_MEDIUM   = 2,
    ALARM_SEVERITY_LOW      = 3
} alarm_severity_t;

typedef enum {
    ALARM_CLASS_SAFETY   = 0,
    ALARM_CLASS_PROCESS  = 1,
    ALARM_CLASS_DEVICE   = 2,
    ALARM_CLASS_SYSTEM   = 3
} alarm_class_t;

/* ========================================================================
 * L2: Alarm State Machine
 *
 * The alarm follows a defined state machine:
 *
 *   NORMAL --(condition true)--> ACTIVE_UNACK
 *   ACTIVE_UNACK --(acknowledge)--> ACTIVE_ACK
 *   ACTIVE_ACK --(condition false)--> NORMAL_UNACK
 *   ACTIVE_UNACK --(condition false)--> NORMAL_UNACK
 *   NORMAL_UNACK --(acknowledge)--> NORMAL
 *   NORMAL_UNACK --(condition true)--> ACTIVE_UNACK
 *
 * Suppressing (setting) and disabling also affect states:
 *   - Suppressed: Alarm condition detection paused (maintenance)
 *   - Disabled: Alarm removed from annunciation (out of service)
 *   - Shelved: Alarm temporarily suppressed with auto-unshelve timer
 *
 * Reference: ISA-18.2, Chapter 8 "Alarm States"
 * ======================================================================== */

typedef enum {
    ALARM_STATE_NORMAL        = 0,
    ALARM_STATE_ACTIVE_UNACK  = 1,
    ALARM_STATE_ACTIVE_ACK    = 2,
    ALARM_STATE_NORMAL_UNACK  = 3,
    ALARM_STATE_SUPPRESSED    = 4,
    ALARM_STATE_DISABLED      = 5,
    ALARM_STATE_SHELVED       = 6
} alarm_state_t;

/* ========================================================================
 * L1: Analog Alarm (ALMA)
 *
 * The ALMA instruction monitors an analog process variable against
 * configurable alarm limits:
 *
 *   - High-High (HH): Highest severity, immediate action
 *   - High (H):       Warning, may escalate to HH
 *   - Low (L):        Warning, may escalate to LL
 *   - Low-Low (LL):   Highest severity for low side
 *
 * Hysteresis (deadband) prevents alarm chatter near trip points.
 * On-delay and off-delay timers prevent nuisance alarms from noise.
 *
 * Each limit has its own severity, message, and can be individually
 * enabled/disabled.
 *
 * Common applications:
 *   - Tank level: L-101 (LL=10%, L=20%, H=80%, HH=90%)
 *   - Reactor temperature: TIC-201 (LL=15C, L=18C, H=32C, HH=35C)
 *   - Column pressure: PIC-301 (LL=0.5bar, L=0.8bar, H=2.2bar, HH=2.5bar)
 *
 * Reference: 1756-RM006, Chapter 1 "ALMA Instruction"
 * ======================================================================== */

typedef struct {
    /* Alarm limits */
    double      high_high_limit;      /* HH trip point */
    double      high_limit;           /* H trip point */
    double      low_limit;            /* L trip point */
    double      low_low_limit;        /* LL trip point */

    /* Hysteresis (deadband) */
    double      deadband;             /* % of span for hysteresis */

    /* Time delays (seconds) */
    double      on_delay_sec;         /* Condition must be true this long */
    double      off_delay_sec;        /* Condition must be false this long */

    /* Alarm enable/disable */
    bool        hh_enabled;
    bool        h_enabled;
    bool        l_enabled;
    bool        ll_enabled;

    /* Input and status */
    double      input;                /* Process variable value */
    double      eu_min;               /* Engineering units minimum */
    double      eu_max;               /* Engineering units maximum */

    /* State per alarm level */
    bool        hh_condition;         /* HH trip condition active */
    bool        h_condition;
    bool        l_condition;
    bool        ll_condition;

    bool        hh_active;            /* HH alarm active (after delay) */
    bool        h_active;
    bool        l_active;
    bool        ll_active;

    bool        hh_acknowledged;
    bool        h_acknowledged;
    bool        l_acknowledged;
    bool        ll_acknowledged;

    bool        any_alarm_active;     /* OR of all active alarms */

    /* Timers (elapsed seconds for on/off delays) */
    double      hh_on_timer;
    double      h_on_timer;
    double      l_on_timer;
    double      ll_on_timer;
    double      hh_off_timer;
    double      h_off_timer;
    double      l_off_timer;
    double      ll_off_timer;

    /* Global alarm state */
    bool        suppressed;           /* Maintenance suppression */
    bool        disabled;             /* Out of service */
    bool        shelved;              /* Temporarily shelved */
    double      shelve_time_remaining_sec;

    /* Messages (for HMI display) */
    char        hh_message[128];
    char        h_message[128];
    char        l_message[128];
    char        ll_message[128];
    char        tag_name[64];
    alarm_severity_t hh_severity;
    alarm_severity_t h_severity;
    alarm_severity_t l_severity;
    alarm_severity_t ll_severity;

    /* Execution */
    double      ts_sec;               /* Sample time */
} logix_alma_t;

/* ========================================================================
 * L1: Digital Alarm (ALMD)
 *
 * The ALMD instruction monitors a boolean condition signal:
 *   - Input is true = alarm condition
 *   - Supports on-delay and off-delay
 *   - Can be latched (requires manual acknowledge even if condition clears)
 *   - Supports severity classification
 *
 * Common applications:
 *   - Motor fault: MTR-501 Faulted (from D2SD fault output)
 *   - Emergency stop: E-Stop pressed
 *   - Communication loss: MSG instruction timeout
 *   - Guard door open: Safety gate open
 *
 * Reference: 1756-RM006, Chapter 1 "ALMD Instruction"
 * ======================================================================== */

typedef struct {
    bool        input;                /* Alarm condition input */
    bool        condition;            /* Condition after delay */
    bool        active;               /* Alarm active */
    bool        acknowledged;
    bool        latched;              /* Requires acknowledge even if cleared */
    double      on_delay_sec;
    double      off_delay_sec;
    double      on_timer;
    double      off_timer;
    alarm_severity_t severity;
    alarm_class_t class;
    bool        suppressed;
    bool        disabled;
    bool        shelved;
    double      shelve_time_remaining_sec;
    double      ts_sec;
    char        message[128];
    char        tag_name[64];
    uint64_t    timestamp_active_ms;  /* When alarm first became active */
} logix_almd_t;

/* ========================================================================
 * L6: Alarm Rationalization Data
 *
 * ISA-18.2 requires alarm rationalization documentation:
 *   - Alarm setpoint basis (design, hazard analysis)
 *   - Consequence of inaction (safety, environmental, economic)
 *   - Operator response time required
 *   - Operator action expected
 *   - Priority (based on consequence + response time)
 *
 * EEMUA 191 priority matrix:
 *   Priority = f(severity_of_consequence, time_to_respond)
 *
 * Reference: ISA-18.2, Chapter 11 "Alarm Rationalization"
 * ======================================================================== */

typedef struct {
    uint32_t alarm_id;
    char     tag_name[64];
    char     description[256];
    alarm_class_t class;
    alarm_severity_t severity;
    double   consequence_safety;       /* 0=none, 1=minor injury, 5=fatality */
    double   consequence_environmental; /* 0=none, 5=major release */
    double   consequence_economic;     /* $ cost of event */
    double   response_time_sec;        /* Required operator response time */
    char     operator_action[256];     /* Expected operator action */
    bool     is_rationalized;          /* Has been reviewed */
    char     reviewed_by[64];
    uint64_t review_date;              /* Unix timestamp */
    uint32_t priority;                 /* Computed priority 1 (highest) - 5 */
} alarm_rationalization_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief Initialize an ALMA (Analog Alarm) instruction
 * Complexity: O(1)
 */
void logix_alma_init(logix_alma_t *alarm, const char *tag_name,
                      double ts_sec);

/**
 * @brief Configure ALMA alarm limits
 * Complexity: O(1)
 */
void logix_alma_set_limits(logix_alma_t *alarm,
                             double hh, double h, double l, double ll,
                             double deadband, double eu_min, double eu_max);

/**
 * @brief Execute ALMA alarm evaluation
 *
 * Evaluates all four alarm conditions with hysteresis and delay timers.
 * Updates alarm state for each level.
 *
 * @param alarm   Alarm instance
 * @param pv      Current process variable value
 * Complexity: O(1)
 */
void logix_alma_execute(logix_alma_t *alarm, double pv);

/**
 * @brief Acknowledge all active alarms in ALMA
 * Complexity: O(1)
 */
void logix_alma_acknowledge(logix_alma_t *alarm);

/**
 * @brief Suppress/unsuppress all alarms
 * Complexity: O(1)
 */
void logix_alma_suppress(logix_alma_t *alarm, bool suppress);

/**
 * @brief Shelve alarms for a specified duration
 *
 * ISA-18.2: Shelving is for temporary suppression with automatic
 * restoration. Auditable event. Must be authorized.
 *
 * @param duration_sec Duration to shelve (max typically 72 hours per ISA-18.2)
 * Complexity: O(1)
 */
void logix_alma_shelve(logix_alma_t *alarm, double duration_sec);

/**
 * @brief Initialize an ALMD (Digital Alarm) instruction
 * Complexity: O(1)
 */
void logix_almd_init(logix_almd_t *alarm, const char *tag_name,
                      double ts_sec, bool latched);

/**
 * @brief Execute ALMD alarm evaluation
 * Complexity: O(1)
 */
void logix_almd_execute(logix_almd_t *alarm, bool input);

/**
 * @brief Acknowledge ALMD alarm
 * Complexity: O(1)
 */
void logix_almd_acknowledge(logix_almd_t *alarm);

/**
 * @brief Suppress/unsuppress ALMD alarm
 * Complexity: O(1)
 */
void logix_almd_suppress(logix_almd_t *alarm, bool suppress);

/**
 * @brief Shelve ALMD alarm
 * Complexity: O(1)
 */
void logix_almd_shelve(logix_almd_t *alarm, double duration_sec);

/**
 * @brief Calculate alarm priority per EEMUA 191 matrix
 *
 * Priority = severity_weight * 2 + (response_time < 60 ? 1 :
 *             response_time < 300 ? 0 : -1)
 *
 * Clamped to range [1, 5], where 1 = highest priority.
 *
 * @param severity_rank  Severity rank (1=critical, 4=low)
 * @param response_sec   Required operator response time
 * @return Priority 1-5
 *
 * Complexity: O(1)
 * Reference: EEMUA 191, Appendix 6 "Alarm Priority"
 */
uint32_t logix_alarm_compute_priority(uint32_t severity_rank,
                                        double response_sec);

/**
 * @brief Evaluate alarm flooding condition
 *
 * ISA-18.2 flood detection: > 10 alarms in a 10-minute window.
 *
 * @param alarm_timestamps  Array of alarm trigger timestamps (Unix ms)
 * @param count             Number of timestamps
 * @param window_sec        Flood detection window (600 sec default)
 * @param max_alarms        Max alarms in window (10 default)
 * @return true if flood condition detected
 *
 * Complexity: O(n)
 * Reference: ISA-18.2, Annex C "Alarm System Performance Metrics"
 */
bool logix_alarm_detect_flood(const uint64_t *alarm_timestamps,
                                uint32_t count,
                                double window_sec,
                                uint32_t max_alarms);

/**
 * @brief Compute alarm KPI metrics
 *
 * Metrics per EEMUA 191:
 *   - Average alarm rate: alarms per 10 minutes
 *   - Peak alarm rate: max alarms in any 10-min window
 *   - Standing alarm count: alarms active > 24 hours
 *   - Chattering alarm count: alarms cycling > 3x per minute
 *
 * @param alarm_events      Array of (timestamp, alarm_id) pairs
 * @param event_count       Number of events
 * @param avg_rate          [out] Average alarms per 10 minutes
 * @param peak_rate         [out] Peak alarms per 10 minutes
 * @param standing_count    [out] Standing alarms count
 *
 * Complexity: O(n)
 * Reference: EEMUA 191, Chapter 7 "Alarm System KPIs"
 */
void logix_alarm_compute_kpi(const uint64_t *alarm_timestamps,
                               uint32_t event_count,
                               double *avg_rate,
                               double *peak_rate,
                               uint32_t *standing_count);

#endif /* LOGIX_ALARM_MODEL_H */